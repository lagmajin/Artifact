module;

#define QT_NO_KEYWORDS

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <QElapsedTimer>
#include <QLineF>
#include <QMatrix4x4>
#include <QPointF>
#include <QQuaternion>
#include <QRect>
#include <QRectF>
#include <QSet>
#include <QSize>
#include <QString>
#include <QTransform>
#include <QVector>
#include <QVector3D>
#include <QVector4D>
#include <QtGlobal>

module Artifact.Widgets.CompositionRenderController:ViewportGeometry;

import Artifact.Widgets.CompositionRenderController;
import Animation.Transform3D;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Camera;
import Artifact.Layer.Construction;
import Artifact.Layer.Image;
import Artifact.Layer.Paint;
import Artifact.Layer.Shape;
import Artifact.Layer.Solid2D;
import Artifact.Layers.Model3D;
import Artifact.Layers.SolidImage;
import Artifact.Layers.Selection.Manager;
import Artifact.Render.IRenderer;
import Artifact.Service.Playback;
import Artifact.Widgets.CompositionMotionPathCommands;
import Artifact.Widgets.Gizmo3D;
import Artifact.Widgets.TransformGizmo;
import Artifact.Widgets.ViewportMath;
import Composition.Settings;
import Core.ArtifactArray;
import Color.Float;
import Frame.Position;
import Frame.Rate;
import Geometry.CameraGuide;
import Math.Interpolate;
import Math.Vec;
import Property.Abstract;
import Time.Rational;
import Utils.Id;

namespace Artifact {

ArtifactCore::Coordinates::ScreenPhysicalPoint2 screenPhysicalPoint(
    const QPointF &point) noexcept {
  return {static_cast<float>(point.x()), static_cast<float>(point.y())};
}

QPointF qPointFromScreenPhysicalPoint(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 point) noexcept {
  return ArtifactCore::Coordinates::toQPointF(point);
}

ArtifactCore::Coordinates::ScreenPhysicalBounds2 screenPhysicalBoundsFromQRectF(
    const QRectF &rect) noexcept {
  if (!rect.isValid()) return {};
  return {{rect.left(), rect.top()}, {rect.right(), rect.bottom()}};
}

QRectF qRectFromScreenPhysicalBounds(
    const ArtifactCore::Coordinates::ScreenPhysicalBounds2 &bounds) noexcept {
  return QRectF(QPointF(bounds.minimum.x, bounds.minimum.y),
                QPointF(bounds.maximum.x, bounds.maximum.y));
}

bool isValidScreenPhysicalBounds(
    const ArtifactCore::Coordinates::ScreenPhysicalBounds2 &bounds) noexcept {
  const QRectF rect = qRectFromScreenPhysicalBounds(bounds);
  return rect.isValid() && rect.width() > 0.0 && rect.height() > 0.0;
}

FramePosition currentFrameForComposition(const ArtifactCompositionPtr &comp) {

  if (!comp) {

    return FramePosition(0);

  }

  FramePosition currentFrame = comp->framePosition();

  if (auto *playback = ArtifactPlaybackService::instance()) {

    const auto playbackComp = playback->currentComposition();

    if (playbackComp && playbackComp->id() == comp->id()) {

      currentFrame = playback->currentFrame();

    }

  }

  return currentFrame;

}

std::vector<RationalTime> motionPathPositionKeyTimes(
    const ArtifactAbstractLayerPtr &layer, const int fps) {
  std::vector<int64_t> frames;
  if (!layer) {
    return {};
  }

  const auto collectPropertyFrames =
      [&frames, fps](const auto &property) {
        if (!property || !property->isAnimatable()) {
          return;
        }
        for (const auto &keyframe : property->getKeyFrames()) {
          frames.push_back(keyframe.time.rescaledTo(fps));
        }
      };
  collectPropertyFrames(
      layer->getProperty(QStringLiteral("transform.position.x")));
  collectPropertyFrames(
      layer->getProperty(QStringLiteral("transform.position.y")));

  if (frames.empty()) {
    for (const auto &time : layer->transform3D().getPositionKeyFrameTimes()) {
      frames.push_back(time.rescaledTo(fps));
    }
  }

  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());

  std::vector<RationalTime> times;
  times.reserve(frames.size());
  for (const int64_t frame : frames) {
    times.emplace_back(frame, fps);
  }
  return times;
}

int motionPathPositionInterpolation(
    const ArtifactAbstractLayerPtr &layer, const RationalTime &time) {
  if (layer) {
    const auto property =
        layer->getProperty(QStringLiteral("transform.position.x"));
    if (property && property->isAnimatable()) {
      for (const auto &keyframe : property->getKeyFrames()) {
        if (keyframe.time == time ||
            keyframe.time.rescaledTo(time.scale()) == time.value()) {
          return static_cast<int>(keyframe.interpolation);
        }
      }
    }
    return static_cast<int>(
        layer->transform3D().positionXKeyFrameInterpolationAt(time));
  }
  return static_cast<int>(ArtifactCore::InterpolationType::Linear);
}



QVector<MotionPathSample>

buildMotionPathSamples(const ArtifactAbstractLayerPtr &layer,

                       const ArtifactCompositionPtr &comp) {

  QVector<MotionPathSample> samples;

  if (!layer || !comp) {

    return samples;

  }



  const int fps =
      std::max(1, static_cast<int>(std::round(comp->frameRate().framerate())));
  const auto keyTimes = motionPathPositionKeyTimes(layer, fps);

  if (keyTimes.empty()) {

    return samples;

  }



  samples.reserve(static_cast<int>(keyTimes.size()) + 1);

  // A motion path follows the layer's anchor point, not its local origin or
  // the geometric centre of its bounds.  This also makes a changed anchor
  // immediately visible as the path's pivot, matching the transform gizmo.
  const auto anchorAt = [&layer](const RationalTime &time, int64_t frame) {
    const auto &transform = layer->transform3D();
    const QTransform globalTransform = layer->getGlobalTransformAt(frame);
    return globalTransform.map(
        QPointF(transform.anchorXAt(time), transform.anchorYAt(time)));
  };

  for (const auto &time : keyTimes) {
    const int64_t frame = time.rescaledTo(fps);
    samples.push_back({anchorAt(time, frame), MotionPathSampleKind::Keyframe,
                       frame});

  }



  const FramePosition currentFrame = currentFrameForComposition(comp);

  const RationalTime currentTime(currentFrame.framePosition(), fps);

  const QPointF currentAnchor =
      anchorAt(currentTime, currentFrame.framePosition());
  samples.push_back({currentAnchor, MotionPathSampleKind::Current,
                     currentFrame.framePosition()});



  return samples;

}



bool historicalBoundsSupported(const ArtifactAbstractLayerPtr &layer) {
  if (layer) {
    if (const auto *variant = layer->getActiveVariant(); variant &&
        HasFlag(variant->overrideFlags_, VariantOverrideFlags::Transform)) return false;
    for (auto parent = layer->parentLayer(); parent; parent = parent->parentLayer()) {
      if (parent->is3D()) return false;
    }
    const ArtifactAbstractLayer *readOnlyLayer = layer.get();
    if (readOnlyLayer->animationLayerStack(QStringLiteral("transform.scale.x")) ||
        readOnlyLayer->animationLayerStack(QStringLiteral("transform.scale.y"))) return false;
  }
  return layer && !layer->is3D() && !layer->isLocked() &&
      !layer->isSelectionLocked() && layer->isVisible() &&
      (dynamic_cast<ArtifactImageLayer *>(layer.get()) ||
       dynamic_cast<ArtifactSolid2DLayer *>(layer.get()) ||
       dynamic_cast<ArtifactSolidImageLayer *>(layer.get()) ||
       dynamic_cast<ArtifactShapeLayer *>(layer.get())) &&
      layer->localBounds().isValid();
}

// Shared by drawing and picking: cap visual density to the latest eight
// position keys before the current frame. Geometry is evaluated at key time.
QVector<int64_t> historicalBoundsFrames(const ArtifactAbstractLayerPtr &layer,
                                       int currentFrame, int fps) {
  QVector<int64_t> frames;
  if (!historicalBoundsSupported(layer)) return frames;
  const auto times = motionPathPositionKeyTimes(layer, fps);
  for (const auto &time : times) {
    const auto frame = time.toFrameCount(fps);
    if (frame < currentFrame) frames.push_back(frame);
  }
  while (frames.size() > 8) frames.removeFirst();
  return frames;
}

bool hitTestMotionPathSample(const QVector<MotionPathSample> &samples,

                             const QPointF &canvasPos, float threshold,

                             MotionPathSample &outSample) {

  const float thresholdSq = threshold * threshold;

  bool found = false;

  float bestDistSq = thresholdSq;

  for (const auto &sample : samples) {

    if (sample.kind != MotionPathSampleKind::Keyframe) {

      continue;

    }

    const QPointF delta = sample.position - canvasPos;

    const float distSq = static_cast<float>(QPointF::dotProduct(delta, delta));

    if (distSq <= bestDistSq) {

      bestDistSq = distSq;

      outSample = sample;

      found = true;

    }

  }

  return found;

}



