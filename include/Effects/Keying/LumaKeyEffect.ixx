module;
#include <memory>
#include <vector>
#include <cmath>
#include <QVariant>

export module Artifact.Effect.Keying.LumaKey;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Memory.SharedPtr;

export namespace Artifact {

class LumaKeyEffectCPUImpl final : public ArtifactEffectImplBase {
  float lowThreshold_ = 0.15f;
  float highThreshold_ = 0.85f;
  float softness_ = 0.08f;

public:
  void setLowThreshold(float value) {
    lowThreshold_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.15f;
  }
  void setHighThreshold(float value) {
    highThreshold_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.85f;
  }
  void setSoftness(float value) {
    softness_ = std::isfinite(value) ? std::clamp(value, 0.001f, 1.0f) : 0.08f;
  }
  float lowThreshold() const { return lowThreshold_; }
  float highThreshold() const { return highThreshold_; }
  float softness() const { return softness_; }
  void applyCPU(const ArtifactCore::ImageF32x4RGBAWithCache& src,
                ArtifactCore::ImageF32x4RGBAWithCache& dst) override;
};

class LumaKeyEffect final : public ArtifactAbstractEffect {
  SharedPtr<LumaKeyEffectCPUImpl> typedCpuImpl_;

public:
  LumaKeyEffect();
  std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
  void setPropertyValue(const ArtifactCore::UniString& name,
                        const QVariant& value) override;
};

}
