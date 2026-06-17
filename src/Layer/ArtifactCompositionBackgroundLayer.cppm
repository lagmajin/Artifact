module;
#include <utility>
#include <algorithm>
#include <QVariant>

module Artifact.Layer.CompositionBackground;

import std;
import Artifact.Layer.Abstract;
import Artifact.Layers.Abstract._2D;
import Artifact.Render.IRenderer;
import Color.Float;
import Property.Group;
import Property;

namespace Artifact {

class ArtifactCompositionBackgroundLayer::Impl {
public:
  float width = 1920.0f;
  float height = 1080.0f;
  float fillOpacity = 1.0f;
  float borderOpacity = 0.85f;
  float borderThickness = 2.0f;
  FloatColor fillColor = {0.10f, 0.10f, 0.10f, 1.0f};
  FloatColor borderColor = {0.92f, 0.92f, 0.92f, 1.0f};
  bool showFill = true;
  bool showBorder = false;
  bool includeInFinalRender = false;
  bool ignoreBlendMode = true;
  bool ignoreAdjustmentLayer = true;
  bool ignoreTransparencySwitch = true;
};

ArtifactCompositionBackgroundLayer::ArtifactCompositionBackgroundLayer()
    : impl_(new Impl()) {
  setSourceSize(Size_2D(static_cast<int>(impl_->width), static_cast<int>(impl_->height)));
  setLayerName(QStringLiteral("Composition Background"));
  setGuide(true);
  setLocked(true);
}

ArtifactCompositionBackgroundLayer::~ArtifactCompositionBackgroundLayer() {
  delete impl_;
}

void ArtifactCompositionBackgroundLayer::draw(ArtifactIRenderer* renderer) {
  if (!renderer) {
    return;
  }

  const float w = std::max(1.0f, impl_->width);
  const float h = std::max(1.0f, impl_->height);
  const auto transform = getGlobalTransform4x4();

  if (impl_->showFill) {
    const float opacity = std::clamp(impl_->fillOpacity, 0.0f, 1.0f);
    if (opacity > 0.001f) {
      renderer->drawSolidRectTransformed(0.0f, 0.0f, w, h, transform,
                                         impl_->fillColor, opacity);
    }
  }

  if (impl_->showBorder) {
    const float thickness = std::max(1.0f, impl_->borderThickness);
    const float opacity = std::clamp(impl_->borderOpacity, 0.0f, 1.0f);
    if (opacity > 0.001f) {
      renderer->drawSolidRectTransformed(0.0f, 0.0f, w, thickness, transform,
                                         impl_->borderColor, opacity);
      renderer->drawSolidRectTransformed(0.0f, h - thickness, w, thickness, transform,
                                         impl_->borderColor, opacity);
      renderer->drawSolidRectTransformed(0.0f, 0.0f, thickness, h, transform,
                                         impl_->borderColor, opacity);
      renderer->drawSolidRectTransformed(w - thickness, 0.0f, thickness, h, transform,
                                         impl_->borderColor, opacity);
    }
  }
}

bool ArtifactCompositionBackgroundLayer::isNullLayer() const { return false; }

bool ArtifactCompositionBackgroundLayer::hasVideo() const { return false; }

QJsonObject ArtifactCompositionBackgroundLayer::toJson() const {
  QJsonObject obj = ArtifactAbstract2DLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::CompositionBackground);
  obj["isCompositionBackground"] = true;
  obj["background.width"] = impl_->width;
  obj["background.height"] = impl_->height;
  obj["background.fillOpacity"] = impl_->fillOpacity;
  obj["background.borderOpacity"] = impl_->borderOpacity;
  obj["background.borderThickness"] = impl_->borderThickness;
  obj["background.fill.r"] = impl_->fillColor.red();
  obj["background.fill.g"] = impl_->fillColor.green();
  obj["background.fill.b"] = impl_->fillColor.blue();
  obj["background.fill.a"] = impl_->fillColor.alpha();
  obj["background.border.r"] = impl_->borderColor.red();
  obj["background.border.g"] = impl_->borderColor.green();
  obj["background.border.b"] = impl_->borderColor.blue();
  obj["background.border.a"] = impl_->borderColor.alpha();
  obj["background.showFill"] = impl_->showFill;
  obj["background.showBorder"] = impl_->showBorder;
  obj["background.includeInFinalRender"] = impl_->includeInFinalRender;
  obj["background.ignoreBlendMode"] = impl_->ignoreBlendMode;
  obj["background.ignoreAdjustmentLayer"] = impl_->ignoreAdjustmentLayer;
  obj["background.ignoreTransparencySwitch"] = impl_->ignoreTransparencySwitch;
  return obj;
}

