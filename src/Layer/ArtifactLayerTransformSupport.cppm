#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

#include <QMatrix4x4>
#include <QPointF>
#include <QTransform>
#include <QVector3D>

#include <DiligentCore/Common/interface/BasicMath.hpp>

import Artifact.Layer.Abstract;
import Container.NamedVector;
import Time.Rational;
import Transform.Hlper;

namespace Artifact {

QPointF parentAutoLayoutOffset(const ArtifactAbstractLayer* layer,
                               const ArtifactAbstractLayerPtr& parent);

ArtifactCore::NamedVector<TwoPointFiveDRenderPass>
buildTwoPointFiveDRenderPasses(const ArtifactAbstractLayer* layer,
                               const QMatrix4x4& baseTransform,
                               bool enabled, float configuredDepth,
                               float configuredCameraDistance,
                               bool depthOfFieldEnabled, float focusDepth,
                               float focusRange, float maxBlur,
                               bool motionBlurEnabled, float shutterAngle,
                               int configuredMotionSamples) {
  using ArtifactCore::ContainerName;
  using ArtifactCore::NamedVector;
  if (!layer || layer->is3D() || !enabled) {
    return NamedVector<TwoPointFiveDRenderPass>{
        ContainerName{"Layer.TwoPointFiveDPasses"}, {{baseTransform, 1.0f}}};
  }
  const float cameraDistance = std::max(1.0f, configuredCameraDistance);
  const float depth = std::clamp(configuredDepth, -cameraDistance * 0.95f,
                                 cameraDistance * 0.95f);
  const float perspectiveScale =
      cameraDistance / std::max(1.0f, cameraDistance - depth);
  const QPointF center = layer->localBounds().center();
  QMatrix4x4 projected = baseTransform;
  projected.translate(static_cast<float>(center.x()), static_cast<float>(center.y()), 0.0f);
  projected.scale(perspectiveScale, perspectiveScale, 1.0f);
  projected.translate(static_cast<float>(-center.x()), static_cast<float>(-center.y()), depth);

  const int motionSamples = motionBlurEnabled && shutterAngle > 0.0f
      ? configuredMotionSamples : 1;
  QPointF previousDelta;
  if (motionSamples > 1 && layer->currentFrame() > 0) {
    previousDelta = layer->getGlobalTransformAt(layer->currentFrame() - 1).map(center) -
                    layer->getGlobalTransformAt(layer->currentFrame()).map(center);
    previousDelta *= std::clamp(shutterAngle / 360.0f, 0.0f, 2.0f);
  }
  const float focusBlur = depthOfFieldEnabled
      ? std::min(maxBlur, std::abs(depth - focusDepth) /
                              std::max(1.0f, focusRange) * maxBlur)
      : 0.0f;
  const std::array<QPointF, 5> blurOffsets = {
      QPointF(0.0, 0.0), QPointF(focusBlur, 0.0), QPointF(-focusBlur, 0.0),
      QPointF(0.0, focusBlur), QPointF(0.0, -focusBlur)};
  const int blurSamples = focusBlur > 0.1f ? static_cast<int>(blurOffsets.size()) : 1;
  NamedVector<TwoPointFiveDRenderPass> passes{
      ContainerName{"Layer.TwoPointFiveDPasses"}};
  passes.reserve(static_cast<std::size_t>(motionSamples * blurSamples));
  const float passOpacity = 1.0f / static_cast<float>(motionSamples * blurSamples);
  for (int motionIndex = 0; motionIndex < motionSamples; ++motionIndex) {
    const float motionT = motionSamples > 1
        ? static_cast<float>(motionIndex) / static_cast<float>(motionSamples - 1) : 1.0f;
    for (int blurIndex = 0; blurIndex < blurSamples; ++blurIndex) {
      const QPointF offset = previousDelta * (1.0f - motionT) + blurOffsets[blurIndex];
      QMatrix4x4 screenOffset;
      screenOffset.translate(static_cast<float>(offset.x()), static_cast<float>(offset.y()), 0.0f);
      passes.add({screenOffset * projected, passOpacity});
    }
  }
  return passes;
}

QTransform composeLayerGlobalTransformAt(const ArtifactAbstractLayer* layer,
                                         int64_t frameNumber,
                                         bool layoutComponentEnabled,
                                         bool layoutResponsiveEnabled) {
  if (!layer) return {};
  QTransform local = layer->getLocalTransformAt(frameNumber);
  const auto parent = layer->parentLayer();
  if (!parent) return local;
  const QPointF layoutOffset = layoutComponentEnabled && layoutResponsiveEnabled
      ? QPointF() : parentAutoLayoutOffset(layer, parent);
  if (!layoutOffset.isNull()) {
    local = QTransform::fromTranslate(layoutOffset.x(), layoutOffset.y()) * local;
  }
  return combineLayerTransform2D(local, parent->getGlobalTransformAt(frameNumber));
}

QMatrix4x4 ArtifactAbstractLayer::getGlobalTransform4x4() const {
  const QMatrix4x4 local = getLocalTransform4x4();
  const auto parent = parentLayer();
  return parent ? combineLayerTransform3D(local, parent->getGlobalTransform4x4())
                : local;
}

QMatrix4x4 ArtifactAbstractLayer::getLocalTransform4x4At(
    const RationalTime& time) const {
  const auto snapshot = transform3D().snapshotAt(time);
  QMatrix4x4 result;
  result.setToIdentity();
  result.translate(snapshot.positionX, snapshot.positionY, snapshot.positionZ);
  result.rotate(snapshot.rotationX, 1.0f, 0.0f, 0.0f);
  result.rotate(snapshot.rotationY, 0.0f, 1.0f, 0.0f);
  result.rotate(snapshot.rotationZ, 0.0f, 0.0f, 1.0f);
  result.scale(snapshot.scaleX, snapshot.scaleY, snapshot.scaleZ);
  result.translate(-snapshot.anchorX, -snapshot.anchorY, -snapshot.anchorZ);
  return result;
}

QMatrix4x4 ArtifactAbstractLayer::getGlobalTransform4x4At(
    const RationalTime& time) const {
  const QMatrix4x4 local = getLocalTransform4x4At(time);
  const auto parent = parentLayer();
  return parent ? combineLayerTransform3D(local, parent->getGlobalTransform4x4At(time))
                : local;
}

Diligent::float4x4 ArtifactAbstractLayer::getLocalTransformMatrix() const {
  const QMatrix4x4 source = getLocalTransform4x4();
  return Diligent::float4x4{
      source(0, 0), source(0, 1), source(0, 2), source(0, 3),
      source(1, 0), source(1, 1), source(1, 2), source(1, 3),
      source(2, 0), source(2, 1), source(2, 2), source(2, 3),
      source(3, 0), source(3, 1), source(3, 2), source(3, 3)};
}

Diligent::float4x4 ArtifactAbstractLayer::getGlobalTransformMatrix() const {
  const Diligent::float4x4 local = getLocalTransformMatrix();
  if (const auto parent = parentLayer()) {
    return parent->getGlobalTransformMatrix() * local;
  }
  return local;
}

} // namespace Artifact
