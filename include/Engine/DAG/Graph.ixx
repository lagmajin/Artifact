module;
#include <QString>

export module Artifact.Engine.DAG.Graph;

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



import Utils.Id;
import Utils.String.UniString;
import Artifact.Engine.DAG.Port;
import Artifact.Engine.DAG.Node;
import Artifact.Engine.DAG.Connection;
import Artifact.Effect.Abstract;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    //  EffectGraph  –  レイヤーのエフェクトパイプライン DAG
    //
    //   仕様書 4.1:
    //     バックエンド (Engine) では各フェーズの出入力は
    //     ノードベース (DAG) として管理され、最適な順序・
    //     並列度でコンパイル・実行される。
    //
    //   1レイヤーにつき1つの EffectGraph が存在する。
    //   複数レイヤー間の接続は、上位の CompositionGraph が管理する。
    // ─────────────────────────────────────────────────────────

    class EffectGraph {
    private:
        UniString name_;

        // ノード管理
        std::vector<EffectNodePtr>  nodes_;
        std::vector<Connection>     connections_;

        // ソート済み実行順 (compile() で生成)
        std::vector<EffectNodePtr>  executionOrder_;
        bool                        compiled_ = false;

    public:
        EffectGraph() = default;
        explicit EffectGraph(const UniString& name) : name_(name) {}
        ~EffectGraph() = default;

        UniString name() const { return name_; }

        // ════════════════════════════════════════════════════
        //  ノード操作
        // ════════════════════════════════════════════════════

        /// ノードを追加
        bool addNode(EffectNodePtr node) {
            if (!node) return false;
            // 重複チェック
            for (const auto& n : nodes_) {
                if (n->id() == node->id()) return false;
            }
            nodes_.push_back(node);
            compiled_ = false;
            return true;
        }

        /// ノードを ID で削除
        bool removeNode(const NodeID& id) {
            // 関連する接続も削除
            connections_.erase(
                std::remove_if(connections_.begin(), connections_.end(),
                    [&id](const Connection& c) {
                        return c.sourceNodeId == id || c.targetNodeId == id;
                    }),
                connections_.end()
            );

            auto it = std::remove_if(nodes_.begin(), nodes_.end(),
                [&id](const EffectNodePtr& n) { return n->id() == id; });
            if (it == nodes_.end()) return false;
            nodes_.erase(it, nodes_.end());
            compiled_ = false;
            return true;
        }

        /// ノードを ID で検索
        EffectNodePtr findNode(const NodeID& id) const {
            for (const auto& n : nodes_) {
                if (n->id() == id) return n;
            }
            return nullptr;
        }

        /// 全ノード取得
        const std::vector<EffectNodePtr>& nodes() const { return nodes_; }

        /// 特定ステージのノードを取得
        std::vector<EffectNodePtr> nodesForStage(EffectPipelineStage stage) const {
            std::vector<EffectNodePtr> result;
            for (const auto& n : nodes_) {
                if (n->stage() == stage) result.push_back(n);
            }
            return result;
        }

        // ════════════════════════════════════════════════════
        //  接続操作
        // ════════════════════════════════════════════════════

        /// 2ノード間を接続
        bool connect(const NodeID& srcId, int srcPort,
                     const NodeID& tgtId, int tgtPort) {
            auto srcNode = findNode(srcId);
            auto tgtNode = findNode(tgtId);
            if (!srcNode || !tgtNode) return false;

            // ポート範囲チェック
            if (srcPort < 0 || srcPort >= static_cast<int>(srcNode->outputPorts().size())) return false;
            if (tgtPort < 0 || tgtPort >= static_cast<int>(tgtNode->inputPorts().size()))  return false;

            // 型互換チェック
            const auto& outPort = srcNode->outputPorts()[srcPort];
            const auto& inPort  = tgtNode->inputPorts()[tgtPort];
            if (!outPort.isCompatibleWith(inPort)) return false;

            // 循環チェック → 接続後に compile を試みる
            Connection conn(ConnectionID(), srcId, srcPort, tgtId, tgtPort);
            connections_.push_back(conn);

            if (hasCycle()) {
                connections_.pop_back();
                return false;
            }

            compiled_ = false;
            // 下流ノードを dirty に
            propagateDirty(tgtId);
            return true;
        }

        /// 接続を除去
        bool disconnect(const NodeID& srcId, int srcPort,
                        const NodeID& tgtId, int tgtPort) {
            auto it = std::remove_if(connections_.begin(), connections_.end(),
                [&](const Connection& c) {
                    return c.sourceNodeId == srcId && c.sourcePortIndex == srcPort
                        && c.targetNodeId  == tgtId && c.targetPortIndex  == tgtPort;
                });
            if (it == connections_.end()) return false;
            connections_.erase(it, connections_.end());
            compiled_ = false;
            return true;
        }

        const std::vector<Connection>& connections() const { return connections_; }

        // ════════════════════════════════════════════════════
        //  コンパイル（トポロジカルソート）
        // ════════════════════════════════════════════════════

        /// グラフをトポロジカルソートして実行順序を確定
        bool compile() {
            executionOrder_.clear();

            // Kahn's algorithm
            // In-degree 計算
            std::unordered_map<std::string, int> inDegree;
            for (const auto& n : nodes_) {
                inDegree[n->id().toString().toStdString()] = 0;
            }
            for (const auto& c : connections_) {
                if (c.enabled) {
                    inDegree[c.targetNodeId.toString().toStdString()]++;
                }
            }

            // In-degree 0 のノードをキューに入れる
            std::deque<EffectNodePtr> queue;
            for (const auto& n : nodes_) {
                if (inDegree[n->id().toString().toStdString()] == 0) {
                    queue.push_back(n);
                }
            }

            int order = 0;
            while (!queue.empty()) {
                auto current = queue.front();
                queue.pop_front();

                current->setSortOrder(order++);
                executionOrder_.push_back(current);

                // このノードから出る接続の先のノードのin-degreeを減らす
                for (const auto& c : connections_) {
                    if (c.enabled && c.sourceNodeId == current->id()) {
                        auto targetKey = c.targetNodeId.toString().toStdString();
                        inDegree[targetKey]--;
                        if (inDegree[targetKey] == 0) {
                            auto tgt = findNode(NodeID(c.targetNodeId.toString()));
                            if (tgt) queue.push_back(tgt);
                        }
                    }
                }
            }

            compiled_ = (executionOrder_.size() == nodes_.size());
            return compiled_;
        }

        bool isCompiled() const { return compiled_; }

        /// コンパイル済みの実行順序を取得
        const std::vector<EffectNodePtr>& executionOrder() const {
            return executionOrder_;
        }

        // ════════════════════════════════════════════════════
        //  Dirty 伝播 (Smart Cache – 仕様書 4.2)
        // ════════════════════════════════════════════════════

        /// 指定ノードとその下流すべてを dirty にマークする
        void propagateDirty(const NodeID& startId) {
            std::deque<NodeID> queue;
            queue.push_back(startId);
            std::unordered_set<std::string> visited;

            while (!queue.empty()) {
                auto currentId = queue.front();
                queue.pop_front();

                auto key = currentId.toString().toStdString();
                if (visited.count(key)) continue;
                visited.insert(key);

                auto node = findNode(currentId);
                if (node) node->markDirty();

                // 下流ノードをキューに追加
                for (const auto& c : connections_) {
                    if (c.enabled && c.sourceNodeId == currentId) {
                        queue.push_back(NodeID(c.targetNodeId.toString()));
                    }
                }
            }
        }

        /// 全ノードを dirty にする
        void markAllDirty() {
            for (auto& n : nodes_) {
                n->markDirty();
            }
        }

        // ════════════════════════════════════════════════════
        //  ユーティリティ
        // ════════════════════════════════════════════════════

        /// ノードの上流（入力元）ノードリストを取得
        std::vector<EffectNodePtr> getUpstreamNodes(const NodeID& id) const {
            std::vector<EffectNodePtr> result;
            for (const auto& c : connections_) {
                if (c.enabled && c.targetNodeId == id) {
                    auto n = findNode(NodeID(c.sourceNodeId.toString()));
                    if (n) result.push_back(n);
                }
            }
            return result;
        }

        /// ノードの下流（出力先）ノードリストを取得
        std::vector<EffectNodePtr> getDownstreamNodes(const NodeID& id) const {
            std::vector<EffectNodePtr> result;
            for (const auto& c : connections_) {
                if (c.enabled && c.sourceNodeId == id) {
                    auto n = findNode(NodeID(c.targetNodeId.toString()));
                    if (n) result.push_back(n);
                }
            }
            return result;
        }

        /// グラフ内のノード数
        size_t nodeCount() const { return nodes_.size(); }

        /// グラフ内の接続数
        size_t connectionCount() const { return connections_.size(); }

    private:
        /// 循環検出 (DFS)
        bool hasCycle() const {
            std::unordered_map<std::string, int> color;  // 0=white, 1=grey, 2=black
            for (const auto& n : nodes_) {
                color[n->id().toString().toStdString()] = 0;
            }

            for (const auto& n : nodes_) {
                auto key = n->id().toString().toStdString();
                if (color[key] == 0) {
                    if (dfsHasCycle(key, color)) return true;
                }
            }
            return false;
        }

        bool dfsHasCycle(const std::string& nodeKey,
                         std::unordered_map<std::string, int>& color) const {
            color[nodeKey] = 1;  // grey = processing

            for (const auto& c : connections_) {
                if (!c.enabled) continue;
                if (c.sourceNodeId.toString().toStdString() != nodeKey) continue;

                auto tgtKey = c.targetNodeId.toString().toStdString();
                if (color[tgtKey] == 1) return true;   // back edge → cycle
                if (color[tgtKey] == 0) {
                    if (dfsHasCycle(tgtKey, color)) return true;
                }
            }

            color[nodeKey] = 2;  // black = done
            return false;
        }
    };

}
