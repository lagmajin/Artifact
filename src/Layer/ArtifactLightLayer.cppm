module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QVariant>
#include <QStringList>
#include <wobjectimpl.h>

module Artifact.Layer.Light;

import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Animation.Transform3D;
import Time.Rational;
import Property.Group;
import Property;
import Color.Float;

namespace Artifact {

namespace {
QColor toQColor(const ArtifactCore::FloatColor& color) {
    return QColor::fromRgbF(
        static_cast<qreal>(color.r()),
        static_cast<qreal>(color.g()),
        static_cast<qreal>(color.b()),
        static_cast<qreal>(color.a())
    );
}

int64_t lightTimelineFps(const ArtifactLightLayer* layer)
{
    if (!layer) {
        return 30;
    }
    if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
        const double fps = comp->frameRate().framerate();
        if (fps > 0.0) {
            return std::max<int64_t>(1, static_cast<int64_t>(std::llround(fps)));
        }
    }
    return 30;
}
}

W_OBJECT_IMPL(ArtifactLightLayer)

struct ArtifactLightLayer::Impl {
    LightType type_ = LightType::Point;
    ArtifactCore::FloatColor color_{1.0f, 1.0f, 1.0f, 1.0f};
    float intensity_ = 100.0f;
    float range_ = 500.0f;
    float areaWidth_ = 100.0f;
    float areaHeight_ = 100.0f;
    AreaLightShape areaShape_ = AreaLightShape::Rectangle;
    float coneAngle_ = 45.0f;
    float coneFeather_ = 10.0f;
    float coneLength_ = 300.0f;
    float shadowRadius_ = 10.0f;
    bool castsShadows_ = true;
    LightLinkMode linkMode_ = LightLinkMode::All;
    QString linkedLayerIdsText_;
    QString excludedLayerIdsText_;
};

ArtifactLightLayer::ArtifactLightLayer()
    : lightImpl_(new Impl())
{
    setLayerName("Light 1");
    setIs3D(true);
}

ArtifactLightLayer::~ArtifactLightLayer()
{
    delete lightImpl_;
}