FloatColor motionPathInterpolationColor(int interpolation, bool isCurrent) {

  const auto type =

      static_cast<ArtifactCore::InterpolationType>(interpolation);

  switch (type) {

  case ArtifactCore::InterpolationType::Constant:

    return isCurrent ? FloatColor{0.42f, 0.88f, 1.0f, 1.0f}

                     : FloatColor{0.25f, 0.72f, 0.92f, 0.95f};

  case ArtifactCore::InterpolationType::Linear:

    return isCurrent ? FloatColor{1.0f, 0.92f, 0.28f, 1.0f}

                     : FloatColor{1.0f, 0.78f, 0.22f, 0.95f};

  case ArtifactCore::InterpolationType::EaseIn:

  case ArtifactCore::InterpolationType::EaseOut:

  case ArtifactCore::InterpolationType::EaseInOut:

  case ArtifactCore::InterpolationType::BackIn:

  case ArtifactCore::InterpolationType::BackOut:

  case ArtifactCore::InterpolationType::BackInOut:

  case ArtifactCore::InterpolationType::Exponential:

  case ArtifactCore::InterpolationType::Bezier:

   case ArtifactCore::InterpolationType::Sine:

   case ArtifactCore::InterpolationType::Cubic:

   case ArtifactCore::InterpolationType::Quintic:

   case ArtifactCore::InterpolationType::CubicIn:

   case ArtifactCore::InterpolationType::CubicInOut:

   case ArtifactCore::InterpolationType::QuarticIn:

   case ArtifactCore::InterpolationType::QuarticInOut:

   case ArtifactCore::InterpolationType::QuinticIn:

   case ArtifactCore::InterpolationType::QuinticInOut:

   case ArtifactCore::InterpolationType::SineIn:

   case ArtifactCore::InterpolationType::SineInOut:

   case ArtifactCore::InterpolationType::CircularIn:

   case ArtifactCore::InterpolationType::CircularInOut:

   case ArtifactCore::InterpolationType::ExponentialIn:

   case ArtifactCore::InterpolationType::ExponentialInOut:

     return isCurrent ? FloatColor{1.0f, 0.58f, 0.30f, 1.0f}

                     : FloatColor{0.97f, 0.48f, 0.24f, 0.95f};

  default:

    return isCurrent ? FloatColor{0.82f, 0.70f, 1.0f, 1.0f}

                     : FloatColor{0.72f, 0.58f, 0.95f, 0.95f};

  }

}



TransformGizmo::HandleType hitTestProjectedFrameCorner(
    const ArtifactAbstractLayerPtr &layer,
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPhysicalPos,
    const QMatrix4x4 &view, const QMatrix4x4 &projection,
    const QRect &viewport, float hitDiameter) {

  if (!layer || viewport.width() <= 0 || viewport.height() <= 0) {
    return TransformGizmo::HandleType::None;
  }

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) {
    return TransformGizmo::HandleType::None;
  }

  // Match the visible source edge; the hit rectangle supplies pointer padding.
  const QRectF frameBounds = localBounds;
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPhysicalPos);
  const QMatrix4x4 world = layer->getGlobalTransform4x4();
  const std::array<std::pair<QPointF, TransformGizmo::HandleType>, 8>
      corners{{
          {frameBounds.topLeft(), TransformGizmo::HandleType::Scale_TL},
          {frameBounds.topRight(), TransformGizmo::HandleType::Scale_TR},
          {frameBounds.bottomLeft(), TransformGizmo::HandleType::Scale_BL},
          {frameBounds.bottomRight(), TransformGizmo::HandleType::Scale_BR},
          {{frameBounds.center().x(), frameBounds.top()},
           TransformGizmo::HandleType::Scale_T},
          {{frameBounds.center().x(), frameBounds.bottom()},
           TransformGizmo::HandleType::Scale_B},
          {{frameBounds.left(), frameBounds.center().y()},
           TransformGizmo::HandleType::Scale_L},
          {{frameBounds.right(), frameBounds.center().y()},
           TransformGizmo::HandleType::Scale_R},
      }};

  const float safeHitDiameter = std::max(18.0f, hitDiameter);
  auto nearestHandle = TransformGizmo::HandleType::None;
  double nearestDistance = std::numeric_limits<double>::max();
  for (const auto &[corner, handle] : corners) {
    const QVector3D worldPoint = world.map(
        QVector3D(static_cast<float>(corner.x()),
                  static_cast<float>(corner.y()), 0.0f));
    const QVector3D projected =
        ViewportMath::projectToTopDown(worldPoint, view, projection, viewport);
    if (!std::isfinite(projected.x()) || !std::isfinite(projected.y()) ||
        projected.z() < 0.0f || projected.z() > 1.0f) {
      continue;
    }
    const QRectF hitRect(projected.x() - safeHitDiameter * 0.5f,
                         projected.y() - safeHitDiameter * 0.5f,
                         safeHitDiameter, safeHitDiameter);
    if (hitRect.contains(viewportPos)) {
      const double distance = QLineF(viewportPos,
          QPointF(projected.x(), projected.y())).length();
      if (distance < nearestDistance) {
        nearestDistance = distance;
        nearestHandle = handle;
      }
    }
  }

  // The projected frame is strictly for move and resize.  Rotation belongs
  // to the dedicated 3D rotation rings, which avoids an ambiguous target
  // collapsing into the frame centre in oblique camera views.
  return nearestHandle;
}

bool hitTestProjectedFrameInterior(const ArtifactAbstractLayerPtr &layer,
                                   ArtifactCore::Coordinates::
                                       ScreenPhysicalPoint2 viewportPos,
                                   const QMatrix4x4 &view,
                                   const QMatrix4x4 &projection,
                                   const QRect &viewport) {
  if (!layer || viewport.width() <= 0 || viewport.height() <= 0) {
    return false;
  }
  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) {
    return false;
  }
  const QRectF bounds = localBounds;
  const QMatrix4x4 world = layer->getGlobalTransform4x4();
  const std::array<QPointF, 4> localPoints{
      bounds.topLeft(), bounds.topRight(), bounds.bottomRight(),
      bounds.bottomLeft()};
  std::array<QPointF, 4> projectedPoints{};
  for (size_t i = 0; i < localPoints.size(); ++i) {
    const QPointF &p = localPoints[i];
    const QVector3D projected = ViewportMath::projectToTopDown(
        world.map(QVector3D(static_cast<float>(p.x()),
                            static_cast<float>(p.y()), 0.0f)),
        view, projection, viewport);
    if (!std::isfinite(projected.x()) || !std::isfinite(projected.y()) ||
        projected.z() < 0.0f || projected.z() > 1.0f) {
      return false;
    }
    projectedPoints[i] = QPointF(projected.x(), projected.y());
  }
  bool hasPositive = false;
  bool hasNegative = false;
  for (size_t i = 0; i < projectedPoints.size(); ++i) {
    const QPointF &a = projectedPoints[i];
    const QPointF &b = projectedPoints[(i + 1) % projectedPoints.size()];
    const double cross = (b.x() - a.x()) * (viewportPos.y - a.y()) -
                         (b.y() - a.y()) * (viewportPos.x - a.x());
    hasPositive = hasPositive || cross > 0.0;
    hasNegative = hasNegative || cross < 0.0;
  }
  return !(hasPositive && hasNegative);
}

ScreenPhysicalBounds2 projectedSelectionFrameBounds(
    const QVector<ArtifactAbstractLayerPtr> &layers, const QMatrix4x4 &view,
    const QMatrix4x4 &projection, const QRect &viewport) {
  double left = std::numeric_limits<double>::max();
  double right = std::numeric_limits<double>::lowest();
  double top = std::numeric_limits<double>::max();
  double bottom = std::numeric_limits<double>::lowest();
  bool hasPoint = false;
  for (const auto &layer : layers) {
    if (!layer) continue;
    const QRectF localBounds = layer->localBounds();
    if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
        localBounds.height() <= 0.0) continue;
    const QMatrix4x4 world = layer->getGlobalTransform4x4();
    const std::array<QPointF, 4> points{
        localBounds.topLeft(), localBounds.topRight(),
        localBounds.bottomRight(), localBounds.bottomLeft()};
    for (const QPointF &point : points) {
      const QVector3D projected = ViewportMath::projectToTopDown(
          world.map(QVector3D(static_cast<float>(point.x()),
                              static_cast<float>(point.y()), 0.0f)),
          view, projection, viewport);
      if (!std::isfinite(projected.x()) || !std::isfinite(projected.y()) ||
          projected.z() < 0.0f || projected.z() > 1.0f) continue;
      left = std::min(left, static_cast<double>(projected.x()));
      right = std::max(right, static_cast<double>(projected.x()));
      top = std::min(top, static_cast<double>(projected.y()));
      bottom = std::max(bottom, static_cast<double>(projected.y()));
      hasPoint = true;
    }
  }
  if (!hasPoint || right <= left || bottom <= top) {
    return {};
  }
  const QRectF result(QPointF(left, top), QPointF(right, bottom));
  return screenPhysicalBoundsFromQRectF(
      result.adjusted(-8.0, -8.0, 8.0, 8.0));
}

