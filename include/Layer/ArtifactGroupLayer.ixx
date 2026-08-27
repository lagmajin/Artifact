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
import Memory.SharedPtr;

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

class ArtifactContainerLayer : public ArtifactAbstractLayer {
public:
    ~ArtifactContainerLayer() override = default;

    // Container identity is intentionally separate from the concrete UI name
    // (Group, Precomp, Switch, etc.).
    virtual bool isContainerLayer() const { return true; }

    virtual void addChild(ArtifactAbstractLayerPtr layer) = 0;
    virtual void removeChild(const LayerID& id) = 0;
    virtual void clearChildren() = 0;
    virtual const std::vector<ArtifactAbstractLayerPtr>& children() const = 0;
    virtual void insertChildAt(int index, ArtifactAbstractLayerPtr layer) = 0;
    virtual int childIndex(const LayerID& id) const = 0;
    virtual bool containsChild(const LayerID& id) const = 0;
};

class ArtifactGroupLayer : public ArtifactContainerLayer {
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
    void addChild(ArtifactAbstractLayerPtr layer) override;
    void removeChild(const LayerID& id) override;
    void clearChildren() override;
    const std::vector<ArtifactAbstractLayerPtr>& children() const override;
    void insertChildAt(int index, ArtifactAbstractLayerPtr layer) override;
    int childIndex(const LayerID& id) const override;
    bool containsChild(const LayerID& id) const override;

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
    ArtifactCore::SharedPtr<GroupOffscreenTexture> createOffscreenTexture(
        ArtifactIRenderer* renderer, int width, int height);
    void applyGroupEffects(
        ArtifactIRenderer* renderer,
        const ArtifactCore::SharedPtr<GroupOffscreenTexture>& offscreen,
        const QRectF& bounds);
    std::vector<ArtifactAbstractLayerPtr> childrenForRender() const;
    void promoteEmbeddedChildrenToComposition();

    class GroupImpl;
    std::unique_ptr<GroupImpl> groupImpl_;
};

// Compatibility name for the higher-level container abstraction.  The
// concrete implementation remains ArtifactGroupLayer until hierarchy and
// render-boundary responsibilities are migrated in later phases.
using ArtifactGroupContainer = ArtifactGroupLayer;

} // namespace Artifact
