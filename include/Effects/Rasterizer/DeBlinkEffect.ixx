module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.DeBlink;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Single-frame exposure anomaly (film blink) correction: unlike Deflicker,
/// which smooths continuously, only frames deviating from BOTH neighbors
/// are gain-corrected. Neighbor disagreement marks a scene cut and is left
/// untouched. Stateless (neighbor sampling), so scrub-safe.
class DeBlinkEffect : public ArtifactAbstractEffect {
public:
    DeBlinkEffect();
    ~DeBlinkEffect() override;

    /// Minimum deviation from both neighbors to count as a blink (luma).
    double threshold() const;
    void setThreshold(double v);

    /// Strength of correction (0=none, 1=full).
    double strength() const;
    void setStrength(double v);

    /// Neighbor disagreement above this marks a scene cut (no correction).
    double sceneCut() const;
    void setSceneCut(double v);

    /// Maximum correction gain.
    double maxGain() const;
    void setMaxGain(double v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return true; }

private:
    double threshold_=0.06, strength_=1.0, sceneCut_=0.25, maxGain_=4.0;
    void syncImpls();
};

} // namespace Artifact
