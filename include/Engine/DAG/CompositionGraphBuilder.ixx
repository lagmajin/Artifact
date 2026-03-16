module;
#include <QString>
#include <vector>
#include <unordered_map>
#include <memory>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QList>

export module Artifact.Engine.DAG.CompositionGraphBuilder;




import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Engine.DAG.Graph;
import Artifact.Engine.DAG.Node;
import Artifact.Engine.DAG.Connection;

export namespace Artifact {

    // ─────────────────────────────────────────────────────────
    //  CompositionGraphBuilder
    //
    //  コンポジション全体のレイヤー、トランスフォーム、エフェクトの
    //  依存関係を解析し、単一の巨大な有向非巡回グラフ (DAG) を構築する。
    //  これにより、DAGExecutor を用いた全自動の並列評価が可能になる。
    // ─────────────────────────────────────────────────────────

    class CompositionGraphBuilder {
    public:
        // コンポジションから評価・レンダリング用グラフを構築
        static std::shared_ptr<EffectGraph> build(ArtifactAbstractComposition* comp) {
            if (!comp) return nullptr;

            auto graph = std::make_shared<EffectGraph>(QString("Composition_%1").arg(comp->id().toString()));
            auto layers = comp->allLayer();

            // レイヤーごとの主要ノードを管理するマップ
            // (キー: LayerID, 値: そのレイヤーの最終出力ノード)
            std::unordered_map<std::string, EffectNodePtr> layerOutputNodes;
            std::unordered_map<std::string, EffectNodePtr> layerTransformNodes;

            // 1. 各レイヤーの基本ノードを生成
            for (auto& layer : layers) {
                if (!layer) continue;
                std::string layerIdStr = layer->id().toString().toStdString();

                // A. Transform計算ノード (親レイヤーのTransformに依存する可能性あり)
                auto transformNode = std::make_shared<EffectNode>(
                    NodeID(QString("Transform_%1").arg(layer->id().toString())),
                    "Transform",
                    EffectPipelineStage::PreProcess,
                    nullptr // 本来は TransformProcessor のようなインターフェースを渡す
                );
                graph->addNode(transformNode);
                layerTransformNodes[layerIdStr] = transformNode;

                // B. ソース生成/エフェクトノード (Transformに依存)
                auto renderNode = std::make_shared<EffectNode>(
                    NodeID(QString("Render_%1").arg(layer->id().toString())),
                    "LayerRender",
                    EffectPipelineStage::Rasterizer,
                    nullptr // LayerRenderProcessor を渡す
                );
                graph->addNode(renderNode);
                layerOutputNodes[layerIdStr] = renderNode;

                // レイヤー内依存: Transformが確定してからRenderする
                graph->connect(transformNode->id(), 0, renderNode->id(), 0);
            }

            // 2. レイヤー間の依存関係 (Parenting / Track Matte) をワイヤリング
            for (auto& layer : layers) {
                if (!layer) continue;
                std::string layerIdStr = layer->id().toString().toStdString();

                // 親レイヤーとのTransform依存解決
                if (layer->hasParent()) {
                    std::string parentIdStr = layer->parentLayerId().toString().toStdString();
                    if (layerTransformNodes.count(parentIdStr)) {
                        // 親のTransform出力 -> 子のTransform入力 に接続
                        graph->connect(
                            layerTransformNodes[parentIdStr]->id(), 0,
                            layerTransformNodes[layerIdStr]->id(), 0
                        );
                    }
                }
                
                // TODO: エクスプレッションやトラックマットによる他レイヤー出力への依存もここで接続
            }

            // 3. コンポジション合成ノード (Root)
            auto compositeNode = std::make_shared<EffectNode>(
                NodeID("Composite_Output"),
                "Final Composite",
                EffectPipelineStage::LayerTransform,
                nullptr
            );
            graph->addNode(compositeNode);

            // Zオーダー順に合成ノードへ接続 (ペインターズアルゴリズムのDAG表現)
            for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
                if (!(*it)) continue;
                std::string layerIdStr = (*it)->id().toString().toStdString();
                // 各レイヤーの出力 -> コンポジション出力 に接続
                graph->connect(layerOutputNodes[layerIdStr]->id(), 0, compositeNode->id(), 0);
            }

            graph->compile(); // トポロジカルソートや循環参照チェックを実行
            return graph;
        }
    };

}
