module;
#include <vector>
#include <QString>

export module Artifact.Effect.CornerPin;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Property.Types;

export namespace Artifact {

class ArtifactCornerPinEffect : public ArtifactAbstractEffect {
public:
    ArtifactCornerPinEffect();
    virtual ~ArtifactCornerPinEffect();

    virtual std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    virtual void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

protected:
    virtual void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
