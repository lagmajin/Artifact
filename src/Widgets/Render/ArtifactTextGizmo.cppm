module;
#include <QFont>
#include <QString>
#include <utility>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <QVariant>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <memory>
#include <vector>

module Artifact.Widgets.TextGizmo;

import Artifact.Layer.Text;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Color.Float;
import Script.Expression.Evaluator;
import Undo.UndoManager;

namespace Artifact {

namespace {

ArtifactCore::AbstractPropertyPtr animatorProperty(
    const ArtifactCore::SharedPtr<ArtifactTextLayer>& layer,
    const int animatorIndex, const QString& suffix, const bool populate = true) {
    if (!layer || animatorIndex < 0 || animatorIndex >= layer->animatorCount()) {
        return {};
    }
    const QString path =
        QStringLiteral("text.animators.%1.%2").arg(animatorIndex).arg(suffix);
    auto property = layer->getProperty(path);
    if (!property && populate) {
        (void)layer->getLayerPropertyGroups();
        property = layer->getProperty(path);
    }
    return property;
}

ArtifactCore::RationalTime currentAnimatorTime(
    const ArtifactCore::SharedPtr<ArtifactTextLayer>& layer) {
    int64_t frame = layer ? layer->currentFrame() : 0;
    int64_t fps = 30;
    if (layer) {
        if (auto *composition = static_cast<ArtifactAbstractComposition *>(
                layer->composition())) {
            frame = composition->framePosition().framePosition();
            const double rate = composition->frameRate().framerate();
            fps = std::max<int64_t>(
                1, static_cast<int64_t>(std::llround(
                       std::isfinite(rate) && rate > 0.0 ? rate : 30.0)));
        }
    }
    return ArtifactCore::RationalTime(frame, fps);
}

float animatorPropertyValue(const ArtifactCore::SharedPtr<ArtifactTextLayer>& layer,
                            const int animatorIndex, const QString& suffix,
                            const float fallback = 0.0f) {
    const auto property = animatorProperty(layer, animatorIndex, suffix);
    if (!property) return fallback;
    QVariant propertyValue;
    if (property->hasExpression()) {
        ArtifactCore::ExpressionEvaluator evaluator;
        propertyValue = property->evaluateValue(currentAnimatorTime(layer),
                                                &evaluator);
    } else {
        propertyValue = property->evaluateValue(currentAnimatorTime(layer));
    }
    const float value = static_cast<float>(propertyValue.toDouble());
    return std::isfinite(value) ? value : fallback;
}

int editableAnimatorIndex(
    const ArtifactCore::SharedPtr<ArtifactTextLayer>& layer) {
    if (!layer) return -1;
    for (int index = 0; index < layer->animatorCount(); ++index) {
        const auto enabled = animatorProperty(
            layer, index, QStringLiteral("enabled"));
        const bool isEnabled = !enabled || enabled->getValue().toBool();
        const int units = static_cast<int>(animatorPropertyValue(
            layer, index, QStringLiteral("units"), 0.0f));
        if (isEnabled && units == 0) return index;
    }
    return -1;
}

bool hasEditablePercentageSelector(
    const ArtifactCore::SharedPtr<ArtifactTextLayer>& layer) {
    return editableAnimatorIndex(layer) >= 0;
}

class SetTextAnimatorPropertyCommand final : public UndoCommand {
public:
    SetTextAnimatorPropertyCommand(ArtifactAbstractLayerPtr layer, QString path,
                                   QVariant before, QVariant after)
        : layer_(layer), path_(std::move(path)), before_(std::move(before)),
          after_(std::move(after)) {}
    void undo() override { apply(before_); }
    void redo() override { apply(after_); }
    QString label() const override {
        return QStringLiteral("Edit Text Animator Range");
    }
private:
    void apply(const QVariant &value) {
        if (auto layer = layer_.lock()) {
            if (const auto property = layer->getProperty(path_)) {
                property->setValue(value);
            }
            layer->setLayerPropertyValue(path_, value);
            if (auto *manager = UndoManager::instance()) {
                manager->notifyAnythingChanged();
            }
        }
    }
    ArtifactAbstractLayerWeak layer_;
    QString path_;
    QVariant before_;
    QVariant after_;
};

float selectorHandleX(const QRectF& bounds, const float percentage) {
    return static_cast<float>(bounds.left()) +
           static_cast<float>(bounds.width()) *
               std::clamp(percentage, 0.0f, 100.0f) / 100.0f;
}

} // namespace

TextGizmo::TextGizmo() {}
TextGizmo::~TextGizmo() {}

void TextGizmo::setLayer(ArtifactAbstractLayerPtr layer) {
    layer_ = layer;
    isDragging_ = false;
    activeHandle_ = HandleType::None;
    dragStartCanvasPos_ = QPointF();
    dragStartLayerPosition_ = QPointF();
    dragStartBounds_ = QRectF();
    dragStartValue_ = 0.0f;
    dragCurrentValue_ = 0.0f;
    dragAnimatorIndex_ = -1;
    dragPropertyPath_.clear();
    dragBeforeKeyframes_.clear();
    dragValueChanged_ = false;
}

void TextGizmo::draw(ArtifactIRenderer* renderer) {
    if (!layer_ || !renderer) return;

    const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
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

    if (hasEditablePercentageSelector(textLayer)) {
        const int animatorIndex = editableAnimatorIndex(textLayer);
        const float start = animatorPropertyValue(
            textLayer, animatorIndex, QStringLiteral("start"));
        const float end = animatorPropertyValue(
            textLayer, animatorIndex, QStringLiteral("end"), 100.0f);
        const float offset = animatorPropertyValue(
            textLayer, animatorIndex, QStringLiteral("offset"));
        const float selectorY = static_cast<float>(bbox.top()) - 12.0f * invZoom;
        const float selectorHeight = 10.0f * invZoom;
        const float selectorHandleWidth = std::max(2.0f * invZoom, handleWidth);
        const float startX = selectorHandleX(bbox, start);
        const float endX = selectorHandleX(bbox, end);
        const float offsetX = selectorHandleX(
            bbox, (start + end) * 0.5f + offset);
        const FloatColor startColor{0.20f, 0.82f, 1.0f, 0.98f};
        const FloatColor endColor{1.0f, 0.42f, 0.24f, 0.98f};
        const FloatColor offsetColor{1.0f, 0.86f, 0.20f, 0.98f};
        renderer->drawSolidRect(startX - selectorHandleWidth * 0.5f,
                                selectorY, selectorHandleWidth,
                                selectorHeight, startColor);
        renderer->drawSolidRect(endX - selectorHandleWidth * 0.5f,
                                selectorY, selectorHandleWidth,
                                selectorHeight, endColor);
        renderer->drawSolidRect(offsetX - selectorHandleWidth,
                                selectorY + selectorHeight * 0.25f,
                                selectorHandleWidth * 2.0f,
                                selectorHeight * 0.5f, offsetColor);
        renderer->drawRectOutline(std::min(startX, endX),
                                  selectorY + selectorHeight * 0.45f,
                                  std::abs(endX - startX),
                                  std::max(invZoom, selectorHeight * 0.1f),
                                  FloatColor{0.65f, 0.78f, 0.92f, 0.85f});
    }

    const auto weightPreview = textLayer->selectorWeightPreview(24);
    if (!weightPreview.isEmpty()) {
        const float heatH = 4.0f * invZoom;
        const float heatGap = 5.0f * invZoom;
        const float heatY = bbox.top() - heatGap - heatH;
        const float stripW = bbox.width() / static_cast<float>(weightPreview.size());
        for (int i = 0; i < weightPreview.size(); ++i) {
            const float w = std::isfinite(weightPreview[i])
                ? std::clamp(weightPreview[i], 0.0f, 1.0f) : 0.0f;
            const FloatColor cool{0.12f, 0.20f, 0.32f, 0.75f};
            const FloatColor mid{0.95f, 0.45f, 0.10f, 0.80f};
            const FloatColor hot{1.00f, 0.92f, 0.25f, 0.90f};
            FloatColor color = (w < 0.5f)
                ? FloatColor(cool.r() + (mid.r() - cool.r()) * (w * 2.0f),
                             cool.g() + (mid.g() - cool.g()) * (w * 2.0f),
                             cool.b() + (mid.b() - cool.b()) * (w * 2.0f),
                             cool.a() + (mid.a() - cool.a()) * (w * 2.0f))
                : FloatColor(mid.r() + (hot.r() - mid.r()) * ((w - 0.5f) * 2.0f),
                             mid.g() + (hot.g() - mid.g()) * ((w - 0.5f) * 2.0f),
                             mid.b() + (hot.b() - mid.b()) * ((w - 0.5f) * 2.0f),
                             mid.a() + (hot.a() - mid.a()) * ((w - 0.5f) * 2.0f));
            const float x = bbox.left() + stripW * static_cast<float>(i);
            renderer->drawSolidRect(x, heatY, std::max(1.0f, stripW - 1.0f * invZoom), heatH, color);
        }
        renderer->drawRectOutline(bbox.left(), heatY, bbox.width(), heatH,
                                  FloatColor{0.9f, 0.9f, 0.95f, 0.5f});

        const auto clusterBoundaries = textLayer->selectorClusterBoundaryPreview();
        QFont labelFont(QStringLiteral("Segoe UI"));
        labelFont.setPointSizeF(static_cast<qreal>(std::max<float>(6.0f, 9.0f * invZoom)));
        for (int i = 0; i < clusterBoundaries.size(); ++i) {
            const float t = std::isfinite(clusterBoundaries[i])
                ? std::clamp(clusterBoundaries[i], 0.0f, 1.0f) : 0.0f;
            const float x = bbox.left() + bbox.width() * t;
            renderer->drawSolidRect(x, heatY - 2.0f * invZoom, 1.0f * invZoom,
                                    heatH + 4.0f * invZoom,
                                    FloatColor{0.95f, 0.90f, 0.25f, 0.90f});
            renderer->drawText(QRectF(x - 10.0f * invZoom, heatY - 10.0f * invZoom,
                                      20.0f * invZoom, 8.0f * invZoom),
                               QStringLiteral("%1").arg(i + 1), labelFont,
                               FloatColor{0.98f, 0.96f, 0.64f, 0.95f},
                               Qt::AlignHCenter | Qt::AlignVCenter);
        }

        const auto lineBoundaries = textLayer->selectorLineBoundaryPreview();
        for (int i = 0; i < lineBoundaries.size(); ++i) {
            const float t = std::isfinite(lineBoundaries[i])
                ? std::clamp(lineBoundaries[i], 0.0f, 1.0f) : 0.0f;
            const float x = bbox.left() + bbox.width() * t;
            renderer->drawSolidRect(x, heatY - 4.0f * invZoom, 1.5f * invZoom,
                                    heatH + 8.0f * invZoom,
                                    FloatColor{0.85f, 0.50f, 0.95f, 0.85f});
            renderer->drawText(QRectF(x - 12.0f * invZoom, heatY + heatH + 2.0f * invZoom,
                                      24.0f * invZoom, 8.0f * invZoom),
                               QStringLiteral("L%1").arg(i + 1), labelFont,
                               FloatColor{0.92f, 0.78f, 0.98f, 0.95f},
                               Qt::AlignHCenter | Qt::AlignVCenter);
        }

        const QString summary =
            QStringLiteral("%1 | %2")
                .arg(textLayer->selectorOverviewSummary(),
                     textLayer->selectorBoundarySummary());
        const QString flowLabel =
            textLayer->writingMode() == TextWritingMode::Vertical
                ? QStringLiteral("visual column order")
                : QStringLiteral("visual flow order");
        const float labelH = 9.0f * invZoom;
        const float labelY = heatY - labelH - 2.0f * invZoom;
        const float labelW = std::max<float>(24.0f * invZoom,
                                             static_cast<float>(bbox.width() * 0.25));
        renderer->drawText(QRectF(bbox.left(), labelY, labelW, labelH),
                           QStringLiteral("logical start"), labelFont,
                           FloatColor{0.92f, 0.95f, 1.0f, 0.95f},
                           Qt::AlignLeft | Qt::AlignVCenter);
        renderer->drawText(QRectF(bbox.right() - labelW, labelY, labelW, labelH),
                           QStringLiteral("logical end"), labelFont,
                           FloatColor{0.92f, 0.95f, 1.0f, 0.95f},
                           Qt::AlignRight | Qt::AlignVCenter);
        renderer->drawText(QRectF(bbox.left() + labelW, labelY, bbox.width() - labelW * 2.0f, labelH),
                           summary, labelFont,
                           FloatColor{1.0f, 0.82f, 0.35f, 0.98f},
                           Qt::AlignHCenter | Qt::AlignVCenter);
        renderer->drawText(QRectF(bbox.left(), heatY + heatH + 1.0f * invZoom,
                                  bbox.width(), labelH),
                           flowLabel, labelFont,
                           FloatColor{0.80f, 0.92f, 1.0f, 0.90f},
                           Qt::AlignHCenter | Qt::AlignVCenter);
    }

    // Side handles (optional, for now just corners)

}

TextGizmo::HandleType TextGizmo::hitTest(const QPointF& viewportPos, ArtifactIRenderer* renderer) const {
    if (!layer_ || !renderer) return HandleType::None;

    const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
    if (!textLayer) return HandleType::None;

    // マウス位置をキャンバス座標に変換
    auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});

    // Get text layer bounds
    QRectF bbox = layer_->transformedBoundingBox();
    if (bbox.isEmpty()) {
        bbox = QRectF(0, 0, 400, 100);
    }

    const float zoom = renderer->getZoom();
    const float hitThreshold = std::isfinite(zoom) && zoom > 0.0001f
        ? 10.0f / zoom : 10.0f;

    if (hasEditablePercentageSelector(textLayer)) {
        const int animatorIndex = editableAnimatorIndex(textLayer);
        const float start = animatorPropertyValue(
            textLayer, animatorIndex, QStringLiteral("start"));
        const float end = animatorPropertyValue(
            textLayer, animatorIndex, QStringLiteral("end"), 100.0f);
        const float offset = animatorPropertyValue(
            textLayer, animatorIndex, QStringLiteral("offset"));
        const float selectorY = static_cast<float>(bbox.top()) - 7.0f / std::max(zoom, 0.0001f);
        const bool selectorYHit =
            std::abs(canvasMouse.y - selectorY) < hitThreshold;
        if (selectorYHit) {
            const float startX = selectorHandleX(bbox, start);
            const float endX = selectorHandleX(bbox, end);
            const float offsetX = selectorHandleX(
                bbox, (start + end) * 0.5f + offset);
            if (std::abs(canvasMouse.x - offsetX) < hitThreshold) {
                return HandleType::RangeOffset;
            }
            if (std::abs(canvasMouse.x - startX) < hitThreshold) {
                return HandleType::RangeStart;
            }
            if (std::abs(canvasMouse.x - endX) < hitThreshold) {
                return HandleType::RangeEnd;
            }
        }
    }

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

    // The text gizmo replaces the regular transform gizmo while a text layer
    // is selected. Keep the body draggable so text positioning does not
    // depend on which gizmo happened to win the hit test.
    if (bbox.contains(QPointF(canvasMouse.x, canvasMouse.y))) {
        return HandleType::Offset;
    }

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
        case HandleType::RangeOffset:
            return Qt::SizeHorCursor;
        case HandleType::Offset:
            return isDragging_ ? Qt::ClosedHandCursor : Qt::OpenHandCursor;
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
        dragStartLayerPosition_ = QPointF(layer_->transform3D().positionX(),
                                          layer_->transform3D().positionY());
        dragStartBounds_ = layer_->transformedBoundingBox();
        if (dragStartBounds_.isEmpty()) {
            dragStartBounds_ = QRectF(0, 0, 400, 100);
        }
        if (activeHandle_ == HandleType::RangeStart) {
            dragAnimatorIndex_ = editableAnimatorIndex(
                ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_));
            dragStartValue_ = animatorPropertyValue(
                ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_),
                dragAnimatorIndex_, QStringLiteral("start"));
        } else if (activeHandle_ == HandleType::RangeEnd) {
            dragAnimatorIndex_ = editableAnimatorIndex(
                ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_));
            dragStartValue_ = animatorPropertyValue(
                ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_),
                dragAnimatorIndex_, QStringLiteral("end"), 100.0f);
        } else if (activeHandle_ == HandleType::RangeOffset) {
            dragAnimatorIndex_ = editableAnimatorIndex(
                ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_));
            dragStartValue_ = animatorPropertyValue(
                ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_),
                dragAnimatorIndex_, QStringLiteral("offset"));
        }
        if (dragAnimatorIndex_ >= 0) {
            QString suffix;
            if (activeHandle_ == HandleType::RangeStart) {
                suffix = QStringLiteral("start");
            } else if (activeHandle_ == HandleType::RangeEnd) {
                suffix = QStringLiteral("end");
            } else if (activeHandle_ == HandleType::RangeOffset) {
                suffix = QStringLiteral("offset");
            }
            if (!suffix.isEmpty()) {
                dragPropertyPath_ = QStringLiteral("text.animators.%1.%2")
                    .arg(dragAnimatorIndex_).arg(suffix);
                if (const auto property = animatorProperty(
                        ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_),
                        dragAnimatorIndex_, suffix)) {
                    if (property->hasExpression()) {
                        isDragging_ = false;
                        activeHandle_ = HandleType::None;
                        dragAnimatorIndex_ = -1;
                        dragPropertyPath_.clear();
                        return false;
                    }
                    dragBeforeKeyframes_ = property->getKeyFrames();
                }
                dragCurrentValue_ = dragStartValue_;
            }
        }
        return true;
    }
    return false;
}

