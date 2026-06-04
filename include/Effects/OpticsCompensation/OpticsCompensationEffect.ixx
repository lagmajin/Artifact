module;
#include <vector>
#include <cmath>

export module Artifact.Effect.Rasterizer.OpticsCompensation;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import ImageProcessing.Distortion;
import Utils.String.UniString;

export namespace Artifact {

class OpticsCompensationEffect : public ArtifactAbstractEffect {
private:
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float fov_ = 45.0f;
    int direction_ = 1;
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst);

public:
    OpticsCompensationEffect();
    ~OpticsCompensationEffect() override = default;

    float centerX() const;
    void setCenterX(float v);
    float centerY() const;
    void setCenterY(float v);
    float fov() const;
    void setFov(float v);
    int direction() const;
    void setDirection(int v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
