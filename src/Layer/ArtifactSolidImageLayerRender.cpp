module;
#include <atomic>
#include <QDebug>
#include <QImage>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QSizeF>

module Artifact.Layers.SolidImage;
import Artifact.Color.OCIOManager;
import Artifact.Layer.Abstract;
import Artifact.Layer.CloneEffectSupport;
import Artifact.Layers.Abstract._2D;
import Artifact.Render.IRenderer;
import Color.Float;
import Size;

namespace Artifact {
using namespace ArtifactCore;
Q_LOGGING_CATEGORY(solidImageLayerLog, "artifact.solidimagelayer")

void ArtifactSolidImageLayer::draw(ArtifactIRenderer *renderer) {
  const auto source = sourceSize();
  const Size_2D size(std::clamp(source.width, 1, 16384),
                     std::clamp(source.height, 1, 16384));
  const float displayWidth = static_cast<float>(size.width) *
                             static_cast<float>(pixelAspectRatio());
  const auto* colorManager = ArtifactOCIOManager::instance();
  const auto color = colorManager->resolveGeneratedColorForRender(this->color());
  const auto fillType = this->fillType();
  const auto gradientStart = colorManager->resolveGeneratedColorForRender(
      gradientStartColor());
  const auto gradientEnd = colorManager->resolveGeneratedColorForRender(
      gradientEndColor());
  const float gradientAngle = gradientAngleDegrees();
  const bool gradientReverseValue = gradientReverse();
  const float gradientCenterXValue = gradientCenterX();
  const float gradientCenterYValue = gradientCenterY();
  const float gradientScaleValue = gradientScale();
  const float gradientOffsetValue = gradientOffset();

  static std::atomic<int> drawLogSamples{0};
  if (drawLogSamples.load(std::memory_order_relaxed) < 5) {
    drawLogSamples.fetch_add(1, std::memory_order_relaxed);
    qCDebug(solidImageLayerLog)
        << "[ArtifactSolidImageLayer::draw] id:" << id().toString()
        << "currentFrame:" << currentFrame() << "color: (" << color.r()
        << color.g() << color.b() << color.a() << ")"
        << "size:" << size.width << "x" << size.height
        << "opacity:" << opacity();
  }

  const QMatrix4x4 baseTransform = getGlobalTransform4x4();
  if (resolveLayerSourceOverride()) {
    const QImage& overrideImage = currentFillImage();
    drawWithClonerEffect(
        this, baseTransform,
        [renderer, size, displayWidth, overrideImage, this]
        (const QMatrix4x4 &transform, float weight) {
          renderer->drawSpriteTransformed(
              0.0f, 0.0f, displayWidth,
              static_cast<float>(size.height), transform, overrideImage,
              opacity() * weight);
        });
    drawFractureOverlay(renderer, baseTransform, QSizeF(displayWidth, size.height), opacity());
    return;
  }
  drawWithClonerEffect(
      this, baseTransform,
      [renderer, size, displayWidth, color, fillType, gradientStart, gradientEnd,
       gradientAngle, gradientReverseValue, gradientCenterXValue,
       gradientCenterYValue, gradientScaleValue, gradientOffsetValue, this]
      (const QMatrix4x4 &transform, float weight) {
        if (fillType != ArtifactSolidFillType::Solid) {
          renderer->drawGradientRectTransformed(
              0.0f, 0.0f, displayWidth, static_cast<float>(size.height), transform,
              gradientStart, gradientEnd, static_cast<int>(fillType), gradientAngle,
              gradientReverseValue, gradientCenterXValue, gradientCenterYValue,
              gradientScaleValue, gradientOffsetValue, this->opacity() * weight);
          return;
        }
        const FloatColor cloneColor(color.r(), color.g(), color.b(),
                                    color.a() * this->opacity() * weight);
        renderer->drawSolidRectTransformed(
            0.0f, 0.0f, displayWidth,
            static_cast<float>(size.height), transform, cloneColor, 1.0f);
      });
  drawFractureOverlay(renderer, baseTransform, QSizeF(displayWidth, size.height), opacity());
}

}
