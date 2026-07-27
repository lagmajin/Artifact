module;
#include <memory>
#include <string>
#include <unordered_map>

module Artifact.Engine.DAG.CompositionGraphBuilder;

import Artifact.Composition.Abstract;
import Artifact.Engine.DAG.Node;
import Memory.SharedPtr;

namespace Artifact {

SharedPtr<EffectGraph> CompositionGraphBuilder::build(ArtifactAbstractComposition* comp)
{
    if (!comp) {
        return nullptr;
    }

    auto graph = makeShared<EffectGraph>(QString("Composition_%1").arg(comp->id().toString()));
    auto layers = comp->allLayer();

    std::unordered_map<std::string, EffectNodePtr> layerOutputNodes;
    std::unordered_map<std::string, EffectNodePtr> layerTransformNodes;

    for (auto& layer : layers) {
        if (!layer) {
            continue;
        }

        std::string layerIdStr = layer->id().toString().toStdString();

        auto transformNode = makeShared<EffectNode>(
            NodeID(QString("Transform_%1").arg(layer->id().toString())),
            "Transform",
            EffectPipelineStage::PreProcess,
            nullptr);
        graph->addNode(transformNode);
        layerTransformNodes[layerIdStr] = transformNode;

        auto renderNode = makeShared<EffectNode>(
            NodeID(QString("Render_%1").arg(layer->id().toString())),
            "LayerRender",
            EffectPipelineStage::Rasterizer,
            nullptr);
        graph->addNode(renderNode);
        layerOutputNodes[layerIdStr] = renderNode;

        graph->connect(transformNode->id(), 0, renderNode->id(), 0);
    }

    for (auto& layer : layers) {
        if (!layer) {
            continue;
        }

        std::string layerIdStr = layer->id().toString().toStdString();
        if (layer->hasParent()) {
            std::string parentIdStr = layer->parentLayerId().toString().toStdString();
            if (layerTransformNodes.count(parentIdStr)) {
                graph->connect(
                    layerTransformNodes[parentIdStr]->id(), 0,
                    layerTransformNodes[layerIdStr]->id(), 0);
            }
        }
    }

    auto compositeNode = makeShared<EffectNode>(
        NodeID("Composite_Output"),
        "Final Composite",
        EffectPipelineStage::LayerTransform,
        nullptr);
    graph->addNode(compositeNode);

    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        if (!(*it)) {
            continue;
        }

        std::string layerIdStr = (*it)->id().toString().toStdString();
        graph->connect(layerOutputNodes[layerIdStr]->id(), 0, compositeNode->id(), 0);
    }

    graph->compile();
    return graph;
}

} // namespace Artifact
