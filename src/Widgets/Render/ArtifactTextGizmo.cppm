module;
#include <QFont>
#include <QString>
#include <QGuiApplication>
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
import Text.LayoutContract;
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
    void undo() override { lastOperationSucceeded_ = apply(before_); }
    void redo() override { lastOperationSucceeded_ = apply(after_); }
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override {
        return QStringLiteral("Edit Text Animator Range");
    }
private:
    bool apply(const QVariant &value) {
        auto layer = layer_.lock();
        if (!layer) return false;
        const auto property = layer->getProperty(path_);
        const QVariant previous = property ? property->getValue() : QVariant();
        if (property) property->setValue(value);
        if (!layer->setLayerPropertyValue(path_, value)) {
            if (property) {
                property->setValue(previous);
                layer->setLayerPropertyValue(path_, previous);
            }
            return false;
        }
        if (property && property->getValue() != value) {
            property->setValue(previous);
            layer->setLayerPropertyValue(path_, previous);
            return false;
        }
        if (auto *manager = UndoManager::instance()) {
            manager->notifyAnythingChanged();
        }
        return true;
    }
    ArtifactAbstractLayerWeak layer_;
    QString path_;
    QVariant before_;
    QVariant after_;
    bool lastOperationSucceeded_ = true;
};

float selectorHandleX(const QRectF& bounds, const float percentage) {
    return static_cast<float>(bounds.left()) +
           static_cast<float>(bounds.width()) *
               std::clamp(percentage, 0.0f, 100.0f) / 100.0f;
}

float selectorHandleY(const QRectF& bounds, const float percentage) {
    return static_cast<float>(bounds.top()) +
           static_cast<float>(bounds.height()) *
               std::clamp(percentage, 0.0f, 100.0f) / 100.0f;
}

constexpr float kTextGizmoPi = 3.14159265358979323846f;

float textPointDistance(const QPointF& a, const QPointF& b) {
    const float dx = static_cast<float>(a.x() - b.x());
    const float dy = static_cast<float>(a.y() - b.y());
    return std::sqrt(dx * dx + dy * dy);
}

QPointF textPointOnCircle(const QPointF& center, const float radius,
                          const float angleDegrees) {
    const float radians = angleDegrees * (kTextGizmoPi / 180.0f);
    return QPointF(center.x() + std::cos(radians) * radius,
                   center.y() + std::sin(radians) * radius);
}

float textAngleDegreesAround(const QPointF& center, const QPointF& point) {
    return static_cast<float>(std::atan2(point.y() - center.y(),
                                         point.x() - center.x()) *
                              180.0 / kTextGizmoPi);
}

float textNormalizeAngleDeltaDegrees(float deltaDegrees) {
    while (deltaDegrees > 180.0f) {
        deltaDegrees -= 360.0f;
    }
    while (deltaDegrees < -180.0f) {
        deltaDegrees += 360.0f;
    }
    return deltaDegrees;
}

QPointF textApplyScaleRotateToVector(const QPointF& v, const float scaleX,
                                     const float scaleY,
                                     const float rotationDegrees) {
    const double radians = rotationDegrees * (kTextGizmoPi / 180.0f);
    const double cosA = std::cos(radians);
    const double sinA = std::sin(radians);
    const double sx = v.x() * scaleX;
    const double sy = v.y() * scaleY;
    return QPointF(sx * cosA - sy * sinA, sx * sinA + sy * cosA);
}

void textSyncAnimatedProperty(const ArtifactAbstractLayerPtr& layer,
                              const QString& propertyPath,
                              const ArtifactCore::RationalTime& time,
                              const QVariant& value) {
    if (!layer) {
        return;
    }
    const auto property = layer->getProperty(propertyPath);
    if (property && property->isAnimatable() &&
        !property->getKeyFrames().empty()) {
        property->addKeyFrame(time, value);
    }
}

struct TextRotateRingGeometry {
    QPointF centerWorld;
    float ringRadius = 0.0f;
    float ringThickness = 0.0f;
    float hitRadius = 0.0f;
    float hitThickness = 0.0f;
    float gripRadius = 0.0f;
    float gripSize = 0.0f;
};

TextRotateRingGeometry computeTextRotateRingGeometry(const QRectF& localRect,
                                                     const QTransform& globalTransform,
                                                     const float invZoom) {
    TextRotateRingGeometry geo;
    geo.centerWorld = globalTransform.map(localRect.center());
    const QPointF tl = globalTransform.map(localRect.topLeft());
    const QPointF tr = globalTransform.map(localRect.topRight());
    const QPointF bl = globalTransform.map(localRect.bottomLeft());
    const QPointF br = globalTransform.map(localRect.bottomRight());
    const float baseRadius = std::max(
        {textPointDistance(geo.centerWorld, tl),
         textPointDistance(geo.centerWorld, tr),
         textPointDistance(geo.centerWorld, bl),
         textPointDistance(geo.centerWorld, br)});
    geo.ringRadius = baseRadius + std::max(12.0f * invZoom, 14.0f);
    geo.ringThickness = std::max(2.0f * invZoom, 2.2f);
    geo.hitRadius = geo.ringRadius;
    geo.hitThickness = std::max(geo.ringThickness * 2.2f, 9.0f * invZoom);
    geo.gripRadius = geo.ringRadius + std::max(5.0f * invZoom, 6.0f);
    geo.gripSize = std::max(3.5f * invZoom, 4.0f);
    return geo;
}

