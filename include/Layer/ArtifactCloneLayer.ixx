module;
#include <utility>
#include <vector>
#include <memory>
#include <QString>
#include <QVariant>
#include <QVector3D>
#include <QMatrix4x4>
export module Artifact.Layer.Clone;


import Artifact.Layers;
import Artifact.Effect.Clone.Core;
import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Utils.Id;


export namespace Artifact {

using namespace ArtifactCore;



class MeshRenderer;  // forward declaration

enum class CloneMode {
    Linear,
    Grid,
    Radial
};

class ArtifactCloneLayerSettings {
public:
    CloneMode mode = CloneMode::Linear;
    int cloneCount = 5;
    
    // Linear
    QVector3D offset = QVector3D(100.0f, 0.0f, 0.0f);
    
    // Grid
    int columns = 3;
    int rows = 3;
    int depth = 1;
    QVector3D gridSpacing = QVector3D(200.0f, 200.0f, 200.0f);
    
    // Radial
    int radialCount = 8;
    float radius = 300.0f;
    float startAngle = 0.0f;
    float endAngle = 360.0f;
    
    float rotationStep = 0.0f;
    float opacityDecay = 0.0f;
    bool useEffector = true;
    
    LayerID sourceLayerId;
};


class ArtifactCloneLayer : public ArtifactAbstractLayer {
private:
    class Impl;
    Impl* impl_;
    ArtifactCloneLayer(const ArtifactCloneLayer&) = delete;
    ArtifactCloneLayer& operator=(const ArtifactCloneLayer&) = delete;
public:
    ArtifactCloneLayer();
    ~ArtifactCloneLayer();

    void draw(ArtifactIRenderer* renderer) override;

    bool isCloneLayer() const override;

    ArtifactCloneLayerSettings cloneSettings() const;
    void setCloneSettings(const ArtifactCloneLayerSettings& settings);

    std::vector<CloneData> generateCloneData() const;

    void addEffector(std::shared_ptr<AbstractCloneEffector> effector);
    void removeEffector(int index);
    void clearEffectors();
    int effectorCount() const;
    std::shared_ptr<AbstractCloneEffector> effectorAt(int index) const;

    QSize sourceSize() const;
    QRectF localBounds() const override;
    QImage toQImage() const;
    std::vector<AbstractProperty> getProperties() const;
    void setPropertyValue(const UniString& name, const QVariant& value);

    // Mesh instancing support (TODO: implement GpuContext abstraction)
    // void initializeMeshRenderer(GpuContext& context, size_t maxInstances, size_t vertexCount, size_t indexCount);
    // void setSourceMesh(const float* positions, const float* normals, const float* uvs, const uint32_t* indices);
};

} // namespace Artifact