bool TextGizmo::handleMouseMove(const QPointF& viewportPos, ArtifactIRenderer* renderer) {
    if (!isDragging_ || !layer_ || !renderer) return false;

    const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
    if (!textLayer) return false;

    auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
    float deltaX = canvasMouse.x - dragStartCanvasPos_.x();
    float deltaY = canvasMouse.y - dragStartCanvasPos_.y();
    if (!std::isfinite(canvasMouse.x) || !std::isfinite(canvasMouse.y) ||
        !std::isfinite(deltaX) || !std::isfinite(deltaY) ||
        std::abs(deltaX) > 1.0e9f || std::abs(deltaY) > 1.0e9f) {
        return false;
    }

    QRectF bbox = dragStartBounds_;

    if (activeHandle_ == HandleType::RangeStart ||
        activeHandle_ == HandleType::RangeEnd ||
        activeHandle_ == HandleType::RangeOffset) {
        if (bbox.width() <= 0.0001) return false;
        const float deltaPercent =
            deltaX / static_cast<float>(bbox.width()) * 100.0f;
        QString suffix;
        if (activeHandle_ == HandleType::RangeStart) {
            suffix = QStringLiteral("start");
        } else if (activeHandle_ == HandleType::RangeEnd) {
            suffix = QStringLiteral("end");
        } else {
            suffix = QStringLiteral("offset");
        }
        const float nextValue = std::clamp(
            dragStartValue_ + deltaPercent, -100000.0f, 100000.0f);
        dragCurrentValue_ = nextValue;
        dragValueChanged_ = dragValueChanged_ ||
            std::abs(dragCurrentValue_ - dragStartValue_) > 0.0001f;
        if (!dragBeforeKeyframes_.empty()) {
            const auto property = animatorProperty(
                textLayer, dragAnimatorIndex_, suffix);
            if (!property) return false;
            const RationalTime editTime = currentAnimatorTime(textLayer);
            const auto existing = std::find_if(
                dragBeforeKeyframes_.cbegin(), dragBeforeKeyframes_.cend(),
                [&editTime](const KeyFrame &keyframe) {
                    return keyframe.time == editTime;
                });
            if (existing != dragBeforeKeyframes_.cend()) {
                property->addKeyFrame(editTime, nextValue,
                                      existing->interpolation,
                                      existing->cp1_x, existing->cp1_y,
                                      existing->cp2_x, existing->cp2_y,
                                      existing->roving);
                property->setKeyFrameAnchorAt(editTime, existing->anchor);
                property->setKeyFrameColorLabelAt(editTime,
                                                   existing->colorLabel);
            } else {
                property->addKeyFrame(editTime, nextValue,
                                      InterpolationType::Linear);
            }
        } else {
            if (const auto property = animatorProperty(
                    textLayer, dragAnimatorIndex_, suffix)) {
                property->setValue(nextValue);
            }
            textLayer->setLayerPropertyValue(dragPropertyPath_, nextValue);
        }
        textLayer->updateImage();
        textLayer->changed();
        return true;
    }

    switch (activeHandle_) {
        case HandleType::Offset: {
            // Write the edit at the layer's current frame.  Using frame 0
            // here made viewport drags silently alter a different time than
            // the one currently being edited.
            auto* composition = static_cast<ArtifactAbstractComposition*>(
                textLayer->composition());
            const double fps = composition
                ? composition->frameRate().framerate() : 30.0;
            const int64_t timeScale = std::max<int64_t>(
                1, static_cast<int64_t>(std::llround(
                    std::isfinite(fps) && fps > 0.0 ? fps : 30.0)));
            const auto frame = ArtifactCore::RationalTime(
                static_cast<int64_t>(textLayer->currentFrame()), timeScale);
            auto &start = textLayer->transform3D();
            start.setPosition(frame,
                              static_cast<float>(dragStartLayerPosition_.x() + deltaX),
                              static_cast<float>(dragStartLayerPosition_.y() + deltaY));
            textLayer->setDirty(LayerDirtyFlag::Transform);
            textLayer->changed();
            break;
        }
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

    constexpr qreal kMinimumTextBoxExtent = 1.0;
    if (bbox.width() < kMinimumTextBoxExtent) {
        if (activeHandle_ == HandleType::BoxLeft ||
            activeHandle_ == HandleType::BoxCornerTopLeft ||
            activeHandle_ == HandleType::BoxCornerBottomLeft) {
            bbox.setLeft(bbox.right() - kMinimumTextBoxExtent);
        } else {
            bbox.setRight(bbox.left() + kMinimumTextBoxExtent);
        }
    }
    if (bbox.height() < kMinimumTextBoxExtent) {
        if (activeHandle_ == HandleType::BoxTop ||
            activeHandle_ == HandleType::BoxCornerTopLeft ||
            activeHandle_ == HandleType::BoxCornerTopRight) {
            bbox.setTop(bbox.bottom() - kMinimumTextBoxExtent);
        } else {
            bbox.setBottom(bbox.top() + kMinimumTextBoxExtent);
        }
    }

    // Update text layer properties based on new bounds
    // Assuming bounds represent maxWidth and boxHeight
    textLayer->setMaxWidth(std::max(1.0f, static_cast<float>(bbox.width())));
    textLayer->setBoxHeight(std::max(1.0f, static_cast<float>(bbox.height())));

    // Update position if needed (simplified)
    // layer_->transform2D().setPosition(...)

    textLayer->setDirty();
    textLayer->updateImage();
    textLayer->changed();

    return true;
}

void TextGizmo::handleMouseRelease() {
    if (dragValueChanged_ && !dragPropertyPath_.isEmpty() && layer_) {
        const auto textLayer =
            ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
        if (auto *manager = UndoManager::instance()) {
            if (!dragBeforeKeyframes_.empty()) {
                const QString suffix = dragPropertyPath_.mid(
                    dragPropertyPath_.lastIndexOf(QLatin1Char('.')) + 1);
                if (const auto property = animatorProperty(
                        textLayer, dragAnimatorIndex_, suffix)) {
                    auto command = std::make_unique<SetLayerPropertyKeyframesCommand>(
                        layer_, dragPropertyPath_, dragBeforeKeyframes_,
                        property->getKeyFrames(),
                        QStringLiteral("Edit Text Animator Range"));
                    if (command->estimatedMemoryBytes() >
                        manager->budget().maxSingleEntryBytes) {
                        command->undo();
                    } else {
                        manager->push(std::move(command));
                    }
                }
            } else {
                auto command = std::make_unique<SetTextAnimatorPropertyCommand>(
                    layer_, dragPropertyPath_, dragStartValue_,
                    dragCurrentValue_);
                if (command->estimatedMemoryBytes() >
                    manager->budget().maxSingleEntryBytes) {
                    command->undo();
                } else {
                    manager->push(std::move(command));
                }
            }
        }
    }
    isDragging_ = false;
    activeHandle_ = HandleType::None;
    dragAnimatorIndex_ = -1;
    dragPropertyPath_.clear();
    dragBeforeKeyframes_.clear();
    dragValueChanged_ = false;
}

} // namespace Artifact
