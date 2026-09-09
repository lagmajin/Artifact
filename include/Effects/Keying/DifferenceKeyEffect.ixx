module;
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>
#include <QVariant>

export module Artifact.Effect.Keying.DifferenceKey;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import FloatRGBA;
import Memory.SharedPtr;

export namespace Artifact {
class DifferenceKeyEffectCPUImpl final : public ArtifactEffectImplBase {
  FloatRGBA referenceColor_{0.0f, 0.0f, 0.0f, 1.0f};
  float threshold_ = 0.1f;
  float softness_ = 0.08f;
  float choke_ = 0.0f;
  float matteBlur_ = 0.0f;
  int viewMode_ = 0;
public:
  void setReferenceColor(const FloatRGBA& value) {
    const auto safeChannel = [](float channel, float fallback) {
      return std::isfinite(channel) ? std::clamp(channel, 0.0f, 1.0f) : fallback;
    };
    referenceColor_ = FloatRGBA(safeChannel(value.r(), 0.0f),
                                safeChannel(value.g(), 0.0f),
                                safeChannel(value.b(), 0.0f),
                                safeChannel(value.a(), 1.0f));
  }
  void setThreshold(float value) {
    threshold_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.732f) : 0.1f;
  }
  void setSoftness(float value) {
    softness_ = std::isfinite(value) ? std::clamp(value, 0.001f, 1.0f) : 0.08f;
  }
  const FloatRGBA& referenceColor() const { return referenceColor_; }
  float threshold() const { return threshold_; }
  float softness() const { return softness_; }
  void setChoke(float value) {
    choke_ = std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
  }
  void setMatteBlur(float value) {
    matteBlur_ = std::isfinite(value) ? std::clamp(value, 0.0f, 2.0f) : 0.0f;
  }
  void setViewMode(int mode) { viewMode_ = std::clamp(mode, 0, 1); }
  float choke() const { return choke_; }
  float matteBlur() const { return matteBlur_; }
  int viewMode() const { return viewMode_; }
  void applyCPU(const ArtifactCore::ImageF32x4RGBAWithCache& src,
                ArtifactCore::ImageF32x4RGBAWithCache& dst) override;
};

class DifferenceKeyEffect final : public ArtifactAbstractEffect {
  SharedPtr<DifferenceKeyEffectCPUImpl> typedCpuImpl_;

  void syncGpuImpl();
public:
  DifferenceKeyEffect();
  bool supportsGPU() const override { return true; }
  std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
  void setPropertyValue(const ArtifactCore::UniString& name,
                        const QVariant& value) override;
};
}
