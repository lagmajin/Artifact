module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Dithering;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

enum class DitherAlgorithm {
    Bayer2x2,
    Bayer4x4,
    Bayer8x8,
    Bayer16x16,
    FloydSteinberg,
    Atkinson,
    Sierra,
    Stucki
};

class DitheringEffect : public ArtifactAbstractEffect {
private:
    DitherAlgorithm algorithm_ = DitherAlgorithm::Bayer4x4;
    int colorCount_ = 16;
    float amount_ = 1.0f;
    float patternScale_ = 1.0f;
    void syncImpls();

public:
    DitheringEffect();
    ~DitheringEffect() override;

    DitherAlgorithm algorithm() const;
    void setAlgorithm(DitherAlgorithm v);
    int colorCount() const;
    void setColorCount(int v);
    float amount() const;
    void setAmount(float v);
    float patternScale() const;
    void setPatternScale(float v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return true; }
};

}

