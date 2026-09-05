module;
#include <utility>
#include <cmath>
#include <algorithm>
#include <QFont>
#include <QRectF>
#include <QSizeF>
#include <QMatrix4x4>
#include <QStringList>
#include <QVariant>
#include <QJsonArray>

module Artifact.Layer.Construction;

import Artifact.Layer.Abstract;
import Artifact.Layers.Abstract._2D;
import Artifact.Render.IRenderer;
import Color.Float;
import Property.Group;
import Property;

namespace Artifact {

namespace {
ConstructionItem defaultConstructionItem(ConstructionItemType type) {
  ConstructionItem item;
  item.type = type;
  item.enabled = false;
  item.start = QPointF(100, 100);
  item.end = QPointF(400, 100);
  item.center = QPointF(240, 240);
  item.radius = 80;
  item.text = QStringLiteral("Note");
  return item;
}
}

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
  GuideSet guideSet;
  std::vector<ConstructionItem> items;

  FloatColor guideColor() const
  {
    return renderAsDesign ? FloatColor{0.18f, 0.86f, 1.0f, 1.0f}
                          : FloatColor{0.20f, 0.78f, 1.0f, 0.90f};
  }
};

QJsonObject ConstructionItem::toJson() const {
  QJsonObject obj;
  obj["id"] = id; obj["type"] = static_cast<int>(type);
  obj["start.x"] = start.x(); obj["start.y"] = start.y();
  obj["end.x"] = end.x(); obj["end.y"] = end.y();
  obj["center.x"] = center.x(); obj["center.y"] = center.y();
  obj["radius"] = radius; obj["text"] = text;
  obj["enabled"] = enabled; obj["opacity"] = opacity;
  return obj;
}

ConstructionItem ConstructionItem::fromJson(const QJsonObject& obj) {
  ConstructionItem item;
  item.id = obj["id"].toString();
  const int rawType = obj["type"].toInt(0);
  item.type = rawType >= 0 && rawType <= 2 ? static_cast<ConstructionItemType>(rawType) : ConstructionItemType::Line;
  item.start = QPointF(obj["start.x"].toDouble(), obj["start.y"].toDouble());
  item.end = QPointF(obj["end.x"].toDouble(), obj["end.y"].toDouble());
  item.center = QPointF(obj["center.x"].toDouble(), obj["center.y"].toDouble());
  item.radius = std::max(0.0, obj["radius"].toDouble(item.radius));
  item.text = obj["text"].toString();
  item.enabled = obj["enabled"].toBool(true);
  item.opacity = std::clamp(obj["opacity"].toDouble(1.0), 0.0, 1.0);
  return item;
}

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
    const float spacing = std::max({2.0f, impl_->gridSpacing, std::max(w, h) / 2048.0f});
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

  const auto drawLine = [&](const QPointF &a, const QPointF &b, float alpha) {
    const double dx = b.x() - a.x(), dy = b.y() - a.y();
    const double length = std::hypot(dx, dy);
    if (!std::isfinite(length) || length < 0.001) return;
    QMatrix4x4 lineTransform = transform;
    lineTransform.translate(float(a.x()), float(a.y()));
    lineTransform.rotate(float(std::atan2(dy, dx) * 180.0 / 3.141592653589793), 0, 0, 1);
    renderer->drawSolidRectTransformed(0, -1, float(length), 2, lineTransform, strong, alpha);
  };
  for (const auto &guide : impl_->guideSet.guides) {
    if (!guide.enabled || !std::isfinite(guide.position)) continue;
    if (guide.orientation == GuideOrientation::Vertical)
      drawLine(QPointF(guide.position, 0), QPointF(guide.position, h), opacity);
    else if (guide.orientation == GuideOrientation::Horizontal)
      drawLine(QPointF(0, guide.position), QPointF(w, guide.position), opacity);
  }
  for (const auto &item : impl_->items) {
    if (!item.enabled || !std::isfinite(item.opacity) || item.opacity <= 0.0) continue;
    const float alpha = opacity * float(std::clamp(item.opacity, 0.0, 1.0));
    if (item.type == ConstructionItemType::Line) {
      drawLine(item.start, item.end, alpha);
    } else if (item.type == ConstructionItemType::Circle) {
      if (!std::isfinite(item.radius) || item.radius <= 0.0) continue;
      constexpr int segments = 128;
      QPointF previous = item.center + QPointF(item.radius, 0);
      for (int i = 1; i <= segments; ++i) {
        const double angle = i * (2.0 * 3.141592653589793 / segments);
        const QPointF next = item.center + QPointF(std::cos(angle), std::sin(angle)) * item.radius;
        drawLine(previous, next, alpha);
        previous = next;
      }
    } else if (!item.text.isEmpty() && std::isfinite(item.center.x()) && std::isfinite(item.center.y())) {
      QFont font;
      font.setPixelSize(20);
      renderer->drawTextTransformed(QRectF(item.center, QSizeF(w, h)), item.text,
          font, warm, transform, Qt::AlignLeft | Qt::AlignTop, alpha);
    }
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
  GuideSet guideSet = impl_->guideSet;
  if (guideSet.ownerId.trimmed().isEmpty()) {
    guideSet.ownerId = id().toString();
  }
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
  obj["construction.guideSet"] = guideSet.toJson();
  QJsonArray itemArray;
  for (const auto& item : impl_->items) itemArray.append(item.toJson());
  obj["construction.items"] = itemArray;
  return obj;
}