bool projectedLayerFrameCorners(
    const ArtifactAbstractLayerPtr &layer, const QMatrix4x4 &view,
    const QMatrix4x4 &projection, const QRect &viewport,
    std::array<ArtifactCore::Coordinates::ScreenPhysicalPoint2, 4>
        &projectedPoints,
    int *visibleCornerCount = nullptr) {
  if (!layer || viewport.width() <= 0 || viewport.height() <= 0) return false;
  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) return false;
  const QMatrix4x4 world = layer->getGlobalTransform4x4();
  const std::array<QPointF, 4> localPoints{
      localBounds.topLeft(), localBounds.topRight(),
      localBounds.bottomRight(), localBounds.bottomLeft()};
  int visibleCorners = 0;
  for (size_t index = 0; index < localPoints.size(); ++index) {
    const QPointF &point = localPoints[index];
    const QVector3D worldPoint = world.map(
        QVector3D(static_cast<float>(point.x()), static_cast<float>(point.y()),
                  0.0f));
    const QVector4D clip = projection * view * QVector4D(worldPoint, 1.0f);
    if (!std::isfinite(clip.w()) || clip.w() <= 0.0f) {
      return false;
    }
    const QVector3D projected = ViewportMath::projectToTopDown(
        world.map(QVector3D(static_cast<float>(point.x()),
                            static_cast<float>(point.y()), 0.0f)),
        view, projection, viewport);
    if (!std::isfinite(projected.x()) || !std::isfinite(projected.y())) {
      return false;
    }
    if (projected.z() >= 0.0f && projected.z() <= 1.0f) ++visibleCorners;
    projectedPoints[index] = {projected.x(), projected.y()};
  }
  if (visibleCornerCount) *visibleCornerCount = visibleCorners;
  return visibleCorners > 0;
}

