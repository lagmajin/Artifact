module;
#include <utility>
#include <vector>
#include <memory>
#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QRectF>

export module Artifact.Layer.Group;

import Artifact.Layer.Abstract;
import Utils.Id;
import Artifact.Render.IRenderer;

export namespace Artifact {

struct GroupOffscreenTexture {
    void* textureView;  // ITextureView*
    int width;
    int height;
    
    GroupOffscreenTexture(void* tex, int w, int h)
        : textureView(tex), width(w), height(h) {}
};

enum class GroupOutputMode {
    All = 0,
    Single = 1,
    Share = 2
};

class ArtifactGroupLayer : public ArtifactAbstractLayer {
public:
    ArtifactGroupLayer();
    ~ArtifactGroupLayer() override;

    bool isGroupLayer() const override;
    bool hasExclusiveChildSelection() const override;
    LayerID selectedChildIdForEvaluation() const override;
    float childEvaluationGain(const LayerID& childId) const override;
    void setComposition(QObject *comp) override;
    void setComposition(void *comp) override;
    void draw(ArtifactIRenderer* renderer) override;

    // Child management
    void addChild(ArtifactAbstractLayerPtr layer);
    void removeChild(const LayerID& id);
    void clearChildren();
    const std::vector<ArtifactAbstractLayerPtr>& children() const;
    void insertChildAt(int index, ArtifactAbstractLayerPtr layer);
    int childIndex(const LayerID& id) const;
    bool containsChild(const LayerID& id) const;

    // Group state
    bool isCollapsed() const;
    void setCollapsed(bool collapsed);

    // Render policy. Composite renders all children; multiplex renders the
    // selected child only, while preserving the group's composite boundary.
    bool isMultiplexer() const;
    void setMultiplexer(bool enabled);
    GroupOutputMode outputMode() const;
    void setOutputMode(GroupOutputMode mode);
    LayerID activeChildId() const;
    void setActiveChildId(const LayerID& id);
    ArtifactAbstractLayerPtr activeChild() const;

    // Serialization
    QJsonObject toJson() const override;
    void fromJsonProperties(const QJsonObject& obj) override;
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

    QRectF localBounds() const override;

private:
    // Offscreen rendering helpers
    void renderToOffscreen(ArtifactIRenderer* renderer);
    void drawChildrenDirect(ArtifactIRenderer* renderer);
    std::shared_ptr<GroupOffscreenTexture> createOffscreenTexture(
        ArtifactIRenderer* renderer, int width, int height);
    void applyGroupEffects(
        ArtifactIRenderer* renderer,
        const std::shared_ptr<GroupOffscreenTexture>& offscreen,
        const QRectF& bounds);
    std::vector<ArtifactAbstractLayerPtr> childrenForRender() const;
    void promoteEmbeddedChildrenToComposition();

    class GroupImpl;
    std::unique_ptr<GroupImpl> groupImpl_;
};

} // namespace Artifact
