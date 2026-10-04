module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <QString>
#include <QColor>
#include <QVariant>
#include <vector>
export module Artifact.Effect.Rasterizer.InnerShadow;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;
export namespace Artifact {
class InnerShadowEffect : public ArtifactAbstractEffect {
public:
    InnerShadowEffect();
    ~InnerShadowEffect() override;
    QColor shadowColor() const;
    void setShadowColor(const QColor& c);
    float distance() const;
    void setDistance(float d);
    float angle() const;
    void setAngle(float a);
    float softness() const;
    void setSoftness(float s);
    float opacity() const;
    void setOpacity(float o);
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "inner_shadow";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }
    bool supportsGPU() const override { return true; }

    // The resident shader gathers from a fixed 32-pixel box, so a wider shadow
    // stays on the CPU where cv::GaussianBlur has no such cap.
    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return softness_ <= 12.8f ? GpuRasterEffectDomain::Spatial
                                  : GpuRasterEffectDomain::None;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        if (gpuRasterEffectDomain() != GpuRasterEffectDomain::Spatial) {
            return false;
        }
        const float rad = angle_ * (3.14159265358979f / 180.0f);
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = -distance_ * std::cos(rad);
        node.parameters[1] = distance_ * std::sin(rad);
        node.parameters[2] = softness_;
        node.parameters[3] =
            std::clamp(opacity_ / 100.0f, 0.0f, 1.0f) * shadowColor_.alphaF();
        node.parameters[4] = shadowColor_.redF();
        node.parameters[5] = shadowColor_.greenF();
        node.parameters[6] = shadowColor_.blueF();
        node.resolutionScaledParameterMask = (1u << 0) | (1u << 1) | (1u << 2);
        return stack.append(node);
    }

    // The inset shadow offsets the source alpha by up to distance_ pixels and
    // then blurs it with sigma = softness_, mirroring DropShadowEffect's
    // pipeline. The offset decides which side of the frame the sampled alpha
    // comes from, so it widens the required input alongside the blur reach.
    EffectROIHint roiHint() const override {
        if (!allowOverscan()) {
            return EffectROIHint{};
        }
        return EffectROIHint{
            .kind = EffectROIHintKind::Blur,
            .expansionPixels = distance_ + softness_ * 3.0f,
            .requiresFullFrame = false
        };
    }
private:
    QColor shadowColor_ = QColor(0, 0, 0, 180);
    float distance_ = 5.0f;
    float angle_ = 135.0f;
    float softness_ = 8.0f;
    float opacity_ = 75.0f;
    void syncImpls();
};
}