// SPEC 3.5 / 13.1 / 13.2: the resize guide needs the handle that follows the
// pointer and the point that stays fixed. Corner handles keep the diagonally
// opposite corner, edge handles keep the opposite edge midpoint. Corner order
// matches projectedLayerFrameCorners (TL, TR, BR, BL).
bool projectedFrameGuidePoints(
    const std::array<ArtifactCore::Coordinates::ScreenPhysicalPoint2, 4>
        &corners,
    TransformGizmo::HandleType handle,
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 &movingPoint,
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 &fixedPoint) {
  const auto midpoint = [](ArtifactCore::Coordinates::ScreenPhysicalPoint2 a,
                           ArtifactCore::Coordinates::ScreenPhysicalPoint2 b) {
    return ArtifactCore::Coordinates::ScreenPhysicalPoint2{
        (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
  };
  switch (handle) {
  case TransformGizmo::HandleType::Scale_TL:
    movingPoint = corners[0];
    fixedPoint = corners[2];
    return true;
  case TransformGizmo::HandleType::Scale_TR:
    movingPoint = corners[1];
    fixedPoint = corners[3];
    return true;
  case TransformGizmo::HandleType::Scale_BR:
    movingPoint = corners[2];
    fixedPoint = corners[0];
    return true;
  case TransformGizmo::HandleType::Scale_BL:
    movingPoint = corners[3];
    fixedPoint = corners[1];
    return true;
  case TransformGizmo::HandleType::Scale_T:
    movingPoint = midpoint(corners[0], corners[1]);
    fixedPoint = midpoint(corners[2], corners[3]);
    return true;
  case TransformGizmo::HandleType::Scale_B:
    movingPoint = midpoint(corners[2], corners[3]);
    fixedPoint = midpoint(corners[0], corners[1]);
    return true;
  case TransformGizmo::HandleType::Scale_L:
    movingPoint = midpoint(corners[0], corners[3]);
    fixedPoint = midpoint(corners[1], corners[2]);
    return true;
  case TransformGizmo::HandleType::Scale_R:
    movingPoint = midpoint(corners[1], corners[2]);
    fixedPoint = midpoint(corners[0], corners[3]);
    return true;
  default:
    return false;
  }
}

bool clipProjectedFrameEdge(const QRectF &clipRect, QPointF &start,
                            QPointF &end) {
  const double dx = end.x() - start.x();
  const double dy = end.y() - start.y();
  double enter = 0.0;
  double leave = 1.0;
  const auto clip = [&enter, &leave](double direction, double distance) {
    if (std::abs(direction) <= 0.000001) return distance >= 0.0;
    const double ratio = distance / direction;
    if (direction < 0.0) {
      if (ratio > leave) return false;
      enter = std::max(enter, ratio);
    } else {
      if (ratio < enter) return false;
      leave = std::min(leave, ratio);
    }
    return true;
  };
  if (!clip(-dx, start.x() - clipRect.left()) ||
      !clip(dx, clipRect.right() - start.x()) ||
      !clip(-dy, start.y() - clipRect.top()) ||
      !clip(dy, clipRect.bottom() - start.y()) || enter > leave) {
    return false;
  }
  const QPointF originalStart = start;
  start = originalStart + QPointF(dx * enter, dy * enter);
  end = originalStart + QPointF(dx * leave, dy * leave);
  return true;
}

bool projectedFixedPlaneFrameCornersAt(
    const Artifact3DLayer &plane, int frame, const QMatrix4x4 &view,
    const QMatrix4x4 &projection, const QRect &viewport,
    std::array<ArtifactCore::Coordinates::ScreenPhysicalPoint2, 4>
        &projectedPoints) {
  const QRectF localBounds = plane.localBounds();
  if (viewport.width() <= 0 || viewport.height() <= 0 ||
      !localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) {
    return false;
  }

  // Match Artifact3DLayer::draw() so each historical frame has the same
  // position, rotation, scale, and anchor transform as the rendered plane.
  const double planeFps = plane.compositionFrameRate();
  const auto snapshot = plane.transform3D().snapshotAt(
      ArtifactCore::RationalTime(
          frame, std::isfinite(planeFps) && planeFps > 0.0 ? planeFps : 30.0));
  QMatrix4x4 world;
  world.setToIdentity();
  world.translate(snapshot.positionX, snapshot.positionY, snapshot.positionZ);
  world.rotate(snapshot.rotationX, 1.0f, 0.0f, 0.0f);
  world.rotate(snapshot.rotationY, 0.0f, 1.0f, 0.0f);
  world.rotate(snapshot.rotationZ, 0.0f, 0.0f, 1.0f);
  world.scale(snapshot.scaleX, snapshot.scaleY, snapshot.scaleZ);
  world.translate(-snapshot.anchorX, -snapshot.anchorY, -snapshot.anchorZ);

  const std::array<QPointF, 4> localPoints{
      localBounds.topLeft(), localBounds.topRight(),
      localBounds.bottomRight(), localBounds.bottomLeft()};
  for (size_t index = 0; index < localPoints.size(); ++index) {
    const QPointF &point = localPoints[index];
    const QVector3D worldPoint = world.map(
        QVector3D(static_cast<float>(point.x()), static_cast<float>(point.y()),
                  0.0f));
    const QVector4D clip = projection * view * QVector4D(worldPoint, 1.0f);
    if (!std::isfinite(clip.w()) || clip.w() <= 0.0f) {
      return false;
    }
    const QVector3D projected = ViewportMath::projectToTopDown(
        worldPoint, view, projection, viewport);
    if (!std::isfinite(projected.x()) || !std::isfinite(projected.y()) ||
        projected.z() < 0.0f || projected.z() > 1.0f) {
      return false;
    }
    projectedPoints[index] = {projected.x(), projected.y()};
  }
  return true;
}

bool intersectPickingRayFixedPlaneAt(const Artifact3DLayer &plane, int frame,
                                     const Ray &ray, QVector3D &hit) {
  const double planeFps = plane.compositionFrameRate();
  const auto snapshot = plane.transform3D().snapshotAt(
      ArtifactCore::RationalTime(
          frame, std::isfinite(planeFps) && planeFps > 0.0 ? planeFps : 30.0));
  QMatrix4x4 world;
  world.setToIdentity();
  world.translate(snapshot.positionX, snapshot.positionY, snapshot.positionZ);
  world.rotate(snapshot.rotationX, 1.0f, 0.0f, 0.0f);
  world.rotate(snapshot.rotationY, 0.0f, 1.0f, 0.0f);
  world.rotate(snapshot.rotationZ, 0.0f, 0.0f, 1.0f);
  world.scale(snapshot.scaleX, snapshot.scaleY, snapshot.scaleZ);
  world.translate(-snapshot.anchorX, -snapshot.anchorY, -snapshot.anchorZ);
  const QVector3D planePoint = world.map(QVector3D());
  const QVector3D normal = world.mapVector(QVector3D(0.0f, 0.0f, 1.0f))
                               .normalized();
  const float denominator = QVector3D::dotProduct(ray.direction, normal);
  if (std::abs(denominator) <= 0.000001f) return false;
  const float distance = QVector3D::dotProduct(planePoint - ray.origin, normal) /
                         denominator;
  if (distance <= 0.0f) return false;
  hit = ray.origin + ray.direction * distance;
  return true;
}

bool hitTestPastFixedPlaneFrameCenter(
    const ArtifactAbstractLayerPtr &layer, int currentFrame, int fps,
    const QMatrix4x4 &view, const QMatrix4x4 &projection,
    const QRect &viewport,
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPos,
    float threshold,
    int &outFrame) {
  const auto *plane = layer ? dynamic_cast<const Artifact3DLayer *>(layer.get())
                            : nullptr;
  if (!plane || plane->fixedGeometry() != FixedGeometry3D::Plane) return false;
  float bestDistance = threshold * threshold;
  bool hit = false;
  const auto keyTimes = motionPathPositionKeyTimes(layer, fps);
  for (auto it = keyTimes.rbegin(); it != keyTimes.rend(); ++it) {
    const int frame = static_cast<int>(it->value());
    if (frame >= currentFrame) continue;
    std::array<ArtifactCore::Coordinates::ScreenPhysicalPoint2, 4> corners;
    if (!projectedFixedPlaneFrameCornersAt(*plane, frame, view, projection,
                                           viewport, corners)) continue;
    float centerX = 0.0f;
    float centerY = 0.0f;
    for (const auto corner : corners) {
      centerX += corner.x;
      centerY += corner.y;
    }
    const float inverseCornerCount = 1.0f / static_cast<float>(corners.size());
    const ArtifactCore::Coordinates::ScreenPhysicalPoint2 center{
        centerX * inverseCornerCount, centerY * inverseCornerCount};
    const auto delta = center - viewportPos;
    const float distance = delta.x * delta.x + delta.y * delta.y;
    if (distance < bestDistance) {
      bestDistance = distance;
      outFrame = frame;
      hit = true;
    }
  }
  return hit;
}

void drawPastFixedPlaneMotionFrames(
    ArtifactIRenderer *renderer, const ArtifactAbstractLayerPtr &layer,
    int currentFrame, int fps, const QMatrix4x4 &view,
    const QMatrix4x4 &projection, const QRect &viewport) {
  const auto *plane = layer ? dynamic_cast<const Artifact3DLayer *>(layer.get())
                            : nullptr;
  if (!renderer || !plane ||
      plane->fixedGeometry() != FixedGeometry3D::Plane) {
    return;
  }

  const auto keyTimes = motionPathPositionKeyTimes(layer, fps);
  constexpr int kMaxPastFrames = 6;
  const QRectF visibleArea(0.0, 0.0, std::max(1, viewport.width()) - 1.0,
                           std::max(1, viewport.height()) - 1.0);
  int drawnFrames = 0;
  for (auto it = keyTimes.rbegin(); it != keyTimes.rend() &&
                                    drawnFrames < kMaxPastFrames;
       ++it) {
    const int frame = static_cast<int>(it->value());
    if (frame >= currentFrame) {
      continue;
    }
    std::array<ArtifactCore::Coordinates::ScreenPhysicalPoint2, 4> corners;
    if (!projectedFixedPlaneFrameCornersAt(*plane, frame, view, projection,
                                           viewport, corners)) {
      continue;
    }

    const float age = static_cast<float>(drawnFrames) /
                      static_cast<float>(kMaxPastFrames);
    const FloatColor color{0.42f, 0.76f, 1.0f, 0.50f * (1.0f - age)};
    for (size_t index = 0; index < corners.size(); ++index) {
      QPointF start = qPointFromScreenPhysicalPoint(corners[index]);
      QPointF end = qPointFromScreenPhysicalPoint(
          corners[(index + 1) % corners.size()]);
      if (!clipProjectedFrameEdge(visibleArea, start, end)) {
        continue;
      }
      renderer->drawDashedLineLocal(
          {static_cast<float>(start.x()), static_cast<float>(start.y())},
          {static_cast<float>(end.x()), static_cast<float>(end.y())}, 1.15f,
          6.0f, 4.0f, color);
    }
    float centerX = 0.0f;
    float centerY = 0.0f;
    for (const auto corner : corners) {
      centerX += corner.x;
      centerY += corner.y;
    }
    const float inverseCornerCount = 1.0f / static_cast<float>(corners.size());
    renderer->drawPoint(centerX * inverseCornerCount,
                        centerY * inverseCornerCount, 3.5f, color);
    ++drawnFrames;
  }
}

bool projectedFrameHandleEnabled(TransformGizmo::Mode mode,
                                 TransformGizmo::HandleType handle) {
  if (handle == TransformGizmo::HandleType::None) return false;
  if (handle == TransformGizmo::HandleType::Rotate) {
    return mode == TransformGizmo::Mode::Rotate;
  }
  return mode == TransformGizmo::Mode::All ||
         mode == TransformGizmo::Mode::Scale;
}

bool projectedFrameScaleFixedPoint(
    const ScreenPhysicalBounds2 &physicalBounds,
    TransformGizmo::HandleType handle, bool fromCenter,
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 &fixedPoint) {
  const QRectF bounds = qRectFromScreenPhysicalBounds(physicalBounds);
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    return false;
  }
  if (fromCenter) {
    fixedPoint = screenPhysicalPoint(bounds.center());
    return handle != TransformGizmo::HandleType::None &&
           handle != TransformGizmo::HandleType::Rotate;
  }
  switch (handle) {
  case TransformGizmo::HandleType::Scale_TL:
    fixedPoint = screenPhysicalPoint(bounds.bottomRight());
    return true;
  case TransformGizmo::HandleType::Scale_TR:
    fixedPoint = screenPhysicalPoint(bounds.bottomLeft());
    return true;
  case TransformGizmo::HandleType::Scale_BL:
    fixedPoint = screenPhysicalPoint(bounds.topRight());
    return true;
  case TransformGizmo::HandleType::Scale_BR:
    fixedPoint = screenPhysicalPoint(bounds.topLeft());
    return true;
  case TransformGizmo::HandleType::Scale_T:
    fixedPoint = screenPhysicalPoint(
        QPointF(bounds.center().x(), bounds.bottom()));
    return true;
  case TransformGizmo::HandleType::Scale_B:
    fixedPoint = screenPhysicalPoint(QPointF(bounds.center().x(), bounds.top()));
    return true;
  case TransformGizmo::HandleType::Scale_L:
    fixedPoint = screenPhysicalPoint(
        QPointF(bounds.right(), bounds.center().y()));
    return true;
  case TransformGizmo::HandleType::Scale_R:
    fixedPoint = screenPhysicalPoint(
        QPointF(bounds.left(), bounds.center().y()));
    return true;
  default:
    return false;
  }
}

TransformGizmo::HandleType hitTestProjectedSelectionFrame(
    const ScreenPhysicalBounds2 &physicalBounds,
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPhysicalPos,
    float hitDiameter) {
  const QRectF bounds = qRectFromScreenPhysicalBounds(physicalBounds);
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    return TransformGizmo::HandleType::None;
  }
  const float diameter = std::max(18.0f, hitDiameter);
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPhysicalPos);
  const std::array<std::pair<QPointF, TransformGizmo::HandleType>, 8> handles{{
      {bounds.topLeft(), TransformGizmo::HandleType::Scale_TL},
      {bounds.topRight(), TransformGizmo::HandleType::Scale_TR},
      {bounds.bottomLeft(), TransformGizmo::HandleType::Scale_BL},
      {bounds.bottomRight(), TransformGizmo::HandleType::Scale_BR},
      {QPointF(bounds.center().x(), bounds.top()),
       TransformGizmo::HandleType::Scale_T},
      {QPointF(bounds.center().x(), bounds.bottom()),
       TransformGizmo::HandleType::Scale_B},
      {QPointF(bounds.left(), bounds.center().y()),
       TransformGizmo::HandleType::Scale_L},
      {QPointF(bounds.right(), bounds.center().y()),
       TransformGizmo::HandleType::Scale_R}}};
  auto nearestHandle = TransformGizmo::HandleType::None;
  double nearestDistance = std::numeric_limits<double>::max();
  for (const auto &[point, handle] : handles) {
    if (QRectF(point.x() - diameter * 0.5, point.y() - diameter * 0.5,
               diameter, diameter).contains(viewportPos)) {
      const double distance = QLineF(viewportPos, point).length();
      if (distance < nearestDistance) {
        nearestDistance = distance;
        nearestHandle = handle;
      }
    }
  }
  return nearestHandle;
}

