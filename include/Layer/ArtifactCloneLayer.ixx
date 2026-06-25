module;
#include <utility>
#include <vector>
#include <memory>
#include <QString>
#include <QVariant>
#include <QVector3D>
#include <QMatrix4x4>
#include <QJsonObject>
#include <Graphics/InstanceData.h>
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
    Linear = 0,
    LinearJitter = 1,
    Curve = 2,
    Grid = 3,
    Radial = 4,
    Random = 5,
    Spline = 6
};

class ArtifactCloneLayerSettings {
public:
    struct TransformStage {
        QVector3D offset = QVector3D(0.0f, 0.0f, 0.0f);
        QVector3D scale = QVector3D(1.0f, 1.0f, 1.0f);
        float rotation = 0.0f;
        bool enabled = false;
    };

    CloneMode mode = CloneMode::Linear;
    int cloneCount = 5;
    
    // Linear
    QVector3D offset = QVector3D(100.0f, 0.0f, 0.0f);
    QVector3D jitter = QVector3D(20.0f, 20.0f, 0.0f);
    int seed = 1;

    // Curve
    float curveRadius = 300.0f;
    float curveStartAngle = -60.0f;
    float curveEndAngle = 60.0f;
    
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
    int sourceIndex = 0;

    // Transform stages
    TransformStage transform1;
    TransformStage transform2;
    TransformStage transform3;
    
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
    QJsonObject toJson() const override;
    void fromJsonProperties(const QJsonObject& obj) override;

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

    // Mesh instancing support (Phase 2)
    // Convert CloneData array to InstanceData and submit to MeshRenderer
    std::vector<ArtifactCore::InstanceData> getInstanceData() const;
};

} // namespace Artifact