void ArtifactLightLayer::draw(ArtifactIRenderer* renderer) {
  if (!renderer || !isVisible()) {
    return;
  }

  // Get position from 3D transform at current frame
  const RationalTime frameTime(currentFrame(), lightTimelineFps(this));
  const auto &t3 = transform3D();
  const QVector3D pos(
      static_cast<float>(t3.positionXAt(frameTime)),
      static_cast<float>(t3.positionYAt(frameTime)),
      static_cast<float>(t3.positionZAt(frameTime))
  );

  const auto type = lightType();
  const auto lightColor = color();
  const float intensityScale = std::clamp(lightImpl_->intensity_ / 100.0f, 0.2f, 4.0f);
  const ArtifactCore::FloatColor tintColor{
      std::min(1.0f, lightColor.r() * 0.85f + 0.10f * intensityScale),
      std::min(1.0f, lightColor.g() * 0.95f + 0.12f * intensityScale),
      std::min(1.0f, lightColor.b() * 1.05f + 0.25f * intensityScale),
      std::min(1.0f, lightColor.a() * (0.38f + 0.62f * std::min(1.0f, intensityScale)))};
  
  // Calculate gizmo size (scale inversely with zoom to keep constant screen size if desired, 
  // or just use a fixed 3D size). Here we use a fixed size that's easy to see.
  const float zoom = renderer->getZoom();
  const float baseSize = 15.0f * intensityScale / (zoom > 0.001f ? zoom : 1.0f);

  // Use renderer's gizmo APIs
  using namespace Artifact::Detail;
  float3 p{pos.x(), pos.y(), pos.z()};

  // Main "bulb" representation: 3 orthogonal rings
  renderer->drawGizmoRing(p, float3{1, 0, 0}, baseSize, tintColor, 1.0f);
  renderer->drawGizmoRing(p, float3{0, 1, 0}, baseSize, tintColor, 1.0f);
  renderer->drawGizmoRing(p, float3{0, 0, 1}, baseSize, tintColor, 1.0f);

  if (lightImpl_->castsShadows_) {
    const float shadowRing = baseSize + std::max(2.0f, lightImpl_->shadowRadius_ * 0.05f);
    renderer->drawGizmoRing(p, float3{0, 1, 0}, shadowRing,
                            ArtifactCore::FloatColor{lightColor.r(), lightColor.g(),
                                                     lightColor.b(), 0.18f},
                            1.0f);
  }

  QMatrix4x4 m = getGlobalTransform4x4();
  QVector3D forward = m.mapVector(QVector3D(0, 0, 100.0f / (zoom > 0.001f ? zoom : 1.0f)));
  if (forward.lengthSquared() <= 0.000001f) {
    forward = QVector3D(0, 0, 1);
  } else {
    forward.normalize();
  }
  const QVector3D tip = pos + forward * (baseSize * 2.4f);
  const QVector3D side = m.mapVector(QVector3D(1, 0, 0)).normalized() * (baseSize * 0.7f);
  const QVector3D up = m.mapVector(QVector3D(0, 1, 0)).normalized() * (baseSize * 0.7f);

  // Direction indicators for oriented lights.
  if (type == LightType::Spot || type == LightType::Parallel) {
    renderer->drawGizmoArrow(p, float3{tip.x(), tip.y(), tip.z()}, tintColor, baseSize);
  }

  if (type == LightType::Point) {
    const float rangeVisual = std::max(baseSize * 1.5f, lightImpl_->range_);
    const ArtifactCore::FloatColor rangeColor{lightColor.r(), lightColor.g(),
                                               lightColor.b(), 0.16f};
    renderer->drawGizmoRing(p, float3{1, 0, 0}, rangeVisual, rangeColor, 0.8f);
    renderer->drawGizmoRing(p, float3{0, 1, 0}, rangeVisual, rangeColor, 0.8f);
    renderer->drawGizmoRing(p, float3{0, 0, 1}, rangeVisual, rangeColor, 0.8f);
    renderer->drawGizmoLine(float3{pos.x() - side.x(), pos.y() - side.y(), pos.z() - side.z()},
                            float3{pos.x() + side.x(), pos.y() + side.y(), pos.z() + side.z()},
                            tintColor, 1.0f);
    renderer->drawGizmoLine(float3{pos.x() - up.x(), pos.y() - up.y(), pos.z() - up.z()},
                            float3{pos.x() + up.x(), pos.y() + up.y(), pos.z() + up.z()},
                            tintColor, 1.0f);
  } else if (type == LightType::Ambient) {
    renderer->drawGizmoRing(p, float3{0, 1, 0}, baseSize * 1.35f, tintColor, 1.0f);
  } else if (type == LightType::Area) {
    if (lightImpl_->areaShape_ == AreaLightShape::Disk) {
      const float radius = std::max(1.0f, std::min(lightImpl_->areaWidth_,
                                                    lightImpl_->areaHeight_) * 0.5f);
      const QVector3D normal = m.mapVector(QVector3D(0, 0, 1)).normalized();
      renderer->drawGizmoRing(float3{pos.x(), pos.y(), pos.z()},
                              float3{normal.x(), normal.y(), normal.z()},
                              radius, tintColor, 1.2f);
    } else {
      const QVector3D areaSide = m.mapVector(QVector3D(lightImpl_->areaWidth_ * 0.5f, 0, 0));
      const QVector3D areaUp = m.mapVector(QVector3D(0, lightImpl_->areaHeight_ * 0.5f, 0));
      const QVector3D a = pos - areaSide - areaUp;
      const QVector3D b = pos + areaSide - areaUp;
      const QVector3D c = pos + areaSide + areaUp;
      const QVector3D d = pos - areaSide + areaUp;
      renderer->drawGizmoLine({a.x(), a.y(), a.z()}, {b.x(), b.y(), b.z()}, tintColor, 1.2f);
      renderer->drawGizmoLine({b.x(), b.y(), b.z()}, {c.x(), c.y(), c.z()}, tintColor, 1.2f);
      renderer->drawGizmoLine({c.x(), c.y(), c.z()}, {d.x(), d.y(), d.z()}, tintColor, 1.2f);
      renderer->drawGizmoLine({d.x(), d.y(), d.z()}, {a.x(), a.y(), a.z()}, tintColor, 1.2f);
    }
    const QVector3D directionTip = pos + forward * std::max(baseSize * 2.0f, 24.0f);
    renderer->drawGizmoArrow(float3{pos.x(), pos.y(), pos.z()},
                             float3{directionTip.x(), directionTip.y(), directionTip.z()},
                             tintColor, baseSize * 0.8f);
  } else if (type == LightType::Spot) {
    renderer->drawGizmoLine(float3{pos.x(), pos.y(), pos.z()},
                            float3{tip.x(), tip.y(), tip.z()}, tintColor, 1.0f);
    renderer->drawGizmoRing(float3{tip.x(), tip.y(), tip.z()}, float3{0, 1, 0},
                            baseSize * 0.75f, tintColor, 1.0f);

    const float coneLength = std::max(1.0f, std::min(lightImpl_->coneLength_,
                                                      lightImpl_->range_));
    const float coneRadius = std::tan(std::clamp(lightImpl_->coneAngle_, 0.1f, 179.0f)
                                      * 3.14159265f / 360.0f) * coneLength;
    QVector3D coneSide = m.mapVector(QVector3D(1, 0, 0));
    QVector3D coneUp = m.mapVector(QVector3D(0, 1, 0));
    if (coneSide.lengthSquared() <= 0.000001f) coneSide = QVector3D(1, 0, 0);
    if (coneUp.lengthSquared() <= 0.000001f) coneUp = QVector3D(0, 1, 0);
    coneSide.normalize();
    coneUp.normalize();
    const QVector3D coneCenter = pos + forward * coneLength;
    const ArtifactCore::FloatColor coneColor{lightColor.r(), lightColor.g(), lightColor.b(), 0.72f};
    const ArtifactCore::FloatColor featherColor{lightColor.r(), lightColor.g(), lightColor.b(), 0.30f};
    renderer->drawGizmoRing(float3{coneCenter.x(), coneCenter.y(), coneCenter.z()},
                            float3{forward.x(), forward.y(), forward.z()}, coneRadius, coneColor, 1.2f);
    for (const float signX : {-1.0f, 1.0f}) {
      for (const float signY : {-1.0f, 1.0f}) {
        const QVector3D edge = coneCenter + coneSide * (coneRadius * signX)
                               + coneUp * (coneRadius * signY);
        renderer->drawGizmoLine(float3{pos.x(), pos.y(), pos.z()},
                                float3{edge.x(), edge.y(), edge.z()}, coneColor, 1.0f);
      }
    }
    const float featherAngle = std::max(0.0f, lightImpl_->coneAngle_ - lightImpl_->coneFeather_);
    if (featherAngle > 0.1f && featherAngle < lightImpl_->coneAngle_) {
      const float innerRadius = std::tan(featherAngle * 3.14159265f / 360.0f) * coneLength;
      renderer->drawGizmoRing(float3{coneCenter.x(), coneCenter.y(), coneCenter.z()},
                              float3{forward.x(), forward.y(), forward.z()}, innerRadius,
                              featherColor, 1.0f);
    }
  } else if (type == LightType::Parallel) {
    renderer->drawGizmoLine(float3{pos.x(), pos.y(), pos.z()},
                            float3{tip.x(), tip.y(), tip.z()}, tintColor, 1.0f);
    renderer->drawGizmoLine(float3{tip.x() - side.x(), tip.y() - side.y(), tip.z() - side.z()},
                            float3{tip.x() + side.x(), tip.y() + side.y(), tip.z() + side.z()},
                            tintColor, 0.9f);
  }
}

