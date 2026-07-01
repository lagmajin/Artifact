module;
#include <QString>
#include <QColor>
#include <QVariant>
#include <vector>
export module Artifact.Effect.Rasterizer.Satin;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;
export namespace Artifact {
class SatinEffect : public ArtifactAbstractEffect {
public:
    SatinEffect();
    ~SatinEffect() override;
    QColor satinColor() const;
    void setSatinColor(const QColor& c);
    float distance() const;
    void setDistance(float d);
    float angle() const;
    void setAngle(float a);
    float softness() const;
    void setSoftness(float s);
    float opacity() const;
    void setOpacity(float o);
    bool invert() const;
    void setInvert(bool v);
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
private:
    QColor satinColor_ = QColor(200, 200, 200, 180);
    float distance_ = 0.0f;
    float angle_ = 0.0f;
    float softness_ = 5.0f;
    float opacity_ = 50.0f;
    bool invert_ = false;
    void syncImpls();
};
}
