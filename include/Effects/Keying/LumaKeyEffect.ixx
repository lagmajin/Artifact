module;
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <cmath>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Keying.LumaKey;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Memory.SharedPtr;
import Property.Abstract;
import Utils.String.UniString;

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
  static constexpr const char* kGpuGenericKeyString = "luma_key";
  static constexpr std::uint32_t kGpuGenericKey =
      gpuGenericKeyFromString(kGpuGenericKeyString);
  std::optional<EffectScalarGizmoParameter>
  scalarGizmoParameter(unsigned int index) const override {
    if (index == 0) {
      return ArtifactAbstractEffect::scalarGizmoParameter(index);
    }
    if (index == 1) {
      return EffectScalarGizmoParameter{
          QStringLiteral("effect.keying.luma_key.low_threshold"),
          QStringLiteral("lowThreshold"), 0.0, 1.0, 0.01, 0.0025,
          100.0, QStringLiteral("%")};
    }
    if (index == 2) {
      return EffectScalarGizmoParameter{
          QStringLiteral("effect.keying.luma_key.high_threshold"),
          QStringLiteral("highThreshold"), 0.0, 1.0, 0.01, 0.0025,
          100.0, QStringLiteral("%")};
    }
    if (index == 3) {
      return EffectScalarGizmoParameter{
          QStringLiteral("effect.keying.luma_key.softness"),
          QStringLiteral("softness"), 0.001, 1.0, 0.001, 0.0025,
          100.0, QStringLiteral("%")};
    }
    if (index == 4) {
      return EffectScalarGizmoParameter{
          QStringLiteral("effect.keying.luma_key.choke"),
          QStringLiteral("choke"), -1.0, 1.0, 0.01, 0.005,
          100.0, QStringLiteral("%")};
    }
    if (index == 5) {
      return EffectScalarGizmoParameter{
          QStringLiteral("effect.keying.luma_key.matte_blur"),
          QStringLiteral("matteBlur"), 0.0, 2.0, 0.01, 0.005,
          1.0, QStringLiteral("x")};
    }
    if (index == 6) {
      EffectScalarGizmoParameter parameter{
          QStringLiteral("effect.keying.luma_key.view_mode"),
          QStringLiteral("viewMode"), 0.0, 1.0, 1.0, 0.05,
          1.0, QString()};
      parameter.choiceCount = 2;
      return parameter;
    }
    return std::nullopt;
  }
  std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }
  GpuRasterEffectDomain gpuRasterEffectDomain() const override {
    return GpuRasterEffectDomain::Spatial;
  }
  bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
    if (std::abs(typedCpuImpl_->choke()) > 1.0e-4f ||
        typedCpuImpl_->matteBlur() > 1.0e-4f) {
      return false;
    }
    GpuSpatialEffectNode node;
    node.kind = GpuSpatialEffectKind::Generic;
    node.genericKey = kGpuGenericKey;
    node.parameters[0] = typedCpuImpl_->lowThreshold();
    node.parameters[1] = typedCpuImpl_->highThreshold();
    node.parameters[2] = typedCpuImpl_->softness();
    node.parameters[3] = typedCpuImpl_->choke();
    node.parameters[4] = typedCpuImpl_->matteBlur();
    node.parameters[5] = static_cast<float>(typedCpuImpl_->viewMode());
    return stack.append(node);
  }
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
