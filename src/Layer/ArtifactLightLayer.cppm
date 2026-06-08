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
      lightColor.r() * std::min(1.0f, intensityScale),
      lightColor.g() * std::min(1.0f, intensityScale),
      lightColor.b() * std::min(1.0f, intensityScale),
      std::min(1.0f, lightColor.a() * (0.35f + 0.65f * std::min(1.0f, intensityScale)))};
  
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

  // Direction indicators for oriented lights
  if (type == LightType::Spot || type == LightType::Parallel) {
    // Draw a small forward axis so the gizmo matches the scene-light direction.
    QMatrix4x4 m = getGlobalTransform4x4();
    QVector3D forward = m.mapVector(QVector3D(0, 0, 100.0f / (zoom > 0.001f ? zoom : 1.0f)));
    QVector3D tip = pos + forward;
    
    renderer->drawGizmoArrow(p, float3{tip.x(), tip.y(), tip.z()}, tintColor, baseSize);
  }
}

LightType ArtifactLightLayer::lightType() const { return lightImpl_->type_; }
void ArtifactLightLayer::setLightType(LightType t) { lightImpl_->type_ = t; changed(); }

ArtifactCore::FloatColor ArtifactLightLayer::color() const { return lightImpl_->color_; }
void ArtifactLightLayer::setColor(const ArtifactCore::FloatColor& c) { lightImpl_->color_ = c; changed(); }

float ArtifactLightLayer::intensity() const { return lightImpl_->intensity_; }
void ArtifactLightLayer::setIntensity(float i) { lightImpl_->intensity_ = i; changed(); }

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
    
    ArtifactCore::PropertyGroup lightOptions("Light Options");
    
    auto typeProp = persistentLayerProperty(QStringLiteral("Light Options/Light Type"),
                                            ArtifactCore::PropertyType::Integer,
                                            static_cast<int>(lightImpl_->type_), -150);
    typeProp->setTooltip(QStringLiteral("0: Point, 1: Spot, 2: Parallel, 3: Ambient"));
    lightOptions.addProperty(typeProp);

    auto colorProp = persistentLayerProperty(QStringLiteral("Light Options/Color"),
                                             ArtifactCore::PropertyType::Color,
                                             toQColor(lightImpl_->color_), -145);
    lightOptions.addProperty(colorProp);

    auto intensityProp = persistentLayerProperty(QStringLiteral("Light Options/Intensity"),
                                                 ArtifactCore::PropertyType::Float,
                                                 static_cast<double>(lightImpl_->intensity_), -140);
    intensityProp->setHardRange(0.0, 1000.0);
    intensityProp->setSoftRange(0.0, 250.0);
    intensityProp->setUnit(QStringLiteral("%"));
    lightOptions.addProperty(intensityProp);

    auto shadowProp = persistentLayerProperty(
        QStringLiteral("Light Options/Shadows"),
        ArtifactCore::PropertyType::Boolean,
        lightImpl_->castsShadows_, -130);
    shadowProp->setTooltip(QStringLiteral("Show a softer outer ring to indicate shadow softness"));
    lightOptions.addProperty(shadowProp);

    auto radiusProp = persistentLayerProperty(
        QStringLiteral("Light Options/Shadow Radius"),
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
    if (propertyPath == "Light Options/Light Type") {
        setLightType(static_cast<LightType>(value.toInt()));
        return true;
    } else if (propertyPath == "Light Options/Color") {
        const QColor qc = value.value<QColor>();
        setColor(ArtifactCore::FloatColor(
            static_cast<float>(qc.redF()),
            static_cast<float>(qc.greenF()),
            static_cast<float>(qc.blueF()),
            static_cast<float>(qc.alphaF())
        ));
        return true;
    } else if (propertyPath == "Light Options/Intensity") {
        setIntensity(value.toFloat());
        return true;
    } else if (propertyPath == "Light Options/Shadows") {
        setCastsShadows(value.toBool());
        return true;
    } else if (propertyPath == "Light Options/Shadow Radius") {
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
