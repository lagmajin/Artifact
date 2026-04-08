module;
#include <utility>
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

void ArtifactGroupLayer::draw(ArtifactIRenderer* renderer) {
    if (!isVisible() || opacity() <= 0.0f) return;

    // TODO: In the future, we should render children to a temporary texture
    // and apply effects/blending of the Group layer to that texture.
    // For now, we simply draw them in sequence (painter's algorithm).
    
    // We sort by reverse order if they were just indices, but here they are a vector.
    // Assuming the vector order is back-to-front.
    for (auto& child : groupImpl_->children) {
        if (child && child->isVisible()) {
            child->draw(renderer);
        }
    }
}

void ArtifactGroupLayer::addChild(ArtifactAbstractLayerPtr layer) {
    if (!layer) return;
    
    // Prevent self-addition or cyclic
    if (layer->id() == this->id()) return;

    // Set parenting in the core system
    layer->setParentById(this->id());
    
    groupImpl_->children.push_back(layer);
    Q_EMIT changed();
}

void ArtifactGroupLayer::removeChild(const LayerID& id) {
    auto it = std::remove_if(groupImpl_->children.begin(), groupImpl_->children.end(), 
        [&](const auto& l) { return l->id() == id; });
    
    if (it != groupImpl_->children.end()) {
        (*it)->clearParent();
        groupImpl_->children.erase(it, groupImpl_->children.end());
        Q_EMIT changed();
    }
}

void ArtifactGroupLayer::clearChildren() {
    for (auto& child : groupImpl_->children) {
        child->clearParent();
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

} // namespace Artifact
