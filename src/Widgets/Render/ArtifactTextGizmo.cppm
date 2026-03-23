module;
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <vector>

module Artifact.Widgets.TextGizmo;

import Artifact.Render.IRenderer;
import Color.Float;

namespace Artifact {

TextGizmo::TextGizmo() {}
TextGizmo::~TextGizmo() {}

void TextGizmo::setLayer(ArtifactAbstractLayerPtr layer) {
    layer_ = layer;
}

void TextGizmo::draw(ArtifactIRenderer* renderer) {
    if (!layer_ || !renderer) return;
    
    // テキストレイヤー特有のプロパティを取得（仮実装）
    // 実際には layer_->textProperties() から RangeSelector や Glyphs を取得
    
    const float zoom = renderer->getZoom();
    const float invZoom = zoom > 0.0001f ? 1.0f / zoom : 1.0f;
    const float handleWidth = HANDLE_WIDTH * invZoom;
    const float rangeLineHeight = RANGE_LINE_HEIGHT * invZoom;
    
    // ベースラインの描画
    // ここで ArtifactCore::GlyphLayout から得られる座標を利用
    // (デモ用: 0,0 から 500,0 へのライン)
    FloatColor baselineColor{1.0f, 1.0f, 1.0f, 0.5f};
    renderer->drawSolidLine({0, 0}, {500.0f, 0}, baselineColor, 1.0f * invZoom);

    // AE風 範囲セレクター (Start / End)
    FloatColor selectorColor{1.0f, 0.2f, 0.2f, 1.0f}; // Reddish
    
    // Start Handle (0% 位置)
    float startX = 0.0f; 
    renderer->drawSolidLine({startX, -rangeLineHeight/2}, {startX, rangeLineHeight/2}, selectorColor, handleWidth);
    
    // End Handle (100% 位置)
    float endX = 500.0f;
    renderer->drawSolidLine({endX, -rangeLineHeight/2}, {endX, rangeLineHeight/2}, selectorColor, handleWidth);
}

TextGizmo::HandleType TextGizmo::hitTest(const QPointF& viewportPos, ArtifactIRenderer* renderer) const {
    if (!layer_ || !renderer) return HandleType::None;
    
    // マウス位置をキャンバス座標に変換
    auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
    
    // Startハンドル付近か判定
    if (std::abs(canvasMouse.x - 0.0f) < 10.0f) return HandleType::RangeStart;
    
    // Endハンドル付近か判定
    if (std::abs(canvasMouse.x - 500.0f) < 10.0f) return HandleType::RangeEnd;

    return HandleType::None;
}

Qt::CursorShape TextGizmo::cursorShapeForViewportPos(const QPointF& viewportPos, ArtifactIRenderer* renderer) const {
    HandleType handle = hitTest(viewportPos, renderer);
    if (handle == HandleType::RangeStart || handle == HandleType::RangeEnd) {
        return Qt::SizeHorCursor;
    }
    return Qt::ArrowCursor;
}

bool TextGizmo::handleMousePress(const QPointF& viewportPos, ArtifactIRenderer* renderer) {
    activeHandle_ = hitTest(viewportPos, renderer);
    if (activeHandle_ != HandleType::None) {
        isDragging_ = true;
        auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
        dragStartCanvasPos_ = QPointF(canvasMouse.x, canvasMouse.y);
        // 現在のセレクター値を保存
        return true;
    }
    return false;
}

bool TextGizmo::handleMouseMove(const QPointF& viewportPos, ArtifactIRenderer* renderer) {
    if (!isDragging_ || !renderer) return false;
    
    auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
    float deltaX = canvasMouse.x - dragStartCanvasPos_.x();
    
    // ここで実際のレイヤーの RangeSelector パラメータを更新する
    // 例: layer_->textAnimator().setStart(newStart);
    
    return true;
}

void TextGizmo::handleMouseRelease() {
    isDragging_ = false;
    activeHandle_ = HandleType::None;
}

} // namespace Artifact
