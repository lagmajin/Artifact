module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Deflicker;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Temporal deflicker: stabilizes luminance fluctuations
/// across frames by analyzing per-frame mean brightness and
/// compensating. Essential for timelapse sequences.
class DeflickerEffect : public ArtifactAbstractEffect {
public:
    DeflickerEffect();
    ~DeflickerEffect() override;

    /// Window size for luminance averaging (frames).
    int   windowSize() const;
    void  setWindowSize(int v);

    /// Strength of correction (0=none, 1=full).
    float strength() const;
    void  setStrength(float v);

    /// 0=RGB equally, 1=luma-weighted.
    float lumaWeight() const;
    void  setLumaWeight(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    int windowSize_=16; float strength_=1.0f,lumaWeight_=1.0f;
    void syncImpls();
};

} // namespace Artifact
