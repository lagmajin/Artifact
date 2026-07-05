module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Strobe;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Strobes between current frame and a frozen reference frame.
/// Creates flashing / pulse effects. Reference frame is captured
/// on first frame or manually triggered.
class StrobeEffect : public ArtifactAbstractEffect {
public:
    StrobeEffect();
    ~StrobeEffect() override;

    /// Frequency in Hz (strobes per second).
    float frequency() const;
    void  setFrequency(float v);

    /// 0=alternate, 0.5=50% mix when "on", 1=solid reference.
    float mixAmount() const;
    void  setMixAmount(float v);

    void captureReference();

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float frequency_=4.0f, mixAmount_=0.0f;
    void syncImpls();
};

} // namespace Artifact
