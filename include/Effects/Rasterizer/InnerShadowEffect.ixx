module;
#include <QString>
#include <QColor>
#include <QVariant>
#include <vector>
export module Artifact.Effect.Rasterizer.InnerShadow;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;
export namespace Artifact {
class InnerShadowEffect : public ArtifactAbstractEffect {
public:
    InnerShadowEffect();
    ~InnerShadowEffect() override;
    QColor shadowColor() const;
    void setShadowColor(const QColor& c);
    float distance() const;
    void setDistance(float d);
    float angle() const;
    void setAngle(float a);
    float softness() const;
    void setSoftness(float s);
    float opacity() const;
    void setOpacity(float o);
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
private:
    QColor shadowColor_ = QColor(0, 0, 0, 180);
    float distance_ = 5.0f;
    float angle_ = 135.0f;
    float softness_ = 8.0f;
    float opacity_ = 75.0f;
    void syncImpls();
};
}
