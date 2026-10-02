module;
#include <utility>
#include <memory>
#include <wobjectdefs.h>
#include <QString>
#include <QJsonObject>
#include <QVariant>
export module Artifact.Layer.Light;


import Artifact.Layer.Abstract;
import Property.Group;
import Color.Float;
import Memory.SharedPtr;
import Math.Vec;

export namespace Artifact {

 enum class LightType {
  Point = 0,
  Spot,
  Parallel,
  Ambient,
  Area
 };

 enum class AreaLightShape { Rectangle = 0, Disk = 1 };

 enum class LightLinkMode {
  All = 0,
  IncludeOnly,
  ExcludeList
 };

 // AfterEffects compatible Light Layer
 class ArtifactLightLayer : public ArtifactAbstractLayer {
  W_OBJECT(ArtifactLightLayer)
 public:
  ArtifactLightLayer();
  virtual ~ArtifactLightLayer();

  // ArtifactAbstractLayer overrides
  void draw(ArtifactIRenderer* renderer) override;
  UniString className() const override { return "ArtifactLightLayer"; }
  bool shouldIncludeInFinalRender() const override { return false; }
  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;

  // Light specific properties
  LightType lightType() const;
  void setLightType(LightType type);

  ArtifactCore::FloatColor color() const;
  void setColor(const ArtifactCore::FloatColor& color);

  ArtifactCore::Units::Percent intensity() const;
  void setIntensity(ArtifactCore::Units::Percent intensity);

  ArtifactCore::Units::Pixels range() const;
  void setRange(ArtifactCore::Units::Pixels range);
  ArtifactCore::Units::Pixels areaWidth() const;
  ArtifactCore::Units::Pixels areaHeight() const;
  void setAreaSize(ArtifactCore::Units::Pixels width,
                   ArtifactCore::Units::Pixels height);
  AreaLightShape areaShape() const;
  void setAreaShape(AreaLightShape shape);

  // Spot-light cone. These are authored in composition-space units and are
  // also the source values for a future volumetric-light render bridge.
  ArtifactCore::Units::Degrees coneAngle() const;
  void setConeAngle(ArtifactCore::Units::Degrees degrees);
  ArtifactCore::Units::Degrees coneFeather() const;
  void setConeFeather(ArtifactCore::Units::Degrees degrees);
  ArtifactCore::Units::Pixels coneLength() const;
  void setConeLength(ArtifactCore::Units::Pixels length);

  // Spot-light GOBO / cookie projection.
  QString goboTexturePath() const;
  void setGoboTexturePath(const QString& path);
  float goboIntensity() const;
  void setGoboIntensity(float intensity);
  ArtifactCore::Units::Degrees goboRotation() const;
  void setGoboRotation(ArtifactCore::Units::Degrees degrees);
  bool goboInvert() const;
  void setGoboInvert(bool enabled);

  // RT Shadow specific: Larger radius = Softer shadows
  ArtifactCore::Units::Pixels shadowRadius() const;
  void setShadowRadius(ArtifactCore::Units::Pixels radius);

   bool castsShadows() const;
   void setCastsShadows(bool enabled);

   // Lux-style visible glow sprite at the light position (viewport preview;
   // final-render glow is a separate render-queue item).
   bool glowEnabled() const;
   void setGlowEnabled(bool enabled);
   float glowSize() const;
   void setGlowSize(float multiplier);
   float glowIntensity() const;
   void setGlowIntensity(float multiplier);

  LightLinkMode lightLinkMode() const;
  void setLightLinkMode(LightLinkMode mode);
  QString linkedLayerIdsText() const;
  void setLinkedLayerIdsText(const QString& ids);
  QString excludedLayerIdsText() const;
  void setExcludedLayerIdsText(const QString& ids);

 // Generic properties for Inspector
   std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
   bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

 // Property paths whose value is keyframable / expression-driven. Type and
   // link-mode switches stay out: they restructure which paths exist at all.
   static const std::vector<QString>& animatablePropertyPaths();

  private:
   // Animated read path. Returns `fallback` when the property is missing or
   // unanimated, so a static light keeps the render path allocation free.
   double animatedDouble(const QString& propertyPath, double fallback) const;
   bool animatedBool(const QString& propertyPath, bool fallback) const;
   ArtifactCore::FloatColor animatedColor(const QString& propertyPath,
                                          const ArtifactCore::FloatColor& fallback) const;

   // Current `Impl` value for `propertyPath`, or an invalid QVariant when the
   // path is unknown.
   QVariant lightPropertyCacheValue(const QString& propertyPath) const;
   void syncLightPropertyCache(const QString& propertyPath);

   struct Impl;
   Impl* lightImpl_;
 };

 using ArtifactLightLayerPtr = SharedPtr<ArtifactLightLayer>;

} // namespace Artifact
