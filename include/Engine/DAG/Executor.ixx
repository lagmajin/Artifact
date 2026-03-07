module;
#include <QString>
#include <functional>
#include <future>
#include <atomic>

export module Artifact.Engine.DAG.Executor;

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



import Artifact.Engine.DAG.Graph;
import Artifact.Engine.DAG.Node;
import Core.ThreadPool;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    //  DAGExecutor  –  DAGノードの並列評価システム
    //
    //   インディグリー（依存する入力数）をベースに、
    //   依存関係が解決（準備完了）したノードから順にタスクを
    //   並列ワーカープール（TBBやスレッドプール）に投入する基盤。
    //   Intel TBB の `tbb::flow::graph` や `task_group` での実装を
    //   想定した抽象レイヤー。
    // ─────────────────────────────────────────────────────────

    class DAGExecutor {
    public:
        using TaskFunc = std::function<void()>;

        virtual ~DAGExecutor() = default;

        // TBB などのバックエンドにタスクを非同期投入する
        virtual void enqueueTask(TaskFunc task) = 0;

        // 全てのタスクが完了するまで待機
        virtual void waitAll() = 0;

        // グラフ全体の並列評価を開始する
        void evaluateGraph(EffectGraph& graph) {
            auto& nodes = graph.nodes();
            auto& conns = graph.connections();

            // 1. 各ノードの未解決の依存数（in-degree）を計算
            std::unordered_map<std::string, std::atomic<int>> pendingDependencies;
            std::unordered_map<std::string, std::vector<EffectNodePtr>> dependents;

            for (const auto& n : nodes) {
                pendingDependencies[n->id().toString().toStdString()] = 0;
            }

            for (const auto& c : conns) {
                if (!c.enabled) continue;
                auto srcKey = c.sourceNodeId.toString().toStdString();
                auto tgtKey = c.targetNodeId.toString().toStdString();
                
                pendingDependencies[tgtKey]++;
                dependents[srcKey].push_back(graph.findNode(NodeID(c.targetNodeId.toString())));
            }

            // 2. 依存がない（既に評価可能）ノードのスケジュール関数
            // ※再帰的にラムダを呼ぶため std::function を使用
            std::function<void(EffectNodePtr)> scheduleNode;
            scheduleNode = [&](EffectNodePtr node) {
                enqueueTask([this, node, &pendingDependencies, &dependents, &scheduleNode]() {
                    // --- ノードの実際の計算処理 ---
                    if (node->isDirty()) {
                        // TODO: 実際の Effect のバックエンド評価 (CPU/GPU)
                        // node->effect()->apply(...)
                        node->markCached(); // 計算完了したらキャッシュ済みに
                    }

                    // --- 後続ノードの依存カウンタを減らす ---
                    auto nodeKey = node->id().toString().toStdString();
                    for (auto& depNode : dependents[nodeKey]) {
                        auto depKey = depNode->id().toString().toStdString();
                        // 依存カウンタが 0 になったら非同期キューに追加
                        if (--pendingDependencies[depKey] == 0) {
                            scheduleNode(depNode);
                        }
                    }
                });
            };

            // 3. 初期タスク（in-degree == 0 のノード）を一斉に発火
            for (const auto& n : nodes) {
                if (pendingDependencies[n->id().toString().toStdString()] == 0) {
                    scheduleNode(n);
                }
            }

            // 4. キューが空になり、全スレッドの処理が終わるまでブロック
            waitAll();
        }
    };

    // ─────────────────────────────────────────────────────────
    //  SimpleThreadPoolExecutor (スタブ実装)
    //   実際にはここで TBB task_group や std::async を用いる
    // ─────────────────────────────────────────────────────────
    class SimpleAsyncExecutor : public DAGExecutor {
    private:
        std::vector<std::future<void>> futures_;
        std::mutex mutex_;

    public:
        void enqueueTask(TaskFunc task) override {
            // ThreadPoolにタスクを投げるだけ（std::asyncのコストを削減）
            ThreadPool::globalInstance().enqueueTask(task);
        }

        void waitAll() override {
            // スレッドプール内のすべてのタスクが終わるのを待つ
            ThreadPool::globalInstance().waitAll();
        }
    };

}