Qt::CursorShape cursorForAxisAlignedFrameHandle(
    TransformGizmo::HandleType handle) {
  switch (handle) {
  case TransformGizmo::HandleType::Scale_TL:
  case TransformGizmo::HandleType::Scale_BR:
    return Qt::SizeFDiagCursor;
  case TransformGizmo::HandleType::Scale_TR:
  case TransformGizmo::HandleType::Scale_BL:
    return Qt::SizeBDiagCursor;
  case TransformGizmo::HandleType::Scale_T:
  case TransformGizmo::HandleType::Scale_B:
    return Qt::SizeVerCursor;
  case TransformGizmo::HandleType::Scale_L:
  case TransformGizmo::HandleType::Scale_R:
    return Qt::SizeHorCursor;
  case TransformGizmo::HandleType::Rotate:
    return Qt::CrossCursor;
  default:
    return Qt::ArrowCursor;
  }
}

int motionPathAdaptiveSampleStep(int minFrame, int maxFrame, float zoom) {
  const int64_t span = std::max<int64_t>(0, static_cast<int64_t>(maxFrame) -
                                             static_cast<int64_t>(minFrame));
  // Keep long work areas bounded while retaining one-frame precision for
  // short paths. Keyframes are added separately, so this only controls the
  // interpolated path samples between them.
  const int64_t kTargetSamples = static_cast<int64_t>(std::clamp<long long>(
      std::lround(240.0 * std::sqrt(std::max(1.0f, zoom))), 240LL, 960LL));
  return static_cast<int>(std::max<int64_t>(1, (span + kTargetSamples - 1) /
                                                kTargetSamples));
}

struct ConstructionHandle {
  size_t index = 0;
  int kind = 0; // 0: translate, 1/2: line endpoints, 3: radius
  QPointF point;
};

ArtifactCore::Array<ConstructionHandle> constructionHandles(const ArtifactConstructionLayer &layer) {
  ArtifactCore::Array<ConstructionHandle> handles;
  const auto &items = layer.constructionItems();
  for (size_t i = 0; i < items.size(); ++i) {
    const auto &item = items[i];
    if (!item.enabled || item.opacity <= 0.001) continue;
    if (item.type == ConstructionItemType::Line) {
      handles.append({i, 1, item.start}); handles.append({i, 2, item.end});
      handles.append({i, 0, (item.start + item.end) * 0.5});
    } else {
      handles.append({i, 0, item.center});
      if (item.type == ConstructionItemType::Circle) {
        for (const auto &offset : {QPointF(item.radius, 0), QPointF(-item.radius, 0),
                                  QPointF(0, item.radius), QPointF(0, -item.radius)})
          handles.append({i, 3, item.center + offset});
      }
    }
  }
  return handles;
}

bool constructionLocalPoint(const QPointF &pointer, const QMatrix4x4 &view,
    const QMatrix4x4 &projection, const QRect &viewport, const QMatrix4x4 &inverse,
    QPointF &point) {
  const auto a = inverse.map(ViewportMath::unprojectFromTopDown(
      QVector3D(float(pointer.x()), float(pointer.y()), 0), view, projection, viewport));
  const auto b = inverse.map(ViewportMath::unprojectFromTopDown(
      QVector3D(float(pointer.x()), float(pointer.y()), 1), view, projection, viewport));
  const auto d = b - a;
  if (std::abs(d.z()) < 1e-7f) return false;
  const float t = -a.z() / d.z();
  const auto p = a + d * t;
  if (!std::isfinite(t) || t < 0 || t > 1 || !std::isfinite(p.x()) || !std::isfinite(p.y())) return false;
  point = QPointF(p.x(), p.y());
  return true;
}

struct ProjectedFrameSnapCache {
  ArtifactCore::Array<double> verticalGuides;
  ArtifactCore::Array<double> horizontalGuides;
  QSet<LayerID> selectedIds;
  const ArtifactAbstractComposition *composition = nullptr;
  QMatrix4x4 view;
  QMatrix4x4 projection;
  QRect viewport;
  QElapsedTimer age;
  bool verticalHeld = false;
  bool horizontalHeld = false;
  double verticalGuide = 0.0;
  double horizontalGuide = 0.0;
  void release() { verticalHeld = horizontalHeld = false; }
  void invalidate() { age.invalidate(); release(); }
};

struct ProjectedFrameSnapResult {
  ArtifactCore::Coordinates::ScreenPhysicalPoint2 pointer{};
  bool verticalGuideValid = false;
  bool horizontalGuideValid = false;
  float verticalGuide = 0.0f;
  float horizontalGuide = 0.0f;
};