LightType ArtifactLightLayer::lightType() const { return lightImpl_->type_; }
void ArtifactLightLayer::setLightType(LightType t) { lightImpl_->type_ = t; changed(); }

ArtifactCore::FloatColor ArtifactLightLayer::color() const { return lightImpl_->color_; }
void ArtifactLightLayer::setColor(const ArtifactCore::FloatColor& c) { lightImpl_->color_ = c; changed(); }

float ArtifactLightLayer::intensity() const { return lightImpl_->intensity_; }
void ArtifactLightLayer::setIntensity(float i) { lightImpl_->intensity_ = i; changed(); }

float ArtifactLightLayer::range() const { return lightImpl_->range_; }
void ArtifactLightLayer::setRange(float range)
{
  lightImpl_->range_ = std::max(1.0f, range);
  changed();
}

float ArtifactLightLayer::areaWidth() const { return lightImpl_->areaWidth_; }
float ArtifactLightLayer::areaHeight() const { return lightImpl_->areaHeight_; }
void ArtifactLightLayer::setAreaSize(float width, float height)
{
  lightImpl_->areaWidth_ = std::max(1.0f, width);
  lightImpl_->areaHeight_ = std::max(1.0f, height);
  changed();
}

AreaLightShape ArtifactLightLayer::areaShape() const { return lightImpl_->areaShape_; }
void ArtifactLightLayer::setAreaShape(AreaLightShape shape)
{
  lightImpl_->areaShape_ = shape;
  changed();
}

