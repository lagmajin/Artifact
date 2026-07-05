module;
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
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
#include <QPointF>
#include <QPainterPath>
export module Artifact.Mask.Path;




import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

/// ベジェ制御点を含むマスク頂点
struct MaskVertex {
    QPointF position;     // アンカーポイント
    QPointF inTangent;    // 入力タンジェント（relative to position）
    QPointF outTangent;   // 出力タンジェント（relative to position）
};

/// 複数マスクの合成モード
enum class MaskMode {
    Add,
    Subtract,
    Intersect,
    Difference
};

/// MaskPath のアニメーション用スナップショット
struct MaskPathKeyframeSnapshot {
    int64_t frame = 0;
    std::vector<MaskVertex> vertices;
    bool closed = true;
    float opacity = 1.0f;
    float feather = 0.0f;
    float featherHorizontal = 0.0f;
    float featherVertical = 0.0f;
    float featherInner = 0.0f;
    float featherOuter = 0.0f;
    float expansion = 0.0f;
    bool inverted = false;
    MaskMode mode = MaskMode::Add;
    UniString name;
};

/// ベジェパスで定義されるマスク
class MaskPath {
private:
    class Impl;
    Impl* impl_;
public:
    MaskPath();
    ~MaskPath();
    MaskPath(const MaskPath& other);
    MaskPath& operator=(const MaskPath& other);

    // 頂点操作
    void addVertex(const MaskVertex& vertex);
    void insertVertex(int index, const MaskVertex& vertex);
    void removeVertex(int index);
    void setVertex(int index, const MaskVertex& vertex);
    MaskVertex vertex(int index) const;
    int vertexCount() const;
    void clearVertices();

    // パス属性
    bool isClosed() const;
    void setClosed(bool closed);

    // マスク属性
    float opacity() const;
    void setOpacity(float opacity);

    float feather() const;
    void setFeather(float feather);
    float featherHorizontal() const;
    void setFeatherHorizontal(float feather);
    float featherVertical() const;
    void setFeatherVertical(float feather);
    float featherInner() const;
    void setFeatherInner(float feather);
    float featherOuter() const;
    void setFeatherOuter(float feather);

    float expansion() const;
    void setExpansion(float expansion);

    bool isInverted() const;
    void setInverted(bool inverted);

    MaskMode mode() const;
    void setMode(MaskMode mode);

    UniString name() const;
    void setName(const UniString& name);

    // === Animation scaffold ===
    void clearAnimationKeyframes();
    void setAnimationKeyframe(int64_t frame, const MaskPathKeyframeSnapshot& snapshot);
    bool removeAnimationKeyframe(int64_t frame);
    bool hasAnimationKeyframes() const;
    std::vector<MaskPathKeyframeSnapshot> animationKeyframes() const;
    MaskPath sampleAtFrame(int64_t frame) const;

    /// QPainterPath の各サブパスを MaskPath のリストに分解する
    /// 外側輪郭は MaskMode::Add、穴は MaskMode::Subtract となる。
    /// text が空でない場合、そのテキストを名前として使う。
    static std::vector<MaskPath> fromQPainterPath(
        const QPainterPath& path,
        const QString& text = QString());

    /// ベジェパスからアルファマスクをラスタライズ (0.0~1.0 single channel)
    /// offsetX/offsetY: レイヤーローカル空間からピクセル空間への変換オフセット
    ///   (通常は -localBounds.x(), -localBounds.y() を渡す)
    /// 戻り値は CV_32FC1 の cv::Mat
    void rasterizeToAlpha(int width, int height, void* outMat,
                          float offsetX = 0.0f, float offsetY = 0.0f,
                          float scaleX = 1.0f, float scaleY = 1.0f) const;
};

}