ProjectedFrameSnapResult snapProjectedFramePointer(
    const ArtifactCompositionPtr &composition,
    const ArtifactAbstractLayerPtr &selectedLayer,
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPhysicalPos,
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 previousViewportPhysicalPos,
    bool movingFrame, TransformGizmo::HandleType handle,
    const QMatrix4x4 &view, const QMatrix4x4 &projection,
    const QRect &viewport, ArtifactIRenderer *renderer, float devicePixelRatio,
    ProjectedFrameSnapCache &cache, const QPointF &constrainedDirection,
    bool axisConstrained) {
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPhysicalPos);
  const QPointF previousViewportPos =
      qPointFromScreenPhysicalPoint(previousViewportPhysicalPos);
  if (!composition || !selectedLayer || !renderer) {
    return ProjectedFrameSnapResult{viewportPhysicalPos};
  }

  auto &verticalGuides = cache.verticalGuides;
  auto &horizontalGuides = cache.horizontalGuides;
  QPointF pointerDelta = viewportPos - previousViewportPos;
  if (axisConstrained) {
    const double lengthSquared = QPointF::dotProduct(constrainedDirection, constrainedDirection);
    pointerDelta = lengthSquared > 1e-12
        ? constrainedDirection * (QPointF::dotProduct(pointerDelta, constrainedDirection) / lengthSquared)
        : QPointF{};
  }
  QVector<ArtifactAbstractLayerPtr> selectedLayers;
  QSet<LayerID> selectedLayerIds;
  if (auto *selection = ArtifactLayerSelectionManager::instance()) {
    for (const auto &layer : selection->selectedLayers()) {
      if (layer) {
        selectedLayers.push_back(layer);
        selectedLayerIds.insert(layer->id());
      }
    }
  }
  if (selectedLayers.isEmpty()) {
    selectedLayers.push_back(selectedLayer);
    selectedLayerIds.insert(selectedLayer->id());
  }
  const bool contextChanged = cache.composition != composition.get() ||
      cache.selectedIds != selectedLayerIds || cache.view != view ||
      cache.projection != projection || cache.viewport != viewport;
  // Selected transforms change on every move. Do not use the composition
  // revision as a key: refresh stationary targets at most four times/second.
  const bool rebuild = contextChanged || !cache.age.isValid() || cache.age.elapsed() >= 250;
  if (contextChanged) cache.release();

  const auto projectedExtents = [&](const ArtifactAbstractLayerPtr &layer,
                                    double &left, double &centerX,
                                    double &right, double &top,
                                    double &centerY, double &bottom) {
    if (!layer) return false;
    const QRectF bounds = layer->localBounds();
    if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
      return false;
    }
    const QMatrix4x4 world = layer->getGlobalTransform4x4();
    const std::array<QPointF, 4> points{
        bounds.topLeft(), bounds.topRight(), bounds.bottomRight(),
        bounds.bottomLeft()};
    left = top = std::numeric_limits<double>::max();
    right = bottom = std::numeric_limits<double>::lowest();
    for (const QPointF &point : points) {
      const QVector3D projected = ViewportMath::projectToTopDown(
          world.map(QVector3D(static_cast<float>(point.x()),
                              static_cast<float>(point.y()), 0.0f)),
          view, projection, viewport);
      if (!std::isfinite(projected.x()) || !std::isfinite(projected.y()) ||
          !std::isfinite(projected.z()) || projected.z() < 0.0f || projected.z() > 1.0f) {
        return false;
      }
      left = std::min(left, static_cast<double>(projected.x()));
      right = std::max(right, static_cast<double>(projected.x()));
      top = std::min(top, static_cast<double>(projected.y()));
      bottom = std::max(bottom, static_cast<double>(projected.y()));
    }
    centerX = (left + right) * 0.5;
    centerY = (top + bottom) * 0.5;
    return true;
  };

  if (rebuild) {
    verticalGuides.resize(0);
    horizontalGuides.resize(0);
    const auto size = composition->settings().compositionSize();
    const std::array<QVector3D, 4> corners{
        QVector3D(0, 0, 0), QVector3D(size.width(), 0, 0),
        QVector3D(size.width(), size.height(), 0), QVector3D(0, size.height(), 0)};
    double left = std::numeric_limits<double>::max();
    double right = std::numeric_limits<double>::lowest();
    double top = left, bottom = right;
    bool valid = true;
    for (const auto &corner : corners) {
      const auto p = ViewportMath::projectToTopDown(corner, view, projection, viewport);
      if (!std::isfinite(p.x()) || !std::isfinite(p.y()) ||
          !std::isfinite(p.z()) || p.z() < 0.0f || p.z() > 1.0f) {
        valid = false;
        break;
      }
      left = std::min(left, double(p.x())); right = std::max(right, double(p.x()));
      top = std::min(top, double(p.y())); bottom = std::max(bottom, double(p.y()));
    }
    if (valid && right - left > 1.0 && bottom - top > 1.0) {
      for (double x : {left, (left + right) * 0.5, right}) verticalGuides.append(x);
      for (double y : {top, (top + bottom) * 0.5, bottom}) horizontalGuides.append(y);
    }
    for (const auto &layer : composition->allLayer()) {
      if (!layer || !layer->isVisible() ||
          selectedLayerIds.contains(layer->id())) continue;
      if (layer->isConstructionLayer()) {
        if (const auto construction = ArtifactCore::dynamicPointerCast<ArtifactConstructionLayer>(layer)) {
          const auto world = layer->getGlobalTransform4x4();
          for (const auto &point : construction->constructionSnapPoints()) {
            const auto projected = ViewportMath::projectToTopDown(
                world.map(QVector3D(float(point.x()), float(point.y()), 0)), view, projection, viewport);
            if (std::isfinite(projected.x()) && std::isfinite(projected.y()) &&
                std::isfinite(projected.z()) && projected.z() >= 0 && projected.z() <= 1) {
              verticalGuides.append(projected.x());
              horizontalGuides.append(projected.y());
            }
          }
        }
        continue;
      }
      double left = 0.0, centerX = 0.0, right = 0.0;
      double top = 0.0, centerY = 0.0, bottom = 0.0;
      if (projectedExtents(layer, left, centerX, right, top, centerY, bottom)) {
        for (double x : {left, centerX, right}) verticalGuides.append(x);
        for (double y : {top, centerY, bottom}) horizontalGuides.append(y);
      }
    }
    if (verticalGuides.size() > 1) std::sort(verticalGuides.begin(), verticalGuides.end());
    if (horizontalGuides.size() > 1) std::sort(horizontalGuides.begin(), horizontalGuides.end());
    cache.composition = composition.get();
    cache.selectedIds = selectedLayerIds;
    cache.view = view; cache.projection = projection; cache.viewport = viewport;
    cache.age.start();
  }

  ArtifactCore::Array<double> xCandidates{viewportPos.x()};
  ArtifactCore::Array<double> yCandidates{viewportPos.y()};
  if (movingFrame) {
    const ScreenPhysicalBounds2 combinedBounds = selectedLayers.size() > 1
        ? projectedSelectionFrameBounds(selectedLayers, view, projection,
                                        viewport)
        : ScreenPhysicalBounds2{};
    const QRectF combinedBoundsRect =
        qRectFromScreenPhysicalBounds(combinedBounds);
    if (isValidScreenPhysicalBounds(combinedBounds)) {
      xCandidates = {combinedBoundsRect.left() + pointerDelta.x(),
                     combinedBoundsRect.center().x() + pointerDelta.x(),
                     combinedBoundsRect.right() + pointerDelta.x()};
      yCandidates = {combinedBoundsRect.top() + pointerDelta.y(),
                     combinedBoundsRect.center().y() + pointerDelta.y(),
                     combinedBoundsRect.bottom() + pointerDelta.y()};
    } else {
      double left = 0.0, centerX = 0.0, right = 0.0;
      double top = 0.0, centerY = 0.0, bottom = 0.0;
      if (projectedExtents(selectedLayer, left, centerX, right, top, centerY,
                           bottom)) {
        xCandidates = {left + pointerDelta.x(), centerX + pointerDelta.x(),
                       right + pointerDelta.x()};
        yCandidates = {top + pointerDelta.y(), centerY + pointerDelta.y(),
                       bottom + pointerDelta.y()};
      }
    }
  } else if (selectedLayers.size() > 1) {
    const ScreenPhysicalBounds2 combinedBounds = projectedSelectionFrameBounds(
        selectedLayers, view, projection, viewport);
    const QRectF combinedBoundsRect =
        qRectFromScreenPhysicalBounds(combinedBounds);
    QPointF handlePoint;
    bool validHandle = isValidScreenPhysicalBounds(combinedBounds);
    switch (handle) {
    case TransformGizmo::HandleType::Scale_TL:
      handlePoint = combinedBoundsRect.topLeft();
      break;
    case TransformGizmo::HandleType::Scale_TR:
      handlePoint = combinedBoundsRect.topRight();
      break;
    case TransformGizmo::HandleType::Scale_BL:
      handlePoint = combinedBoundsRect.bottomLeft();
      break;
    case TransformGizmo::HandleType::Scale_BR:
      handlePoint = combinedBoundsRect.bottomRight();
      break;
    case TransformGizmo::HandleType::Scale_T:
      handlePoint = QPointF(combinedBoundsRect.center().x(),
                            combinedBoundsRect.top());
      break;
    case TransformGizmo::HandleType::Scale_B:
      handlePoint = QPointF(combinedBoundsRect.center().x(),
                            combinedBoundsRect.bottom());
      break;
    case TransformGizmo::HandleType::Scale_L:
      handlePoint = QPointF(combinedBoundsRect.left(),
                            combinedBoundsRect.center().y());
      break;
    case TransformGizmo::HandleType::Scale_R:
      handlePoint = QPointF(combinedBoundsRect.right(),
                            combinedBoundsRect.center().y());
      break;
    default:
      validHandle = false;
      break;
    }
    if (validHandle) {
      xCandidates = {handlePoint.x() + pointerDelta.x()};
      yCandidates = {handlePoint.y() + pointerDelta.y()};
    }
  } else {
    const QRectF localBounds = selectedLayer->localBounds();
    const QRectF bounds = localBounds;
    QPointF handleLocal;
    bool hasHandlePoint = true;
    switch (handle) {
    case TransformGizmo::HandleType::Scale_TL:
      handleLocal = bounds.topLeft();
      break;
    case TransformGizmo::HandleType::Scale_TR:
      handleLocal = bounds.topRight();
      break;
    case TransformGizmo::HandleType::Scale_BL:
      handleLocal = bounds.bottomLeft();
      break;
    case TransformGizmo::HandleType::Scale_BR:
      handleLocal = bounds.bottomRight();
      break;
    case TransformGizmo::HandleType::Scale_T:
      handleLocal = QPointF(bounds.center().x(), bounds.top());
      break;
    case TransformGizmo::HandleType::Scale_B:
      handleLocal = QPointF(bounds.center().x(), bounds.bottom());
      break;
    case TransformGizmo::HandleType::Scale_L:
      handleLocal = QPointF(bounds.left(), bounds.center().y());
      break;
    case TransformGizmo::HandleType::Scale_R:
      handleLocal = QPointF(bounds.right(), bounds.center().y());
      break;
    default:
      hasHandlePoint = false;
      break;
    }
    if (hasHandlePoint) {
      const QVector3D projected = ViewportMath::projectToTopDown(
          selectedLayer->getGlobalTransform4x4().map(
              QVector3D(static_cast<float>(handleLocal.x()),
                        static_cast<float>(handleLocal.y()), 0.0f)),
          view, projection, viewport);
      if (std::isfinite(projected.x()) && std::isfinite(projected.y())) {
        xCandidates = {projected.x() + pointerDelta.x()};
        yCandidates = {projected.y() + pointerDelta.y()};
      }
    }
  }

  const double threshold = 10.0 * std::max(1.0f, devicePixelRatio);
  struct AxisSnap {
    double adjustment = 0.0;
    double guide = 0.0;
    bool valid = false;
  };
  const auto closestAdjustment = [threshold](
                                     const ArtifactCore::Array<double> &candidates,
                                     const ArtifactCore::Array<double> &guides,
                                     bool held, double heldGuide) {
    AxisSnap result;
    if (guides.isEmpty()) return result;
    // Keep the same target until the pointer escapes the wider release band.
    if (held && std::binary_search(guides.begin(), guides.end(), heldGuide)) {
      double distance = threshold * 1.6;
      for (double candidate : candidates) {
        const double adjustment = heldGuide - candidate;
        if (std::abs(adjustment) <= distance) {
          result = AxisSnap{adjustment, heldGuide, true};
          distance = std::abs(adjustment);
        }
      }
      if (result.valid) return result;
    }
    double best = 0.0;
    double bestDistance = threshold + 1.0;
    for (double candidate : candidates) {
      const auto nearest = std::lower_bound(guides.begin(), guides.end(), candidate);
      const auto consider = [&](double guide) {
        const double adjustment = guide - candidate;
        const double distance = std::abs(adjustment);
        if (distance <= threshold && distance < bestDistance) {
          best = adjustment;
          bestDistance = distance;
          result.guide = guide;
          result.valid = true;
        }
      };
      if (nearest != guides.end()) consider(*nearest);
      if (nearest != guides.begin()) consider(*(nearest - 1));
    }
    result.adjustment = best;
    return result;
  };
  AxisSnap vertical = closestAdjustment(xCandidates, verticalGuides,
                                        cache.verticalHeld, cache.verticalGuide);
  AxisSnap horizontal = closestAdjustment(yCandidates, horizontalGuides,
                                          cache.horizontalHeld, cache.horizontalGuide);
  QPointF adjustment(vertical.adjustment, horizontal.adjustment);
  if (axisConstrained) {
    // One degree of freedom: intersect the projected movement axis with ONE
    // guide. Applying independent X/Y corrections would violate the axis.
    const double dx = constrainedDirection.x(), dy = constrainedDirection.y();
    const double length = std::hypot(dx, dy);
    const double ux = length > 1e-6 ? dx / length : 0.0;
    const double uy = length > 1e-6 ? dy / length : 0.0;
    const double limit = threshold * 1.6;
    const double tx = std::abs(ux) > 0.05 ? vertical.adjustment / ux : limit + 1.0;
    const double ty = std::abs(uy) > 0.05 ? horizontal.adjustment / uy : limit + 1.0;
    vertical.valid = vertical.valid && std::abs(tx) <= limit;
    horizontal.valid = horizontal.valid && std::abs(ty) <= limit;
    const bool useX = vertical.valid && (!horizontal.valid ||
        (cache.verticalHeld && !cache.horizontalHeld) ||
        (!cache.horizontalHeld && std::abs(tx) <= std::abs(ty)));
    const bool useY = horizontal.valid && !useX;
    const double t = useX ? tx : useY ? ty : 0.0;
    adjustment = QPointF(ux * t, uy * t);
    vertical.valid = useX; horizontal.valid = useY;
  }
  cache.verticalHeld = vertical.valid;
  cache.horizontalHeld = horizontal.valid;
  cache.verticalGuide = vertical.guide;
  cache.horizontalGuide = horizontal.guide;
  ProjectedFrameSnapResult result;
  result.pointer = screenPhysicalPoint(viewportPos + adjustment);
  result.verticalGuideValid = vertical.valid;
  result.horizontalGuideValid = horizontal.valid;
  result.verticalGuide = static_cast<float>(vertical.guide);
  result.horizontalGuide = static_cast<float>(horizontal.guide);
  return result;
}

