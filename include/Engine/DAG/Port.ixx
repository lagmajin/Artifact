module;
#include <QString>

export module Artifact.Engine.DAG.Port;

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



import Utils.String.UniString;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    //  PortDataType  –  DAG ポート間で流れるデータの型タグ
    //
    //   パイプラインの各フェーズ境界で変わるデータ型:
    //     Generator → GeometryTransform : GeometryBuffer
    //     GeometryTransform → MaterialRender : GeometryBuffer
    //     MaterialRender → Rasterizer : ImageBuffer (after rasterization)
    //     Rasterizer → LayerTransform : ImageBuffer
    // ─────────────────────────────────────────────────────────

    enum class PortDataType {
        GeometryBuffer,    // 頂点バッファ / インスタンスデータ
        ImageBuffer,       // RGBA 2D テクスチャ (ラスタライズ後)
        ScalarField,       // float フィールドデータ (Field → Transform 用)
        MaterialData,      // マテリアル記述 (PBR パラメータ群)
        TransformMatrix,   // 4×4 変換行列
        Any                // 型チェックをスキップする特殊用途
    };

    // ─────────────────────────────────────────────────────────
    //  PortDirection  –  入力か出力か
    // ─────────────────────────────────────────────────────────

    enum class PortDirection {
        Input,
        Output
    };

    // ─────────────────────────────────────────────────────────
    //  Port  –  ノード上の接続ポイント
    // ─────────────────────────────────────────────────────────

    class Port {
    private:
        UniString     name_;
        PortDataType  dataType_;
        PortDirection direction_;
        int           index_ = 0;  // ノード内でのポート番号

    public:
        Port() = default;
        Port(const UniString& name, PortDataType type, PortDirection dir, int index = 0)
            : name_(name), dataType_(type), direction_(dir), index_(index) {}

        UniString      name()      const { return name_; }
        PortDataType   dataType()  const { return dataType_; }
        PortDirection  direction() const { return direction_; }
        int            index()     const { return index_; }

        // 型互換性チェック
        bool isCompatibleWith(const Port& other) const {
            if (direction_ == other.direction_) return false;  // 同方向はNG
            if (dataType_ == PortDataType::Any || other.dataType_ == PortDataType::Any) return true;
            return dataType_ == other.dataType_;
        }
    };

}