void ArtifactConstructionLayer::fromJsonProperties(const QJsonObject& obj) {
  ArtifactAbstract2DLayer::fromJsonProperties(obj);
  const auto readFinite = [&obj](const QString& key, float fallback,
                                 float minimum, float maximum) {
    const double raw = obj.value(key).toDouble(fallback);
    return std::isfinite(raw)
        ? std::clamp(static_cast<float>(raw), minimum, maximum)
        : fallback;
  };
  impl_->width = readFinite(QStringLiteral("construction.width"), impl_->width,
                            1.0f, 100000.0f);
  impl_->height = readFinite(QStringLiteral("construction.height"), impl_->height,
                             1.0f, 100000.0f);
  impl_->gridSpacing = readFinite(
      QStringLiteral("construction.gridSpacing"), impl_->gridSpacing,
      2.0f, 100000.0f);
  impl_->majorEvery = readFinite(
      QStringLiteral("construction.majorEvery"), impl_->majorEvery,
      1.0f, 64.0f);
  impl_->safeMargin = readFinite(
      QStringLiteral("construction.safeMargin"), impl_->safeMargin,
      0.0f, 0.45f);
  impl_->baselineY = readFinite(
      QStringLiteral("construction.baselineY"), impl_->baselineY,
      0.0f, 1.0f);
  impl_->opacity = readFinite(
      QStringLiteral("construction.opacity"), impl_->opacity,
      0.0f, 1.0f);
  impl_->showGrid = obj.value(QStringLiteral("construction.showGrid")).toBool(impl_->showGrid);
  impl_->showThirds = obj.value(QStringLiteral("construction.showThirds")).toBool(impl_->showThirds);
  impl_->showCenter = obj.value(QStringLiteral("construction.showCenter")).toBool(impl_->showCenter);
  impl_->showSafeArea = obj.value(QStringLiteral("construction.showSafeArea")).toBool(impl_->showSafeArea);
  impl_->showBaseline = obj.value(QStringLiteral("construction.showBaseline")).toBool(impl_->showBaseline);
  impl_->renderAsDesign = obj.value(QStringLiteral("construction.renderAsDesign")).toBool(impl_->renderAsDesign);
  impl_->includeInFinalRender = obj.value(QStringLiteral("construction.includeInFinalRender")).toBool(impl_->includeInFinalRender);
  setGuide(!(impl_->renderAsDesign || impl_->includeInFinalRender));
  impl_->guideSet = GuideSet::fromJson(obj.value(QStringLiteral("construction.guideSet")).toObject());
  impl_->items.clear();
  for (const auto value : obj.value(QStringLiteral("construction.items")).toArray())
    if (value.isObject()) impl_->items.push_back(ConstructionItem::fromJson(value.toObject()));
  if (impl_->guideSet.ownerId.trimmed().isEmpty()) {
    impl_->guideSet.ownerId = id().toString();
  }
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
  // Stable type/ordinal paths keep value-based Undo valid when an empty
  // creation slot becomes a stored item. Disabled items are never erased here.
  for (int typeIndex = 0; typeIndex < 3; ++typeIndex) {
    const auto type = static_cast<ConstructionItemType>(typeIndex);
    const QString title = type == ConstructionItemType::Line ? QStringLiteral("Line")
        : type == ConstructionItemType::Circle ? QStringLiteral("Circle") : QStringLiteral("Annotation");
    int ordinal = 0;
    const auto addItemGroup = [&](const ConstructionItem &item, int index) {
      PropertyGroup itemGroup(QStringLiteral("Construction · %1 %2").arg(title).arg(index + 1));
      const QString prefix = QStringLiteral("construction.item.%1.%2.").arg(typeIndex).arg(index);
      const auto add = [&](const QString &field, PropertyType propertyType, const QVariant &v) {
        auto property = makeProp(prefix + field, propertyType, v);
        property->setAnimatable(false);
        if (propertyType == PropertyType::Float) {
          if (field == QStringLiteral("opacity")) {
            property->setSoftRange(0.0, 1.0);
            property->setStep(0.01);
          } else {
            property->setSoftRange(field == QStringLiteral("radius") ? 0.0 : -100000.0, 100000.0);
            property->setUnit(QStringLiteral("px"));
          }
        }
        itemGroup.addProperty(property);
      };
      add(QStringLiteral("enabled"), PropertyType::Boolean, item.enabled);
      add(QStringLiteral("opacity"), PropertyType::Float, item.opacity);
      if (type == ConstructionItemType::Line) {
        add(QStringLiteral("startX"), PropertyType::Float, item.start.x());
        add(QStringLiteral("startY"), PropertyType::Float, item.start.y());
        add(QStringLiteral("endX"), PropertyType::Float, item.end.x());
        add(QStringLiteral("endY"), PropertyType::Float, item.end.y());
      } else {
        add(QStringLiteral("centerX"), PropertyType::Float, item.center.x());
        add(QStringLiteral("centerY"), PropertyType::Float, item.center.y());
        if (type == ConstructionItemType::Circle)
          add(QStringLiteral("radius"), PropertyType::Float, item.radius);
        else
          add(QStringLiteral("text"), PropertyType::String, item.text);
      }
      groups.push_back(itemGroup);
    };
    for (const auto &item : impl_->items) {
      if (item.type == type) addItemGroup(item, ordinal++);
    }
    if (ordinal < 128) addItemGroup(defaultConstructionItem(type), ordinal);
  }
  return groups;
}

