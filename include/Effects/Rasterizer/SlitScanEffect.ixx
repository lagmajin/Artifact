module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.SlitScan;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Classic slit-scan: accumulates single pixel row/column slices
/// from successive frames into a persistent buffer, creating
/// temporal displacement along one axis.
class SlitScanEffect : public ArtifactAbstractEffect {
public:
    SlitScanEffect();
    ~SlitScanEffect() override;

    /// 0=horizontal (columns slide), 1=vertical (rows slide).
    float direction() const;
    void  setDirection(float v);

    /// Pixels per frame the slit advances.
    float speed() const;
    void  setSpeed(float v);

    /// Persistence of old slices (0=instant, 1=infinite).
    float persistence() const;
    void  setPersistence(float v);

    /// Slit position (0-1, normalised). -1 = auto-advance.
    float position() const;
    void  setPosition(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float direction_=0.0f, speed_=2.0f, persistence_=0.95f, position_=-1.0f;
    void syncImpls();
};

} // namespace Artifact
