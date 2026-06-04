module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Mosaic;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class MosaicEffect : public ArtifactAbstractEffect {
private:
    float cellSize_ = 8.0f;
    bool shapeMode_ = false;
    void syncImpls();

public:
    MosaicEffect();
    ~MosaicEffect() override;

    float cellSize() const;
    void setCellSize(float v);
    bool shapeMode() const;
    void setShapeMode(bool v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
