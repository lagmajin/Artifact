module;
#include <memory>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.TemporalDenoise;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Temporal noise reduction using multi-frame averaging.
/// Averages the current frame with the previous N frames
/// using per-pixel variance clamping to avoid ghosting on
/// fast-moving objects.
///
/// Uses EffectTemporalSampleMode::PreviousFrame + relative offset.
class TemporalDenoiseEffect : public ArtifactAbstractEffect {
public:
    TemporalDenoiseEffect();
    ~TemporalDenoiseEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.temporal_denoise.strength"),
                QStringLiteral("strength"),
                0.0, 1.0, 0.01, 0.0025, 1.0, QString()};
        }
        if (index == 2) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.temporal_denoise.history_frames"),
                QStringLiteral("frameCount"),
                1.0, 8.0, 1.0, 0.02, 1.0, QStringLiteral("frames")};
        }
        return std::nullopt;
    }

    std::optional<EffectViewportToggleParameter>
    viewportToggleParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::viewportToggleParameter(index);
        }
        if (index == 1) {
            return EffectViewportToggleParameter{
                QStringLiteral("effect.temporal_denoise.motion_adaptive"),
                QStringLiteral("motionAdaptive")};
        }
        if (index == 2) {
            return EffectViewportToggleParameter{
                QStringLiteral("effect.temporal_denoise.reject_scene_cuts"),
                QStringLiteral("rejectSceneCuts")};
        }
        return std::nullopt;
    }

    /// 0.0 = no denoise, 1.0 = max blend with history.
    float strength() const;
    void  setStrength(float v);

    /// Number of past frames to reference (1–8).
    int   frameCount() const;
    void  setFrameCount(int v);

    /// Variance threshold: below this, pixel is considered static
    /// and gets full temporal blend; above, blend is reduced.
    float varianceThreshold() const;
    void  setVarianceThreshold(float v);

    bool motionAdaptive() const;
    void setMotionAdaptive(bool enabled);
    float ghostSuppression() const;
    void setGhostSuppression(float v);

    bool rejectSceneCuts() const;
    void setRejectSceneCuts(bool enabled);
    float sceneCutThreshold() const;
    void setSceneCutThreshold(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float strength_          = 0.5f;
    int   frameCount_        = 3;
    float varianceThreshold_ = 0.05f;
    bool motionAdaptive_ = true;
    float ghostSuppression_ = 1.0f;
    bool rejectSceneCuts_ = true;
    float sceneCutThreshold_ = 0.25f;
    void syncImpls();
};

} // namespace Artifact
