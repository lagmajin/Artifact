module;
#include <QString>

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
export module Artifact.Engine.DAG.LayerGraphBuilder;




import Utils.Id;
import Utils.String.UniString;
import Artifact.Engine.DAG.Port;
import Artifact.Engine.DAG.Node;
import Artifact.Engine.DAG.Connection;
import Artifact.Engine.DAG.Graph;
import Artifact.Effect.Abstract;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    //  LayerGraphBuilder  –  UIスタック → DAG 変換ブリッジ
    //
    //   仕様書 4.1:
    //     フロントエンド (UI) ではユーザーに分かりやすい
    //     「固定ラック (スタック) 形式」で表示し、
    //     バックエンドではDAGとして最適化実行する。
    //
    //   この Builder はレイヤーに追加されたエフェクトを
    //   パイプラインステージ順に並べ替え、
    //   自動的にノード間をチェーン接続して EffectGraph を構築する。
    // ─────────────────────────────────────────────────────────

    class LayerGraphBuilder {
    public:
        LayerGraphBuilder() = default;
        ~LayerGraphBuilder() = default;

        /// レイヤーのエフェクトリストから EffectGraph を構築する。
        ///
        /// - effects は UI 上での追加順（ラック内の並び順）で渡される
        /// - ビルダーが EffectPipelineStage 順にソートして DAG を組み立てる
        /// - 同一ステージ内のエフェクトはチェーン（直列）接続される
        /// - ステージ間は自動で入出力をワイヤリングする
        static EffectGraph build(const UniString& layerName,
                                 const std::vector<ArtifactAbstractEffectPtr>& effects) {
            EffectGraph graph(layerName);

            // ステージ別にグルーピング
            std::array<std::vector<ArtifactAbstractEffectPtr>, 5> stageEffects;
            for (const auto& e : effects) {
                if (!e) continue;
                int idx = static_cast<int>(e->pipelineStage());
                if (idx >= 0 && idx < 5) {
                    stageEffects[idx].push_back(e);
                }
            }

            // 各ステージのノードを生成
            std::array<std::vector<EffectNodePtr>, 5> stageNodes;
            int nodeCounter = 0;
            for (int s = 0; s < 5; ++s) {
                auto stage = static_cast<EffectPipelineStage>(s);
                for (const auto& effect : stageEffects[s]) {
                    auto nodeId = NodeID(QString("node_%1").arg(nodeCounter++));
                    auto node = std::make_shared<EffectNode>(
                        nodeId, effect->displayName(), stage, effect
                    );
                    graph.addNode(node);
                    stageNodes[s].push_back(node);
                }
            }

            // ステージ内チェーン接続
            for (int s = 0; s < 5; ++s) {
                auto& nodes = stageNodes[s];
                for (size_t i = 1; i < nodes.size(); ++i) {
                    // 前のノードの output[0] → 次のノードの input[0]
                    graph.connect(nodes[i-1]->id(), 0, nodes[i]->id(), 0);
                }
            }

            // ステージ間接続: 前ステージの最後のノード → 次ステージの最初のノード
            EffectNodePtr lastNode = nullptr;
            for (int s = 0; s < 5; ++s) {
                auto& nodes = stageNodes[s];
                if (nodes.empty()) continue;

                if (lastNode) {
                    // 型が合わない場合でもフレキシブル接続を試みる
                    // (Port::isCompatibleWith が Any を許容するため)
                    graph.connect(lastNode->id(), 0, nodes.front()->id(), 0);
                }
                lastNode = nodes.back();
            }

            // トポロジカルソートでコンパイル
            graph.compile();

            return graph;
        }
    };

}
