module;
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <algorithm>
#include <cmath>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Keying.DifferenceKey;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;
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
  static constexpr const char* kGpuGenericKeyString = "difference_key";
  static constexpr std::uint32_t kGpuGenericKey =
      gpuGenericKeyFromString(kGpuGenericKeyString);
  std::optional<EffectScalarGizmoParameter>
  scalarGizmoParameter(unsigned int index) const override {
    if (index == 0) {
      return ArtifactAbstractEffect::scalarGizmoParameter(index);
    }
    if (index == 1) {
      return EffectScalarGizmoParameter{
          QStringLiteral("effect.keying.difference_key.threshold"),
          QStringLiteral("threshold"), 0.0, 1.732, 0.01, 0.00433,
          1.0, QString()};
    }
    if (index == 2) {
      return EffectScalarGizmoParameter{
          QStringLiteral("effect.keying.difference_key.softness"),
          QStringLiteral("softness"), 0.001, 1.0, 0.001, 0.0025,
          100.0, QStringLiteral("%")};
    }
    if (index == 3) {
      return EffectScalarGizmoParameter{
          QStringLiteral("effect.keying.difference_key.choke"),
          QStringLiteral("choke"), -1.0, 1.0, 0.01, 0.005,
          100.0, QStringLiteral("%")};
    }
    if (index == 4) {
      return EffectScalarGizmoParameter{
          QStringLiteral("effect.keying.difference_key.matte_blur"),
          QStringLiteral("matteBlur"), 0.0, 2.0, 0.01, 0.005,
          1.0, QStringLiteral("x")};
    }
    if (index == 5) {
      EffectScalarGizmoParameter parameter{
          QStringLiteral("effect.keying.difference_key.view_mode"),
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
    const auto& color = typedCpuImpl_->referenceColor();
    if (std::abs(typedCpuImpl_->choke()) > 1.0e-4f ||
        typedCpuImpl_->matteBlur() > 1.0e-4f) {
      return false;
    }
    GpuSpatialEffectNode node;
    node.kind = GpuSpatialEffectKind::Generic;
    node.genericKey = kGpuGenericKey;
    node.parameters[0] = color.r();
    node.parameters[1] = color.g();
    node.parameters[2] = color.b();
    node.parameters[3] = typedCpuImpl_->threshold();
    node.parameters[4] = typedCpuImpl_->softness();
    node.parameters[5] = typedCpuImpl_->choke();
    node.parameters[6] = typedCpuImpl_->matteBlur();
    node.parameters[7] = static_cast<float>(typedCpuImpl_->viewMode());
    return stack.append(node);
  }
  bool supportsGPU() const override { return true; }
  std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
  void setPropertyValue(const ArtifactCore::UniString& name,
                        const QVariant& value) override;
};
}