float ArtifactLightLayer::coneAngle() const { return lightImpl_->coneAngle_; }
void ArtifactLightLayer::setConeAngle(float degrees)
{
  lightImpl_->coneAngle_ = std::clamp(degrees, 0.1f, 179.0f);
  lightImpl_->coneFeather_ = std::clamp(lightImpl_->coneFeather_, 0.0f, lightImpl_->coneAngle_);
  changed();
}

float ArtifactLightLayer::coneFeather() const { return lightImpl_->coneFeather_; }
void ArtifactLightLayer::setConeFeather(float degrees)
{
  lightImpl_->coneFeather_ = std::clamp(degrees, 0.0f, lightImpl_->coneAngle_);
  changed();
}

float ArtifactLightLayer::coneLength() const { return lightImpl_->coneLength_; }
void ArtifactLightLayer::setConeLength(float length)
{
  lightImpl_->coneLength_ = std::max(1.0f, length);
  changed();
}

float ArtifactLightLayer::shadowRadius() const { return lightImpl_->shadowRadius_; }
void ArtifactLightLayer::setShadowRadius(float r) { lightImpl_->shadowRadius_ = r; changed(); }

bool ArtifactLightLayer::castsShadows() const { return lightImpl_->castsShadows_; }
void ArtifactLightLayer::setCastsShadows(bool e) { lightImpl_->castsShadows_ = e; changed(); }

LightLinkMode ArtifactLightLayer::lightLinkMode() const { return lightImpl_->linkMode_; }
void ArtifactLightLayer::setLightLinkMode(LightLinkMode mode)
{
  lightImpl_->linkMode_ = mode;
  changed();
}

QString ArtifactLightLayer::linkedLayerIdsText() const { return lightImpl_->linkedLayerIdsText_; }
void ArtifactLightLayer::setLinkedLayerIdsText(const QString& ids)
{
  lightImpl_->linkedLayerIdsText_ = ids.trimmed();
  changed();
}

QString ArtifactLightLayer::excludedLayerIdsText() const { return lightImpl_->excludedLayerIdsText_; }
void ArtifactLightLayer::setExcludedLayerIdsText(const QString& ids)
{
  lightImpl_->excludedLayerIdsText_ = ids.trimmed();
  changed();
}