bool ArtifactConstructionLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value) {
  if (propertyPath.startsWith(QStringLiteral("construction.item."))) {
    const auto parts = propertyPath.split(QLatin1Char('.'));
    if (parts.size() != 5) return false;
    bool typeOk = false, indexOk = false;
    const int typeIndex = parts[2].toInt(&typeOk), index = parts[3].toInt(&indexOk);
    if (!typeOk || !indexOk || typeIndex < 0 || typeIndex > 2 || index < 0 || index >= 128) return false;
    const auto type = static_cast<ConstructionItemType>(typeIndex);
    const QString field = parts[4];
    const bool common = field == QStringLiteral("enabled") || field == QStringLiteral("opacity");
    const bool lineField = field == QStringLiteral("startX") || field == QStringLiteral("startY") ||
        field == QStringLiteral("endX") || field == QStringLiteral("endY");
    const bool centerField = field == QStringLiteral("centerX") || field == QStringLiteral("centerY");
    if (!(common || (type == ConstructionItemType::Line && lineField) ||
          (type != ConstructionItemType::Line && centerField) ||
          (type == ConstructionItemType::Circle && field == QStringLiteral("radius")) ||
          (type == ConstructionItemType::Annotation && field == QStringLiteral("text")))) return false;
    bool numericOk = false;
    double number = value.toDouble(&numericOk);
    if (field != QStringLiteral("enabled") && field != QStringLiteral("text") &&
        (!numericOk || !std::isfinite(number))) return false;
    ConstructionItem *target = nullptr;
    int ordinal = 0;
    for (auto &item : impl_->items) {
      if (item.type == type && ordinal++ == index) { target = &item; break; }
    }
    if (!target) {
      if (ordinal != index) return false;
      impl_->items.push_back(defaultConstructionItem(type));
      target = &impl_->items.back();
      target->id = QStringLiteral("inspector-%1-%2").arg(typeIndex).arg(index);
    }
    number = std::clamp(number, -100000.0, 100000.0);
    if (field == QStringLiteral("enabled")) target->enabled = value.toBool();
    else if (field == QStringLiteral("opacity")) target->opacity = std::clamp(number, 0.0, 1.0);
    else if (field == QStringLiteral("startX")) target->start.setX(number);
    else if (field == QStringLiteral("startY")) target->start.setY(number);
    else if (field == QStringLiteral("endX")) target->end.setX(number);
    else if (field == QStringLiteral("endY")) target->end.setY(number);
    else if (field == QStringLiteral("centerX")) target->center.setX(number);
    else if (field == QStringLiteral("centerY")) target->center.setY(number);
    else if (field == QStringLiteral("radius")) target->radius = std::max(0.0, number);
    else if (field == QStringLiteral("text")) target->text = value.toString();
    return true;
  }
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
    setGuide(!(impl_->renderAsDesign || impl_->includeInFinalRender));
  } else if (propertyPath == QStringLiteral("construction.includeInFinalRender")) {
    impl_->includeInFinalRender = value.toBool();
    setGuide(!(impl_->renderAsDesign || impl_->includeInFinalRender));
  } else {
    return ArtifactAbstract2DLayer::setLayerPropertyValue(propertyPath, value);
  }

  if (sizeChanged) {
    setSourceSize(Size_2D(static_cast<int>(std::round(impl_->width)),
                          static_cast<int>(std::round(impl_->height))));
  }
  return true;
}

