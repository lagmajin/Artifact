module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.RadialBlur;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class RadialBlurEffect : public ArtifactAbstractEffect {
private:
    float amount_ = 10.0f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    int samples_ = 16;
    int type_ = 0;
    void syncImpls();

public:
    RadialBlurEffect();
    ~RadialBlurEffect() override;

    float amount() const;
    void setAmount(float v);
    float centerX() const;
    void setCenterX(float v);
    float centerY() const;
    void setCenterY(float v);
    int samples() const;
    void setSamples(int v);
    int type() const;
    void setType(int v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