std::vector<ArtifactCore::PropertyGroup> ArtifactLightLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    
    ArtifactCore::PropertyGroup lightOptions("Light");
    
    auto typeProp = persistentLayerProperty(QStringLiteral("Light/Type"),
                                            ArtifactCore::PropertyType::Integer,
                                            static_cast<int>(lightImpl_->type_), -150);
    typeProp->setTooltip(QStringLiteral("0: Point, 1: Spot, 2: Parallel, 3: Ambient, 4: Area"));
    lightOptions.addProperty(typeProp);

    auto colorProp = persistentLayerProperty(QStringLiteral("Light/Color"),
                                             ArtifactCore::PropertyType::Color,
                                             toQColor(lightImpl_->color_), -145);
    lightOptions.addProperty(colorProp);

    auto intensityProp = persistentLayerProperty(QStringLiteral("Light/Intensity"),
                                                 ArtifactCore::PropertyType::Float,
                                                 static_cast<double>(lightImpl_->intensity_), -140);
    intensityProp->setHardRange(0.0, 1000.0);
    intensityProp->setSoftRange(0.0, 250.0);
    intensityProp->setUnit(QStringLiteral("%"));
    lightOptions.addProperty(intensityProp);

    if (lightImpl_->type_ == LightType::Point || lightImpl_->type_ == LightType::Spot ||
        lightImpl_->type_ == LightType::Area) {
    auto rangeProp = persistentLayerProperty(
        QStringLiteral("Light/Range"), ArtifactCore::PropertyType::Float,
        static_cast<double>(lightImpl_->range_), -139);
    rangeProp->setHardRange(1.0, 100000.0);
    rangeProp->setSoftRange(25.0, 5000.0);
    rangeProp->setUnit(QStringLiteral("px"));
    rangeProp->setTooltip(QStringLiteral("Effective point/spot light range"));
    lightOptions.addProperty(rangeProp);
    }

    if (lightImpl_->type_ == LightType::Area) {
    auto widthProp = persistentLayerProperty(QStringLiteral("Light/Area Width"),
                                             ArtifactCore::PropertyType::Float,
                                             static_cast<double>(lightImpl_->areaWidth_), -138);
    widthProp->setHardRange(1.0, 100000.0);
    widthProp->setSoftRange(10.0, 2000.0);
    widthProp->setUnit(QStringLiteral("px"));
    lightOptions.addProperty(widthProp);
    auto heightProp = persistentLayerProperty(QStringLiteral("Light/Area Height"),
                                              ArtifactCore::PropertyType::Float,
                                              static_cast<double>(lightImpl_->areaHeight_), -137);
    heightProp->setHardRange(1.0, 100000.0);
    heightProp->setSoftRange(10.0, 2000.0);
    heightProp->setUnit(QStringLiteral("px"));
    lightOptions.addProperty(heightProp);
    auto shapeProp = persistentLayerProperty(QStringLiteral("Light/Area Shape"),
                                             ArtifactCore::PropertyType::Integer,
                                             static_cast<int>(lightImpl_->areaShape_), -136);
    shapeProp->setTooltip(QStringLiteral("0: Rectangle, 1: Disk"));
    lightOptions.addProperty(shapeProp);
    }

    if (lightImpl_->type_ == LightType::Spot) {
    auto coneAngleProp = persistentLayerProperty(QStringLiteral("Light/Cone Angle"),
                                                  ArtifactCore::PropertyType::Float,
                                                  static_cast<double>(lightImpl_->coneAngle_), -138);
    coneAngleProp->setHardRange(0.1, 179.0);
    coneAngleProp->setSoftRange(1.0, 120.0);
    coneAngleProp->setUnit(QStringLiteral("deg"));
    coneAngleProp->setTooltip(QStringLiteral("Spot-light outer cone angle"));
    lightOptions.addProperty(coneAngleProp);

    auto coneFeatherProp = persistentLayerProperty(QStringLiteral("Light/Cone Feather"),
                                                    ArtifactCore::PropertyType::Float,
                                                    static_cast<double>(lightImpl_->coneFeather_), -137);
    coneFeatherProp->setHardRange(0.0, 179.0);
    coneFeatherProp->setSoftRange(0.0, 60.0);
    coneFeatherProp->setUnit(QStringLiteral("deg"));
    coneFeatherProp->setTooltip(QStringLiteral("Soft edge width inside the spot cone"));
    lightOptions.addProperty(coneFeatherProp);

    auto coneLengthProp = persistentLayerProperty(QStringLiteral("Light/Cone Length"),
                                                   ArtifactCore::PropertyType::Float,
                                                   static_cast<double>(lightImpl_->coneLength_), -136);
    coneLengthProp->setHardRange(1.0, 10000.0);
    coneLengthProp->setSoftRange(25.0, 1000.0);
    coneLengthProp->setUnit(QStringLiteral("px"));
    coneLengthProp->setTooltip(QStringLiteral("Spot-light cone range in composition space"));
    lightOptions.addProperty(coneLengthProp);
    }

    auto shadowProp = persistentLayerProperty(
        QStringLiteral("Light/Shadows"),
        ArtifactCore::PropertyType::Boolean,
        lightImpl_->castsShadows_, -130);
    shadowProp->setTooltip(QStringLiteral("Enable the light's shadow cue for 3D preview"));
    lightOptions.addProperty(shadowProp);

    auto radiusProp = persistentLayerProperty(
        QStringLiteral("Light/Shadow Radius"),
        ArtifactCore::PropertyType::Float,
        static_cast<double>(lightImpl_->shadowRadius_), -120);
    radiusProp->setHardRange(0.0, 500.0);
    radiusProp->setSoftRange(0.0, 200.0);
    radiusProp->setUnit(QStringLiteral("px"));
    lightOptions.addProperty(radiusProp);

    ArtifactCore::PropertyGroup linkingOptions("Light Linking");

    auto linkModeProp = persistentLayerProperty(
        QStringLiteral("Light Linking/Link Mode"),
        ArtifactCore::PropertyType::Integer,
        static_cast<int>(lightImpl_->linkMode_), -110);
    linkModeProp->setTooltip(QStringLiteral("0: All, 1: Include Only, 2: Exclude List"));
    linkingOptions.addProperty(linkModeProp);

    auto includeProp = persistentLayerProperty(
        QStringLiteral("Light Linking/Include Layer IDs"),
        ArtifactCore::PropertyType::String,
        lightImpl_->linkedLayerIdsText_, -105);
    includeProp->setTooltip(QStringLiteral("Comma-separated layer IDs that this light affects when Link Mode is Include Only"));
    linkingOptions.addProperty(includeProp);

    auto excludeProp = persistentLayerProperty(
        QStringLiteral("Light Linking/Exclude Layer IDs"),
        ArtifactCore::PropertyType::String,
        lightImpl_->excludedLayerIdsText_, -100);
    excludeProp->setTooltip(QStringLiteral("Comma-separated layer IDs that this light ignores when Link Mode is Exclude List"));
    linkingOptions.addProperty(excludeProp);
    
    groups.push_back(lightOptions);
    groups.push_back(linkingOptions);
    return groups;
}

