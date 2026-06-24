module;
#include <utility>
#define NOMINMAX
#define QT_NO_KEYWORDS
#include <Layer/ArtifactCloneEffectSupport.hpp>
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QLinearGradient>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QPainter>
#include <QSize>
#include <QVariant>


module Artifact.Layers.SolidImage;

import std;
import Artifact.Layers.Abstract._2D;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Property.Abstract;
import Property.Group;
import Animation.Value;
import Time.Rational;

namespace Artifact {
namespace {
Q_LOGGING_CATEGORY(solidImageLayerLog, "artifact.solidimagelayer")

int64_t effectiveSolidTimelineFrame(const ArtifactSolidImageLayer *layer) {
  if (!layer) {
    return 0;
  }
  if (auto *composition = static_cast<ArtifactAbstractComposition *>(
          layer->composition())) {
    return composition->framePosition().framePosition();
  }
  return layer->currentFrame();
}

int64_t effectiveSolidTimelineFps(const ArtifactSolidImageLayer *layer) {
  if (!layer) {
    return 30;
  }
  if (auto *composition = static_cast<ArtifactAbstractComposition *>(
          layer->composition())) {
    return std::max<int64_t>(
        1, static_cast<int64_t>(
               std::llround(composition->frameRate().framerate())));
  }
  return 30;
}

FramePosition effectiveSolidTimelineFramePosition(
    const ArtifactSolidImageLayer *layer) {
  return FramePosition(static_cast<int>(effectiveSolidTimelineFrame(layer)));
}

RationalTime effectiveSolidTimelineTime(const ArtifactSolidImageLayer *layer) {
  return RationalTime(effectiveSolidTimelineFrame(layer),
                      effectiveSolidTimelineFps(layer));
}

FloatColor toFloatColor(const QColor &color) {
  return FloatColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

QColor toQColor(const FloatColor& color, const float alphaScale = 1.0f) {
  return QColor::fromRgbF(std::clamp(color.r(), 0.0f, 1.0f),
                          std::clamp(color.g(), 0.0f, 1.0f),
                          std::clamp(color.b(), 0.0f, 1.0f),
                          std::clamp(color.a() * alphaScale, 0.0f, 1.0f));
}

QPointF gradientPointForAngle(const float angleDegrees, const QSize& size, const bool startPoint,
                              const bool reverse, const float centerX, const float centerY,
                              const float scale, const float offset) {
  const float radians = angleDegrees * 3.14159265358979323846f / 180.0f;
  const QPointF center(static_cast<qreal>(size.width()) * std::clamp(centerX, 0.0f, 1.0f),
                       static_cast<qreal>(size.height()) * std::clamp(centerY, 0.0f, 1.0f));
  const qreal dx = std::cos(radians);
  const qreal dy = -std::sin(radians);
  const qreal halfSpan = std::max(1.0, std::hypot(static_cast<double>(size.width()),
                                                  static_cast<double>(size.height()))) * 0.5 *
                         std::max(0.01f, scale);
  const qreal direction = reverse ? -1.0 : 1.0;
  const qreal sign = startPoint ? -1.0 : 1.0;
  return QPointF(center.x() + dx * halfSpan * direction * sign + dx * halfSpan * offset,
                 center.y() + dy * halfSpan * direction * sign + dy * halfSpan * offset);
}

QImage makeSolidGradientImage(const QSize& size,
                              const FloatColor& startColor,
                              const FloatColor& endColor,
                              const float angleDegrees,
                              const bool reverse,
                              const float centerX,
                              const float centerY,
                              const float scale,
                              const float offset) {
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, false);
  const QPointF gradientStart =
      gradientPointForAngle(angleDegrees, size, true, reverse, centerX, centerY, scale, offset);
  const QPointF gradientEnd =
      gradientPointForAngle(angleDegrees, size, false, reverse, centerX, centerY, scale, offset);
  QLinearGradient gradient(gradientStart, gradientEnd);
  gradient.setColorAt(0.0, toQColor(startColor));
  gradient.setColorAt(1.0, toQColor(endColor));
  painter.fillRect(image.rect(), gradient);
  return image;
}
}

ArtifactSolidImageLayerSettings::ArtifactSolidImageLayerSettings() = default;
ArtifactSolidImageLayerSettings::~ArtifactSolidImageLayerSettings() = default;

class ArtifactSolidImageLayer::Impl {
public:
  AnimatableValueT<FloatColor> color_;
  FloatColor defaultColor_ =
      FloatColor(1.0f, 1.0f, 1.0f, 1.0f); // デフォルトは白色
  ArtifactSolidFillType fillType_ = ArtifactSolidFillType::Solid;
  FloatColor gradientStartColor_ = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
  FloatColor gradientEndColor_ = FloatColor(0.2f, 0.2f, 0.2f, 1.0f);
  float gradientAngleDegrees_ = 90.0f;
  bool gradientReverse_ = false;
  float gradientCenterX_ = 0.5f;
  float gradientCenterY_ = 0.5f;
  float gradientScale_ = 1.0f;
  float gradientOffset_ = 0.0f;
  mutable QImage cachedImage_;
  mutable QSize cachedSize_;
  mutable FloatColor cachedColor_ = FloatColor(-1.0f, -1.0f, -1.0f, -1.0f);
  mutable ArtifactSolidFillType cachedFillType_ = ArtifactSolidFillType::Solid;
  mutable FloatColor cachedGradientStartColor_ = FloatColor(-1.0f, -1.0f, -1.0f, -1.0f);
  mutable FloatColor cachedGradientEndColor_ = FloatColor(-1.0f, -1.0f, -1.0f, -1.0f);
  mutable float cachedGradientAngleDegrees_ = -1000.0f;
  mutable bool cachedGradientReverse_ = false;
  mutable float cachedGradientCenterX_ = -1.0f;
  mutable float cachedGradientCenterY_ = -1.0f;
  mutable float cachedGradientScale_ = -1.0f;
  mutable float cachedGradientOffset_ = -1000.0f;

