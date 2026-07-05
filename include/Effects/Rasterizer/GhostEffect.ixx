module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Ghost;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Subtle ghosting via opacity-only temporal blend.
/// Simpler than Echo: only alpha-fades the previous frame
/// overlay without color mixing, preserving hue accuracy.
class GhostEffect : public ArtifactAbstractEffect {
public:
    GhostEffect();
    ~GhostEffect() override;

    /// Ghost opacity (0=none, 1=previous frame at full opacity).
    float opacity() const;
    void  setOpacity(float v);

    /// Number of ghost copies (1-8).
    int   ghostCount() const;
    void  setGhostCount(int v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float opacity_ = 0.3f;
    int   ghostCount_ = 3;
    void syncImpls();
};

} // namespace Artifact
