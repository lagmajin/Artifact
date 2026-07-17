module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.HexGrid;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Hexagonal grid / honeycomb pattern generator.
class HexGridEffect : public ArtifactAbstractEffect {
public:
    HexGridEffect();
    ~HexGridEffect() override;

    float cellSize() const;
    void  setCellSize(float v);
    float lineWidth() const;
    void  setLineWidth(float v);
    float angle() const;
    void  setAngle(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return true; }

private:
    float cellSize_=32.0f,lineWidth_=2.0f,angle_=0.0f;
    void syncImpls();
};

} // namespace Artifact
