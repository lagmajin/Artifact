module;
#include <utility>
#include <memory>
#include <wobjectdefs.h>
#include <QString>
export module Artifact.Layer.Light;


import Artifact.Layer.Abstract;
import Property.Group;
import Color.Float;

export namespace Artifact {

 enum class LightType {
  Point = 0,
  Spot,
  Parallel,
  Ambient
 };

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

  // Light specific properties
  LightType lightType() const;
  void setLightType(LightType type);

  ArtifactCore::FloatColor color() const;
  void setColor(const ArtifactCore::FloatColor& color);

  float intensity() const;
  void setIntensity(float intensity);

  // RT Shadow specific: Larger radius = Softer shadows
  float shadowRadius() const;
  void setShadowRadius(float radius);

  bool castsShadows() const;
  void setCastsShadows(bool enabled);

  LightLinkMode lightLinkMode() const;
  void setLightLinkMode(LightLinkMode mode);
  QString linkedLayerIdsText() const;
  void setLinkedLayerIdsText(const QString& ids);
  QString excludedLayerIdsText() const;
  void setExcludedLayerIdsText(const QString& ids);

  // Generic properties for Inspector
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

 private:
  struct Impl;
  Impl* lightImpl_;
 };

 using ArtifactLightLayerPtr = std::shared_ptr<ArtifactLightLayer>;

} // namespace Artifact
