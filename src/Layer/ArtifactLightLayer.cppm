module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QJsonObject>
#include <QDir>
#include <QVariant>
#include <QStringList>
#include <wobjectimpl.h>

module Artifact.Layer.Light;

import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Animation.Transform3D;
import Time.Rational;
import Graphics.ParticleData;
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

double lightTimelineFps(const ArtifactLightLayer* layer)
{
    if (!layer) {
        return 30;
    }
    if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
        const double fps = comp->frameRate().framerate();
        if (fps > 0.0) {
            return fps;
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
    QString goboTexturePath_;
    float goboIntensity_ = 1.0f;
    float goboRotation_ = 0.0f;
    bool goboInvert_ = false;
    float shadowRadius_ = 10.0f;
    bool castsShadows_ = true;
    bool glowEnabled_ = true;
    float glowSize_ = 1.0f;
    float glowIntensity_ = 1.0f;
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

QJsonObject ArtifactLightLayer::toJson() const
{
  QJsonObject obj = ArtifactAbstractLayer::toJson();
  obj[QStringLiteral("type")] = static_cast<int>(LayerType::Light);
  obj[QStringLiteral("light.type")] = static_cast<int>(lightImpl_->type_);
  obj[QStringLiteral("light.color.r")] = lightImpl_->color_.r();
  obj[QStringLiteral("light.color.g")] = lightImpl_->color_.g();
  obj[QStringLiteral("light.color.b")] = lightImpl_->color_.b();
  obj[QStringLiteral("light.color.a")] = lightImpl_->color_.a();
  obj[QStringLiteral("light.intensity")] = lightImpl_->intensity_;
  obj[QStringLiteral("light.range")] = lightImpl_->range_;
  obj[QStringLiteral("light.areaWidth")] = lightImpl_->areaWidth_;
  obj[QStringLiteral("light.areaHeight")] = lightImpl_->areaHeight_;
  obj[QStringLiteral("light.areaShape")] = static_cast<int>(lightImpl_->areaShape_);
  obj[QStringLiteral("light.coneAngle")] = lightImpl_->coneAngle_;
  obj[QStringLiteral("light.coneFeather")] = lightImpl_->coneFeather_;
  obj[QStringLiteral("light.coneLength")] = lightImpl_->coneLength_;
  obj[QStringLiteral("light.goboTexturePath")] = lightImpl_->goboTexturePath_;
  obj[QStringLiteral("light.goboIntensity")] = lightImpl_->goboIntensity_;
  obj[QStringLiteral("light.goboRotation")] = lightImpl_->goboRotation_;
  obj[QStringLiteral("light.goboInvert")] = lightImpl_->goboInvert_;
  obj[QStringLiteral("light.shadowRadius")] = lightImpl_->shadowRadius_;
  obj[QStringLiteral("light.castsShadows")] = lightImpl_->castsShadows_;
  obj[QStringLiteral("light.glowEnabled")] = lightImpl_->glowEnabled_;
  obj[QStringLiteral("light.glowSize")] = lightImpl_->glowSize_;
  obj[QStringLiteral("light.glowIntensity")] = lightImpl_->glowIntensity_;
  obj[QStringLiteral("light.linkMode")] = static_cast<int>(lightImpl_->linkMode_);
  obj[QStringLiteral("light.linkedLayerIds")] = lightImpl_->linkedLayerIdsText_;
  obj[QStringLiteral("light.excludedLayerIds")] = lightImpl_->excludedLayerIdsText_;
  return obj;
}

void ArtifactLightLayer::fromJsonProperties(const QJsonObject& obj)
{
  ArtifactAbstractLayer::fromJsonProperties(obj);
  if (obj.contains(QStringLiteral("light.type"))) {
    setLightType(static_cast<LightType>(std::clamp(
        obj.value(QStringLiteral("light.type")).toInt(), 0, 4)));
  }
  if (obj.contains(QStringLiteral("light.color.r")) ||
      obj.contains(QStringLiteral("light.color.g")) ||
      obj.contains(QStringLiteral("light.color.b")) ||
      obj.contains(QStringLiteral("light.color.a"))) {
    setColor(ArtifactCore::FloatColor(
        static_cast<float>(obj.value(QStringLiteral("light.color.r")).toDouble(1.0)),
        static_cast<float>(obj.value(QStringLiteral("light.color.g")).toDouble(1.0)),
        static_cast<float>(obj.value(QStringLiteral("light.color.b")).toDouble(1.0)),
        static_cast<float>(obj.value(QStringLiteral("light.color.a")).toDouble(1.0))));
  }
  if (obj.contains(QStringLiteral("light.intensity"))) setIntensity(
      static_cast<float>(obj.value(QStringLiteral("light.intensity")).toDouble()));
  if (obj.contains(QStringLiteral("light.range"))) setRange(
      static_cast<float>(obj.value(QStringLiteral("light.range")).toDouble()));
  if (obj.contains(QStringLiteral("light.areaWidth")) ||
      obj.contains(QStringLiteral("light.areaHeight"))) {
    setAreaSize(
        static_cast<float>(obj.value(QStringLiteral("light.areaWidth")).toDouble(areaWidth())),
        static_cast<float>(obj.value(QStringLiteral("light.areaHeight")).toDouble(areaHeight())));
  }
  if (obj.contains(QStringLiteral("light.areaShape"))) setAreaShape(
      static_cast<AreaLightShape>(std::clamp(
          obj.value(QStringLiteral("light.areaShape")).toInt(), 0, 1)));
  if (obj.contains(QStringLiteral("light.coneAngle"))) setConeAngle(
      static_cast<float>(obj.value(QStringLiteral("light.coneAngle")).toDouble()));
  if (obj.contains(QStringLiteral("light.coneFeather"))) setConeFeather(
      static_cast<float>(obj.value(QStringLiteral("light.coneFeather")).toDouble()));
  if (obj.contains(QStringLiteral("light.coneLength"))) setConeLength(
      static_cast<float>(obj.value(QStringLiteral("light.coneLength")).toDouble()));
  if (obj.contains(QStringLiteral("light.goboTexturePath"))) setGoboTexturePath(
      obj.value(QStringLiteral("light.goboTexturePath")).toString());
  if (obj.contains(QStringLiteral("light.goboIntensity"))) setGoboIntensity(
      static_cast<float>(obj.value(QStringLiteral("light.goboIntensity")).toDouble(1.0)));
  if (obj.contains(QStringLiteral("light.goboRotation"))) setGoboRotation(
      static_cast<float>(obj.value(QStringLiteral("light.goboRotation")).toDouble()));
  if (obj.contains(QStringLiteral("light.goboInvert"))) setGoboInvert(
      obj.value(QStringLiteral("light.goboInvert")).toBool());
  if (obj.contains(QStringLiteral("light.shadowRadius"))) setShadowRadius(
      static_cast<float>(obj.value(QStringLiteral("light.shadowRadius")).toDouble()));
  if (obj.contains(QStringLiteral("light.castsShadows"))) setCastsShadows(
      obj.value(QStringLiteral("light.castsShadows")).toBool());
  if (obj.contains(QStringLiteral("light.glowEnabled"))) setGlowEnabled(
      obj.value(QStringLiteral("light.glowEnabled")).toBool(lightImpl_->glowEnabled_));
  if (obj.contains(QStringLiteral("light.glowSize"))) setGlowSize(
      static_cast<float>(obj.value(QStringLiteral("light.glowSize")).toDouble(lightImpl_->glowSize_)));
  if (obj.contains(QStringLiteral("light.glowIntensity"))) setGlowIntensity(
      static_cast<float>(obj.value(QStringLiteral("light.glowIntensity")).toDouble(lightImpl_->glowIntensity_)));
  if (obj.contains(QStringLiteral("light.linkMode"))) setLightLinkMode(
      static_cast<LightLinkMode>(std::clamp(
          obj.value(QStringLiteral("light.linkMode")).toInt(), 0, 2)));
  if (obj.contains(QStringLiteral("light.linkedLayerIds"))) setLinkedLayerIdsText(
      obj.value(QStringLiteral("light.linkedLayerIds")).toString());
  if (obj.contains(QStringLiteral("light.excludedLayerIds"))) setExcludedLayerIdsText(
      obj.value(QStringLiteral("light.excludedLayerIds")).toString());
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
  // The local-space +Z axis, transformed by the layer's 3D transform, gives
  // the light's "outgoing" axis for Point/Spot/Area. Parallel (Directional)
  // light is intentionally position-less: its forward is the *opposite* of
  // +Z (the direction the light travels), so the gizmo points the way the
  // rays go. See "Parallel light gizmo" below.
  QVector3D forward = m.mapVector(QVector3D(0, 0, 1.0f));
  if (forward.lengthSquared() <= 0.000001f) {
    forward = QVector3D(0, 0, 1);
  } else {
    forward.normalize();
  }
  const QVector3D tip = pos + forward * (baseSize * 2.4f);
  const QVector3D side = m.mapVector(QVector3D(1, 0, 0)).normalized() * (baseSize * 0.7f);
  const QVector3D up = m.mapVector(QVector3D(0, 1, 0)).normalized() * (baseSize * 0.7f);

  // Shadow cue: render a soft secondary shape near the bulb whose placement
  // and form depend on the light type, instead of a single Y-axis ring that
  // exists for every type. Point: 3-axis wider ring; Spot: a thin inner cone
  // surface; Area: a thin inner disk on the same normal; Parallel/Ambient:
  // no shadow cue (the concept does not apply in their reference frame).
  if (lightImpl_->castsShadows_) {
    const ArtifactCore::FloatColor shadowTint{
        lightColor.r(), lightColor.g(), lightColor.b(), 0.18f};
    const float shadowSize =
        baseSize + std::max(2.0f, lightImpl_->shadowRadius_ * 0.05f);
    if (type == LightType::Point) {
      renderer->drawGizmoRing(p, float3{1, 0, 0}, shadowSize, shadowTint, 0.9f);
      renderer->drawGizmoRing(p, float3{0, 1, 0}, shadowSize, shadowTint, 0.9f);
      renderer->drawGizmoRing(p, float3{0, 0, 1}, shadowSize, shadowTint, 0.9f);
    } else if (type == LightType::Spot) {
      const float coneLength = std::max(1.0f, lightImpl_->coneLength_);
      const float coneRadius = std::tan(std::clamp(lightImpl_->coneAngle_,
                                                    0.1f, 179.0f) *
                                        3.14159265f / 360.0f) * coneLength;
      const QVector3D coneCenter = pos + forward * coneLength;
      renderer->drawGizmoRing(
          float3{coneCenter.x(), coneCenter.y(), coneCenter.z()},
          float3{forward.x(), forward.y(), forward.z()},
          coneRadius * 0.95f, shadowTint, 0.9f);
    } else if (type == LightType::Area) {
      const float shadowRadius = std::max(
          1.0f, std::min(lightImpl_->areaWidth_, lightImpl_->areaHeight_) * 0.5f);
      const QVector3D normal = m.mapVector(QVector3D(0, 0, 1)).normalized();
      renderer->drawGizmoRing(
          float3{pos.x(), pos.y(), pos.z()},
          float3{normal.x(), normal.y(), normal.z()},
          shadowRadius * 0.95f, shadowTint, 0.9f);
    }
  }

  // Direction indicators for oriented lights.
  if (type == LightType::Spot) {
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

    // Spot cone: the gizmo always honors the user-authored coneLength_.
    // The attenuation range_ is shown as a separate, lighter ring only when
    // coneLength_ is shorter than range_, so the user can see whether the
    // attenuation kicks in before the cone ends. (Previously coneLength was
    // silently clamped to range_, which made the gizmo and the property
    // value disagree.)
    const float coneLength = std::max(1.0f, lightImpl_->coneLength_);
    const float coneRadius = std::tan(std::clamp(lightImpl_->coneAngle_,
                                                  0.1f, 179.0f) *
                                      3.14159265f / 360.0f) * coneLength;
    QVector3D coneSide = m.mapVector(QVector3D(1, 0, 0));
    QVector3D coneUp = m.mapVector(QVector3D(0, 1, 0));
    if (coneSide.lengthSquared() <= 0.000001f) coneSide = QVector3D(1, 0, 0);
    if (coneUp.lengthSquared() <= 0.000001f) coneUp = QVector3D(0, 1, 0);
    coneSide.normalize();
    coneUp.normalize();
    const QVector3D coneCenter = pos + forward * coneLength;
    const ArtifactCore::FloatColor coneColor{lightColor.r(), lightColor.g(),
                                             lightColor.b(), 0.72f};
    const ArtifactCore::FloatColor featherColor{lightColor.r(), lightColor.g(),
                                                 lightColor.b(), 0.30f};
    renderer->drawGizmoRing(float3{coneCenter.x(), coneCenter.y(), coneCenter.z()},
                            float3{forward.x(), forward.y(), forward.z()}, coneRadius,
                            coneColor, 1.2f);
    for (const float signX : {-1.0f, 1.0f}) {
      for (const float signY : {-1.0f, 1.0f}) {
        const QVector3D edge = coneCenter + coneSide * (coneRadius * signX)
                               + coneUp * (coneRadius * signY);
        renderer->drawGizmoLine(float3{pos.x(), pos.y(), pos.z()},
                                float3{edge.x(), edge.y(), edge.z()}, coneColor, 1.0f);
      }
    }
    // Feather ring: inner edge = outer edge - feather angle (inward). Always
    // shown when feather is non-zero, even if it is very small, so the user
    // can see the soft band develop live as they drag the property.
    if (lightImpl_->coneFeather_ > 0.0f) {
      const float featherAngle =
          std::max(0.0f, lightImpl_->coneAngle_ - lightImpl_->coneFeather_);
      if (featherAngle < lightImpl_->coneAngle_) {
        const float innerRadius = std::tan(featherAngle * 3.14159265f / 360.0f) *
                                  coneLength;
        renderer->drawGizmoRing(float3{coneCenter.x(), coneCenter.y(), coneCenter.z()},
                                float3{forward.x(), forward.y(), forward.z()},
                                innerRadius, featherColor, 1.0f);
      }
    }
    // Attenuation range indicator: only when range_ extends past the cone.
    if (lightImpl_->range_ > coneLength + 0.5f) {
      const ArtifactCore::FloatColor rangeColor{lightColor.r(), lightColor.g(),
                                                 lightColor.b(), 0.16f};
      const float rangeRadius = std::tan(std::clamp(lightImpl_->coneAngle_,
                                                    0.1f, 179.0f) *
                                        3.14159265f / 360.0f) *
                                 lightImpl_->range_;
      const QVector3D rangeCenter = pos + forward * lightImpl_->range_;
      renderer->drawGizmoRing(float3{rangeCenter.x(), rangeCenter.y(), rangeCenter.z()},
                              float3{forward.x(), forward.y(), forward.z()}, rangeRadius,
                              rangeColor, 0.9f);
    }
  } else if (type == LightType::Parallel) {
    // Parallel (Directional) light is position-less. Draw the sun-style
    // gizmo at the layer's position with an arrow that points along the
    // *incoming* ray direction (i.e. the opposite of the layer's local +Z,
    // which is the direction rays travel). The crossbar at the tip marks
    // the incoming ray plane.
    const QVector3D incoming = -forward;
    renderer->drawGizmoLine(float3{pos.x(), pos.y(), pos.z()},
                            float3{pos.x() + incoming.x() * (baseSize * 2.4f),
                                   pos.y() + incoming.y() * (baseSize * 2.4f),
                                   pos.z() + incoming.z() * (baseSize * 2.4f)},
                            tintColor, 1.0f);
    renderer->drawGizmoLine(
        float3{pos.x() + incoming.x() * (baseSize * 2.4f) - side.x(),
               pos.y() + incoming.y() * (baseSize * 2.4f) - side.y(),
               pos.z() + incoming.z() * (baseSize * 2.4f) - side.z()},
        float3{pos.x() + incoming.x() * (baseSize * 2.4f) + side.x(),
               pos.y() + incoming.y() * (baseSize * 2.4f) + side.y(),
               pos.z() + incoming.z() * (baseSize * 2.4f) + side.z()},
        tintColor, 0.9f);
    renderer->drawGizmoLine(
        float3{pos.x() + incoming.x() * (baseSize * 2.4f) - up.x(),
               pos.y() + incoming.y() * (baseSize * 2.4f) - up.y(),
               pos.z() + incoming.z() * (baseSize * 2.4f) - up.z()},
        float3{pos.x() + incoming.x() * (baseSize * 2.4f) + up.x(),
               pos.y() + incoming.y() * (baseSize * 2.4f) + up.y(),
               pos.z() + incoming.z() * (baseSize * 2.4f) + up.z()},
        tintColor, 0.9f);
  }

  // Lux-style visible glow: one additive camera-facing sprite at the light
  // position. Parallel is direction-only and Ambient has no meaningful
  // origin, so only positioned lights participate. The layer stays excluded
  // from the final render; final-render glow is a separate render-queue item.
  if (lightImpl_->glowEnabled_ &&
      (type == LightType::Point || type == LightType::Spot ||
       type == LightType::Area)) {
    const float falloffBase = type == LightType::Spot
        ? std::max(1.0f, lightImpl_->coneLength_)
        : std::max(1.0f, lightImpl_->range_);
    const float glowRadius = std::clamp(
        falloffBase * 0.25f * lightImpl_->glowSize_, 1.0f, 100000.0f);
    const float glowAlpha = std::clamp(
        (lightImpl_->intensity_ / 100.0f) * lightImpl_->glowIntensity_ *
            opacity(),
        0.0f, 1.0f);
    if (glowAlpha > 0.001f) {
      ArtifactCore::ParticleRenderData glowData;
      glowData.frameNumber = currentFrame();
      glowData.options.blend = ArtifactCore::ParticleBlendPolicy::Additive;
      glowData.options.billboard = ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
      glowData.options.depthTest = false;
      glowData.options.depthWrite = false;
      ArtifactCore::ParticleVertex glowVertex;
      glowVertex.px = pos.x();
      glowVertex.py = pos.y();
      glowVertex.pz = pos.z();
      glowVertex.vx = 0.0f;
      glowVertex.vy = 0.0f;
      glowVertex.vz = 0.0f;
      glowVertex.r = lightColor.r();
      glowVertex.g = lightColor.g();
      glowVertex.b = lightColor.b();
      glowVertex.a = glowAlpha;
      glowVertex.size = glowRadius;
      glowVertex.stretch = 1.0f;
      glowVertex.rotation = 0.0f;
      glowVertex.age = 0.0f;
      glowVertex.lifetime = 1.0f;
      glowData.particles.push_back(glowVertex);
      renderer->drawParticles(glowData);
    }
  }
}

LightType ArtifactLightLayer::lightType() const { return lightImpl_->type_; }
void ArtifactLightLayer::setLightType(LightType t) {
  lightImpl_->type_ = static_cast<LightType>(std::clamp(
      static_cast<int>(t), 0, 4));
  changed();
}

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
  lightImpl_->areaShape_ = static_cast<AreaLightShape>(std::clamp(
      static_cast<int>(shape), 0, 1));
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

QString ArtifactLightLayer::goboTexturePath() const { return lightImpl_->goboTexturePath_; }
void ArtifactLightLayer::setGoboTexturePath(const QString& path)
{
  const QString trimmedPath = path.trimmed();
  lightImpl_->goboTexturePath_ = trimmedPath.isEmpty()
      ? QString{}
      : QDir::cleanPath(trimmedPath);
  changed();
}
float ArtifactLightLayer::goboIntensity() const { return lightImpl_->goboIntensity_; }
void ArtifactLightLayer::setGoboIntensity(float intensity)
{
  lightImpl_->goboIntensity_ = std::clamp(intensity, 0.0f, 1.0f);
  changed();
}
float ArtifactLightLayer::goboRotation() const { return lightImpl_->goboRotation_; }
void ArtifactLightLayer::setGoboRotation(float degrees)
{
  lightImpl_->goboRotation_ = std::fmod(degrees, 360.0f);
  changed();
}
bool ArtifactLightLayer::goboInvert() const { return lightImpl_->goboInvert_; }
void ArtifactLightLayer::setGoboInvert(bool enabled)
{
  lightImpl_->goboInvert_ = enabled;
  changed();
}

float ArtifactLightLayer::shadowRadius() const { return lightImpl_->shadowRadius_; }
void ArtifactLightLayer::setShadowRadius(float r) { lightImpl_->shadowRadius_ = r; changed(); }

bool ArtifactLightLayer::castsShadows() const { return lightImpl_->castsShadows_; }
void ArtifactLightLayer::setCastsShadows(bool e) { lightImpl_->castsShadows_ = e; changed(); }

bool ArtifactLightLayer::glowEnabled() const { return lightImpl_->glowEnabled_; }
void ArtifactLightLayer::setGlowEnabled(bool e) { lightImpl_->glowEnabled_ = e; changed(); }

float ArtifactLightLayer::glowSize() const { return lightImpl_->glowSize_; }
void ArtifactLightLayer::setGlowSize(float multiplier)
{
  lightImpl_->glowSize_ = std::isfinite(multiplier)
      ? std::clamp(multiplier, 0.0f, 8.0f)
      : 1.0f;
  changed();
}

float ArtifactLightLayer::glowIntensity() const { return lightImpl_->glowIntensity_; }
void ArtifactLightLayer::setGlowIntensity(float multiplier)
{
  lightImpl_->glowIntensity_ = std::isfinite(multiplier)
      ? std::clamp(multiplier, 0.0f, 4.0f)
      : 1.0f;
  changed();
}

LightLinkMode ArtifactLightLayer::lightLinkMode() const { return lightImpl_->linkMode_; }
void ArtifactLightLayer::setLightLinkMode(LightLinkMode mode)
{
  lightImpl_->linkMode_ = static_cast<LightLinkMode>(std::clamp(
      static_cast<int>(mode), 0, 2));
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
    shapeProp->setTooltip(QStringLiteral("0: Rectangle, 1: Disk (radius = min(width, height) / 2)"));
    lightOptions.addProperty(shapeProp);
    }

    if (lightImpl_->type_ == LightType::Spot) {
    auto coneAngleProp = persistentLayerProperty(QStringLiteral("Light/Cone Angle"),
                                                  ArtifactCore::PropertyType::Float,
                                                  static_cast<double>(lightImpl_->coneAngle_), -138);
    coneAngleProp->setHardRange(0.1, 179.0);
    coneAngleProp->setSoftRange(1.0, 120.0);
    coneAngleProp->setUnit(QStringLiteral("deg"));
    coneAngleProp->setTooltip(QStringLiteral("Spot-light outer cone angle (full apex angle, degrees)"));
    lightOptions.addProperty(coneAngleProp);

    auto coneFeatherProp = persistentLayerProperty(QStringLiteral("Light/Cone Feather"),
                                                    ArtifactCore::PropertyType::Float,
                                                    static_cast<double>(lightImpl_->coneFeather_), -137);
    coneFeatherProp->setHardRange(0.0, 179.0);
    coneFeatherProp->setSoftRange(0.0, 60.0);
    coneFeatherProp->setUnit(QStringLiteral("deg"));
    coneFeatherProp->setTooltip(QStringLiteral("Soft edge width (degrees inward from the outer cone edge)"));
    lightOptions.addProperty(coneFeatherProp);

    auto coneLengthProp = persistentLayerProperty(QStringLiteral("Light/Cone Length"),
                                                   ArtifactCore::PropertyType::Float,
                                                   static_cast<double>(lightImpl_->coneLength_), -136);
    coneLengthProp->setHardRange(1.0, 10000.0);
    coneLengthProp->setSoftRange(25.0, 1000.0);
    coneLengthProp->setUnit(QStringLiteral("px"));
    coneLengthProp->setTooltip(QStringLiteral("Spot-light cone range in composition space"));
    lightOptions.addProperty(coneLengthProp);

    auto goboPathProp = persistentLayerProperty(
        QStringLiteral("Light/GOBO Texture"), ArtifactCore::PropertyType::String,
        lightImpl_->goboTexturePath_, -135);
    goboPathProp->setTooltip(QStringLiteral("Image projected by this Spot light; empty disables GOBO"));
    lightOptions.addProperty(goboPathProp);
    auto goboIntensityProp = persistentLayerProperty(
        QStringLiteral("Light/GOBO Intensity"), ArtifactCore::PropertyType::Float,
        static_cast<double>(lightImpl_->goboIntensity_), -134);
    goboIntensityProp->setHardRange(0.0, 1.0);
    goboIntensityProp->setSoftRange(0.0, 1.0);
    lightOptions.addProperty(goboIntensityProp);
    auto goboRotationProp = persistentLayerProperty(
        QStringLiteral("Light/GOBO Rotation"), ArtifactCore::PropertyType::Float,
        static_cast<double>(lightImpl_->goboRotation_), -133);
    goboRotationProp->setHardRange(-360.0, 360.0);
    goboRotationProp->setUnit(QStringLiteral("deg"));
    lightOptions.addProperty(goboRotationProp);
    auto goboInvertProp = persistentLayerProperty(
        QStringLiteral("Light/GOBO Invert"), ArtifactCore::PropertyType::Boolean,
        lightImpl_->goboInvert_, -132);
    lightOptions.addProperty(goboInvertProp);
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

    auto glowEnabledProp = persistentLayerProperty(
        QStringLiteral("Light/Glow"),
        ArtifactCore::PropertyType::Boolean,
        lightImpl_->glowEnabled_, -119);
    glowEnabledProp->setTooltip(QStringLiteral("Lux-style visible glow sprite at the light position (Point/Spot/Area)"));
    lightOptions.addProperty(glowEnabledProp);

    auto glowSizeProp = persistentLayerProperty(
        QStringLiteral("Light/Glow Size"),
        ArtifactCore::PropertyType::Float,
        static_cast<double>(lightImpl_->glowSize_), -118);
    glowSizeProp->setHardRange(0.0, 8.0);
    glowSizeProp->setSoftRange(0.25, 4.0);
    glowSizeProp->setTooltip(QStringLiteral("Glow radius multiplier relative to the light range"));
    lightOptions.addProperty(glowSizeProp);

    auto glowIntensityProp = persistentLayerProperty(
        QStringLiteral("Light/Glow Intensity"),
        ArtifactCore::PropertyType::Float,
        static_cast<double>(lightImpl_->glowIntensity_), -117);
    glowIntensityProp->setHardRange(0.0, 4.0);
    glowIntensityProp->setSoftRange(0.0, 2.0);
    lightOptions.addProperty(glowIntensityProp);

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
    } else if (propertyPath == "Light/GOBO Texture") {
        setGoboTexturePath(value.toString());
        return true;
    } else if (propertyPath == "Light/GOBO Intensity") {
        setGoboIntensity(value.toFloat());
        return true;
    } else if (propertyPath == "Light/GOBO Rotation") {
        setGoboRotation(value.toFloat());
        return true;
    } else if (propertyPath == "Light/GOBO Invert") {
        setGoboInvert(value.toBool());
        return true;
    } else if (propertyPath == "Light/Shadows") {
        setCastsShadows(value.toBool());
        return true;
    } else if (propertyPath == "Light/Shadow Radius") {
        setShadowRadius(value.toFloat());
        return true;
    } else if (propertyPath == "Light/Glow") {
        setGlowEnabled(value.toBool());
        return true;
    } else if (propertyPath == "Light/Glow Size") {
        setGlowSize(value.toFloat());
        return true;
    } else if (propertyPath == "Light/Glow Intensity") {
        setGlowIntensity(value.toFloat());
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
