module;
#include <algorithm>
#include <cmath>
#include <vector>
#include <QString>
#include <QVariant>

export module SurfaceMemoryEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

// Stores a decaying image history in layer-local surface coordinates and adds
// it to the current frame. GPU residency is intentionally deferred: the
// research contract is CPU-only and the host needs a dedicated persistent
// texture before a GPU path can be claimed.
class SurfaceMemoryEffect : public ArtifactAbstractEffect {
private:
    float decay_ = 0.92f;
    float gain_ = 0.8f;

    void syncImpls();

public:
    SurfaceMemoryEffect();
    ~SurfaceMemoryEffect() override;

    void setDecay(float value);
    float decay() const { return decay_; }
    void setGain(float value);
    float gain() const { return gain_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsCPU() const override { return true; }
    bool supportsGPU() const override { return false; }
};

} // namespace Artifact
