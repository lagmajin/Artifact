module;
#include <QString>
#include <QColor>
#include <QVariant>
#include <vector>
export module Artifact.Effect.Rasterizer.Stroke;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;
export namespace Artifact {
class StrokeEffect : public ArtifactAbstractEffect {
public:
    StrokeEffect();
    ~StrokeEffect() override;
    QColor strokeColor() const;
    void setStrokeColor(const QColor& c);
    float width() const;
    void setWidth(float w);
    float opacity() const;
    void setOpacity(float o);
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return true; }
private:
    QColor strokeColor_ = QColor(255, 255, 255, 255);
    float width_ = 3.0f;
    float opacity_ = 100.0f;
    void syncImpls();
};
}
