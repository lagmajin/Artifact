module;

#include <QPointF>
#include <QRectF>
#include <QCursor>
#include <cmath>
#include <algorithm>
#include <vector>

module Artifact.Widgets.PointTrackerGizmo;

import Artifact.Render.IRenderer;
import Artifact.Layer.Abstract;
import Artifact.Widgets.CompositionRenderOverlay;
import Tracking.MotionTracker;
import Color.Float;

namespace Artifact {

// ============================================================================
// ArtifactPointTrackerGizmo::Impl
// ============================================================================

class ArtifactPointTrackerGizmo::Impl {
public:
    ArtifactAbstractLayerPtr layer;
    TrackerBoxState state;
    HandleType activeHandle = HandleType::None;
    bool isDragging = false;
    QPointF dragStartViewport;
    QPointF dragStartCenter;
    float dragStartInnerHalfW = 0.0f;
    float dragStartInnerHalfH = 0.0f;

    // MotionTracker 参照 (所有しない)
    ArtifactCore::MotionTracker* tracker = nullptr;
    double currentTime = 0.0;

    // PathPoint ヒットテスト・ドラッグ用
    int hitPathIndex = -1;
    int draggedPathIndex = -1;
    QPointF dragStartPathPos;
};

// ============================================================================
// ArtifactPointTrackerGizmo
// ============================================================================

ArtifactPointTrackerGizmo::ArtifactPointTrackerGizmo()
    : impl_(new Impl()) {}

ArtifactPointTrackerGizmo::~ArtifactPointTrackerGizmo() {
    delete impl_;
}

void ArtifactPointTrackerGizmo::setLayer(ArtifactAbstractLayerPtr layer) {
    impl_->layer = layer;
}

ArtifactAbstractLayerPtr ArtifactPointTrackerGizmo::layer() const {
    return impl_->layer;
}

void ArtifactPointTrackerGizmo::setTracker(ArtifactCore::MotionTracker* tracker) {
    impl_->tracker = tracker;
}

ArtifactCore::MotionTracker* ArtifactPointTrackerGizmo::tracker() const {
    return impl_->tracker;
}

void ArtifactPointTrackerGizmo::setCurrentFrame(double time) {
    impl_->currentTime = time;
}

double ArtifactPointTrackerGizmo::currentFrame() const {
    return impl_->currentTime;
}

const ArtifactPointTrackerGizmo::TrackerBoxState& ArtifactPointTrackerGizmo::state() const {
    return impl_->state;
}

void ArtifactPointTrackerGizmo::setState(const TrackerBoxState& state) {
    impl_->state = state;
}

// ============================================================================
// 描画 (Diligent ネイティブ)
// ============================================================================

void ArtifactPointTrackerGizmo::draw(ArtifactIRenderer* renderer) {
    if (!renderer) return;

    const float zoom = std::max(0.001f, renderer->getZoom());
    const float invZoom = 1.0f / zoom;

    // 太さを zoom 不変にする
    const float lineThickness = 1.5f * invZoom;
    const float handleSize = 4.5f * invZoom;

    const auto& st = impl_->state;
    const float cx = static_cast<float>(st.innerCenter.x());
    const float cy = static_cast<float>(st.innerCenter.y());

    // --- Outer Box (探索領域): 水色の破線 ---
    {
        const float ox = cx - st.outerHalfW;
        const float oy = cy - st.outerHalfH;
        const float ow = st.outerHalfW * 2.0f;
        const float oh = st.outerHalfH * 2.0f;
        const FloatColor outerColor{0.2f, 0.75f, 1.0f, 0.65f};
        renderer->drawDashedRectOutline(ox, oy, ow, oh, outerColor, lineThickness, 6.0f * invZoom, 4.0f * invZoom);
    }

    // --- Inner Box (特徴領域): 黄色の実線 ---
    {
        const float ix = cx - st.innerHalfW;
        const float iy = cy - st.innerHalfH;
        const float iw = st.innerHalfW * 2.0f;
        const float ih = st.innerHalfH * 2.0f;
        const FloatColor innerColor{1.0f, 0.92f, 0.15f, 0.9f};
        renderer->drawRectOutline(ix, iy, iw, ih, innerColor);

        // コーナーハンドル (Inner Box の4角に小さな四角)
        const FloatColor handleColor{1.0f, 0.6f, 0.1f, 1.0f};
        renderer->drawPoint(ix, iy, handleSize, handleColor);                   // TL
        renderer->drawPoint(ix + iw, iy, handleSize, handleColor);              // TR
        renderer->drawPoint(ix, iy + ih, handleSize, handleColor);             // BL
        renderer->drawPoint(ix + iw, iy + ih, handleSize, handleColor);        // BR
    }

    // --- 中心点: native pin ---
    {
        const FloatColor fillColor{1.0f, 0.38f, 0.28f, 1.0f};
        const FloatColor accentColor{1.0f, 0.72f, 0.36f, 1.0f};
        drawTrackerPinOverlay(renderer, cx, cy, std::max(10.0f, 11.5f * invZoom),
                              fillColor, accentColor, false);
    }

    // --- モーションパス軌跡 (tracker 参照がある場合) ---
    if (impl_->tracker && impl_->tracker->hasResult()) {
        // 最初のトラッキングポイント(=pointId 0) の軌跡を描画
        const auto path = impl_->tracker->motionPath(0);
        if (path.size() >= 2) {
            std::vector<Detail::float2> pts;
            pts.reserve(path.size());
            for (const auto& p : path) {
                pts.emplace_back(static_cast<float>(p.x()), static_cast<float>(p.y()));
            }
            // 軌跡: 緑の半透明ライン
            const FloatColor pathColor{0.2f, 0.95f, 0.4f, 0.55f};
            renderer->drawPolyline(pts, pathColor, 1.5f * invZoom);

            // 各パスポイントを native pin で描く
            const FloatColor dotFill{0.2f, 0.85f, 0.4f, 0.88f};
            const FloatColor dotAccent{0.45f, 1.0f, 0.62f, 1.0f};
            const float dotSize = 5.6f * invZoom;
            for (const auto& p : pts) {
                drawTrackerPinOverlay(renderer, p.x, p.y, dotSize, dotFill,
                                      dotAccent, false);
            }
        }
    }
}

// ============================================================================
// ヒットテスト
// ============================================================================

QRectF ArtifactPointTrackerGizmo::canvasRectForHandle(const QPointF& canvasPos, float halfSize, float zoom) {
    const float screenHalf = halfSize / zoom + 4.0f;
    return QRectF(canvasPos.x() - screenHalf, canvasPos.y() - screenHalf,
                  screenHalf * 2.0f, screenHalf * 2.0f);
}

ArtifactPointTrackerGizmo::HandleType ArtifactPointTrackerGizmo::hitTest(
    const QPointF& viewportPos, ArtifactIRenderer* renderer) const
{
    if (!renderer) return HandleType::None;

    const float zoom = std::max(0.001f, renderer->getZoom());
    const auto canvasMouse = renderer->viewportToCanvas(
        {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
    const QPointF mouseCanvas(canvasMouse.x, canvasMouse.y);

    const auto& st = impl_->state;
    const float cx = static_cast<float>(st.innerCenter.x());
    const float cy = static_cast<float>(st.innerCenter.y());

    // Inner Box のコーナーを確認
    const float ix1 = cx - st.innerHalfW;
    const float iy1 = cy - st.innerHalfH;
    const float ix2 = cx + st.innerHalfW;
    const float iy2 = cy + st.innerHalfH;

    struct Corner { QPointF pos; HandleType type; };
    std::vector<Corner> corners = {
        {{ix1, iy1}, HandleType::InnerScale_TL},
        {{ix2, iy1}, HandleType::InnerScale_TR},
        {{ix1, iy2}, HandleType::InnerScale_BL},
        {{ix2, iy2}, HandleType::InnerScale_BR}
    };

    for (const auto& c : corners) {
        if (canvasRectForHandle(c.pos, 4.0f, zoom).contains(mouseCanvas)) {
            return c.type;
        }
    }

    // Inner Box の辺を確認
    const float edgeThreshold = 5.0f / zoom;
    if (std::abs(mouseCanvas.y() - iy1) < edgeThreshold && mouseCanvas.x() > ix1 && mouseCanvas.x() < ix2)
        return HandleType::InnerScale_T;
    if (std::abs(mouseCanvas.y() - iy2) < edgeThreshold && mouseCanvas.x() > ix1 && mouseCanvas.x() < ix2)
        return HandleType::InnerScale_B;
    if (std::abs(mouseCanvas.x() - ix1) < edgeThreshold && mouseCanvas.y() > iy1 && mouseCanvas.y() < iy2)
        return HandleType::InnerScale_L;
    if (std::abs(mouseCanvas.x() - ix2) < edgeThreshold && mouseCanvas.y() > iy1 && mouseCanvas.y() < iy2)
        return HandleType::InnerScale_R;

    // Inner Box 内部 → Move
    if (mouseCanvas.x() >= ix1 && mouseCanvas.x() <= ix2 &&
        mouseCanvas.y() >= iy1 && mouseCanvas.y() <= iy2) {
        return HandleType::InnerMove;
    }

    // モーションパス上のポイントを確認 (tracker 参照がある場合)
    if (impl_->tracker && impl_->tracker->hasResult()) {
        const auto path = impl_->tracker->motionPath(0);
        const float pathHitRadius = 6.0f / zoom;
        for (int i = 0; i < static_cast<int>(path.size()); ++i) {
            const float dx = mouseCanvas.x() - path[i].x();
            const float dy = mouseCanvas.y() - path[i].y();
            if (dx * dx + dy * dy < pathHitRadius * pathHitRadius) {
                // PathPoint のインデックスを記録 (const cast で const メソッド内で一時保存)
                const_cast<Impl*>(impl_)->hitPathIndex = i;
                return HandleType::PathPoint;
            }
        }
    }

    return HandleType::None;
}

// ============================================================================
// カーソル形状
// ============================================================================

Qt::CursorShape ArtifactPointTrackerGizmo::cursorShapeForViewportPos(
    const QPointF& viewportPos, ArtifactIRenderer* renderer) const
{
    const HandleType ht = handleAtViewportPos(viewportPos, renderer);
    switch (ht) {
    case HandleType::InnerScale_TL:
    case HandleType::InnerScale_BR:
        return Qt::SizeFDiagCursor;
    case HandleType::InnerScale_TR:
    case HandleType::InnerScale_BL:
        return Qt::SizeBDiagCursor;
    case HandleType::InnerScale_T:
    case HandleType::InnerScale_B:
        return Qt::SizeVerCursor;
    case HandleType::InnerScale_L:
    case HandleType::InnerScale_R:
        return Qt::SizeHorCursor;
    case HandleType::InnerMove:
        return Qt::SizeAllCursor;
    case HandleType::PathPoint:
        return Qt::PointingHandCursor;
    default:
        return Qt::ArrowCursor;
    }
}

// ============================================================================
// マウス操作
// ============================================================================

ArtifactPointTrackerGizmo::HandleType ArtifactPointTrackerGizmo::handleAtViewportPos(
    const QPointF& viewportPos, ArtifactIRenderer* renderer) const
{
    return hitTest(viewportPos, renderer);
}

bool ArtifactPointTrackerGizmo::isDragging() const {
    return impl_->isDragging;
}

ArtifactPointTrackerGizmo::HandleType ArtifactPointTrackerGizmo::activeHandle() const {
    return impl_->activeHandle;
}

int ArtifactPointTrackerGizmo::draggedPathIndex() const {
    return impl_->draggedPathIndex;
}

bool ArtifactPointTrackerGizmo::handleMousePress(const QPointF& viewportPos, ArtifactIRenderer* renderer) {
    const HandleType ht = hitTest(viewportPos, renderer);
    if (ht == HandleType::None) return false;

    impl_->activeHandle = ht;
    impl_->isDragging = true;
    impl_->dragStartViewport = viewportPos;

    if (ht == HandleType::PathPoint && impl_->tracker && impl_->tracker->hasResult()) {
        // PathPoint ドラッグ開始
        const auto path = impl_->tracker->motionPath(0);
        const int idx = impl_->hitPathIndex;
        if (idx >= 0 && idx < static_cast<int>(path.size())) {
            impl_->draggedPathIndex = idx;
            impl_->dragStartPathPos = path[idx];
        }
    } else {
        impl_->dragStartCenter = impl_->state.innerCenter;
        impl_->dragStartInnerHalfW = impl_->state.innerHalfW;
        impl_->dragStartInnerHalfH = impl_->state.innerHalfH;
    }
    return true;
}

bool ArtifactPointTrackerGizmo::handleMouseMove(const QPointF& viewportPos, ArtifactIRenderer* renderer) {
    if (!impl_->isDragging || impl_->activeHandle == HandleType::None) return false;
    if (!renderer) return false;

    const float zoom = std::max(0.001f, renderer->getZoom());

    // ビューポートの差分をキャンバス座標に変換
    const float dxViewport = static_cast<float>(viewportPos.x() - impl_->dragStartViewport.x());
    const float dyViewport = static_cast<float>(viewportPos.y() - impl_->dragStartViewport.y());
    const float dxCanvas = dxViewport / zoom;
    const float dyCanvas = dyViewport / zoom;

    auto& st = impl_->state;

    // PathPoint ドラッグ: 即時ビジュアルフィードバック用に補正を適用
    if (impl_->activeHandle == HandleType::PathPoint &&
        impl_->draggedPathIndex >= 0 &&
        impl_->tracker && impl_->tracker->hasResult()) {
        const QPointF correctedPos(
            impl_->dragStartPathPos.x() + dxCanvas,
            impl_->dragStartPathPos.y() + dyCanvas);
        // ドラッグ中は補正を即時適用（release で確定）
        const auto result = impl_->tracker->result();
        const auto& frames = result.frames;
        const int idx = impl_->draggedPathIndex;
        if (idx < static_cast<int>(frames.size())) {
            impl_->tracker->applyCorrection(frames[idx].time, 0, correctedPos);
        }
        return true;
    }

    switch (impl_->activeHandle) {
    case HandleType::InnerMove:
        st.innerCenter = QPointF(
            impl_->dragStartCenter.x() + dxCanvas,
            impl_->dragStartCenter.y() + dyCanvas);
        break;

    case HandleType::InnerScale_TL:
        st.innerHalfW = std::max(4.0f, impl_->dragStartInnerHalfW - dxCanvas);
        st.innerHalfH = std::max(4.0f, impl_->dragStartInnerHalfH - dyCanvas);
        st.innerCenter = QPointF(
            impl_->dragStartCenter.x() + dxCanvas * 0.5f,
            impl_->dragStartCenter.y() + dyCanvas * 0.5f);
        break;

    case HandleType::InnerScale_TR:
        st.innerHalfW = std::max(4.0f, impl_->dragStartInnerHalfW + dxCanvas);
        st.innerHalfH = std::max(4.0f, impl_->dragStartInnerHalfH - dyCanvas);
        st.innerCenter = QPointF(
            impl_->dragStartCenter.x() + dxCanvas * 0.5f,
            impl_->dragStartCenter.y() + dyCanvas * 0.5f);
        break;

    case HandleType::InnerScale_BL:
        st.innerHalfW = std::max(4.0f, impl_->dragStartInnerHalfW - dxCanvas);
        st.innerHalfH = std::max(4.0f, impl_->dragStartInnerHalfH + dyCanvas);
        st.innerCenter = QPointF(
            impl_->dragStartCenter.x() + dxCanvas * 0.5f,
            impl_->dragStartCenter.y() + dyCanvas * 0.5f);
        break;

    case HandleType::InnerScale_BR:
        st.innerHalfW = std::max(4.0f, impl_->dragStartInnerHalfW + dxCanvas);
        st.innerHalfH = std::max(4.0f, impl_->dragStartInnerHalfH + dyCanvas);
        break;

    case HandleType::InnerScale_T:
        st.innerHalfH = std::max(4.0f, impl_->dragStartInnerHalfH - dyCanvas);
        st.innerCenter = QPointF(
            impl_->dragStartCenter.x(),
            impl_->dragStartCenter.y() + dyCanvas * 0.5f);
        break;

    case HandleType::InnerScale_B:
        st.innerHalfH = std::max(4.0f, impl_->dragStartInnerHalfH + dyCanvas);
        st.innerCenter = QPointF(
            impl_->dragStartCenter.x(),
            impl_->dragStartCenter.y() + dyCanvas * 0.5f);
        break;

    case HandleType::InnerScale_L:
        st.innerHalfW = std::max(4.0f, impl_->dragStartInnerHalfW - dxCanvas);
        st.innerCenter = QPointF(
            impl_->dragStartCenter.x() + dxCanvas * 0.5f,
            impl_->dragStartCenter.y());
        break;

    case HandleType::InnerScale_R:
        st.innerHalfW = std::max(4.0f, impl_->dragStartInnerHalfW + dxCanvas);
        st.innerCenter = QPointF(
            impl_->dragStartCenter.x() + dxCanvas * 0.5f,
            impl_->dragStartCenter.y());
        break;

    default:
        break;
    }

    // Outer Box は Inner Box の2倍を維持
    st.outerHalfW = st.innerHalfW * 2.0f;
    st.outerHalfH = st.innerHalfH * 2.0f;

    return true;
}

void ArtifactPointTrackerGizmo::handleMouseRelease() {
    // PathPoint ドラッグの後始末
    // (補正は handleMouseMove で即時適用済み)
    impl_->isDragging = false;
    impl_->activeHandle = HandleType::None;
    impl_->draggedPathIndex = -1;
    impl_->hitPathIndex = -1;
}

} // namespace Artifact