void drawTextRotateRing(ArtifactIRenderer* renderer,
                        const TextRotateRingGeometry& geo,
                        const bool rotateActive, const float invZoom) {
    if (!renderer || geo.ringRadius <= 0.0f) {
        return;
    }
    const FloatColor ringColor = rotateActive
        ? FloatColor{0.55f, 0.95f, 1.0f, 1.0f}
        : FloatColor{0.5f, 0.8f, 1.0f, 0.85f};
    const FloatColor shadowColor{0.0f, 0.0f, 0.0f, 0.35f};
    const float shadowOffset = std::max(1.0f, 0.9f * invZoom);
    renderer->drawCircle(
        static_cast<float>(geo.centerWorld.x()) + shadowOffset,
        static_cast<float>(geo.centerWorld.y()) + shadowOffset,
        geo.ringRadius, shadowColor, geo.ringThickness + 0.8f * invZoom);
    renderer->drawCircle(static_cast<float>(geo.centerWorld.x()),
                         static_cast<float>(geo.centerWorld.y()),
                         geo.ringRadius, ringColor, geo.ringThickness);
    const float cardinalStep = 90.0f;
    for (int i = 0; i < 4; ++i) {
        const float angle = i * cardinalStep;
        const QPointF outer = textPointOnCircle(
            geo.centerWorld, geo.ringRadius + geo.ringThickness, angle);
        const QPointF inner = textPointOnCircle(
            geo.centerWorld, geo.ringRadius - geo.ringThickness * 2.0f, angle);
        renderer->drawSolidLine(
            {static_cast<float>(outer.x()), static_cast<float>(outer.y())},
            {static_cast<float>(inner.x()), static_cast<float>(inner.y())},
            FloatColor{0.5f, 0.8f, 1.0f, 0.55f}, std::max(1.0f, 1.1f * invZoom));
    }
    const QPointF gripCenter =
        textPointOnCircle(geo.centerWorld, geo.gripRadius, -90.0f);
    renderer->drawCircle(static_cast<float>(gripCenter.x()),
                         static_cast<float>(gripCenter.y()),
                         geo.gripSize, ringColor);
    renderer->drawSolidLine(
        {static_cast<float>(geo.centerWorld.x()),
         static_cast<float>(geo.centerWorld.y() - geo.ringRadius)},
        {static_cast<float>(gripCenter.x()), static_cast<float>(gripCenter.y())},
        FloatColor{0.5f, 0.8f, 1.0f, 0.7f}, std::max(1.0f, 1.2f * invZoom));
}

