module;
#include <QPointF>
export module Artifact.Mask.Path;

import std;
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

    float expansion() const;
    void setExpansion(float expansion);

    bool isInverted() const;
    void setInverted(bool inverted);

    MaskMode mode() const;
    void setMode(MaskMode mode);

    UniString name() const;
    void setName(const UniString& name);

    /// ベジェパスからアルファマスクをラスタライズ (0.0~1.0 single channel)
    /// 戻り値は CV_32FC1 の cv::Mat
    void rasterizeToAlpha(int width, int height, void* outMat) const;
};

}
