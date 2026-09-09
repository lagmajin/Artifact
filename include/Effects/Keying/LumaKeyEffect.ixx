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
  float choke_ = 0.0f;
  float matteBlur_ = 0.0f;
  int viewMode_ = 0;

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
  void setChoke(float value) {
    choke_ = std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
  }
  void setMatteBlur(float value) {
    matteBlur_ = std::isfinite(value) ? std::clamp(value, 0.0f, 2.0f) : 0.0f;
  }
  void setViewMode(int mode) { viewMode_ = std::clamp(mode, 0, 1); }
  float lowThreshold() const { return lowThreshold_; }
  float highThreshold() const { return highThreshold_; }
  float softness() const { return softness_; }
  float choke() const { return choke_; }
  float matteBlur() const { return matteBlur_; }
  int viewMode() const { return viewMode_; }
  void applyCPU(const ArtifactCore::ImageF32x4RGBAWithCache& src,
                ArtifactCore::ImageF32x4RGBAWithCache& dst) override;
};

class LumaKeyEffect final : public ArtifactAbstractEffect {
  SharedPtr<LumaKeyEffectCPUImpl> typedCpuImpl_;

  void syncGpuImpl();

public:
  LumaKeyEffect();
  bool supportsGPU() const override { return true; }
  std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
  void setPropertyValue(const ArtifactCore::UniString& name,
                        const QVariant& value) override;
  void setChoke(float value);
  float choke() const;
  void setMatteBlur(float value);
  float matteBlur() const;
  void setViewMode(int mode);
  int viewMode() const;
};

}