bool ArtifactLightLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == "Light/Type") {
        setLightType(static_cast<LightType>(value.toInt()));
        return true;
    } else if (propertyPath == "Light/Color") {
        const QColor qc = value.value<QColor>();
        setColor(ArtifactCore::FloatColor(
            static_cast<float>(qc.redF()),
            static_cast<float>(qc.greenF()),
            static_cast<float>(qc.blueF()),
            static_cast<float>(qc.alphaF())
        ));
        return true;
    } else if (propertyPath == "Light/Intensity") {
        setIntensity(value.toFloat());
        return true;
    } else if (propertyPath == "Light/Range") {
        setRange(value.toFloat());
        return true;
    } else if (propertyPath == "Light/Area Width") {
        setAreaSize(value.toFloat(), areaHeight());
        return true;
    } else if (propertyPath == "Light/Area Height") {
        setAreaSize(areaWidth(), value.toFloat());
        return true;
    } else if (propertyPath == "Light/Area Shape") {
        setAreaShape(static_cast<AreaLightShape>(value.toInt()));
        return true;
    } else if (propertyPath == "Light/Cone Angle") {
        setConeAngle(value.toFloat());
        return true;
    } else if (propertyPath == "Light/Cone Feather") {
        setConeFeather(value.toFloat());
        return true;
    } else if (propertyPath == "Light/Cone Length") {
        setConeLength(value.toFloat());
        return true;
    } else if (propertyPath == "Light/Shadows") {
        setCastsShadows(value.toBool());
        return true;
    } else if (propertyPath == "Light/Shadow Radius") {
        setShadowRadius(value.toFloat());
        return true;
    } else if (propertyPath == "Light Linking/Link Mode") {
        setLightLinkMode(static_cast<LightLinkMode>(value.toInt()));
        return true;
    } else if (propertyPath == "Light Linking/Include Layer IDs") {
        setLinkedLayerIdsText(value.toString());
        return true;
    } else if (propertyPath == "Light Linking/Exclude Layer IDs") {
        setExcludedLayerIdsText(value.toString());
        return true;
    }
    
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

} // namespace Artifact