Qt::CursorShape cursorForProjectedFrameCorner(
    TransformGizmo::HandleType handle,
    const ArtifactAbstractLayerPtr &layer, const QMatrix4x4 &view,
    const QMatrix4x4 &projection, const QRect &viewport) {
  if (handle == TransformGizmo::HandleType::Rotate) {
    return Qt::CrossCursor;
  }
  if (!layer) {
    return Qt::ArrowCursor;
  }

  const QRectF localBounds = layer->localBounds();
  const QRectF bounds = localBounds;
  QPointF handleLocal;
  QPointF oppositeLocal;
  switch (handle) {
  case TransformGizmo::HandleType::Scale_TL:
    handleLocal = bounds.topLeft();
    oppositeLocal = bounds.bottomRight();
    break;
  case TransformGizmo::HandleType::Scale_TR:
    handleLocal = bounds.topRight();
    oppositeLocal = bounds.bottomLeft();
    break;
  case TransformGizmo::HandleType::Scale_BL:
    handleLocal = bounds.bottomLeft();
    oppositeLocal = bounds.topRight();
    break;
  case TransformGizmo::HandleType::Scale_BR:
    handleLocal = bounds.bottomRight();
    oppositeLocal = bounds.topLeft();
    break;
  case TransformGizmo::HandleType::Scale_T:
    handleLocal = QPointF(bounds.center().x(), bounds.top());
    oppositeLocal = QPointF(bounds.center().x(), bounds.bottom());
    break;
  case TransformGizmo::HandleType::Scale_B:
    handleLocal = QPointF(bounds.center().x(), bounds.bottom());
    oppositeLocal = QPointF(bounds.center().x(), bounds.top());
    break;
  case TransformGizmo::HandleType::Scale_L:
    handleLocal = QPointF(bounds.left(), bounds.center().y());
    oppositeLocal = QPointF(bounds.right(), bounds.center().y());
    break;
  case TransformGizmo::HandleType::Scale_R:
    handleLocal = QPointF(bounds.right(), bounds.center().y());
    oppositeLocal = QPointF(bounds.left(), bounds.center().y());
    break;
  default:
    return Qt::ArrowCursor;
  }

  const QMatrix4x4 world = layer->getGlobalTransform4x4();
  const auto projectLocal = [&](const QPointF &point) {
    return ViewportMath::projectToTopDown(
        world.map(QVector3D(static_cast<float>(point.x()),
                            static_cast<float>(point.y()), 0.0f)),
        view, projection, viewport);
  };
  const QVector3D handleProjected = projectLocal(handleLocal);
  const QVector3D oppositeProjected = projectLocal(oppositeLocal);
  if (!std::isfinite(handleProjected.x()) ||
      !std::isfinite(handleProjected.y()) ||
      !std::isfinite(oppositeProjected.x()) ||
      !std::isfinite(oppositeProjected.y())) {
    return Qt::ArrowCursor;
  }
  const float dx = handleProjected.x() - oppositeProjected.x();
  const float dy = handleProjected.y() - oppositeProjected.y();
  if (dx * dx + dy * dy <= 0.0001f) {
    return Qt::SizeAllCursor;
  }
  float angle = std::atan2(dy, dx) * 180.0f /
                static_cast<float>(3.14159265358979323846);
  angle = std::fmod(angle + 180.0f, 180.0f);
  if (angle < 22.5f || angle >= 157.5f) {
    return Qt::SizeHorCursor;
  }
  if (angle < 67.5f) {
    return Qt::SizeFDiagCursor;
  }
  if (angle < 112.5f) {
    return Qt::SizeVerCursor;
  }
  return Qt::SizeBDiagCursor;
}

ArtifactCore::Coordinates::WorldPoint3 unprojectClipCorner(
                              const QMatrix4x4 &invViewProj, float x, float y,

                              float z) {

  const QVector4D clipPos(x, y, z, 1.0f);

  const QVector4D worldPos = invViewProj * clipPos;

  if (std::abs(worldPos.w()) < 1e-6f) {

    return {};

  }

  return {worldPos.x() / worldPos.w(), worldPos.y() / worldPos.w(),
          worldPos.z() / worldPos.w()};

}



CompositionRenderController::CameraFrustumVisual

buildCameraFrustumVisual(const ArtifactCompositionPtr &comp,

                         const LayerID &selectedLayerId) {

  CompositionRenderController::CameraFrustumVisual visual;

  if (!comp || selectedLayerId.isNil()) {

    return visual;

  }



  auto layer = comp->layerById(selectedLayerId);

  auto *layerPtr = layer.get();

  const auto camera = dynamic_cast<ArtifactCameraLayer *>(layerPtr);

  if (!camera) {

    return visual;

  }



  const auto size = comp->settings().compositionSize();

  const float width =

      static_cast<float>(size.width() > 0 ? size.width() : 1920);

  const float height =

      static_cast<float>(size.height() > 0 ? size.height() : 1080);

  const float aspect = std::max(0.001f, width / std::max(0.001f, height));



  const QMatrix4x4 view = camera->viewMatrix();

  const QMatrix4x4 proj = camera->projectionMatrix(aspect);

  bool invertible = false;

  const QMatrix4x4 invViewProj = (proj * view).inverted(&invertible);

  if (!invertible) {

    return visual;

  }



  visual.valid = true;

  visual.layerId = camera->id();

  visual.cameraPosition =
      ArtifactCore::Coordinates::worldPoint3FromQVector3D(
          view.inverted().map(QVector3D(0.0f, 0.0f, 0.0f)));

  visual.viewMatrix = view;

  visual.projectionMatrix = proj;

  visual.guide = makeNukeStyleCameraGuidePrimitive();

  visual.aspect = ArtifactCore::Units::ScaleFactor{aspect};

  visual.zoom = camera->zoom();



  visual.nearPlaneCorners.reserve(4);

  visual.farPlaneCorners.reserve(4);

  visual.nearPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, -1.0f, -1.0f, -1.0f));

  visual.nearPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, 1.0f, -1.0f, -1.0f));

  visual.nearPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, 1.0f, 1.0f, -1.0f));

  visual.nearPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, -1.0f, 1.0f, -1.0f));

  visual.farPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, -1.0f, -1.0f, 1.0f));

  visual.farPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, 1.0f, -1.0f, 1.0f));

  visual.farPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, 1.0f, 1.0f, 1.0f));

  visual.farPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, -1.0f, 1.0f, 1.0f));

  return visual;

}



