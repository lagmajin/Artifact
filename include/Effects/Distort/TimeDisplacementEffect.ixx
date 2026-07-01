module;
#include <memory>
#include <vector>

#include <QString>
#include <QVariant>

export module Artifact.Effect.Distort.TimeDisplacement;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {

using namespace ArtifactCore;

enum class TimeDisplaceChannel : int {
    Luminance = 0,
    Red       = 1,
    Green     = 2,
    Blue      = 3,
    Alpha     = 4,
};

class TimeDisplacementEffect : public ArtifactAbstractEffect {
public:
    TimeDisplacementEffect();
    ~TimeDisplacementEffect();

    float maxOffsetFrames() const;
    void setMaxOffsetFrames(float value);

    TimeDisplaceChannel sourceChannel() const;
    void setSourceChannel(TimeDisplaceChannel channel);

    bool frameBlend() const;
    void setFrameBlend(bool value);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return false; }

private:
    float maxOffsetFrames_ = 12.0f;
    TimeDisplaceChannel sourceChannel_ = TimeDisplaceChannel::Luminance;
    bool frameBlend_ = true;

    void syncImpls();
};

} // export namespace Artifact
