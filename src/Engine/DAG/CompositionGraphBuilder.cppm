module;
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>

module Artifact.Engine.DAG.CompositionGraphBuilder;

import Artifact.Composition.Abstract;
import Artifact.Engine.DAG.Node;
import Artifact.Engine.DAG.Port;
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

        EffectNodePtr previousNode = transformNode;
        int effectIndex = 0;
        auto effects = layer->getEffects();
        std::stable_sort(
            effects.begin(), effects.end(),
            [](const auto &lhs, const auto &rhs) {
                if (!lhs || !rhs) {
                    return static_cast<bool>(lhs) > static_cast<bool>(rhs);
                }
                return static_cast<int>(lhs->pipelineStage()) <
                       static_cast<int>(rhs->pipelineStage());
            });
        for (const auto &effect : effects) {
            if (!effect) {
                continue;
            }
            auto effectNode = makeShared<EffectNode>(
                NodeID(QString("Effect_%1_%2")
                           .arg(layer->id().toString())
                           .arg(effectIndex++)),
                effect->displayName(),
                effect->pipelineStage(),
                effect);
            graph->addNode(effectNode);
            graph->connect(previousNode->id(), 0, effectNode->id(), 0);
            previousNode = effectNode;
        }
        graph->connect(previousNode->id(), 0, renderNode->id(), 0);
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
    int compositeInputIndex = 0;
    for (const auto &layer : layers) {
        if (!layer) {
            continue;
        }
        if (compositeInputIndex > 0) {
            compositeNode->addInputPort(Port(
                UniString(QString("layer_in_%1").arg(compositeInputIndex)),
                PortDataType::ImageBuffer,
                PortDirection::Input,
                compositeInputIndex));
        }
        ++compositeInputIndex;
    }
    compositeInputIndex = 0;
    graph->addNode(compositeNode);

    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        if (!(*it)) {
            continue;
        }

        std::string layerIdStr = (*it)->id().toString().toStdString();
        graph->connect(layerOutputNodes[layerIdStr]->id(), 0,
                       compositeNode->id(), compositeInputIndex++);
    }

    graph->compile();
    return graph;
}

} // namespace Artifact