  Impl() {
    // デフォルトのキーフレームを追加（フレーム0）
    color_.addKeyFrame(FramePosition(0), defaultColor_);
  }
};

ArtifactSolidImageLayer::ArtifactSolidImageLayer() : impl_(new Impl()) {}

ArtifactSolidImageLayer::~ArtifactSolidImageLayer() { delete impl_; }

FloatColor ArtifactSolidImageLayer::color() const {
  if (const auto property = getProperty(QStringLiteral("solid.color"))) {
    const QVariant value =
        property->getKeyFrames().empty()
            ? property->getValue()
            : property->interpolateValue(effectiveSolidTimelineTime(this));
    if (value.canConvert<QColor>()) {
      return toFloatColor(value.value<QColor>());
    }
  }

  const auto frame = effectiveSolidTimelineFramePosition(this);
  if (impl_->color_.getKeyFrameCount() == 0) {
    return impl_->defaultColor_;
  }
  return impl_->color_.at(frame);
}

void ArtifactSolidImageLayer::setColor(const FloatColor &color) {
  const auto frame = effectiveSolidTimelineFramePosition(this);
  if (impl_->color_.hasKeyFrameAt(frame)) {
    impl_->color_.removeKeyFrameAt(frame);
  }
  impl_->color_.addKeyFrame(frame, color);

  if (const auto property = getProperty(QStringLiteral("solid.color"))) {
    const QColor nextColor = QColor::fromRgbF(color.r(), color.g(), color.b(),
                                              color.a());
    property->setAnimatable(true);
    if (!property->getKeyFrames().empty()) {
      property->addKeyFrame(effectiveSolidTimelineTime(this),
                            QVariant::fromValue(nextColor));
    } else {
      property->setColorValue(nextColor);
      property->setValue(QVariant::fromValue(nextColor));
    }
  }
}

ArtifactSolidFillType ArtifactSolidImageLayer::fillType() const {
  return impl_->fillType_;
}

void ArtifactSolidImageLayer::setFillType(const ArtifactSolidFillType fillType) {
  impl_->fillType_ = fillType;
}

bool ArtifactSolidImageLayer::isGradientEnabled() const {
  return fillType() == ArtifactSolidFillType::LinearGradient;
}

FloatColor ArtifactSolidImageLayer::gradientStartColor() const {
  return impl_->gradientStartColor_;
}

void ArtifactSolidImageLayer::setGradientStartColor(const FloatColor& color) {
  impl_->gradientStartColor_ = color;
}

FloatColor ArtifactSolidImageLayer::gradientEndColor() const {
  return impl_->gradientEndColor_;
}

void ArtifactSolidImageLayer::setGradientEndColor(const FloatColor& color) {
  impl_->gradientEndColor_ = color;
}

float ArtifactSolidImageLayer::gradientAngleDegrees() const {
  return impl_->gradientAngleDegrees_;
}

void ArtifactSolidImageLayer::setGradientAngleDegrees(const float degrees) {
  impl_->gradientAngleDegrees_ = degrees;
}

bool ArtifactSolidImageLayer::gradientReverse() const { return impl_->gradientReverse_; }
void ArtifactSolidImageLayer::setGradientReverse(const bool reverse) { impl_->gradientReverse_ = reverse; }
float ArtifactSolidImageLayer::gradientCenterX() const { return impl_->gradientCenterX_; }
void ArtifactSolidImageLayer::setGradientCenterX(const float value) { impl_->gradientCenterX_ = value; }
float ArtifactSolidImageLayer::gradientCenterY() const { return impl_->gradientCenterY_; }
void ArtifactSolidImageLayer::setGradientCenterY(const float value) { impl_->gradientCenterY_ = value; }
float ArtifactSolidImageLayer::gradientScale() const { return impl_->gradientScale_; }
void ArtifactSolidImageLayer::setGradientScale(const float value) { impl_->gradientScale_ = value; }
float ArtifactSolidImageLayer::gradientOffset() const { return impl_->gradientOffset_; }
void ArtifactSolidImageLayer::setGradientOffset(const float value) { impl_->gradientOffset_ = value; }

void ArtifactSolidImageLayer::setSize(const int width, const int height) {
  setSourceSize(Size_2D(width, height));
}

QJsonObject ArtifactSolidImageLayer::toJson() const {
  QJsonObject obj = ArtifactAbstract2DLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Solid);
  obj["solidWidth"] = sourceSize().width;
  obj["solidHeight"] = sourceSize().height;
  QJsonObject colorObj;
  const auto c = color();
  colorObj["r"] = c.r();
  colorObj["g"] = c.g();
  colorObj["b"] = c.b();
  colorObj["a"] = c.a();
  obj["solidColor"] = colorObj;
  obj["solidFillType"] = static_cast<int>(fillType());
  QJsonObject gradientStartObj;
  const auto start = gradientStartColor();
  gradientStartObj["r"] = start.r();
  gradientStartObj["g"] = start.g();
  gradientStartObj["b"] = start.b();
  gradientStartObj["a"] = start.a();
  obj["solidGradientStartColor"] = gradientStartObj;
  QJsonObject gradientEndObj;
  const auto end = gradientEndColor();
  gradientEndObj["r"] = end.r();
  gradientEndObj["g"] = end.g();
  gradientEndObj["b"] = end.b();
  gradientEndObj["a"] = end.a();
  obj["solidGradientEndColor"] = gradientEndObj;
  obj["solidGradientAngleDegrees"] = gradientAngleDegrees();
  obj["solidGradientReverse"] = gradientReverse();
  obj["solidGradientCenterX"] = gradientCenterX();
  obj["solidGradientCenterY"] = gradientCenterY();
  obj["solidGradientScale"] = gradientScale();
  obj["solidGradientOffset"] = gradientOffset();
  return obj;
}

