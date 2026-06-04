module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>

export module Artifact.Effect.Rasterizer.RadialShadow;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class RadialShadowEffect : public ArtifactAbstractEffect {
private:
    QColor color_ = QColor(0, 0, 0, 180);
    float distance_ = 10.0f;
    float softness_ = 8.0f;
    float opacity_ = 0.75f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    void syncImpls();

public:
    RadialShadowEffect();
    ~RadialShadowEffect() override;

    QColor color() const;
    void setColor(const QColor& v);
    float distance() const;
    void setDistance(float v);
    float softness() const;
    void setSoftness(float v);
    float opacity() const;
    void setOpacity(float v);
    float centerX() const;
    void setCenterX(float v);
    float centerY() const;
    void setCenterY(float v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
