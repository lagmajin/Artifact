module;
#include <utility>
#define NOMINMAX
#define QT_NO_KEYWORDS
#include <Layer/ArtifactCloneEffectSupport.hpp>
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QSize>
#include <QVariant>


module Artifact.Layers.SolidImage;

import std;
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
}

ArtifactSolidImageLayerSettings::ArtifactSolidImageLayerSettings() = default;
ArtifactSolidImageLayerSettings::~ArtifactSolidImageLayerSettings() = default;

class ArtifactSolidImageLayer::Impl {
public:
  AnimatableValueT<FloatColor> color_;
  FloatColor defaultColor_ =
      FloatColor(1.0f, 1.0f, 1.0f, 1.0f); // デフォルトは白色
  mutable QImage cachedImage_;
  mutable QSize cachedSize_;
  mutable FloatColor cachedColor_ = FloatColor(-1.0f, -1.0f, -1.0f, -1.0f);

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

void ArtifactSolidImageLayer::setSize(const int width, const int height) {
  setSourceSize(Size_2D(width, height));
}

QJsonObject ArtifactSolidImageLayer::toJson() const {
  QJsonObject obj = ArtifactAbstractLayer::toJson();
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
  return obj;
}

void ArtifactSolidImageLayer::fromJsonProperties(const QJsonObject &obj) {
  ArtifactAbstractLayer::fromJsonProperties(obj);
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
}

std::vector<ArtifactCore::PropertyGroup>
ArtifactSolidImageLayer::getLayerPropertyGroups() const {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup solidGroup(QStringLiteral("Solid"));

  auto property = persistentLayerProperty(QStringLiteral("solid.color"),
                                          ArtifactCore::PropertyType::Color,
                                          QVariant(), -120);
  const auto c = color();
  property->setColorValue(QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()));
  property->setValue(property->getColorValue());
  property->setAnimatable(true); // キーフレーム可能に設定
  solidGroup.addProperty(property);

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
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

void ArtifactSolidImageLayer::draw(ArtifactIRenderer *renderer) {
  const auto size = sourceSize();
  const auto color = this->color();

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
      [renderer, size, color, this](const QMatrix4x4 &transform, float weight) {
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
      impl_->cachedColor_.a() == color.a()) {
    return impl_->cachedImage_;
  }

  const auto c = QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
  impl_->cachedImage_ =
      QImage(size.width, size.height, QImage::Format_ARGB32_Premultiplied);
  impl_->cachedImage_.fill(c);
  impl_->cachedSize_ = targetSize;
  impl_->cachedColor_ = color;
  return impl_->cachedImage_;
}
} // namespace Artifact
