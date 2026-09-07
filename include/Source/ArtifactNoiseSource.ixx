module;
#include <algorithm>
#include <cmath>
#include <QImage>
#include <QSize>
#include <QString>

export module Artifact.Source.Noise;

import Color.Float;
import ImageProcessing.ProceduralTexture;
import Image.ImageF32x4_RGBA;

export namespace Artifact {

// Reusable authored state for procedural noise. Rendering and layer placement
// deliberately remain outside this source component.
class ArtifactNoiseSource {
public:
  const ArtifactCore::ProceduralTextureSettings& settings() const { return settings_; }
  void setSettings(const ArtifactCore::ProceduralTextureSettings& value) { settings_ = value; }
  bool colorMappingEnabled() const { return colorMappingEnabled_; }
  void setColorMappingEnabled(bool value) { colorMappingEnabled_ = value; }
  ArtifactCore::FloatColor colorA() const { return colorA_; }
  void setColorA(const ArtifactCore::FloatColor& value) { colorA_ = normalized(value, ArtifactCore::FloatColor(0, 0, 0, 1)); }
  ArtifactCore::FloatColor colorB() const { return colorB_; }
  void setColorB(const ArtifactCore::FloatColor& value) { colorB_ = normalized(value, ArtifactCore::FloatColor(1, 1, 1, 1)); }

protected:
  ArtifactCore::ProceduralTextureSettings settings_;
  bool colorMappingEnabled_ = false;
  ArtifactCore::FloatColor colorA_ = ArtifactCore::FloatColor(0, 0, 0, 1);
  ArtifactCore::FloatColor colorB_ = ArtifactCore::FloatColor(1, 1, 1, 1);
  // CPU source cache; presentation-layer thumbnail conversion remains a
  // consumer concern, while the generated source data belongs here.
  mutable ArtifactCore::ImageF32x4_RGBA buffer_;
  mutable QString bufferSignature_;
  mutable QImage cachedImage_;
  mutable QSize cachedSize_;
  mutable QString cachedSignature_;

private:
  static ArtifactCore::FloatColor normalized(const ArtifactCore::FloatColor& value,
                                              const ArtifactCore::FloatColor& fallback) {
    if (!std::isfinite(value.r()) || !std::isfinite(value.g()) || !std::isfinite(value.b()) || !std::isfinite(value.a())) return fallback;
    return ArtifactCore::FloatColor(std::clamp(value.r(), 0.0f, 1.0f), std::clamp(value.g(), 0.0f, 1.0f), std::clamp(value.b(), 0.0f, 1.0f), std::clamp(value.a(), 0.0f, 1.0f));
  }
};

}
