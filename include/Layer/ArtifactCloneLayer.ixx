module;
#include <QString>
#include <QVariant>
#include <QVector3D>
#include <QMatrix4x4>
#include <vector>
#include <memory>

export module Artifact.Layer.Clone;

import Artifact.Layers;
import Artifact.Effect.Clone.Core;
import Artifact.Effect.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class ArtifactCloneLayerSettings {
public:
    int cloneCount = 3;
    QVector3D offset = QVector3D(160.0f, 48.0f, 0.0f);
    float rotationStep = 0.0f;
    float opacityDecay = 0.0f;
    bool useEffector = false;
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
};

} // namespace Artifact
