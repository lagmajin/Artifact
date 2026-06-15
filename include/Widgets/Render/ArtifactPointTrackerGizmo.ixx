module;
#include <QPointF>
#include <QRectF>
#include <QCursor>

export module Artifact.Widgets.PointTrackerGizmo;

import Artifact.Render.IRenderer;
export import Artifact.Layer.Abstract;
import Color.Float;

namespace ArtifactCore { class MotionTracker; }

export namespace Artifact {

/// 2D Point Tracker のキャンバスオーバーレイ Gizmo。
/// Inner Box（特徴領域）と Outer Box（探索領域）を Diligent ネイティブで描画し、
/// ドラッグで移動・リサイズ可能にする。MotionTracker 参照時は軌跡も描画する。
class ArtifactPointTrackerGizmo {
public:
    enum class HandleType {
        None,
        InnerMove,
        InnerScale_TL, InnerScale_TR, InnerScale_BL, InnerScale_BR,
        InnerScale_T, InnerScale_B, InnerScale_L, InnerScale_R,
        PathPoint  ///< モーションパス上の補正ポイント
    };

    /// Inner Box の状態 (canvas 座標)
    struct TrackerBoxState {
        QPointF innerCenter;       ///< 特徴領域の中心
        float  innerHalfW = 16.0f; ///< 特徴領域の半幅
        float  innerHalfH = 16.0f; ///< 特徴領域の半高
        float  outerHalfW = 32.0f; ///< 探索領域の半幅
        float  outerHalfH = 32.0f; ///< 探索領域の半高
    };

    ArtifactPointTrackerGizmo();
    ~ArtifactPointTrackerGizmo();

    void setLayer(ArtifactAbstractLayerPtr layer);
    ArtifactAbstractLayerPtr layer() const;

    /// MotionTracker を設定する（軌跡描画・手動補正用）。
    void setTracker(ArtifactCore::MotionTracker* tracker);
    ArtifactCore::MotionTracker* tracker() const;

    /// 現在の再生フレーム時刻を設定する（軌跡の現在位置マーカ用）。
    void setCurrentFrame(double time);
    double currentFrame() const;

    /// キャンバス上に Inner/Outer Box + 軌跡 を描画する (Diligent ネイティブ)。
    void draw(ArtifactIRenderer* renderer);

    /// マウス座標から操作対象のハンドルを判定する。
    HandleType handleAtViewportPos(const QPointF& viewportPos, ArtifactIRenderer* renderer) const;

    Qt::CursorShape cursorShapeForViewportPos(const QPointF& viewportPos, ArtifactIRenderer* renderer) const;

    bool handleMousePress(const QPointF& viewportPos, ArtifactIRenderer* renderer);
    bool handleMouseMove(const QPointF& viewportPos, ArtifactIRenderer* renderer);
    void handleMouseRelease();

    bool isDragging() const;
    HandleType activeHandle() const;

    /// ドラッグ中の PathPoint インデックス（-1 の場合は非 PathPoint ドラッグ）。
    int draggedPathIndex() const;

    const TrackerBoxState& state() const;
    void setState(const TrackerBoxState& state);

private:
    HandleType hitTest(const QPointF& viewportPos, ArtifactIRenderer* renderer) const;

    static QRectF canvasRectForHandle(const QPointF& canvasPos, float halfSize, float zoom);

    class Impl;
    Impl* impl_;
};

} // namespace Artifact
