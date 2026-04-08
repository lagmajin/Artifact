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

export module Artifact.Engine.DAG.Node;




import Utils.String.UniString;
import Utils.Id;
import Artifact.Engine.DAG.Port;
import Artifact.Effect.Abstract;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    //  NodeID  –  グラフ内でノードを一意に識別する ID
    // ─────────────────────────────────────────────────────────
    class NodeID : public Id {
    public:
        using Id::Id;
        NodeID(const Id& other) : Id(other) {}
    };

    // ─────────────────────────────────────────────────────────
    //  NodeState  –  ノードの実行状態（キャッシュ管理用）
    // ─────────────────────────────────────────────────────────
    enum class NodeState {
        Dirty,       // 再計算が必要
        Computing,   // 計算中
        Cached,      // キャッシュ済み（再利用可能）
        Error        // エラー状態
    };

    // ─────────────────────────────────────────────────────────
    //  EffectNode  –  DAG 内の1つの処理ノード
    //
    //   ArtifactAbstractEffect をラップし、入出力ポートと
    //   キャッシュの dirty/clean 状態を管理する。
    //   仕様書 4.2 の Smart Cache に対応する dirty flag を内蔵。
    // ─────────────────────────────────────────────────────────

    class EffectNode {
    private:
        NodeID                          id_;
        UniString                       name_;
        EffectPipelineStage             stage_;
        ArtifactAbstractEffectPtr       effect_;        // ラップ対象のエフェクト

        std::vector<Port>               inputPorts_;
        std::vector<Port>               outputPorts_;

        NodeState                       state_ = NodeState::Dirty;
        int                             sortOrder_ = -1;  // トポロジカルソート後の実行順

    public:
        EffectNode() = default;

        EffectNode(const NodeID& id,
                   const UniString& name,
                   EffectPipelineStage stage,
                   ArtifactAbstractEffectPtr effect = nullptr)
            : id_(id), name_(name), stage_(stage), effect_(effect)
        {
            // パイプラインステージに応じてデフォルト入出力ポートを生成
            setupDefaultPorts();
        }

        virtual ~EffectNode() = default;

        // ── 識別 ──
        NodeID    id()    const { return id_; }
        UniString name()  const { return name_; }
        EffectPipelineStage stage() const { return stage_; }

        // ── エフェクト ──
        ArtifactAbstractEffectPtr effect() const { return effect_; }
        void setEffect(ArtifactAbstractEffectPtr e) { effect_ = e; markDirty(); }

        // ── ポート ──
        const std::vector<Port>& inputPorts()  const { return inputPorts_; }
        const std::vector<Port>& outputPorts() const { return outputPorts_; }

        void addInputPort(const Port& p)  { inputPorts_.push_back(p); }
        void addOutputPort(const Port& p) { outputPorts_.push_back(p); }

        const Port* findInputPort(const UniString& name) const {
            for (auto& p : inputPorts_) {
                if (p.name() == name) return &p;
            }
            return nullptr;
        }

        const Port* findOutputPort(const UniString& name) const {
            for (auto& p : outputPorts_) {
                if (p.name() == name) return &p;
            }
            return nullptr;
        }

        // ── キャッシュ / Dirty 管理 (仕様書 4.2 Smart Cache) ──
        NodeState state()     const { return state_; }
        bool      isDirty()   const { return state_ == NodeState::Dirty; }
        bool      isCached()  const { return state_ == NodeState::Cached; }

        void markDirty()   { state_ = NodeState::Dirty; }
        void markCached()  { state_ = NodeState::Cached; }
        void markError()   { state_ = NodeState::Error; }

        // ── ソート順 ──
        int  sortOrder()              const { return sortOrder_; }
        void setSortOrder(int order)  { sortOrder_ = order; }

    private:
        void setupDefaultPorts() {
            switch (stage_) {
                case EffectPipelineStage::Generator:
                    // 入力なし → ジオメトリ出力
                    outputPorts_.push_back(Port(UniString("geometry_out"), PortDataType::GeometryBuffer, PortDirection::Output, 0));
                    break;

                case EffectPipelineStage::GeometryTransform:
                    // ジオメトリ入力 → ジオメトリ出力、オプションでフィールド入力
                    inputPorts_.push_back(Port(UniString("geometry_in"), PortDataType::GeometryBuffer, PortDirection::Input, 0));
                    inputPorts_.push_back(Port(UniString("field_in"), PortDataType::ScalarField, PortDirection::Input, 1));
                    outputPorts_.push_back(Port(UniString("geometry_out"), PortDataType::GeometryBuffer, PortDirection::Output, 0));
                    break;

                case EffectPipelineStage::MaterialRender:
                    // ジオメトリ入力 + マテリアル → 画像出力（ラスタライズ境界）
                    inputPorts_.push_back(Port(UniString("geometry_in"), PortDataType::GeometryBuffer, PortDirection::Input, 0));
                    inputPorts_.push_back(Port(UniString("material_in"), PortDataType::MaterialData, PortDirection::Input, 1));
                    outputPorts_.push_back(Port(UniString("image_out"), PortDataType::ImageBuffer, PortDirection::Output, 0));
                    break;

                case EffectPipelineStage::Rasterizer:
                    // 画像入力 → 画像出力
                    inputPorts_.push_back(Port(UniString("image_in"), PortDataType::ImageBuffer, PortDirection::Input, 0));
                    outputPorts_.push_back(Port(UniString("image_out"), PortDataType::ImageBuffer, PortDirection::Output, 0));
                    break;

                case EffectPipelineStage::LayerTransform:
                    // 画像入力 → 変換行列付き画像出力
                    inputPorts_.push_back(Port(UniString("image_in"), PortDataType::ImageBuffer, PortDirection::Input, 0));
                    outputPorts_.push_back(Port(UniString("image_out"), PortDataType::ImageBuffer, PortDirection::Output, 0));
                    outputPorts_.push_back(Port(UniString("transform_out"), PortDataType::TransformMatrix, PortDirection::Output, 1));
                    break;
            }
        }
    };

    typedef std::shared_ptr<EffectNode> EffectNodePtr;

}
