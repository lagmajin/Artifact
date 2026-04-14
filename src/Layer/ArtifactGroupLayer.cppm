module;
#include <utility>
#include <algorithm>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

module Artifact.Layer.Group;

import std;

import Artifact.Layer.Abstract;
import Utils.Id;
import Artifact.Render.IRenderer;
import Artifact.Composition.Abstract;

namespace Artifact {

class ArtifactGroupLayer::GroupImpl {
public:
    std::vector<ArtifactAbstractLayerPtr> children;
};

ArtifactGroupLayer::ArtifactGroupLayer()
    : groupImpl_(std::make_unique<GroupImpl>()) {
    setLayerName("Layer Group");
    // setLayerType(LayerType::Group); // Implicitly via constructor initialization
}

ArtifactGroupLayer::~ArtifactGroupLayer() = default;

bool ArtifactGroupLayer::isGroupLayer() const {
    return true;
}

void ArtifactGroupLayer::setComposition(void *comp) {
    ArtifactAbstractLayer::setComposition(comp);
    for (auto& child : groupImpl_->children) {
        if (child) {
            child->setComposition(comp);
        }
    }
}

void ArtifactGroupLayer::draw(ArtifactIRenderer* renderer) {
    if (!isVisible() || opacity() <= 0.0f || groupImpl_->children.empty()) return;

    // Render children to offscreen texture first, then apply group effects
    renderToOffscreen(renderer);
}

void ArtifactGroupLayer::addChild(ArtifactAbstractLayerPtr layer) {
    if (!layer) return;
    
    // Prevent self-addition or cyclic
    if (layer->id() == this->id()) return;

    // Set parenting in the core system
    layer->setParentById(this->id());
    layer->setComposition(composition());
    
    groupImpl_->children.push_back(layer);
    Q_EMIT changed();
}

void ArtifactGroupLayer::removeChild(const LayerID& id) {
    auto it = std::remove_if(groupImpl_->children.begin(), groupImpl_->children.end(), 
        [&](const auto& l) {
            if (!l || l->id() != id) {
                return false;
            }
            l->clearParent();
            l->setComposition(nullptr);
            return true;
        });
    
    if (it != groupImpl_->children.end()) {
        groupImpl_->children.erase(it, groupImpl_->children.end());
        Q_EMIT changed();
    }
}

void ArtifactGroupLayer::clearChildren() {
    for (auto& child : groupImpl_->children) {
        if (!child) {
            continue;
        }
        child->clearParent();
        child->setComposition(nullptr);
    }
    groupImpl_->children.clear();
    Q_EMIT changed();
}

const std::vector<ArtifactAbstractLayerPtr>& ArtifactGroupLayer::children() const {
    return groupImpl_->children;
}

QRectF ArtifactGroupLayer::localBounds() const {
    // Group bounds depends on its children's transformed bounds
    QRectF bounds;
    for (const auto& child : groupImpl_->children) {
        if (child) {
            bounds = bounds.united(child->transformedBoundingBox());
        }
    }
    return bounds.isValid() ? bounds : QRectF(0, 0, 100, 100);
}

QJsonObject ArtifactGroupLayer::toJson() const {
    QJsonObject obj = ArtifactAbstractLayer::toJson();
    obj["type"] = static_cast<int>(LayerType::Group);
    obj["childCount"] = static_cast<int>(groupImpl_->children.size());
    
    QJsonArray childrenArr;
    for (const auto& child : groupImpl_->children) {
        if (child) {
            childrenArr.append(child->toJson());
        }
    }
    obj["children"] = childrenArr;
    
    return obj;
}

void ArtifactGroupLayer::fromJsonProperties(const QJsonObject& obj) {
    ArtifactAbstractLayer::fromJsonProperties(obj);
    clearChildren();

    if (obj.contains("children") && obj["children"].isArray()) {
        QJsonArray childrenArr = obj["children"].toArray();
        for (const auto& v : childrenArr) {
            QJsonObject childObj = v.toObject();
            auto child = ArtifactAbstractLayer::fromJson(childObj);
            if (child) {
                addChild(child);
            }
        }
    }
}

void ArtifactGroupLayer::renderToOffscreen(ArtifactIRenderer* renderer) {
    if (!renderer || groupImpl_->children.empty()) return;

    // Get group bounds
    QRectF bounds = localBounds();
    int width = std::max(1, static_cast<int>(bounds.width()));
    int height = std::max(1, static_cast<int>(bounds.height()));

    // Create offscreen render target
    auto offscreen = createOffscreenTexture(renderer, width, height);
    if (!offscreen) {
        // Fallback: draw children directly
        drawChildrenDirect(renderer);
        return;
    }

    // Render children to offscreen texture
    renderer->pushRenderTarget(offscreen);
    renderer->clearRenderTarget({0.0f, 0.0f, 0.0f, 0.0f});

    for (const auto& child : groupImpl_->children) {
        if (child && child->isVisible()) {
            child->draw(renderer);
        }
    }

    renderer->popRenderTarget();

    // Apply group opacity and effects to the offscreen texture
    applyGroupEffects(renderer, offscreen, bounds);
}

void ArtifactGroupLayer::drawChildrenDirect(ArtifactIRenderer* renderer) {
    // Fallback: draw children without offscreen pass
    for (const auto& child : groupImpl_->children) {
        if (child && child->isVisible()) {
            child->draw(renderer);
        }
    }
}

std::shared_ptr<GroupOffscreenTexture> ArtifactGroupLayer::createOffscreenTexture(
    ArtifactIRenderer* renderer, int width, int height) {
    
    if (!renderer) return nullptr;

    // Try to create offscreen texture via renderer
    auto texture = renderer->createOffscreenTexture(width, height);
    if (texture) {
        return std::make_shared<GroupOffscreenTexture>(texture, width, height);
    }

    return nullptr;
}

void ArtifactGroupLayer::applyGroupEffects(
    ArtifactIRenderer* renderer,
    const std::shared_ptr<GroupOffscreenTexture>& offscreen,
    const QRectF& bounds) {
    
    if (!renderer || !offscreen) return;

    // Apply group opacity
    float groupOpacity = opacity();
    
    // Draw the offscreen texture with group opacity
    renderer->drawOffscreenTexture(
        offscreen->textureView,
        bounds,
        groupOpacity
    );

    // TODO: Apply group blend mode
    // TODO: Apply group effects stack
}

} // namespace Artifact
