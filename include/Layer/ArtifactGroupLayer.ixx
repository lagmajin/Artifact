module;
#include <utility>
#include <vector>
#include <memory>
#include <QString>
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

class ArtifactGroupLayer : public ArtifactAbstractLayer {
public:
    ArtifactGroupLayer();
    ~ArtifactGroupLayer() override;

    bool isGroupLayer() const override;
    void setComposition(void *comp) override;
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
    // Offscreen rendering helpers
    void renderToOffscreen(ArtifactIRenderer* renderer);
    void drawChildrenDirect(ArtifactIRenderer* renderer);
    std::shared_ptr<GroupOffscreenTexture> createOffscreenTexture(
        ArtifactIRenderer* renderer, int width, int height);
    void applyGroupEffects(
        ArtifactIRenderer* renderer,
        const std::shared_ptr<GroupOffscreenTexture>& offscreen,
        const QRectF& bounds);

    class GroupImpl;
    std::unique_ptr<GroupImpl> groupImpl_;
};

} // namespace Artifact