GuideSet ArtifactConstructionLayer::constructionGuideSet() const {
  return impl_->guideSet;
}

ArtifactCore::Array<QPointF> ArtifactConstructionLayer::constructionSnapPoints() const {
  ArtifactCore::Array<QPointF> points;
  if (impl_->opacity <= 0.001f || opacity() <= 0.001f) return points;
  const double w = impl_->width, h = impl_->height;
  const auto add = [&](QPointF p) {
    if (std::isfinite(p.x()) && std::isfinite(p.y())) points.append(p);
  };
  const auto vertical = [&](double x) { add(QPointF(x, 0)); add(QPointF(x, h)); };
  const auto horizontal = [&](double y) { add(QPointF(0, y)); add(QPointF(w, y)); };
  if (impl_->showGrid && impl_->gridSpacing > 2.0f) {
    const double spacing = std::max({2.0, double(impl_->gridSpacing), std::max(w, h) / 2048.0});
    for (double x = 0; x <= w; x += spacing) vertical(x);
    for (double y = 0; y <= h; y += spacing) horizontal(y);
  }
  if (impl_->showThirds) { vertical(w / 3); vertical(w * 2 / 3); horizontal(h / 3); horizontal(h * 2 / 3); }
  if (impl_->showCenter) { vertical(w / 2); horizontal(h / 2); }
  if (impl_->showSafeArea) {
    const double margin = std::clamp(double(impl_->safeMargin), 0.0, 0.45);
    for (double x : {w * margin, w * (1 - margin)})
      for (double y : {h * margin, h * (1 - margin)}) add(QPointF(x, y));
  }
  if (impl_->showBaseline) horizontal(h * std::clamp(double(impl_->baselineY), 0.0, 1.0));
  for (const auto &guide : impl_->guideSet.guides) {
    if (!guide.enabled) continue;
    if (guide.orientation == GuideOrientation::Vertical) vertical(guide.position);
    else if (guide.orientation == GuideOrientation::Horizontal) horizontal(guide.position);
  }
  for (const auto &item : impl_->items) {
    if (!item.enabled || !std::isfinite(item.opacity) || item.opacity <= 0.001) continue;
    if (item.type == ConstructionItemType::Line) {
      add(item.start); add(item.end); add((item.start + item.end) * 0.5);
    } else {
      add(item.center);
      if (item.type == ConstructionItemType::Circle && std::isfinite(item.radius) && item.radius > 0) {
        add(item.center + QPointF(item.radius, 0)); add(item.center - QPointF(item.radius, 0));
        add(item.center + QPointF(0, item.radius)); add(item.center - QPointF(0, item.radius));
      }
    }
  }
  return points;
}

void ArtifactConstructionLayer::setConstructionGuideSet(const GuideSet& guideSet) {
  impl_->guideSet = guideSet;
  if (impl_->guideSet.ownerId.trimmed().isEmpty()) {
    impl_->guideSet.ownerId = id().toString();
  }
}

void ArtifactConstructionLayer::addConstructionGuide(const GuideDefinition& guide) {
  impl_->guideSet.guides.push_back(guide);
  if (impl_->guideSet.ownerId.trimmed().isEmpty()) {
    impl_->guideSet.ownerId = id().toString();
  }
}

void ArtifactConstructionLayer::clearConstructionGuides() {
  impl_->guideSet.guides.clear();
  impl_->guideSet.bindings.clear();
  if (impl_->guideSet.ownerId.trimmed().isEmpty()) {
    impl_->guideSet.ownerId = id().toString();
  }
}

const std::vector<ConstructionItem>& ArtifactConstructionLayer::constructionItems() const { return impl_->items; }
bool ArtifactConstructionLayer::setConstructionItem(size_t index, const ConstructionItem& item) {
  if (index >= impl_->items.size() || impl_->items[index].id != item.id ||
      impl_->items[index].type != item.type) return false;
  impl_->items[index] = item;
  setDirty(LayerDirtyFlag::Source);
  changed();
  return true;
}
void ArtifactConstructionLayer::setConstructionItems(const std::vector<ConstructionItem>& items) { impl_->items = items; }
void ArtifactConstructionLayer::addConstructionItem(const ConstructionItem& item) { impl_->items.push_back(item); }
void ArtifactConstructionLayer::clearConstructionItems() { impl_->items.clear(); }

} // namespace Artifact
