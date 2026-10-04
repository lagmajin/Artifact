module;

#include <QPointF>
#include <QRectF>

#include <vector>

export module Artifact.Widgets.LayerEditor.Geometry;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Mask.Path;

export namespace Artifact {

enum class MaskHandleType { None, InTangent, OutTangent };

/// マスク bounding box ギズモのハンドル種別。
/// TransformGizmo の Scale_* と同じ割で、中心は Scale_Center に相当する。
enum class MaskBoundsHandle {
    None,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Top,
    Bottom,
    Left,
    Right,
    Center
};

/// bounds ハンドルからスケール係数（-1..1 の軸方向）を返す。
/// Corner は (sx, sy)、辺ハンドルは軸のみ、Center は (0,0)。
QPointF maskBoundsHandleScale(MaskBoundsHandle handle);

QPointF maskHandlePosition(const MaskPath& path, int vertexIndex,
                           MaskHandleType handleType);

float proportionalEditWeight(qreal distance, qreal radius);

std::vector<MaskVertex> proportionalMaskVertices(
    const std::vector<MaskVertex>& source, const QPointF& origin,
    const QPointF& target, qreal radius);

/// マスク形状を制御するアフィン変換（レイヤーローカル空間）。
/// 非等方スケールや回転在内包し、feather / expansion の追従倍率も保持する。
struct MaskAffineTransform {
    QPointF center;      // 変換の中心（レイヤーローカル空間）
    float scaleX = 1.0f; // X 方向スケール
    float scaleY = 1.0f; // Y 方向スケール
    float rotation = 0.0f; // ラジアン
    QPointF translate;   // 変換後に加算される平行移動

    static MaskAffineTransform identity();
    static MaskAffineTransform scaling(const QPointF& center,
                                       const QPointF& factors);
    static MaskAffineTransform rotating(const QPointF& center, double radians);
    static MaskAffineTransform translating(const QPointF& delta);

    QPointF map(const QPointF& point) const;
    /// 線分ベクトルへの作用（平行移動を含まない）。
    QPointF mapVector(const QPointF& vector) const;
    /// feather / expansion 用の平均スケール倍率。
    float lengthScale() const;
    bool isIdentity() const;
};

/// 単一マスクパスの全頂点にアフィン変換を適用する。
/// 位置は map()、in/out タンジェントは mapVector() で変換する。
std::vector<MaskVertex> transformMaskVertices(
    const std::vector<MaskVertex>& source, const MaskAffineTransform& transform);

/// マスクの頂点群から制御点 bbox（制御点そのものは含むが
/// ベジェ曲線の外側は含まない）を求める。頂点が空なら false。
bool maskVerticesBounds(const std::vector<MaskVertex>& vertices, QRectF& outBounds);

/// レイヤーの全マスクパス（指定マスクのみの場合 maskIndex >= 0）の
/// 制御点 bbox を合算する。何か一つでも頂点があれば true。
bool maskBounds(const ArtifactAbstractLayerPtr& layer, int maskIndex,
                QRectF& outBounds);

/// feather / expansion をスケール倍率に応じて調整した値を返す。
/// 倍率が 1.0 なら元値をそのまま返す。
ArtifactCore::Units::LayerLocalLength scaleMaskLength(
    ArtifactCore::Units::LayerLocalLength value, float factor);

/// bounds ハンドル.Kind に対応する頂点（レイヤーローカル空間）を bounds から求める。
/// Center は幾何中心、Top は上辺中央。None は bounds Modificationなし。
QPointF maskBoundsHandlePosition(const QRectF& bounds, MaskBoundsHandle handle);

/// .canvas 座標上で bounds ハンドルを当たり判定する。
/// bounds はレイヤーローカル空間の矩形、transform は
/// レイヤーローカル→キャンバスの変換。閾値は zoom で割った値。
MaskBoundsHandle hitTestMaskBoundsHandle(
    const QRectF& localBounds, const QTransform& layerToCanvas,
    const QPointF& canvasPosition, float threshold);

std::vector<QPointF> proportionalShapePoints(
    const std::vector<QPointF>& source, const QPointF& origin,
    const QPointF& target, qreal radius, qreal width, qreal height);
std::vector<CustomPathVertex> proportionalPathVertices(
    const std::vector<CustomPathVertex>& source, const QPointF& origin,
    const QPointF& target, qreal radius);

std::vector<QPointF> buildShapeEditSeedPoints(
    const ArtifactShapeLayer& shape);
bool ensureShapeEditSeedGeometry(const ArtifactAbstractLayerPtr& layer);

QPointF shapeCornerRadiusHandlePosition(const ArtifactShapeLayer& shape);
QPointF shapeStarInnerRadiusHandlePosition(const ArtifactShapeLayer& shape);

int extrudePolygonVertex(std::vector<QPointF>& points, int sourceIndex);
int extrudePathVertex(std::vector<CustomPathVertex>& vertices, int sourceIndex);

// F6: corner<->bezier toggle for one custom-path vertex. Smooth-on seeds
// handle offsets along the neighbor chord (keeps authored non-zero handles);
// corner restores straight segments by discarding handle offsets.
void togglePathVertexSmooth(std::vector<CustomPathVertex>& vertices, int index,
                            bool closed);

std::vector<QPointF> translateSelectedPolygon(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    const QPointF& delta);
std::vector<CustomPathVertex> translateSelectedPath(
    const std::vector<CustomPathVertex>& source,
    const std::vector<int>& selected, const QPointF& delta);
std::vector<CustomPathVertex> rotateSelectedPath(
    const std::vector<CustomPathVertex>& source,
    const std::vector<int>& selected, double radians);
std::vector<QPointF> rotateSelectedPolygon(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    double radians);
std::vector<QPointF> scaleSelectedPolygon(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    double factor);
std::vector<QPointF> scaleSelectedPolygonAxes(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    const QPointF& factors);
std::vector<CustomPathVertex> scaleSelectedPathAxes(
    const std::vector<CustomPathVertex>& source,
    const std::vector<int>& selected, const QPointF& factors);
std::vector<QPointF> insetPolygon(
    const std::vector<QPointF>& source, double factor);

bool hitTestMaskHandle(const ArtifactAbstractLayerPtr& layer,
                       const QPointF& canvasPos, float threshold,
                       int& maskIndex, int& pathIndex, int& vertexIndex,
                       MaskHandleType& handleType);

bool hitTestMaskVertexGeometry(const ArtifactAbstractLayerPtr& layer,
                               const QPointF& canvasPos, float threshold,
                               int& maskIndex, int& pathIndex,
                               int& vertexIndex);

bool hitTestMaskBezierSegmentGeometry(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos,
    float threshold, int& maskIndex, int& pathIndex, int& segmentIndex);

}