void ArtifactSolidImageLayer::fromJsonProperties(const QJsonObject &obj) {
  ArtifactAbstract2DLayer::fromJsonProperties(obj);
  if (obj.contains("solidWidth") || obj.contains("solidHeight")) {
    const int width = obj.value("solidWidth").toInt(sourceSize().width);
    const int height = obj.value("solidHeight").toInt(sourceSize().height);
    setSize(width, height);
  }
  if (obj.contains("solidColor") && obj["solidColor"].isObject()) {
    const auto colorObj = obj["solidColor"].toObject();
    setColor(FloatColor(static_cast<float>(colorObj.value("r").toDouble(1.0)),
                        static_cast<float>(colorObj.value("g").toDouble(1.0)),
                        static_cast<float>(colorObj.value("b").toDouble(1.0)),
                        static_cast<float>(colorObj.value("a").toDouble(1.0))));
  }
  setFillType(static_cast<ArtifactSolidFillType>(
      obj.value("solidFillType").toInt(static_cast<int>(ArtifactSolidFillType::Solid))));
  if (obj.contains("solidGradientStartColor") && obj["solidGradientStartColor"].isObject()) {
    const auto startObj = obj["solidGradientStartColor"].toObject();
    setGradientStartColor(FloatColor(
        static_cast<float>(startObj.value("r").toDouble(1.0)),
        static_cast<float>(startObj.value("g").toDouble(1.0)),
        static_cast<float>(startObj.value("b").toDouble(1.0)),
        static_cast<float>(startObj.value("a").toDouble(1.0))));
  }
  if (obj.contains("solidGradientEndColor") && obj["solidGradientEndColor"].isObject()) {
    const auto endObj = obj["solidGradientEndColor"].toObject();
    setGradientEndColor(FloatColor(
        static_cast<float>(endObj.value("r").toDouble(0.2)),
        static_cast<float>(endObj.value("g").toDouble(0.2)),
        static_cast<float>(endObj.value("b").toDouble(0.2)),
        static_cast<float>(endObj.value("a").toDouble(1.0))));
  }
  if (obj.contains("solidGradientAngleDegrees")) {
    setGradientAngleDegrees(
        static_cast<float>(obj.value("solidGradientAngleDegrees").toDouble(90.0)));
  }
  setGradientReverse(obj.value("solidGradientReverse").toBool(false));
  setGradientCenterX(static_cast<float>(obj.value("solidGradientCenterX").toDouble(0.5)));
  setGradientCenterY(static_cast<float>(obj.value("solidGradientCenterY").toDouble(0.5)));
  setGradientScale(static_cast<float>(obj.value("solidGradientScale").toDouble(1.0)));
  setGradientOffset(static_cast<float>(obj.value("solidGradientOffset").toDouble(0.0)));
}

