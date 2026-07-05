module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.FreezeFrame;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Freeze (hold) the output at a specific past frame.
/// All subsequent frames show the frozen frame until reset.
class FreezeFrameEffect : public ArtifactAbstractEffect {
public:
    FreezeFrameEffect();
    ~FreezeFrameEffect() override;

    /// Trigger freeze at the current frame.
    void freeze();
    /// Release and show live frames again.
    void release();
    bool isFrozen() const;

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    void syncImpls();
};

} // namespace Artifact
