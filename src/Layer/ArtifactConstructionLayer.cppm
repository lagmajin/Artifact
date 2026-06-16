module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QVariant>

module Artifact.Layer.Construction;

import std;
import Artifact.Layer.Abstract;
import Artifact.Layers.Abstract._2D;
import Artifact.Render.IRenderer;
import Color.Float;
import Property.Group;
import Property;

namespace Artifact {

class ArtifactConstructionLayer::Impl {
public:
  Impl() = default;
  ~Impl() = default;

  float width = 1920.0f;
  float height = 1080.0f;
  float gridSpacing = 64.0f;
  float majorEvery = 4.0f;
  float safeMargin = 0.10f;
  float baselineY = 0.82f;
  float opacity = 0.70f;
  bool showGrid = true;
  bool showThirds = true;
  bool showCenter = true;
  bool showSafeArea = true;
  bool showBaseline = true;
  bool renderAsDesign = false;
  bool includeInFinalRender = false;

  FloatColor guideColor() const
  {
    return renderAsDesign ? FloatColor{0.18f, 0.86f, 1.0f, 1.0f}
                          : FloatColor{0.20f, 0.78f, 1.0f, 0.90f};
  }
};

ArtifactConstructionLayer::ArtifactConstructionLayer()
    : impl_(new Impl()) {
  setSourceSize(Size_2D(static_cast<int>(impl_->width), static_cast<int>(impl_->height)));
  setLayerName(QString("Construction Layer"));
  setGuide(true);
}

ArtifactConstructionLayer::~ArtifactConstructionLayer() {
  delete impl_;
}

void ArtifactConstructionLayer::draw(ArtifactIRenderer* renderer) {
  if (!renderer) {
    return;
  }

  const float w = std::max(1.0f, impl_->width);
  const float h = std::max(1.0f, impl_->height);
  const float opacity = std::clamp(impl_->opacity, 0.0f, 1.0f) * this->opacity();
  if (opacity <= 0.001f) {
    return;
  }

  const auto transform = getGlobalTransform4x4();
  const FloatColor cyan = impl_->guideColor();
  const FloatColor faint = {cyan.red(), cyan.green(), cyan.blue(), impl_->renderAsDesign ? 0.28f : 0.20f};
  const FloatColor strong = {cyan.red(), cyan.green(), cyan.blue(), impl_->renderAsDesign ? 0.96f : 0.78f};
  const FloatColor warm = {1.0f, 0.72f, 0.22f, impl_->renderAsDesign ? 0.92f : 0.68f};

  if (impl_->showGrid && impl_->gridSpacing > 2.0f) {
    const float spacing = std::max(2.0f, impl_->gridSpacing);
    for (float x = 0.0f; x <= w + 0.5f; x += spacing) {
      renderer->drawSolidRectTransformed(x - 0.5f, 0.0f, 1.0f, h, transform, faint, opacity);
    }
    for (float y = 0.0f; y <= h + 0.5f; y += spacing) {
      renderer->drawSolidRectTransformed(0.0f, y - 0.5f, w, 1.0f, transform, faint, opacity);
    }

    const int majorEvery = std::max(1, static_cast<int>(std::round(impl_->majorEvery)));
    const float majorSpacing = spacing * static_cast<float>(majorEvery);
    for (float x = 0.0f; x <= w + 0.5f; x += majorSpacing) {
      renderer->drawSolidRectTransformed(x - 1.0f, 0.0f, 2.0f, h, transform, strong, opacity * 0.75f);
    }
    for (float y = 0.0f; y <= h + 0.5f; y += majorSpacing) {
      renderer->drawSolidRectTransformed(0.0f, y - 1.0f, w, 2.0f, transform, strong, opacity * 0.75f);
    }
  }

  if (impl_->showThirds) {
    const float x1 = w / 3.0f;
    const float x2 = w * 2.0f / 3.0f;
    const float y1 = h / 3.0f;
    const float y2 = h * 2.0f / 3.0f;
    renderer->drawSolidRectTransformed(x1 - 1.0f, 0.0f, 2.0f, h, transform, warm, opacity);
    renderer->drawSolidRectTransformed(x2 - 1.0f, 0.0f, 2.0f, h, transform, warm, opacity);
    renderer->drawSolidRectTransformed(0.0f, y1 - 1.0f, w, 2.0f, transform, warm, opacity);
    renderer->drawSolidRectTransformed(0.0f, y2 - 1.0f, w, 2.0f, transform, warm, opacity);
  }

  if (impl_->showCenter) {
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;
    renderer->drawSolidRectTransformed(cx - 1.5f, 0.0f, 3.0f, h, transform, strong, opacity);
    renderer->drawSolidRectTransformed(0.0f, cy - 1.5f, w, 3.0f, transform, strong, opacity);
    renderer->drawSolidRectTransformed(cx - 8.0f, cy - 1.0f, 16.0f, 2.0f, transform, strong, opacity);
    renderer->drawSolidRectTransformed(cx - 1.0f, cy - 8.0f, 2.0f, 16.0f, transform, strong, opacity);
  }

  if (impl_->showSafeArea) {
    const float margin = std::clamp(impl_->safeMargin, 0.0f, 0.45f);
    const float sx = w * margin;
    const float sy = h * margin;
    const float sw = w - sx * 2.0f;
    const float sh = h - sy * 2.0f;
    renderer->drawSolidRectTransformed(sx, sy, sw, 2.0f, transform, strong, opacity);
    renderer->drawSolidRectTransformed(sx, sy + sh - 2.0f, sw, 2.0f, transform, strong, opacity);
    renderer->drawSolidRectTransformed(sx, sy, 2.0f, sh, transform, strong, opacity);
    renderer->drawSolidRectTransformed(sx + sw - 2.0f, sy, 2.0f, sh, transform, strong, opacity);
  }

  if (impl_->showBaseline) {
    const float y = h * std::clamp(impl_->baselineY, 0.0f, 1.0f);
    renderer->drawSolidRectTransformed(0.0f, y - 1.0f, w, 2.0f, transform, warm, opacity);
  }
}

bool ArtifactConstructionLayer::isNullLayer() const {
  return false;
}

bool ArtifactConstructionLayer::hasVideo() const {
  return false;
}

QJsonObject ArtifactConstructionLayer::toJson() const {
  QJsonObject obj = ArtifactAbstract2DLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Construction);
  obj["isConstruction"] = true;
  obj["construction.width"] = impl_->width;
  obj["construction.height"] = impl_->height;
  obj["construction.gridSpacing"] = impl_->gridSpacing;
  obj["construction.majorEvery"] = impl_->majorEvery;
  obj["construction.safeMargin"] = impl_->safeMargin;
  obj["construction.baselineY"] = impl_->baselineY;
  obj["construction.opacity"] = impl_->opacity;
  obj["construction.showGrid"] = impl_->showGrid;
  obj["construction.showThirds"] = impl_->showThirds;
  obj["construction.showCenter"] = impl_->showCenter;
  obj["construction.showSafeArea"] = impl_->showSafeArea;
  obj["construction.showBaseline"] = impl_->showBaseline;
  obj["construction.renderAsDesign"] = impl_->renderAsDesign;
  obj["construction.includeInFinalRender"] = impl_->includeInFinalRender;
  return obj;
}

