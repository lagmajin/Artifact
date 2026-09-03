module;
#include <vector>
#include <QImage>
#include <QJsonObject>
#include <QVariant>

export module Artifact.Layers.Noise;
import Color.Float;
import Artifact.Layer.InitParams;
import Artifact.Layers.Abstract._2D;
import Image.ImageF32x4_RGBA;
import ImageProcessing.ProceduralTexture;

export namespace Artifact {
using namespace ArtifactCore;

class ArtifactNoiseLayer : public ArtifactAbstract2DLayer
{
private:
  class Impl;
  Impl* impl_;
  const QImage& currentNoiseImage() const;

public:
  ArtifactNoiseLayer();
  ~ArtifactNoiseLayer();

  void setSize(int width, int height);
  const ArtifactCore::ProceduralTextureSettings& settings() const;
  void setSettings(const ArtifactCore::ProceduralTextureSettings& settings);
  bool isColorMappingEnabled() const;
  void setColorMappingEnabled(bool enabled);
  FloatColor colorA() const;
  void setColorA(const FloatColor& color);
  FloatColor colorB() const;
  void setColorB(const FloatColor& color);

  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

  void draw(ArtifactIRenderer* renderer) override;
  QImage toQImage() const;
  QImage getThumbnail(int width = 128, int height = 128) const override;

protected:
  const ArtifactCore::ImageF32x4_RGBA* resolveLayerSourceOverride() const override;
  QJsonObject sourceComponentSettingsSnapshot() const override;
};

} // namespace Artifact
