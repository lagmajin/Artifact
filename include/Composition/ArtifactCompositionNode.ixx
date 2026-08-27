module;
#include <vector>

export module Artifact.Composition.Node;

import Utils.Id;

export namespace Artifact {

enum class CompositionNodeKind {
    Layer,
    Container,
};

class ArtifactCompositionNode {
public:
    explicit ArtifactCompositionNode(const ArtifactCore::Id& id = ArtifactCore::Id::Nil());
    virtual ~ArtifactCompositionNode() = default;

    const ArtifactCore::Id& id() const;
    const ArtifactCore::Id& parentId() const;
    void setParentId(const ArtifactCore::Id& parentId);
    virtual CompositionNodeKind kind() const = 0;

private:
    ArtifactCore::Id id_;
    ArtifactCore::Id parentId_;
};

class ArtifactContainerNode : public ArtifactCompositionNode {
public:
    explicit ArtifactContainerNode(const ArtifactCore::Id& id = ArtifactCore::Id::Nil());

    CompositionNodeKind kind() const override;
    bool addChild(const ArtifactCore::Id& childId);
    bool removeChild(const ArtifactCore::Id& childId);
    bool containsChild(const ArtifactCore::Id& childId) const;
    const std::vector<ArtifactCore::Id>& children() const;

private:
    std::vector<ArtifactCore::Id> children_;
};

using ArtifactGroupContainer = ArtifactContainerNode;

} // namespace Artifact