void ArtifactConstructionLayer::fromJsonProperties(const QJsonObject& obj) {
  ArtifactAbstract2DLayer::fromJsonProperties(obj);
  impl_->width = static_cast<float>(obj.value(QStringLiteral("construction.width")).toDouble(impl_->width));
  impl_->height = static_cast<float>(obj.value(QStringLiteral("construction.height")).toDouble(impl_->height));
  impl_->gridSpacing = static_cast<float>(obj.value(QStringLiteral("construction.gridSpacing")).toDouble(impl_->gridSpacing));
  impl_->majorEvery = static_cast<float>(obj.value(QStringLiteral("construction.majorEvery")).toDouble(impl_->majorEvery));
  impl_->safeMargin = static_cast<float>(obj.value(QStringLiteral("construction.safeMargin")).toDouble(impl_->safeMargin));
  impl_->baselineY = static_cast<float>(obj.value(QStringLiteral("construction.baselineY")).toDouble(impl_->baselineY));
  impl_->opacity = static_cast<float>(obj.value(QStringLiteral("construction.opacity")).toDouble(impl_->opacity));
  impl_->showGrid = obj.value(QStringLiteral("construction.showGrid")).toBool(impl_->showGrid);
  impl_->showThirds = obj.value(QStringLiteral("construction.showThirds")).toBool(impl_->showThirds);
  impl_->showCenter = obj.value(QStringLiteral("construction.showCenter")).toBool(impl_->showCenter);
  impl_->showSafeArea = obj.value(QStringLiteral("construction.showSafeArea")).toBool(impl_->showSafeArea);
  impl_->showBaseline = obj.value(QStringLiteral("construction.showBaseline")).toBool(impl_->showBaseline);
  impl_->renderAsDesign = obj.value(QStringLiteral("construction.renderAsDesign")).toBool(impl_->renderAsDesign);
  impl_->includeInFinalRender = obj.value(QStringLiteral("construction.includeInFinalRender")).toBool(impl_->includeInFinalRender);
  setSourceSize(Size_2D(static_cast<int>(std::round(impl_->width)),
                        static_cast<int>(std::round(impl_->height))));
}

bool ArtifactConstructionLayer::isConstructionLayer() const {
  return true;
}

bool ArtifactConstructionLayer::shouldIncludeInFinalRender() const {
  return impl_->includeInFinalRender;
}

