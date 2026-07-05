module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Feedback;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Recursive feedback effect: feeds the output back into
/// subsequent frames with configurable transform and decay.
/// Classic VJ / psychedelic trail effect.
///
/// Uses EffectTemporalSampleMode::PreviousFrame internally.
class FeedbackEffect : public ArtifactAbstractEffect {
public:
    FeedbackEffect();
    ~FeedbackEffect() override;

    /// 0.0 = no feedback, 1.0 = pure feedback.
    float amount() const;
    void  setAmount(float v);

    /// Decay per frame (0 = instant fade, 1 = infinite).
    float decay() const;
    void  setDecay(float v);

    /// Feedback center offset in pixels.
    float centerOffsetX() const;
    float centerOffsetY() const;
    void  setCenterOffsetX(float v);
    void  setCenterOffsetY(float v);

    /// Scale of feedback image (1.0 = same size).
    float zoom() const;
    void  setZoom(float v);

    /// Rotation of feedback in degrees.
    float rotation() const;
    void  setRotation(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float amount_ = 0.5f;
    float decay_  = 0.9f;
    float cx_ = 0.0f, cy_ = 0.0f;
    float zoom_  = 1.0f;
    float rotation_ = 0.0f;
    void syncImpls();
};

} // namespace Artifact
