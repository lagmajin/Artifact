module;
#include <vector>
#include <memory>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

export module Artifact.Layer.Group;

import Artifact.Layer.Abstract;
import Utils.Id;
import Artifact.Render.IRenderer;

export namespace Artifact {

class ArtifactGroupLayer : public ArtifactAbstractLayer {
public:
    ArtifactGroupLayer();
    ~ArtifactGroupLayer() override;

    void draw(ArtifactIRenderer* renderer) override;
    
    // Child management
    void addChild(ArtifactAbstractLayerPtr layer);
    void removeChild(const LayerID& id);
    void clearChildren();
    const std::vector<ArtifactAbstractLayerPtr>& children() const;

    // Serialization
    QJsonObject toJson() const override;
    void fromJsonProperties(const QJsonObject& obj) override;

    QRectF localBounds() const override;

private:
    class GroupImpl;
    std::unique_ptr<GroupImpl> groupImpl_;
};

} // namespace Artifact