void ArtifactCompositionBackgroundLayer::fromJsonProperties(const QJsonObject& obj) {
  ArtifactAbstract2DLayer::fromJsonProperties(obj);
  impl_->width = static_cast<float>(obj.value(QStringLiteral("background.width")).toDouble(impl_->width));
  impl_->height = static_cast<float>(obj.value(QStringLiteral("background.height")).toDouble(impl_->height));
  impl_->fillOpacity = static_cast<float>(obj.value(QStringLiteral("background.fillOpacity")).toDouble(impl_->fillOpacity));
  impl_->borderOpacity = static_cast<float>(obj.value(QStringLiteral("background.borderOpacity")).toDouble(impl_->borderOpacity));
  impl_->borderThickness = static_cast<float>(obj.value(QStringLiteral("background.borderThickness")).toDouble(impl_->borderThickness));
  impl_->fillColor = {
      static_cast<float>(obj.value(QStringLiteral("background.fill.r")).toDouble(impl_->fillColor.red())),
      static_cast<float>(obj.value(QStringLiteral("background.fill.g")).toDouble(impl_->fillColor.green())),
      static_cast<float>(obj.value(QStringLiteral("background.fill.b")).toDouble(impl_->fillColor.blue())),
      static_cast<float>(obj.value(QStringLiteral("background.fill.a")).toDouble(impl_->fillColor.alpha()))
  };
  impl_->borderColor = {
      static_cast<float>(obj.value(QStringLiteral("background.border.r")).toDouble(impl_->borderColor.red())),
      static_cast<float>(obj.value(QStringLiteral("background.border.g")).toDouble(impl_->borderColor.green())),
      static_cast<float>(obj.value(QStringLiteral("background.border.b")).toDouble(impl_->borderColor.blue())),
      static_cast<float>(obj.value(QStringLiteral("background.border.a")).toDouble(impl_->borderColor.alpha()))
  };
  impl_->showFill = obj.value(QStringLiteral("background.showFill")).toBool(impl_->showFill);
  impl_->showBorder = obj.value(QStringLiteral("background.showBorder")).toBool(impl_->showBorder);
  impl_->includeInFinalRender = obj.value(QStringLiteral("background.includeInFinalRender")).toBool(impl_->includeInFinalRender);
  impl_->ignoreBlendMode = obj.value(QStringLiteral("background.ignoreBlendMode")).toBool(impl_->ignoreBlendMode);
  impl_->ignoreAdjustmentLayer = obj.value(QStringLiteral("background.ignoreAdjustmentLayer")).toBool(impl_->ignoreAdjustmentLayer);
  impl_->ignoreTransparencySwitch = obj.value(QStringLiteral("background.ignoreTransparencySwitch")).toBool(impl_->ignoreTransparencySwitch);
  setSourceSize(Size_2D(static_cast<int>(std::round(impl_->width)),
                        static_cast<int>(std::round(impl_->height))));
}

bool ArtifactCompositionBackgroundLayer::isCompositionBackgroundLayer() const {
  return true;
}

bool ArtifactCompositionBackgroundLayer::shouldIncludeInFinalRender() const {
  return impl_->includeInFinalRender;
}