CompositionRenderController::CameraFrustumVisual

buildCameraFrustumVisual(const ArtifactCameraLayer *camera,

                         const ArtifactCompositionPtr &comp) {

  CompositionRenderController::CameraFrustumVisual visual;

  if (!camera || !comp) {

    return visual;

  }



  const auto size = comp->settings().compositionSize();

  const float width =

      static_cast<float>(size.width() > 0 ? size.width() : 1920);

  const float height =

      static_cast<float>(size.height() > 0 ? size.height() : 1080);

  const float aspect = std::max(0.001f, width / std::max(0.001f, height));



  const QMatrix4x4 view = camera->viewMatrix();

  const QMatrix4x4 proj = camera->projectionMatrix(aspect);

  bool invertible = false;

  const QMatrix4x4 invViewProj = (proj * view).inverted(&invertible);

  if (!invertible) {

    return visual;

  }



  visual.valid = true;

  visual.layerId = camera->id();

  visual.cameraPosition =
      ArtifactCore::Coordinates::worldPoint3FromQVector3D(
          view.inverted().map(QVector3D(0.0f, 0.0f, 0.0f)));

  visual.viewMatrix = view;

  visual.projectionMatrix = proj;

  visual.guide = makeNukeStyleCameraGuidePrimitive();

  visual.aspect = ArtifactCore::Units::ScaleFactor{aspect};

  visual.zoom = camera->zoom();



  visual.nearPlaneCorners.reserve(4);

  visual.farPlaneCorners.reserve(4);

  visual.nearPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, -1.0f, -1.0f, -1.0f));

  visual.nearPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, 1.0f, -1.0f, -1.0f));

  visual.nearPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, 1.0f, 1.0f, -1.0f));

  visual.nearPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, -1.0f, 1.0f, -1.0f));

  visual.farPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, -1.0f, -1.0f, 1.0f));

  visual.farPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, 1.0f, -1.0f, 1.0f));

  visual.farPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, 1.0f, 1.0f, 1.0f));

  visual.farPlaneCorners.push_back(

      unprojectClipCorner(invViewProj, -1.0f, 1.0f, 1.0f));

  return visual;

}



CompositionRenderController::CameraFrustumVisual
buildDefaultCameraFrustumVisual(const QMatrix4x4 &view,
                                const QMatrix4x4 &proj) {
  CompositionRenderController::CameraFrustumVisual visual;
  bool invertible = false;
  const QMatrix4x4 invViewProj = (proj * view).inverted(&invertible);
  if (!invertible) {
    return visual;
  }
  visual.valid = true;
  visual.cameraPosition =
      ArtifactCore::Coordinates::worldPoint3FromQVector3D(
          view.inverted().map(QVector3D(0.0f, 0.0f, 0.0f)));
  visual.viewMatrix = view;
  visual.projectionMatrix = proj;
  visual.guide = makeNukeStyleCameraGuidePrimitive();
  visual.nearPlaneCorners = {
      unprojectClipCorner(invViewProj, -1.0f, -1.0f, -1.0f),
      unprojectClipCorner(invViewProj, 1.0f, -1.0f, -1.0f),
      unprojectClipCorner(invViewProj, 1.0f, 1.0f, -1.0f),
      unprojectClipCorner(invViewProj, -1.0f, 1.0f, -1.0f)};
  visual.farPlaneCorners = {
      unprojectClipCorner(invViewProj, -1.0f, -1.0f, 1.0f),
      unprojectClipCorner(invViewProj, 1.0f, -1.0f, 1.0f),
      unprojectClipCorner(invViewProj, 1.0f, 1.0f, 1.0f),
      unprojectClipCorner(invViewProj, -1.0f, 1.0f, 1.0f)};
  return visual;
}

void drawCameraFrustumOverlay(
    ArtifactIRenderer *renderer,
    const CompositionRenderController::CameraFrustumVisual &visual,
    const QMatrix4x4 &viewportView, const QMatrix4x4 &viewportProjection,
    bool activeCamera) {

  if (!renderer || !visual.valid || visual.nearPlaneCorners.size() < 4 ||

      visual.farPlaneCorners.size() < 4) {

    return;

  }



  const FloatColor outerColor = activeCamera

                                    ? FloatColor{0.35f, 0.95f, 0.58f, 0.92f}

                                    : FloatColor{0.36f, 0.72f, 0.98f, 0.88f};

  const FloatColor innerColor = activeCamera

                                    ? FloatColor{0.08f, 0.18f, 0.12f, 0.58f}

                                    : FloatColor{0.07f, 0.10f, 0.14f, 0.56f};

  const auto drawLine = [renderer](
                                   ArtifactCore::Coordinates::WorldPoint3 a,
                                   ArtifactCore::Coordinates::WorldPoint3 b,

                                   const FloatColor &color, float thickness) {

    renderer->drawGizmoLine(Detail::float3{a.x, a.y, a.z},

                            Detail::float3{b.x, b.y, b.z}, color,

                            thickness);

  };



  renderer->setUseExternalMatrices(true);

  // The frustum is geometry belonging to `visual`, but it must be viewed by
  // the current viewport camera. Rendering it with visual.viewMatrix places
  // the viewer at the represented camera's apex and clips the guide itself.
  renderer->set3DCameraMatrices(viewportView, viewportProjection);

  for (int i = 0; i < 4; ++i) {

    const int next = (i + 1) % 4;

    drawLine(visual.nearPlaneCorners[i], visual.nearPlaneCorners[next], outerColor,

         1.2f);

    drawLine(visual.farPlaneCorners[i], visual.farPlaneCorners[next], innerColor,

         0.8f);

    drawLine(visual.nearPlaneCorners[i], visual.farPlaneCorners[i], outerColor,

         0.9f);

  }



  const auto &corners = visual.nearPlaneCorners;
  const ArtifactCore::Coordinates::WorldPoint3 center{
      (corners[0].x + corners[1].x + corners[2].x + corners[3].x) * 0.25f,
      (corners[0].y + corners[1].y + corners[2].y + corners[3].y) * 0.25f,
      (corners[0].z + corners[1].z + corners[2].z + corners[3].z) * 0.25f};

  const auto camPos = visual.cameraPosition;

  drawLine(camPos, center, outerColor, 1.0f);

  // Frustum lines are queued by PrimitiveRenderer3D and must be submitted
  // before their camera matrices are reset.
  renderer->flushGizmo3D();
  renderer->reset3DCameraMatrices();

  renderer->setUseExternalMatrices(false);

}



QMatrix4x4 viewportOrientationViewMatrix(

    const QQuaternion& orientation, const QPointF& target,

    const float distance) {

  const QQuaternion cameraOrientation = orientation.conjugated();
  const QVector3D targetPosition(
      static_cast<float>(target.x()), static_cast<float>(target.y()), 0.0f);

  const QVector3D eye =

      targetPosition +
      cameraOrientation.rotatedVector(QVector3D(0.0f, 0.0f, 1.0f)) *

                   std::max(1.0f, distance);

  QVector3D up =
      cameraOrientation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));

  if (up.lengthSquared() < 1.0e-6f) {

    up = QVector3D(0.0f, 1.0f, 0.0f);

  }

  QMatrix4x4 view;

  view.lookAt(eye, targetPosition, up.normalized());

  return view;

}



QMatrix4x4 viewportOrientationProjectionMatrix(const float viewportW,

                                               const float viewportH,

                                               const float fovDegrees) {

  QMatrix4x4 proj;

  const float w = std::max(1.0f, viewportW);

  const float h = std::max(1.0f, viewportH);

  const float aspect = w / h;

  proj.perspective(std::clamp(fovDegrees, 8.0f, 120.0f), aspect, 1.0f,

                   100000.0f);
  proj(1, 1) = -proj(1, 1);

  return proj;

}

bool compositionToPaintPosition(const ArtifactPaintLayer* layer,
                                const QPointF& compositionPosition,
                                QPointF* paintPosition) {
  if (!layer || !paintPosition || !std::isfinite(compositionPosition.x()) ||
      !std::isfinite(compositionPosition.y())) {
    return false;
  }
  bool invertible = false;
  const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
  if (!invertible) {
    return false;
  }
  const QPointF mapped = inverse.map(compositionPosition);
  if (!std::isfinite(mapped.x()) || !std::isfinite(mapped.y())) {
    return false;
  }
  *paintPosition = mapped;
  return true;
}

} // namespace Artifact