void drawTextAnchorCrosshair(ArtifactIRenderer* renderer,
                             const QPointF& anchorWorld,
                             const bool anchorActive, const float invZoom) {
    if (!renderer) {
        return;
    }
    const float handleSize = 6.0f * invZoom;
    const FloatColor outer = anchorActive
        ? FloatColor{0.95f, 0.95f, 0.95f, 1.0f}
        : FloatColor{0.0f, 0.0f, 0.0f, 1.0f};
    const FloatColor inner = anchorActive
        ? FloatColor{1.0f, 1.0f, 1.0f, 1.0f}
        : FloatColor{1.0f, 0.82f, 0.18f, 1.0f};
    const float shadowOffset = std::max(1.0f, 0.9f * invZoom);
    renderer->drawCrosshair(static_cast<float>(anchorWorld.x()) + shadowOffset,
                            static_cast<float>(anchorWorld.y()) + shadowOffset,
                            handleSize * 2.45f,
                            {0.0f, 0.0f, 0.0f, anchorActive ? 0.50f : 0.36f});
    renderer->drawCrosshair(static_cast<float>(anchorWorld.x()),
                            static_cast<float>(anchorWorld.y()),
                            handleSize * 2.0f, outer);
    renderer->drawCrosshair(
        static_cast<float>(anchorWorld.x()) - std::max(1.0f, 0.4f * invZoom),
        static_cast<float>(anchorWorld.y()) - std::max(1.0f, 0.4f * invZoom),
        handleSize * 1.32f, inner);
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
    dragLastCanvasPos_ = QPointF();
    dragStartBounds_ = QRectF();
    dragStartValue_ = 0.0f;
    dragCurrentValue_ = 0.0f;
    dragAnimatorIndex_ = -1;
    dragPropertyPath_.clear();
    dragBeforeKeyframes_.clear();
    dragValueChanged_ = false;
    dragAccumulatedRotationDelta_ = 0.0f;
    transformDragChanged_ = false;
    transformBeforeStates_.clear();
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
        const bool verticalSelector =
            textLayer->writingMode() == TextWritingMode::Vertical;
        const FloatColor startColor{0.20f, 0.82f, 1.0f, 0.98f};
        const FloatColor endColor{1.0f, 0.42f, 0.24f, 0.98f};
        const FloatColor offsetColor{1.0f, 0.86f, 0.20f, 0.98f};
        if (verticalSelector) {
            const float selectorX =
                static_cast<float>(bbox.left()) - 12.0f * invZoom;
            const float selectorWidth = 10.0f * invZoom;
            const float selectorHandleHeight =
                std::max(2.0f * invZoom, handleWidth);
            const float startY = selectorHandleY(bbox, start);
            const float endY = selectorHandleY(bbox, end);
            const float offsetY = selectorHandleY(
                bbox, (start + end) * 0.5f + offset);
            renderer->drawSolidRect(selectorX, startY - selectorHandleHeight * 0.5f,
                                    selectorWidth, selectorHandleHeight,
                                    startColor);
            renderer->drawSolidRect(selectorX, endY - selectorHandleHeight * 0.5f,
                                    selectorWidth, selectorHandleHeight,
                                    endColor);
            renderer->drawSolidRect(selectorX + selectorWidth * 0.25f,
                                    offsetY - selectorHandleHeight,
                                    selectorWidth * 0.5f,
                                    selectorHandleHeight * 2.0f, offsetColor);
            renderer->drawRectOutline(selectorX + selectorWidth * 0.45f,
                                      std::min(startY, endY),
                                      std::max(invZoom, selectorWidth * 0.1f),
                                      std::abs(endY - startY),
                                      FloatColor{0.65f, 0.78f, 0.92f, 0.85f});
        } else {
        const float selectorY = static_cast<float>(bbox.top()) - 12.0f * invZoom;
        const float selectorHeight = 10.0f * invZoom;
        const float selectorHandleWidth = std::max(2.0f * invZoom, handleWidth);
        const float startX = selectorHandleX(bbox, start);
        const float endX = selectorHandleX(bbox, end);
        const float offsetX = selectorHandleX(
            bbox, (start + end) * 0.5f + offset);
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

    // Baseline / line-box overlays derived from the shaped glyph layout.
    if (textLayer->layoutMode() != TextLayoutMode::Path) {
        const auto glyphGeometry = textLayer->textGlyphGeometry();
        if (!glyphGeometry.glyphs.empty()) {
            const QTransform overlayTransform = layer_->getGlobalTransform();
            const QPointF &origin = glyphGeometry.drawOrigin;
            const float lineAlpha = editSessionActive_ ? 0.45f : 0.25f;
            const FloatColor lineBoxColor{0.65f, 0.85f, 1.0f, lineAlpha};
            const FloatColor baselineColor{1.0f, 0.86f, 0.35f,
                                           std::min(1.0f, lineAlpha + 0.2f)};
            const float baselineThickness = std::max(1.0f, 1.0f * invZoom);
            int currentLineIndex = -1;
            QRectF lineBox;
            bool baselineValid = false;
            float baselineY = 0.0f;
            float baselineLeft = 0.0f;
            float baselineRight = 0.0f;
            const auto flushLine = [&]() {
                if (currentLineIndex < 0) {
                    return;
                }
                if (lineBox.width() > 0.0 && lineBox.height() > 0.0) {
                    renderer->drawSolidRectTransformed(
                        static_cast<float>(lineBox.left() + origin.x()),
                        static_cast<float>(lineBox.top() + origin.y()),
                        static_cast<float>(lineBox.width()),
                        static_cast<float>(lineBox.height()),
                        overlayTransform, lineBoxColor);
                }
                if (baselineValid) {
                    const QPointF startCanvas = overlayTransform.map(QPointF(
                        baselineLeft + origin.x(), baselineY + origin.y()));
                    const QPointF endCanvas = overlayTransform.map(QPointF(
                        baselineRight + origin.x(), baselineY + origin.y()));
                    renderer->drawSolidLine(
                        {static_cast<float>(startCanvas.x()),
                         static_cast<float>(startCanvas.y())},
                        {static_cast<float>(endCanvas.x()),
                         static_cast<float>(endCanvas.y())},
                        baselineColor, baselineThickness);
                }
            };
            for (const auto &glyph : glyphGeometry.glyphs) {
                if (glyph.lineIndex != currentLineIndex) {
                    flushLine();
                    currentLineIndex = glyph.lineIndex;
                    lineBox = QRectF();
                    baselineValid = false;
                }
                lineBox = lineBox.isNull()
                    ? glyph.bounds : lineBox.united(glyph.bounds);
                if (glyph.bounds.width() > 0.0) {
                    if (!baselineValid) {
                        baselineValid = true;
                        baselineY = static_cast<float>(glyph.basePosition.y());
                        baselineLeft =
                            static_cast<float>(glyph.bounds.left());
                    }
                    baselineRight = static_cast<float>(glyph.bounds.right());
                }
            }
            flushLine();
        }
    }

    // Rotate ring and anchor crosshair follow the layer transform.
    const QRectF textLocalBounds = textLayer->localBounds();
    if (textLocalBounds.isValid() && textLocalBounds.width() > 0.0 &&
        textLocalBounds.height() > 0.0) {
        const QTransform textGlobalTransform = layer_->getGlobalTransform();
        const TextRotateRingGeometry ringGeo = computeTextRotateRingGeometry(
            textLocalBounds, textGlobalTransform, invZoom);
        drawTextRotateRing(renderer, ringGeo,
                           activeHandle_ == HandleType::Rotate, invZoom);

        const auto &textTransform = layer_->transform3D();
        const QPointF anchorWorld = textGlobalTransform.map(
            QPointF(textTransform.anchorX(), textTransform.anchorY()));
        drawTextAnchorCrosshair(renderer, anchorWorld,
                                activeHandle_ == HandleType::AnchorPoint,
                                invZoom);
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
        const bool verticalSelector =
            textLayer->writingMode() == TextWritingMode::Vertical;
        if (verticalSelector) {
            const float selectorX =
                static_cast<float>(bbox.left()) - 7.0f / std::max(zoom, 0.0001f);
            const bool selectorXHit =
                std::abs(canvasMouse.x - selectorX) < hitThreshold;
            if (selectorXHit) {
                const float startY = selectorHandleY(bbox, start);
                const float endY = selectorHandleY(bbox, end);
                const float offsetY = selectorHandleY(
                    bbox, (start + end) * 0.5f + offset);
                if (std::abs(canvasMouse.y - offsetY) < hitThreshold) {
                    return HandleType::RangeOffset;
                }
                if (std::abs(canvasMouse.y - startY) < hitThreshold) {
                    return HandleType::RangeStart;
                }
                if (std::abs(canvasMouse.y - endY) < hitThreshold) {
                    return HandleType::RangeEnd;
                }
            }
        } else {
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

    // Rotate ring: ImGui-style outer ring around the transformed bounds.
    {
        const QRectF textLocalBounds = textLayer->localBounds();
        if (textLocalBounds.isValid() && textLocalBounds.width() > 0.0 &&
            textLocalBounds.height() > 0.0) {
            const float invZoom = std::isfinite(zoom) && zoom > 0.0001f
                ? 1.0f / zoom : 1.0f;
            const QTransform textGlobalTransform =
                layer_->getGlobalTransform();
            const TextRotateRingGeometry ringGeo =
                computeTextRotateRingGeometry(textLocalBounds,
                                              textGlobalTransform, invZoom);
            const QPointF mouseCanvasPoint(canvasMouse.x, canvasMouse.y);
            const float distToCenter =
                textPointDistance(mouseCanvasPoint, ringGeo.centerWorld);
            const float distToRing =
                std::abs(distToCenter - ringGeo.hitRadius);
            const QPointF gripCenter = textPointOnCircle(
                ringGeo.centerWorld, ringGeo.gripRadius, -90.0f);
            const bool hitGrip =
                textPointDistance(mouseCanvasPoint, gripCenter) <=
                ringGeo.gripSize * 1.8f;
            if (distToRing <= ringGeo.hitThickness || hitGrip) {
                return HandleType::Rotate;
            }
        }
    }

    // Anchor point crosshair at the transformed anchor position.
    {
        const QTransform textGlobalTransform = layer_->getGlobalTransform();
        const auto &textTransform = layer_->transform3D();
        const QPointF anchorWorld = textGlobalTransform.map(QPointF(
            textTransform.anchorX(), textTransform.anchorY()));
        const auto anchorVP = renderer->canvasToViewport(
            {static_cast<float>(anchorWorld.x()),
             static_cast<float>(anchorWorld.y())});
        constexpr float kAnchorHitRadius = 12.0f;
        if (QRectF(anchorVP.x - kAnchorHitRadius, anchorVP.y - kAnchorHitRadius,
                   kAnchorHitRadius * 2.0f, kAnchorHitRadius * 2.0f)
                .contains(viewportPos)) {
            return HandleType::AnchorPoint;
        }
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
        case HandleType::RangeOffset: {
            const auto rangeLayer =
                ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
            if (rangeLayer &&
                rangeLayer->writingMode() == TextWritingMode::Vertical) {
                return Qt::SizeVerCursor;
            }
            return Qt::SizeHorCursor;
        }
        case HandleType::Rotate:
            return Qt::CrossCursor;
        case HandleType::AnchorPoint:
            return Qt::SizeAllCursor;
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
        } else if (activeHandle_ == HandleType::Rotate ||
                   activeHandle_ == HandleType::AnchorPoint) {
            dragStartGlobalTransform_ = layer_->getGlobalTransform();
            dragStartLocalBounds_ = layer_->localBounds();
            const auto &startTransform = layer_->transform3D();
            dragStartAnchor_ = QPointF(startTransform.anchorX(),
                                       startTransform.anchorY());
            dragStartScaleX_ = startTransform.scaleX();
            dragStartScaleY_ = startTransform.scaleY();
            dragStartRotation_ = startTransform.rotation();
            dragAccumulatedRotationDelta_ = 0.0f;
            transformDragChanged_ = false;
            dragLastCanvasPos_ = dragStartCanvasPos_;
            captureTransformBeforeStates();
        } else if (activeHandle_ == HandleType::Offset ||
                   activeHandle_ == HandleType::BoxLeft ||
                   activeHandle_ == HandleType::BoxRight ||
                   activeHandle_ == HandleType::BoxTop ||
                   activeHandle_ == HandleType::BoxBottom ||
                   activeHandle_ == HandleType::BoxCornerTopLeft ||
                   activeHandle_ == HandleType::BoxCornerTopRight ||
                   activeHandle_ == HandleType::BoxCornerBottomLeft ||
                   activeHandle_ == HandleType::BoxCornerBottomRight) {
            transformDragChanged_ = false;
            captureTransformBeforeStates();
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
        const bool verticalSelector =
            textLayer->writingMode() == TextWritingMode::Vertical;
        if (verticalSelector) {
            if (bbox.height() <= 0.0001) return false;
        } else {
            if (bbox.width() <= 0.0001) return false;
        }
        float deltaPercent = verticalSelector
            ? deltaY / static_cast<float>(bbox.height()) * 100.0f
            : deltaX / static_cast<float>(bbox.width()) * 100.0f;
        // Ctrl = fine adjustment (x0.1). Shift = snap to whole percent.
        // Both combine: fine delta first, then snap.
        const auto rangeMods = QGuiApplication::keyboardModifiers();
        if (rangeMods.testFlag(Qt::ControlModifier) &&
            !rangeMods.testFlag(Qt::AltModifier)) {
            deltaPercent *= 0.1f;
        }
        float nextValue = std::clamp(
            dragStartValue_ + deltaPercent, -100000.0f, 100000.0f);
        if (rangeMods.testFlag(Qt::ShiftModifier)) {
            nextValue = std::round(nextValue);
        }
        QString suffix;
        if (activeHandle_ == HandleType::RangeStart) {
            suffix = QStringLiteral("start");
        } else if (activeHandle_ == HandleType::RangeEnd) {
            suffix = QStringLiteral("end");
        } else {
            suffix = QStringLiteral("offset");
        }
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

    // Ctrl = fine box resize (x0.1). Rotate/Anchor keep their own
    // Ctrl meanings (snap), Offset/Range apply their own fine scaling.
    float boxDX = deltaX;
    float boxDY = deltaY;
    {
        const auto boxMods = QGuiApplication::keyboardModifiers();
        if (boxMods.testFlag(Qt::ControlModifier) &&
            !boxMods.testFlag(Qt::AltModifier)) {
            boxDX *= 0.1f;
            boxDY *= 0.1f;
        }
    }

    switch (activeHandle_) {
        case HandleType::Rotate: {
            auto &dragTransform = textLayer->transform3D();
            const QPointF pivotLocal = dragStartLocalBounds_.center();
            const QPointF pivotWorldStart =
                dragStartGlobalTransform_.map(pivotLocal);
            const float previousAngle = textAngleDegreesAround(
                pivotWorldStart, dragLastCanvasPos_);
            const float currentAngle = textAngleDegreesAround(
                pivotWorldStart, QPointF(canvasMouse.x, canvasMouse.y));
            dragAccumulatedRotationDelta_ += textNormalizeAngleDeltaDegrees(
                currentAngle - previousAngle);
            float newRotation =
                dragStartRotation_ + dragAccumulatedRotationDelta_;
            if (QGuiApplication::keyboardModifiers().testFlag(
                    Qt::ShiftModifier)) {
                constexpr float kRotationSnapStep = 15.0f;
                newRotation = std::round(newRotation / kRotationSnapStep) *
                              kRotationSnapStep;
                dragAccumulatedRotationDelta_ =
                    newRotation - dragStartRotation_;
            }
            const RationalTime editTime = currentAnimatorTime(textLayer);
            if (dragTransform.hasRotationKeyFrameAt(editTime) ||
                dragTransform.getRotationKeyFrameCount() > 0) {
                dragTransform.setRotation(editTime, newRotation);
            } else {
                dragTransform.removeRotationKeyFrameAt(editTime);
                dragTransform.setInitialRotation(editTime, newRotation);
            }
            textSyncAnimatedProperty(
                layer_, QStringLiteral("transform.rotation"), editTime,
                newRotation);
            const QPointF localOffset = pivotLocal - dragStartAnchor_;
            const QPointF startOffset = textApplyScaleRotateToVector(
                localOffset, dragStartScaleX_, dragStartScaleY_,
                dragStartRotation_);
            const QPointF newOffset = textApplyScaleRotateToVector(
                localOffset, dragStartScaleX_, dragStartScaleY_, newRotation);
            const float newPosX = static_cast<float>(
                dragStartLayerPosition_.x() +
                (startOffset.x() - newOffset.x()));
            const float newPosY = static_cast<float>(
                dragStartLayerPosition_.y() +
                (startOffset.y() - newOffset.y()));
            if (dragTransform.hasPositionKeyFrameAt(editTime) ||
                dragTransform.getPositionKeyFrameCount() > 0) {
                dragTransform.setPosition(editTime, newPosX, newPosY);
            } else {
                dragTransform.removePositionKeyFrameAt(editTime);
                dragTransform.setInitialPosition(editTime, newPosX, newPosY);
            }
            textSyncAnimatedProperty(
                layer_, QStringLiteral("transform.position.x"), editTime,
                newPosX);
            textSyncAnimatedProperty(
                layer_, QStringLiteral("transform.position.y"), editTime,
                newPosY);
            textLayer->setDirty(LayerDirtyFlag::Transform);
            textLayer->changed();
            dragLastCanvasPos_ = QPointF(canvasMouse.x, canvasMouse.y);
            transformDragChanged_ = true;
            return true;
        }
        case HandleType::AnchorPoint: {
            bool invertible = false;
            const QTransform inverse =
                dragStartGlobalTransform_.inverted(&invertible);
            if (!invertible) {
                return false;
            }
            QPointF targetLocalAnchor =
                inverse.map(QPointF(canvasMouse.x, canvasMouse.y));
            const auto modifiers = QGuiApplication::keyboardModifiers();
            if (modifiers.testFlag(Qt::ShiftModifier)) {
                const QPointF anchorDelta =
                    targetLocalAnchor - dragStartAnchor_;
                if (std::abs(anchorDelta.x()) >= std::abs(anchorDelta.y())) {
                    targetLocalAnchor.setY(dragStartAnchor_.y());
                } else {
                    targetLocalAnchor.setX(dragStartAnchor_.x());
                }
            }
            const bool enableSnapping =
                modifiers.testFlag(Qt::ControlModifier) &&
                !modifiers.testFlag(Qt::AltModifier);
            if (enableSnapping) {
                const float snapDistance =
                    8.0f / std::max(0.001f, renderer->getZoom());
                const QRectF snapBounds = dragStartLocalBounds_;
                const std::vector<float> vLines = {
                    static_cast<float>(snapBounds.left()),
                    static_cast<float>(snapBounds.center().x()),
                    static_cast<float>(snapBounds.right())};
                const std::vector<float> hLines = {
                    static_cast<float>(snapBounds.top()),
                    static_cast<float>(snapBounds.center().y()),
                    static_cast<float>(snapBounds.bottom())};
                for (const float line : vLines) {
                    if (std::abs(targetLocalAnchor.x() - line) <
                        snapDistance) {
                        targetLocalAnchor.setX(line);
                        break;
                    }
                }
                for (const float line : hLines) {
                    if (std::abs(targetLocalAnchor.y() - line) <
                        snapDistance) {
                        targetLocalAnchor.setY(line);
                        break;
                    }
                }
            }
            const QPointF deltaAnchor = targetLocalAnchor - dragStartAnchor_;
            const QPointF compensation = textApplyScaleRotateToVector(
                deltaAnchor, dragStartScaleX_, dragStartScaleY_,
                dragStartRotation_);
            const RationalTime editTime = currentAnimatorTime(textLayer);
            auto &anchorTransform = textLayer->transform3D();
            anchorTransform.setAnchor(
                editTime, static_cast<float>(targetLocalAnchor.x()),
                static_cast<float>(targetLocalAnchor.y()),
                anchorTransform.anchorZ());
            textSyncAnimatedProperty(
                layer_, QStringLiteral("transform.anchor.x"), editTime,
                static_cast<float>(targetLocalAnchor.x()));
            textSyncAnimatedProperty(
                layer_, QStringLiteral("transform.anchor.y"), editTime,
                static_cast<float>(targetLocalAnchor.y()));
            const float newPosX = static_cast<float>(
                dragStartLayerPosition_.x() + compensation.x());
            const float newPosY = static_cast<float>(
                dragStartLayerPosition_.y() + compensation.y());
            if (anchorTransform.hasPositionKeyFrameAt(editTime) ||
                anchorTransform.getPositionKeyFrameCount() > 0) {
                anchorTransform.setPosition(editTime, newPosX, newPosY);
            } else {
                anchorTransform.removePositionKeyFrameAt(editTime);
                anchorTransform.setInitialPosition(editTime, newPosX, newPosY);
            }
            textSyncAnimatedProperty(
                layer_, QStringLiteral("transform.position.x"), editTime,
                newPosX);
            textSyncAnimatedProperty(
                layer_, QStringLiteral("transform.position.y"), editTime,
                newPosY);
            textLayer->setDirty(LayerDirtyFlag::Transform);
            textLayer->changed();
            transformDragChanged_ = true;
            return true;
        }
        case HandleType::Offset: {
            // Write the edit at the layer's current frame.  Using frame 0
            // here made viewport drags silently alter a different time than
            // the one currently being edited.
            // Ctrl = fine adjustment (x0.1). Shift constrains to the
            // dominant axis (same convention as anchor drag and the
            // line shape tool). Both combine.
            float moveX = deltaX;
            float moveY = deltaY;
            const auto moveMods = QGuiApplication::keyboardModifiers();
            if (moveMods.testFlag(Qt::ControlModifier) &&
                !moveMods.testFlag(Qt::AltModifier)) {
                moveX *= 0.1f;
                moveY *= 0.1f;
            }
            if (moveMods.testFlag(Qt::ShiftModifier)) {
                if (std::abs(moveX) >= std::abs(moveY)) {
                    moveY = 0.0f;
                } else {
                    moveX = 0.0f;
                }
            }
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
            const float newPosX = static_cast<float>(
                dragStartLayerPosition_.x() + moveX);
            const float newPosY = static_cast<float>(
                dragStartLayerPosition_.y() + moveY);
            start.setPosition(frame, newPosX, newPosY);
            textSyncAnimatedProperty(
                layer_, QStringLiteral("transform.position.x"), frame,
                newPosX);
            textSyncAnimatedProperty(
                layer_, QStringLiteral("transform.position.y"), frame,
                newPosY);
            textLayer->setDirty(LayerDirtyFlag::Transform);
            textLayer->changed();
            transformDragChanged_ = true;
            return true;
        }
        case HandleType::BoxLeft:
            bbox.setLeft(bbox.left() + boxDX);
            break;
        case HandleType::BoxRight:
            bbox.setRight(bbox.right() + boxDX);
            break;
        case HandleType::BoxTop:
            bbox.setTop(bbox.top() + boxDY);
            break;
        case HandleType::BoxBottom:
            bbox.setBottom(bbox.bottom() + boxDY);
            break;
        case HandleType::BoxCornerTopLeft:
            bbox.setTopLeft(bbox.topLeft() + QPointF(boxDX, boxDY));
            break;
        case HandleType::BoxCornerTopRight:
            bbox.setTopRight(bbox.topRight() + QPointF(boxDX, boxDY));
            break;
        case HandleType::BoxCornerBottomLeft:
            bbox.setBottomLeft(bbox.bottomLeft() + QPointF(boxDX, boxDY));
            break;
        case HandleType::BoxCornerBottomRight:
            bbox.setBottomRight(bbox.bottomRight() + QPointF(boxDX, boxDY));
            break;
        default:
            return false;
    }

    // Shift constrains corner box drags to a square, anchored at the
    // opposite (fixed) corner. Edge handles stay single-axis.
    if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
        const bool isCorner =
            activeHandle_ == HandleType::BoxCornerTopLeft ||
            activeHandle_ == HandleType::BoxCornerTopRight ||
            activeHandle_ == HandleType::BoxCornerBottomLeft ||
            activeHandle_ == HandleType::BoxCornerBottomRight;
        if (isCorner && bbox.isValid()) {
            const qreal side =
                std::max(bbox.width(), bbox.height());
            if (activeHandle_ == HandleType::BoxCornerTopLeft) {
                bbox.setLeft(bbox.right() - side);
                bbox.setTop(bbox.bottom() - side);
            } else if (activeHandle_ == HandleType::BoxCornerTopRight) {
                bbox.setRight(bbox.left() + side);
                bbox.setTop(bbox.bottom() - side);
            } else if (activeHandle_ == HandleType::BoxCornerBottomLeft) {
                bbox.setLeft(bbox.right() - side);
                bbox.setBottom(bbox.top() + side);
            } else {
                bbox.setRight(bbox.left() + side);
                bbox.setBottom(bbox.top() + side);
            }
        }
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
    // Assuming bounds represent maxWidth and boxHeight.
    // Route through the property system as well so VP drags participate
    // in the same undo path captured in captureTransformBeforeStates().
    // Default snaps to whole pixels; Ctrl keeps the fine float size.
    float newMaxWidth =
        std::max(1.0f, static_cast<float>(bbox.width()));
    float newBoxHeight =
        std::max(1.0f, static_cast<float>(bbox.height()));
    {
        const auto boxSnapMods = QGuiApplication::keyboardModifiers();
        const bool fineBoxSize =
            boxSnapMods.testFlag(Qt::ControlModifier) &&
            !boxSnapMods.testFlag(Qt::AltModifier);
        if (!fineBoxSize) {
            newMaxWidth = std::round(newMaxWidth);
            newBoxHeight = std::round(newBoxHeight);
            newMaxWidth = std::max(1.0f, newMaxWidth);
            newBoxHeight = std::max(1.0f, newBoxHeight);
        }
    }
    textLayer->setMaxWidth(newMaxWidth);
    textLayer->setBoxHeight(newBoxHeight);
    if (const auto maxWidthProperty =
            layer_->getProperty(QStringLiteral("text.maxWidth"))) {
        maxWidthProperty->setValue(newMaxWidth);
    }
    textLayer->setLayerPropertyValue(
        QStringLiteral("text.maxWidth"), newMaxWidth);
    if (const auto boxHeightProperty =
            layer_->getProperty(QStringLiteral("text.boxHeight"))) {
        boxHeightProperty->setValue(newBoxHeight);
    }
    textLayer->setLayerPropertyValue(
        QStringLiteral("text.boxHeight"), newBoxHeight);
    transformDragChanged_ = true;

    // Update position if needed (simplified)
    // layer_->transform2D().setPosition(...)

    textLayer->setDirty();
    textLayer->updateImage();
    textLayer->changed();

    return true;
}

void TextGizmo::handleMouseRelease() {
    pushTransformUndoIfNeeded();
    if (dragValueChanged_ && !dragPropertyPath_.isEmpty() && layer_) {
        const auto textLayer =
            ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
        if (auto *manager = UndoManager::instance()) {
            const auto restoreDragBeforeState = [this, textLayer]() {
                if (!textLayer) return;
                const QString suffix = dragPropertyPath_.mid(
                    dragPropertyPath_.lastIndexOf(QLatin1Char('.')) + 1);
                const auto property = animatorProperty(
                    textLayer, dragAnimatorIndex_, suffix);
                if (!property) return;
                if (!dragBeforeKeyframes_.empty()) {
                    property->clearKeyFrames();
                    for (const auto &keyframe : dragBeforeKeyframes_) {
                        property->addKeyFrame(
                            keyframe.time, keyframe.value,
                            keyframe.interpolation, keyframe.cp1_x,
                            keyframe.cp1_y, keyframe.cp2_x, keyframe.cp2_y,
                            keyframe.roving);
                        property->setKeyFrameAnchorAt(
                            keyframe.time, keyframe.anchor);
                        property->setKeyFrameColorLabelAt(
                            keyframe.time, keyframe.colorLabel);
                    }
                } else {
                    property->setValue(dragStartValue_);
                    textLayer->setLayerPropertyValue(
                        dragPropertyPath_, dragStartValue_);
                }
                textLayer->updateImage();
                textLayer->changed();
            };
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
                        manager->budget().maxSingleEntryBytes ||
                        command->estimatedMemoryBytes() >
                            manager->budget().maxMemoryBytes ||
                        manager->budget().maxEntryCount == 0) {
                        restoreDragBeforeState();
                    } else {
                        if (!manager->push(std::move(command))) {
                            restoreDragBeforeState();
                        }
                    }
                }
            } else {
                auto command = std::make_unique<SetTextAnimatorPropertyCommand>(
                    layer_, dragPropertyPath_, dragStartValue_,
                    dragCurrentValue_);
                if (command->estimatedMemoryBytes() >
                    manager->budget().maxSingleEntryBytes ||
                    command->estimatedMemoryBytes() >
                        manager->budget().maxMemoryBytes ||
                    manager->budget().maxEntryCount == 0) {
                    restoreDragBeforeState();
                } else {
                    if (!manager->push(std::move(command))) {
                        restoreDragBeforeState();
                    }
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
    transformDragChanged_ = false;
    transformBeforeStates_.clear();
    dragAccumulatedRotationDelta_ = 0.0f;
}

bool TextGizmo::cancelInteraction() {
    if (!isDragging_) {
        return false;
    }
    // Restore transform/box before-states without pushing undo.
    // Mirrors the restore path in pushTransformUndoIfNeeded().
    for (const auto &before : transformBeforeStates_) {
        if (!layer_) {
            break;
        }
        const auto property = layer_->getProperty(before.path);
        if (!property) {
            continue;
        }
        if (before.animated) {
            property->clearKeyFrames();
            for (const auto &keyframe : before.keyframes) {
                property->addKeyFrame(
                    keyframe.time, keyframe.value,
                    keyframe.interpolation, keyframe.cp1_x,
                    keyframe.cp1_y, keyframe.cp2_x, keyframe.cp2_y,
                    keyframe.roving);
                property->setKeyFrameAnchorAt(keyframe.time,
                                              keyframe.anchor);
                property->setKeyFrameColorLabelAt(keyframe.time,
                                                  keyframe.colorLabel);
            }
        } else {
            property->setValue(before.staticValue);
            layer_->setLayerPropertyValue(before.path,
                                          before.staticValue);
        }
    }
    // Restore in-progress animator range drag.
    if (dragAnimatorIndex_ >= 0 && !dragPropertyPath_.isEmpty() &&
        layer_) {
        if (const auto textLayer =
                ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(
                    layer_)) {
            const QString suffix = dragPropertyPath_.mid(
                dragPropertyPath_.lastIndexOf(QLatin1Char('.')) + 1);
            if (const auto property = animatorProperty(
                    textLayer, dragAnimatorIndex_, suffix)) {
                if (!dragBeforeKeyframes_.empty()) {
                    property->clearKeyFrames();
                    for (const auto &keyframe : dragBeforeKeyframes_) {
                        property->addKeyFrame(
                            keyframe.time, keyframe.value,
                            keyframe.interpolation, keyframe.cp1_x,
                            keyframe.cp1_y, keyframe.cp2_x, keyframe.cp2_y,
                            keyframe.roving);
                        property->setKeyFrameAnchorAt(keyframe.time,
                                                      keyframe.anchor);
                        property->setKeyFrameColorLabelAt(
                            keyframe.time, keyframe.colorLabel);
                    }
                } else {
                    property->setValue(dragStartValue_);
                    textLayer->setLayerPropertyValue(dragPropertyPath_,
                                                     dragStartValue_);
                }
            }
            textLayer->setDirty();
            textLayer->updateImage();
            textLayer->changed();
        }
    } else if (const auto textLayer =
                   ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(
                       layer_)) {
        textLayer->setDirty();
        textLayer->updateImage();
        textLayer->changed();
    }
    isDragging_ = false;
    activeHandle_ = HandleType::None;
    dragAnimatorIndex_ = -1;
    dragPropertyPath_.clear();
    dragBeforeKeyframes_.clear();
    dragValueChanged_ = false;
    transformDragChanged_ = false;
    transformBeforeStates_.clear();
    dragAccumulatedRotationDelta_ = 0.0f;
    return true;
}

void TextGizmo::captureTransformBeforeStates() {
    transformBeforeStates_.clear();
    if (!layer_) {
        return;
    }
    const std::vector<QString> paths = {
        QStringLiteral("transform.rotation"),
        QStringLiteral("transform.anchor.x"),
        QStringLiteral("transform.anchor.y"),
        QStringLiteral("transform.position.x"),
        QStringLiteral("transform.position.y"),
        QStringLiteral("text.maxWidth"),
        QStringLiteral("text.boxHeight")};
    for (const QString &path : paths) {
        TransformPathBeforeState state;
        state.path = path;
        if (const auto property = layer_->getProperty(path)) {
            state.staticValue = property->getValue();
            state.animated = property->isAnimatable() &&
                             !property->getKeyFrames().empty();
            if (state.animated) {
                state.keyframes = property->getKeyFrames();
            }
        }
        transformBeforeStates_.push_back(std::move(state));
    }
}

void TextGizmo::pushTransformUndoIfNeeded() {
    if (!transformDragChanged_ || !layer_ || transformBeforeStates_.empty()) {
        return;
    }
    auto *manager = UndoManager::instance();
    if (!manager) {
        return;
    }
    const auto restoreTransformBeforeState = [this]() {
        for (const auto &before : transformBeforeStates_) {
            const auto property = layer_->getProperty(before.path);
            if (!property) {
                continue;
            }
            if (before.animated) {
                property->clearKeyFrames();
                for (const auto &keyframe : before.keyframes) {
                    property->addKeyFrame(
                        keyframe.time, keyframe.value,
                        keyframe.interpolation, keyframe.cp1_x,
                        keyframe.cp1_y, keyframe.cp2_x, keyframe.cp2_y,
                        keyframe.roving);
                    property->setKeyFrameAnchorAt(keyframe.time,
                                                  keyframe.anchor);
                    property->setKeyFrameColorLabelAt(keyframe.time,
                                                      keyframe.colorLabel);
                }
            } else {
                property->setValue(before.staticValue);
                layer_->setLayerPropertyValue(before.path,
                                              before.staticValue);
            }
        }
        if (const auto textLayer =
                ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_)) {
            textLayer->setDirty();
            textLayer->updateImage();
            textLayer->changed();
        }
    };

    auto macro = std::make_unique<MacroUndoCommand>(
        QStringLiteral("Transform Text Layer"));
    bool anyCommand = false;
    for (const auto &before : transformBeforeStates_) {
        const auto property = layer_->getProperty(before.path);
        if (!property) {
            continue;
        }
        if (before.animated) {
            const auto afterKeys = property->getKeyFrames();
            bool keyframesUnchanged =
                afterKeys.size() == before.keyframes.size();
            if (keyframesUnchanged) {
                for (size_t i = 0; i < afterKeys.size(); ++i) {
                    const auto &lhs = afterKeys[i];
                    const auto &rhs = before.keyframes[i];
                    if (!(lhs.time == rhs.time && lhs.value == rhs.value &&
                          lhs.interpolation == rhs.interpolation &&
                          lhs.cp1_x == rhs.cp1_x && lhs.cp1_y == rhs.cp1_y &&
                          lhs.cp2_x == rhs.cp2_x && lhs.cp2_y == rhs.cp2_y &&
                          lhs.roving == rhs.roving && lhs.anchor == rhs.anchor &&
                          lhs.colorLabel == rhs.colorLabel)) {
                        keyframesUnchanged = false;
                        break;
                    }
                }
            }
            if (keyframesUnchanged) {
                continue;
            }
            macro->addChild(std::make_unique<SetLayerPropertyKeyframesCommand>(
                layer_, before.path, before.keyframes, afterKeys,
                QStringLiteral("Transform Text Layer")));
            anyCommand = true;
        } else {
            const QVariant afterValue = property->getValue();
            if (afterValue == before.staticValue) {
                continue;
            }
            macro->addChild(std::make_unique<SetLayerPropertyValueCommand>(
                layer_, before.path, before.staticValue, afterValue,
                QStringLiteral("Transform Text Layer")));
            anyCommand = true;
        }
    }
    if (!anyCommand) {
        return;
    }
    if (!manager->push(std::move(macro))) {
        restoreTransformBeforeState();
    }
}

} // namespace Artifact
