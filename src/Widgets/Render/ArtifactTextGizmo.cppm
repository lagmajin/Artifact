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

    const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer_);
    if (!textLayer) return;

    const float zoom = renderer->getZoom();
    const float invZoom = zoom > 0.0001f ? 1.0f / zoom : 1.0f;
    const float handleWidth = HANDLE_WIDTH * invZoom;
    const float rangeLineHeight = RANGE_LINE_HEIGHT * invZoom;

    // Get text layer bounds
    QRectF bbox = layer_->transformedBoundingBox();
    if (bbox.isEmpty()) {
        bbox = QRectF(0, 0, 400, 100);
    }

    // Draw text box bounds
    FloatColor boundsColor{0.5f, 0.8f, 1.0f, 0.8f}; // Light blue
    renderer->drawRectOutline(bbox.left(), bbox.top(), bbox.width(), bbox.height(), boundsColor);

    // Draw resize handles
    FloatColor handleColor{0.5f, 0.8f, 1.0f, 1.0f};
    const float handleSize = 6.0f * invZoom;

    // Corner handles
    renderer->drawSolidRect(bbox.left() - handleSize/2, bbox.top() - handleSize/2, handleSize, handleSize, handleColor);
    renderer->drawSolidRect(bbox.right() - handleSize/2, bbox.top() - handleSize/2, handleSize, handleSize, handleColor);
    renderer->drawSolidRect(bbox.left() - handleSize/2, bbox.bottom() - handleSize/2, handleSize, handleSize, handleColor);
    renderer->drawSolidRect(bbox.right() - handleSize/2, bbox.bottom() - handleSize/2, handleSize, handleSize, handleColor);

    // Side handles (optional, for now just corners)

    // If text animator is present, draw range selectors (legacy)
    // ... existing code for range selectors if needed
}

TextGizmo::HandleType TextGizmo::hitTest(const QPointF& viewportPos, ArtifactIRenderer* renderer) const {
    if (!layer_ || !renderer) return HandleType::None;

    const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer_);
    if (!textLayer) return HandleType::None;

    // マウス位置をキャンバス座標に変換
    auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});

    // Get text layer bounds
    QRectF bbox = layer_->transformedBoundingBox();
    if (bbox.isEmpty()) {
        bbox = QRectF(0, 0, 400, 100);
    }

    const float hitThreshold = 10.0f / renderer->getZoom();

    // Check corner handles
    if (std::abs(canvasMouse.x - bbox.left()) < hitThreshold && std::abs(canvasMouse.y - bbox.top()) < hitThreshold) {
        return HandleType::BoxCornerTopLeft;
    }
    if (std::abs(canvasMouse.x - bbox.right()) < hitThreshold && std::abs(canvasMouse.y - bbox.top()) < hitThreshold) {
        return HandleType::BoxCornerTopRight;
    }
    if (std::abs(canvasMouse.x - bbox.left()) < hitThreshold && std::abs(canvasMouse.y - bbox.bottom()) < hitThreshold) {
        return HandleType::BoxCornerBottomLeft;
    }
    if (std::abs(canvasMouse.x - bbox.right()) < hitThreshold && std::abs(canvasMouse.y - bbox.bottom()) < hitThreshold) {
        return HandleType::BoxCornerBottomRight;
    }

    // Check side handles (simplified, just check if on edges)
    if (std::abs(canvasMouse.x - bbox.left()) < hitThreshold && canvasMouse.y > bbox.top() && canvasMouse.y < bbox.bottom()) {
        return HandleType::BoxLeft;
    }
    if (std::abs(canvasMouse.x - bbox.right()) < hitThreshold && canvasMouse.y > bbox.top() && canvasMouse.y < bbox.bottom()) {
        return HandleType::BoxRight;
    }
    if (std::abs(canvasMouse.y - bbox.top()) < hitThreshold && canvasMouse.x > bbox.left() && canvasMouse.x < bbox.right()) {
        return HandleType::BoxTop;
    }
    if (std::abs(canvasMouse.y - bbox.bottom()) < hitThreshold && canvasMouse.x > bbox.left() && canvasMouse.x < bbox.right()) {
        return HandleType::BoxBottom;
    }

    // Legacy range selector hits if no bounds hit
    // ... existing code for range selectors if needed

    return HandleType::None;
}

Qt::CursorShape TextGizmo::cursorShapeForViewportPos(const QPointF& viewportPos, ArtifactIRenderer* renderer) const {
    HandleType handle = hitTest(viewportPos, renderer);
    switch (handle) {
        case HandleType::BoxLeft:
        case HandleType::BoxRight:
            return Qt::SizeHorCursor;
        case HandleType::BoxTop:
        case HandleType::BoxBottom:
            return Qt::SizeVerCursor;
        case HandleType::BoxCornerTopLeft:
        case HandleType::BoxCornerBottomRight:
            return Qt::SizeFDiagCursor;
        case HandleType::BoxCornerTopRight:
        case HandleType::BoxCornerBottomLeft:
            return Qt::SizeBDiagCursor;
        case HandleType::RangeStart:
        case HandleType::RangeEnd:
            return Qt::SizeHorCursor;
        default:
            return Qt::ArrowCursor;
    }
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
    if (!isDragging_ || !layer_ || !renderer) return false;

    const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer_);
    if (!textLayer) return false;

    auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
    float deltaX = canvasMouse.x - dragStartCanvasPos_.x();
    float deltaY = canvasMouse.y - dragStartCanvasPos_.y();

    QRectF bbox = layer_->transformedBoundingBox();
    if (bbox.isEmpty()) {
        bbox = QRectF(0, 0, 400, 100);
    }

    switch (activeHandle_) {
        case HandleType::BoxLeft:
            bbox.setLeft(bbox.left() + deltaX);
            break;
        case HandleType::BoxRight:
            bbox.setRight(bbox.right() + deltaX);
            break;
        case HandleType::BoxTop:
            bbox.setTop(bbox.top() + deltaY);
            break;
        case HandleType::BoxBottom:
            bbox.setBottom(bbox.bottom() + deltaY);
            break;
        case HandleType::BoxCornerTopLeft:
            bbox.setTopLeft(bbox.topLeft() + QPointF(deltaX, deltaY));
            break;
        case HandleType::BoxCornerTopRight:
            bbox.setTopRight(bbox.topRight() + QPointF(deltaX, deltaY));
            break;
        case HandleType::BoxCornerBottomLeft:
            bbox.setBottomLeft(bbox.bottomLeft() + QPointF(deltaX, deltaY));
            break;
        case HandleType::BoxCornerBottomRight:
            bbox.setBottomRight(bbox.bottomRight() + QPointF(deltaX, deltaY));
            break;
        default:
            return false;
    }

    // Update text layer properties based on new bounds
    // Assuming bounds represent maxWidth and boxHeight
    textLayer->setMaxWidth(std::max(1.0f, static_cast<float>(bbox.width())));
    textLayer->setBoxHeight(std::max(1.0f, static_cast<float>(bbox.height())));

    // Update position if needed (simplified)
    // layer_->transform2D().setPosition(...)

    textLayer->setDirty();
    textLayer->updateImage();

    return true;
}

void TextGizmo::handleMouseRelease() {
    isDragging_ = false;
    activeHandle_ = HandleType::None;
}

} // namespace Artifact