std::vector<ArtifactCore::PropertyGroup> ArtifactConstructionLayer::getLayerPropertyGroups() const {
  using namespace ArtifactCore;
  auto groups = ArtifactAbstract2DLayer::getLayerPropertyGroups();
  PropertyGroup group(QStringLiteral("Construction"));

  auto makeProp = [this](const QString& name, PropertyType type, const QVariant& value, int priority = 0) {
    return persistentLayerProperty(name, type, value, priority);
  };

  auto widthProp = makeProp(QStringLiteral("construction.width"), PropertyType::Float, impl_->width, -120);
  widthProp->setSoftRange(16.0, 8192.0);
  widthProp->setUnit(QStringLiteral("px"));
  group.addProperty(widthProp);

  auto heightProp = makeProp(QStringLiteral("construction.height"), PropertyType::Float, impl_->height, -119);
  heightProp->setSoftRange(16.0, 8192.0);
  heightProp->setUnit(QStringLiteral("px"));
  group.addProperty(heightProp);

  auto spacingProp = makeProp(QStringLiteral("construction.gridSpacing"), PropertyType::Float, impl_->gridSpacing, -110);
  spacingProp->setSoftRange(4.0, 512.0);
  spacingProp->setUnit(QStringLiteral("px"));
  group.addProperty(spacingProp);

  auto majorProp = makeProp(QStringLiteral("construction.majorEvery"), PropertyType::Float, impl_->majorEvery, -109);
  majorProp->setSoftRange(1.0, 12.0);
  group.addProperty(majorProp);

  auto safeProp = makeProp(QStringLiteral("construction.safeMargin"), PropertyType::Float, impl_->safeMargin, -100);
  safeProp->setSoftRange(0.0, 0.45);
  safeProp->setStep(0.01);
  group.addProperty(safeProp);

  auto baselineProp = makeProp(QStringLiteral("construction.baselineY"), PropertyType::Float, impl_->baselineY, -99);
  baselineProp->setSoftRange(0.0, 1.0);
  baselineProp->setStep(0.01);
  group.addProperty(baselineProp);

  auto opacityProp = makeProp(QStringLiteral("construction.opacity"), PropertyType::Float, impl_->opacity, -98);
  opacityProp->setSoftRange(0.0, 1.0);
  opacityProp->setStep(0.01);
  group.addProperty(opacityProp);

  group.addProperty(makeProp(QStringLiteral("construction.showGrid"), PropertyType::Boolean, impl_->showGrid, -90));
  group.addProperty(makeProp(QStringLiteral("construction.showThirds"), PropertyType::Boolean, impl_->showThirds, -89));
  group.addProperty(makeProp(QStringLiteral("construction.showCenter"), PropertyType::Boolean, impl_->showCenter, -88));
  group.addProperty(makeProp(QStringLiteral("construction.showSafeArea"), PropertyType::Boolean, impl_->showSafeArea, -87));
  group.addProperty(makeProp(QStringLiteral("construction.showBaseline"), PropertyType::Boolean, impl_->showBaseline, -86));
  group.addProperty(makeProp(QStringLiteral("construction.renderAsDesign"), PropertyType::Boolean, impl_->renderAsDesign, -80));
  group.addProperty(makeProp(QStringLiteral("construction.includeInFinalRender"), PropertyType::Boolean, impl_->includeInFinalRender, -79));

  groups.push_back(group);
  return groups;
}

bool ArtifactConstructionLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value) {
  bool sizeChanged = false;
  if (propertyPath == QStringLiteral("construction.width")) {
    impl_->width = std::max(1.0f, value.toFloat());
    sizeChanged = true;
  } else if (propertyPath == QStringLiteral("construction.height")) {
    impl_->height = std::max(1.0f, value.toFloat());
    sizeChanged = true;
  } else if (propertyPath == QStringLiteral("construction.gridSpacing")) {
    impl_->gridSpacing = std::max(2.0f, value.toFloat());
  } else if (propertyPath == QStringLiteral("construction.majorEvery")) {
    impl_->majorEvery = std::clamp(value.toFloat(), 1.0f, 64.0f);
  } else if (propertyPath == QStringLiteral("construction.safeMargin")) {
    impl_->safeMargin = std::clamp(value.toFloat(), 0.0f, 0.45f);
  } else if (propertyPath == QStringLiteral("construction.baselineY")) {
    impl_->baselineY = std::clamp(value.toFloat(), 0.0f, 1.0f);
  } else if (propertyPath == QStringLiteral("construction.opacity")) {
    impl_->opacity = std::clamp(value.toFloat(), 0.0f, 1.0f);
  } else if (propertyPath == QStringLiteral("construction.showGrid")) {
    impl_->showGrid = value.toBool();
  } else if (propertyPath == QStringLiteral("construction.showThirds")) {
    impl_->showThirds = value.toBool();
  } else if (propertyPath == QStringLiteral("construction.showCenter")) {
    impl_->showCenter = value.toBool();
  } else if (propertyPath == QStringLiteral("construction.showSafeArea")) {
    impl_->showSafeArea = value.toBool();
  } else if (propertyPath == QStringLiteral("construction.showBaseline")) {
    impl_->showBaseline = value.toBool();
  } else if (propertyPath == QStringLiteral("construction.renderAsDesign")) {
    impl_->renderAsDesign = value.toBool();
    setGuide(!impl_->renderAsDesign);
  } else if (propertyPath == QStringLiteral("construction.includeInFinalRender")) {
    impl_->includeInFinalRender = value.toBool();
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
