module;
#include <algorithm>

module Artifact.Composition.Node;

namespace Artifact {

ArtifactCompositionNode::ArtifactCompositionNode(const ArtifactCore::Id& id)
    : id_(id), parentId_(ArtifactCore::Id::Nil()) {}

const ArtifactCore::Id& ArtifactCompositionNode::id() const { return id_; }
const ArtifactCore::Id& ArtifactCompositionNode::parentId() const { return parentId_; }
void ArtifactCompositionNode::setParentId(const ArtifactCore::Id& parentId) { parentId_ = parentId; }

ArtifactContainerNode::ArtifactContainerNode(const ArtifactCore::Id& id)
    : ArtifactCompositionNode(id) {}

CompositionNodeKind ArtifactContainerNode::kind() const { return CompositionNodeKind::Container; }

bool ArtifactContainerNode::addChild(const ArtifactCore::Id& childId) {
    if (childId.isNil() || containsChild(childId)) return false;
    children_.push_back(childId);
    return true;
}

bool ArtifactContainerNode::removeChild(const ArtifactCore::Id& childId) {
    const auto it = std::find(children_.begin(), children_.end(), childId);
    if (it == children_.end()) return false;
    children_.erase(it);
    return true;
}

bool ArtifactContainerNode::containsChild(const ArtifactCore::Id& childId) const {
    return std::find(children_.begin(), children_.end(), childId) != children_.end();
}

const std::vector<ArtifactCore::Id>& ArtifactContainerNode::children() const { return children_; }

bool ArtifactCompositionNodeStore::add(std::unique_ptr<ArtifactCompositionNode> node) {
    if (!node || node->id().isNil() || find(node->id())) return false;
    nodes_.push_back(std::move(node));
    return true;
}

bool ArtifactCompositionNodeStore::remove(const ArtifactCore::Id& id) {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [&id](const auto& node) { return node && node->id() == id; });
    if (it == nodes_.end()) return false;
    nodes_.erase(it);
    return true;
}

ArtifactCompositionNode* ArtifactCompositionNodeStore::find(const ArtifactCore::Id& id) const {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [&id](const auto& node) { return node && node->id() == id; });
    return it == nodes_.end() ? nullptr : it->get();
}

std::size_t ArtifactCompositionNodeStore::size() const { return nodes_.size(); }

} // namespace Artifact