std::vector<ArtifactCore::PropertyGroup>
ArtifactSolidImageLayer::getLayerPropertyGroups() const {
  auto groups = ArtifactAbstract2DLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup solidGroup(QStringLiteral("Solid"));

  auto property = persistentLayerProperty(QStringLiteral("solid.color"),
                                          ArtifactCore::PropertyType::Color,
                                          QVariant(), -120);
  const auto c = color();
  property->setColorValue(QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()));
  property->setValue(property->getColorValue());
  property->setAnimatable(true); // キーフレーム可能に設定
  property->setDisplayLabel(QStringLiteral("Color"));
  solidGroup.addProperty(property);

  auto fillTypeProperty = persistentLayerProperty(
      QStringLiteral("solid.fillType"), ArtifactCore::PropertyType::Integer,
      static_cast<int>(fillType()), -119);
  fillTypeProperty->setValue(static_cast<int>(fillType()));
  fillTypeProperty->setDisplayLabel(QStringLiteral("Fill Mode"));
  fillTypeProperty->setTooltip(QStringLiteral("0=Solid, 1=Linear Gradient"));
  solidGroup.addProperty(fillTypeProperty);

  const auto start = gradientStartColor();
  auto startProperty = persistentLayerProperty(
      QStringLiteral("solid.gradientStartColor"),
      ArtifactCore::PropertyType::Color,
      QColor::fromRgbF(start.r(), start.g(), start.b(), start.a()), -118);
  startProperty->setColorValue(QColor::fromRgbF(start.r(), start.g(), start.b(), start.a()));
  startProperty->setValue(startProperty->getColorValue());
  startProperty->setDisplayLabel(QStringLiteral("開始色"));
  solidGroup.addProperty(startProperty);

  const auto end = gradientEndColor();
  auto endProperty = persistentLayerProperty(
      QStringLiteral("solid.gradientEndColor"),
      ArtifactCore::PropertyType::Color,
      QColor::fromRgbF(end.r(), end.g(), end.b(), end.a()), -117);
  endProperty->setColorValue(QColor::fromRgbF(end.r(), end.g(), end.b(), end.a()));
  endProperty->setValue(endProperty->getColorValue());
  endProperty->setDisplayLabel(QStringLiteral("終了色"));
  solidGroup.addProperty(endProperty);

  auto angleProperty = persistentLayerProperty(
      QStringLiteral("solid.gradientAngleDegrees"),
      ArtifactCore::PropertyType::Float,
      gradientAngleDegrees(), -116);
  angleProperty->setValue(gradientAngleDegrees());
  angleProperty->setDisplayLabel(QStringLiteral("角度"));
  angleProperty->setTooltip(QStringLiteral("Linear gradient angle in degrees"));
  solidGroup.addProperty(angleProperty);

  auto reverseProperty = persistentLayerProperty(QStringLiteral("solid.gradientReverse"),
                                                 ArtifactCore::PropertyType::Boolean,
                                                 gradientReverse(), -115);
  reverseProperty->setValue(gradientReverse());
  reverseProperty->setDisplayLabel(QStringLiteral("反転"));
  solidGroup.addProperty(reverseProperty);

  auto centerXProperty = persistentLayerProperty(QStringLiteral("solid.gradientCenterX"),
                                                 ArtifactCore::PropertyType::Float,
                                                 gradientCenterX(), -114);
  centerXProperty->setValue(gradientCenterX());
  centerXProperty->setDisplayLabel(QStringLiteral("中心X"));
  solidGroup.addProperty(centerXProperty);

  auto centerYProperty = persistentLayerProperty(QStringLiteral("solid.gradientCenterY"),
                                                 ArtifactCore::PropertyType::Float,
                                                 gradientCenterY(), -113);
  centerYProperty->setValue(gradientCenterY());
  centerYProperty->setDisplayLabel(QStringLiteral("中心Y"));
  solidGroup.addProperty(centerYProperty);

  auto scaleProperty = persistentLayerProperty(QStringLiteral("solid.gradientScale"),
                                               ArtifactCore::PropertyType::Float,
                                               gradientScale(), -112);
  scaleProperty->setValue(gradientScale());
  scaleProperty->setDisplayLabel(QStringLiteral("拡大率"));
  solidGroup.addProperty(scaleProperty);

  auto offsetProperty = persistentLayerProperty(QStringLiteral("solid.gradientOffset"),
                                                ArtifactCore::PropertyType::Float,
                                                gradientOffset(), -111);
  offsetProperty->setValue(gradientOffset());
  offsetProperty->setDisplayLabel(QStringLiteral("オフセット"));
  solidGroup.addProperty(offsetProperty);

  groups.push_back(solidGroup);
  return groups;
}

