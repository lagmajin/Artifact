module;
#include <memory>
#include <vector>
#include <cstdint>

export module Artifact.Layer.SandSim2D;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Physics.SandSim2D;
import Image.ImageF32x4_RGBA;
import FloatRGBA;
import Property.Abstract;
import Property.Group;

export namespace Artifact {

class ArtifactSandSim2DLayer : public ArtifactAbstractLayer {
private:
    class Impl;
    Impl* impl_;
    ArtifactSandSim2DLayer(const ArtifactSandSim2DLayer&) = delete;
    ArtifactSandSim2DLayer& operator=(const ArtifactSandSim2DLayer&) = delete;
public:
    ArtifactSandSim2DLayer();
    ~ArtifactSandSim2DLayer();

    void draw(ArtifactIRenderer* renderer) override;

    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

    QImage toQImage() const;
    QJsonObject toJson() const override;
    void fromJsonProperties(const QJsonObject& obj) override;
    static std::shared_ptr<ArtifactSandSim2DLayer> fromJson(const QJsonObject& obj);

    bool isAdjustmentLayer() const override;
    bool isNullLayer() const override;

    // Sand simulation controls
    void setSimResolution(int cellsPerDimension);
    int simResolution() const;
    void setToolMaterial(ArtifactCore::SandMaterial mat);
    ArtifactCore::SandMaterial toolMaterial() const;
    void setToolRadius(int radius);
    int toolRadius() const;
    void paintAt(int x, int y);
    void clearSim();

    ArtifactCore::SandSim2D* simulation();
};

} // namespace Artifact
