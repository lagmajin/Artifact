module;
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
export module Artifact.Engine.DAG.CompositionGraph;




import Utils.Id;
import Utils.String.UniString;
import Artifact.Engine.DAG.Node;
import Artifact.Engine.DAG.Graph;
import Artifact.Engine.DAG.Connection;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    //  CompositionGraph  –  コンポジション全体を司る最上位グラフ
    //
    //   各レイヤーは自身の内部に EffectGraph を持つが、
    //   「レイヤーAのRasterizer出力をレイヤーBのGenerator入力へ接続」
    //   といったクロスレイヤールーティングは、このクラスで管理する。
    //   最終的に1つの巨大な DAG として統合し、Executor で丸ごと
    //   並列評価することができる。
    // ─────────────────────────────────────────────────────────

    class CompositionGraph {
    private:
        UniString name_;

        // 登録されているレイヤーのローカルグラフ
        std::unordered_map<std::string, std::shared_ptr<EffectGraph>> layerGraphs_;

        // レイヤー間の接続
        std::vector<Connection> interLayerConnections_;

    public:
        CompositionGraph() = default;
        explicit CompositionGraph(const UniString& name) : name_(name) {}
        ~CompositionGraph() = default;

        UniString name() const { return name_; }

        // レイヤーごとのローカルグラフを登録
        void registerLayerGraph(const LayerID& layerId, std::shared_ptr<EffectGraph> graph) {
            layerGraphs_[layerId.toString().toStdString()] = graph;
        }

        void unregisterLayerGraph(const LayerID& layerId) {
            layerGraphs_.erase(layerId.toString().toStdString());
            // 関連するクロスレイヤー接続も削除
            auto lid = layerId.toString().toStdString();
            interLayerConnections_.erase(
                std::remove_if(interLayerConnections_.begin(), interLayerConnections_.end(),
                    [&](const Connection& c) {
                        return c.sourceNodeId.toString().toStdString().starts_with(lid) || 
                               c.targetNodeId.toString().toStdString().starts_with(lid);
                    }),
                interLayerConnections_.end()
            );
        }

        std::shared_ptr<EffectGraph> getLayerGraph(const LayerID& layerId) const {
            auto it = layerGraphs_.find(layerId.toString().toStdString());
            if (it != layerGraphs_.end()) return it->second;
            return nullptr;
        }

        // ════════════════════════════════════════════════════
        //  クロスレイヤー接続
        // ════════════════════════════════════════════════════

        // レイヤー間のポート接続（トラックマットやソース参照用）
        bool connectLayers(const LayerID& srcLayerId, const NodeID& srcNodeId, int srcPort,
                           const LayerID& tgtLayerId, const NodeID& tgtNodeId, int tgtPort) 
        {
            auto srcGraph = getLayerGraph(srcLayerId);
            auto tgtGraph = getLayerGraph(tgtLayerId);
            if (!srcGraph || !tgtGraph) return false;

            auto srcNode = srcGraph->findNode(srcNodeId);
            auto tgtNode = tgtGraph->findNode(tgtNodeId);
            if (!srcNode || !tgtNode) return false;

            // 型互換チェック
            if (srcPort < 0 || srcPort >= static_cast<int>(srcNode->outputPorts().size())) return false;
            if (tgtPort < 0 || tgtPort >= static_cast<int>(tgtNode->inputPorts().size()))  return false;

            const auto& outPort = srcNode->outputPorts()[srcPort];
            const auto& inPort  = tgtNode->inputPorts()[tgtPort];
            if (!outPort.isCompatibleWith(inPort)) return false;

            ConnectionID cid; // UUID自動生成想定
            Connection conn(cid, srcNodeId, srcPort, tgtNodeId, tgtPort);
            interLayerConnections_.push_back(conn);

            // ターゲット側のレイヤーをDirtyとしてマーク開始
            tgtGraph->propagateDirty(tgtNodeId);

            return true;
        }

        const std::vector<Connection>& crossLayerConnections() const {
            return interLayerConnections_;
        }

        // ════════════════════════════════════════════════════
        //  フラット化 (Flat DAG 化)
        // ════════════════════════════════════════════════════
        
        // Executorに渡すために、複数のLayerGraphとレイヤー間接続を
        // 1つの巨大なEffectGraphとして結合して返す。
        EffectGraph flattenGraph() const {
            EffectGraph globalGraph(UniString("Global_Flattened_Graph"));

            // 全ノードとローカル接続をコピー
            for (const auto& [layerIdStr, graph] : layerGraphs_) {
                for (const auto& node : graph->nodes()) {
                    globalGraph.addNode(node);
                }
                for (const auto& conn : graph->connections()) {
                    globalGraph.connect(NodeID(conn.sourceNodeId.toString()), conn.sourcePortIndex,
                                        NodeID(conn.targetNodeId.toString()), conn.targetPortIndex);
                }
            }

            // クロスレイヤー接続を追加
            for (const auto& cLayerConn : interLayerConnections_) {
                globalGraph.connect(NodeID(cLayerConn.sourceNodeId.toString()), cLayerConn.sourcePortIndex,
                                    NodeID(cLayerConn.targetNodeId.toString()), cLayerConn.targetPortIndex);
            }

            globalGraph.compile();
            return globalGraph;
        }
    };

}