bool ArtifactSolidImageLayer::setLayerPropertyValue(const QString &propertyPath,
                                                    const QVariant &value) {
  if (propertyPath == QStringLiteral("solid.color")) {
    const auto c = value.value<QColor>();
    setColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("solid.fillType")) {
    setFillType(value.toInt() >= 1 ? ArtifactSolidFillType::LinearGradient
                                   : ArtifactSolidFillType::Solid);
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientStartColor")) {
    const auto c = value.value<QColor>();
    setGradientStartColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientEndColor")) {
    const auto c = value.value<QColor>();
    setGradientEndColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientAngleDegrees")) {
    setGradientAngleDegrees(value.toFloat());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientReverse")) {
    setGradientReverse(value.toBool());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientCenterX")) {
    setGradientCenterX(value.toFloat());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientCenterY")) {
    setGradientCenterY(value.toFloat());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientScale")) {
    setGradientScale(value.toFloat());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientOffset")) {
    setGradientOffset(value.toFloat());
    Q_EMIT changed();
    return true;
  }
  return ArtifactAbstract2DLayer::setLayerPropertyValue(propertyPath, value);
}

void ArtifactSolidImageLayer::draw(ArtifactIRenderer *renderer) {
  const auto size = sourceSize();
  const auto color = this->color();
  const auto fillType = this->fillType();
  const auto gradientStart = this->gradientStartColor();
  const auto gradientEnd = this->gradientEndColor();
  const float gradientAngle = this->gradientAngleDegrees();

  static int drawLogSamples = 0;
  if (drawLogSamples < 5) {
    ++drawLogSamples;
    qCDebug(solidImageLayerLog)
        << "[ArtifactSolidImageLayer::draw] id:" << id().toString()
        << "currentFrame:" << currentFrame() << "color: (" << color.r()
        << color.g() << color.b() << color.a() << ")"
        << "size:" << size.width << "x" << size.height
        << "opacity:" << opacity();
  }

  const QMatrix4x4 baseTransform = getGlobalTransform4x4();
  drawWithClonerEffect(
      this, baseTransform,
      [renderer, size, color, fillType, gradientStart, gradientEnd, gradientAngle,
       this]
      (const QMatrix4x4 &transform, float weight) {
        if (fillType == ArtifactSolidFillType::LinearGradient) {
          QImage gradientImage = makeSolidGradientImage(
              QSize(size.width, size.height),
              FloatColor(gradientStart.r(), gradientStart.g(), gradientStart.b(),
                         gradientStart.a() * this->opacity() * weight),
              FloatColor(gradientEnd.r(), gradientEnd.g(), gradientEnd.b(),
                         gradientEnd.a() * this->opacity() * weight),
              gradientAngle, gradientReverse(), gradientCenterX(), gradientCenterY(),
              gradientScale(), gradientOffset());
          renderer->drawSpriteTransformed(0.0f, 0.0f, static_cast<float>(size.width),
                                          static_cast<float>(size.height), transform,
                                          gradientImage, 1.0f);
          return;
        }
        const FloatColor cloneColor(color.r(), color.g(), color.b(),
                                    color.a() * this->opacity() * weight);
        renderer->drawSolidRectTransformed(
            0.0f, 0.0f, static_cast<float>(size.width),
            static_cast<float>(size.height), transform, cloneColor, 1.0f);
      });
}

