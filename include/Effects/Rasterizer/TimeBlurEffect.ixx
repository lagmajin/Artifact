module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.TimeBlur;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Gaussian-weighted temporal blur. Blurs across time
/// (past and future frames) with a configurable sigma,
/// producing natural-looking motion blur without velocity
/// estimation.
class TimeBlurEffect : public ArtifactAbstractEffect {
public:
    TimeBlurEffect();
    ~TimeBlurEffect() override;

    /// Temporal sigma in frames (0.5-16).
    float sigma() const;
    void  setSigma(float v);

    /// Number of lookback frames (1-32).
    int   lookback() const;
    void  setLookback(int v);

    /// 0=centered, 1=trailing only.
    float direction() const;
    void  setDirection(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float sigma_=3.0f; int lookback_=8; float direction_=0.0f;
    void syncImpls();
};

} // namespace Artifact
