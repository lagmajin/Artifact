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
#include <QVariant>


module Artifact.Layers.SolidImage;

import std;
import Artifact.Render.IRenderer;
import Property.Abstract;
import Property.Group;
import Animation.Value;

namespace Artifact {
namespace {
Q_LOGGING_CATEGORY(solidImageLayerLog, "artifact.solidimagelayer")
}

ArtifactSolidImageLayerSettings::ArtifactSolidImageLayerSettings() = default;
ArtifactSolidImageLayerSettings::~ArtifactSolidImageLayerSettings() = default;

class ArtifactSolidImageLayer::Impl {
public:
  AnimatableValueT<FloatColor> color_;
  FloatColor defaultColor_ =
      FloatColor(1.0f, 1.0f, 1.0f, 1.0f); // デフォルトは白色

  Impl() {
    // デフォルトのキーフレームを追加（フレーム0）
    color_.addKeyFrame(FramePosition(0), defaultColor_);
  }
};

ArtifactSolidImageLayer::ArtifactSolidImageLayer() : impl_(new Impl()) {}

ArtifactSolidImageLayer::~ArtifactSolidImageLayer() { delete impl_; }

FloatColor ArtifactSolidImageLayer::color() const {
  // 現在のフレーム位置に基づく補間値を返す
  auto frame = FramePosition(currentFrame());
  // キーフレームが存在しない場合はデフォルト値を返す
  if (impl_->color_.getKeyFrameCount() == 0) {
    return impl_->defaultColor_;
  }
  return impl_->color_.at(frame);
}

void ArtifactSolidImageLayer::setColor(const FloatColor &color) {
  // 現在のフレーム位置にキーフレームを追加または更新
  auto frame = FramePosition(currentFrame());
  // 既存のキーフレームを削除してから新しいキーフレームを追加
  if (impl_->color_.hasKeyFrameAt(frame)) {
    impl_->color_.removeKeyFrameAt(frame);
  }
  impl_->color_.addKeyFrame(frame, color);
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
  // 現在のフレーム位置に基づく補間値を取得
  auto frame = FramePosition(currentFrame());
  auto color = impl_->color_.at(frame);

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
  auto frame = FramePosition(currentFrame());
  auto color = impl_->color_.at(frame);
  const auto c = QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
  QImage image(size.width, size.height, QImage::Format_ARGB32_Premultiplied);
  image.fill(c);
  return image;
}
} // namespace Artifact
