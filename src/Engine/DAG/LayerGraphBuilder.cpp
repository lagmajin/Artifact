module;

#include <array>

module Artifact.Engine.DAG.LayerGraphBuilder;

import Artifact.Engine.DAG.Port;
import Artifact.Engine.DAG.Node;
import Artifact.Engine.DAG.Connection;
import Artifact.Engine.DAG.Graph;
import Artifact.Effect.Abstract;
import Memory.SharedPtr;
import Utils.String.UniString;

namespace Artifact {

EffectGraph LayerGraphBuilder::build(
    const UniString& layerName,
    const std::vector<ArtifactAbstractEffectPtr>& effects)
{
    EffectGraph graph(layerName);
    constexpr int stageCount = 6;
    std::array<std::vector<ArtifactAbstractEffectPtr>, stageCount> stageEffects;
    for (const auto& effect : effects) {
        if (!effect) {
            continue;
        }
        const int index = static_cast<int>(effect->pipelineStage());
        if (index >= 0 && index < stageCount) {
            stageEffects[index].push_back(effect);
        }
    }

    std::array<std::vector<EffectNodePtr>, stageCount> stageNodes;
    int nodeCounter = 0;
    for (int stageIndex = 0; stageIndex < stageCount; ++stageIndex) {
        const auto stage = static_cast<EffectPipelineStage>(stageIndex);
        for (const auto& effect : stageEffects[stageIndex]) {
            const auto nodeId = NodeID(
                QStringLiteral("node_%1").arg(nodeCounter++));
            auto node = ArtifactCore::makeShared<EffectNode>(
                nodeId, effect->displayName(), stage, effect);
            graph.addNode(node);
            stageNodes[stageIndex].push_back(node);
        }
    }

    for (int stageIndex = 0; stageIndex < stageCount; ++stageIndex) {
        auto& nodes = stageNodes[stageIndex];
        for (std::size_t index = 1; index < nodes.size(); ++index) {
            graph.connect(nodes[index - 1]->id(), 0, nodes[index]->id(), 0);
        }
    }

    EffectNodePtr lastNode = nullptr;
    for (int stageIndex = 0; stageIndex < stageCount; ++stageIndex) {
        auto& nodes = stageNodes[stageIndex];
        if (nodes.empty()) {
            continue;
        }
        if (lastNode) {
            graph.connect(lastNode->id(), 0, nodes.front()->id(), 0);
        }
        lastNode = nodes.back();
    }

    graph.compile();
    return graph;
}

} // namespace Artifact