std::vector<ArtifactCore::PropertyGroup> ArtifactCompositionBackgroundLayer::getLayerPropertyGroups() const {
  using namespace ArtifactCore;
  auto groups = ArtifactAbstract2DLayer::getLayerPropertyGroups();
  PropertyGroup group(QStringLiteral("Composition Background"));

  auto makeProp = [this](const QString& name, PropertyType type, const QVariant& value, int priority = 0) {
    return persistentLayerProperty(name, type, value, priority);
  };

  group.addProperty(makeProp(QStringLiteral("background.width"), PropertyType::Float, impl_->width, -120));
  group.addProperty(makeProp(QStringLiteral("background.height"), PropertyType::Float, impl_->height, -119));
  group.addProperty(makeProp(QStringLiteral("background.fillOpacity"), PropertyType::Float, impl_->fillOpacity, -110));
  group.addProperty(makeProp(QStringLiteral("background.borderOpacity"), PropertyType::Float, impl_->borderOpacity, -109));
  group.addProperty(makeProp(QStringLiteral("background.borderThickness"), PropertyType::Float, impl_->borderThickness, -108));
  group.addProperty(makeProp(QStringLiteral("background.showFill"), PropertyType::Boolean, impl_->showFill, -100));
  group.addProperty(makeProp(QStringLiteral("background.showBorder"), PropertyType::Boolean, impl_->showBorder, -99));
  group.addProperty(makeProp(QStringLiteral("background.includeInFinalRender"), PropertyType::Boolean, impl_->includeInFinalRender, -98));
  group.addProperty(makeProp(QStringLiteral("background.ignoreBlendMode"), PropertyType::Boolean, impl_->ignoreBlendMode, -97));
  group.addProperty(makeProp(QStringLiteral("background.ignoreAdjustmentLayer"), PropertyType::Boolean, impl_->ignoreAdjustmentLayer, -96));
  group.addProperty(makeProp(QStringLiteral("background.ignoreTransparencySwitch"), PropertyType::Boolean, impl_->ignoreTransparencySwitch, -95));

  groups.push_back(group);
  return groups;
}

bool ArtifactCompositionBackgroundLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value) {
  bool sizeChanged = false;
  if (propertyPath == QStringLiteral("background.width")) {
    impl_->width = std::max(1.0f, value.toFloat());
    sizeChanged = true;
  } else if (propertyPath == QStringLiteral("background.height")) {
    impl_->height = std::max(1.0f, value.toFloat());
    sizeChanged = true;
  } else if (propertyPath == QStringLiteral("background.fillOpacity")) {
    impl_->fillOpacity = std::clamp(value.toFloat(), 0.0f, 1.0f);
  } else if (propertyPath == QStringLiteral("background.borderOpacity")) {
    impl_->borderOpacity = std::clamp(value.toFloat(), 0.0f, 1.0f);
  } else if (propertyPath == QStringLiteral("background.borderThickness")) {
    impl_->borderThickness = std::max(1.0f, value.toFloat());
  } else if (propertyPath == QStringLiteral("background.showFill")) {
    impl_->showFill = value.toBool();
  } else if (propertyPath == QStringLiteral("background.showBorder")) {
    impl_->showBorder = value.toBool();
  } else if (propertyPath == QStringLiteral("background.includeInFinalRender")) {
    impl_->includeInFinalRender = value.toBool();
  } else if (propertyPath == QStringLiteral("background.ignoreBlendMode")) {
    impl_->ignoreBlendMode = value.toBool();
  } else if (propertyPath == QStringLiteral("background.ignoreAdjustmentLayer")) {
    impl_->ignoreAdjustmentLayer = value.toBool();
  } else if (propertyPath == QStringLiteral("background.ignoreTransparencySwitch")) {
    impl_->ignoreTransparencySwitch = value.toBool();
  } else {
    return ArtifactAbstract2DLayer::setLayerPropertyValue(propertyPath, value);
  }

  if (sizeChanged) {
    setSourceSize(Size_2D(static_cast<int>(std::round(impl_->width)),
                          static_cast<int>(std::round(impl_->height))));
  }
  return true;
}

} // namespace Artifact