QImage ArtifactSolidImageLayer::toQImage() const {
  const auto size = sourceSize();
  if (size.width <= 0 || size.height <= 0) {
    return QImage();
  }
  const auto color = this->color();
  const QSize targetSize(size.width, size.height);
  if (!impl_->cachedImage_.isNull() && impl_->cachedSize_ == targetSize &&
      impl_->cachedColor_.r() == color.r() &&
      impl_->cachedColor_.g() == color.g() &&
      impl_->cachedColor_.b() == color.b() &&
      impl_->cachedColor_.a() == color.a() &&
      impl_->cachedFillType_ == fillType() &&
      impl_->cachedGradientStartColor_.r() == gradientStartColor().r() &&
      impl_->cachedGradientStartColor_.g() == gradientStartColor().g() &&
      impl_->cachedGradientStartColor_.b() == gradientStartColor().b() &&
      impl_->cachedGradientStartColor_.a() == gradientStartColor().a() &&
      impl_->cachedGradientEndColor_.r() == gradientEndColor().r() &&
      impl_->cachedGradientEndColor_.g() == gradientEndColor().g() &&
      impl_->cachedGradientEndColor_.b() == gradientEndColor().b() &&
      impl_->cachedGradientEndColor_.a() == gradientEndColor().a() &&
      impl_->cachedGradientAngleDegrees_ == gradientAngleDegrees() &&
      impl_->cachedGradientReverse_ == gradientReverse() &&
      impl_->cachedGradientCenterX_ == gradientCenterX() &&
      impl_->cachedGradientCenterY_ == gradientCenterY() &&
      impl_->cachedGradientScale_ == gradientScale() &&
      impl_->cachedGradientOffset_ == gradientOffset()) {
    return impl_->cachedImage_;
  }

  if (fillType() == ArtifactSolidFillType::LinearGradient) {
    impl_->cachedImage_ = makeSolidGradientImage(
        targetSize, gradientStartColor(), gradientEndColor(), gradientAngleDegrees(),
        gradientReverse(), gradientCenterX(), gradientCenterY(), gradientScale(),
        gradientOffset());
  } else {
    const auto c = QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
    impl_->cachedImage_ =
        QImage(size.width, size.height, QImage::Format_ARGB32_Premultiplied);
    impl_->cachedImage_.fill(c);
  }
  impl_->cachedSize_ = targetSize;
  impl_->cachedColor_ = color;
  impl_->cachedFillType_ = fillType();
  impl_->cachedGradientStartColor_ = gradientStartColor();
  impl_->cachedGradientEndColor_ = gradientEndColor();
  impl_->cachedGradientAngleDegrees_ = gradientAngleDegrees();
  impl_->cachedGradientReverse_ = gradientReverse();
  impl_->cachedGradientCenterX_ = gradientCenterX();
  impl_->cachedGradientCenterY_ = gradientCenterY();
  impl_->cachedGradientScale_ = gradientScale();
  impl_->cachedGradientOffset_ = gradientOffset();
  return impl_->cachedImage_;
}
} // namespace Artifact
