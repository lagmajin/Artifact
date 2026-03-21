module;

#include <QColor>
#include <QVariant>

module Artifact.Layers.SolidImage;

import std;
import Artifact.Render.IRenderer;
import Property.Abstract;
import Property.Group;
import Animation.Value;

namespace Artifact {
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

std::vector<ArtifactCore::PropertyGroup>
ArtifactSolidImageLayer::getLayerPropertyGroups() const {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup solidGroup(QStringLiteral("Solid"));

  auto property = std::make_shared<ArtifactCore::AbstractProperty>();
  property->setName(QStringLiteral("solid.color"));
  property->setType(ArtifactCore::PropertyType::Color);
  const auto c = color();
  property->setColorValue(QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()));
  property->setValue(property->getColorValue());
  property->setDisplayPriority(-120);
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
  
  qDebug() << "[ArtifactSolidImageLayer::draw] id:" << id().toString()
           << "currentFrame:" << currentFrame() 
           << "color: (" << color.r() << color.g() << color.b() << color.a() << ")"
           << "size:" << size.width << "x" << size.height
           << "opacity:" << opacity();

  renderer->drawSolidRect(0.0f, 0.0f,
                          static_cast<float>(size.width),
                          static_cast<float>(size.height),
                          color,
                          this->opacity());
}
} // namespace Artifact
