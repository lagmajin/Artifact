module;
#include <QString>
#include <QVariant>
#include <vector>

export module Artifact.Effect.Distort.ImageMorph;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import ImageProcessing.Distortion;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {
class ImageMorphEffect final : public ArtifactAbstractEffect {
public:
    ImageMorphEffect();
    ~ImageMorphEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return false; }
private:
    QString targetImagePath_;
    QString controlPointsJson_;
    float amount_ = 0.0f;
    void syncImpl();
};
}
