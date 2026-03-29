module;
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QSignalBlocker>
#include <QDateTime>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QPair>
#include <QStringList>
#include <QVector>
#include <wobjectimpl.h>

#include <algorithm>
#include <functional>

module Artifact.Widgets.SoftwareRenderInspectors;

import Artifact.Render.SoftwareCompositor;
import Artifact.Service.Project;
import Artifact.Project.Items;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Text;
import Artifact.Layer.Video;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Solid2D;
import Artifact.Mask.Path;
import Artifact.Mask.LayerMask;
import Layer.Blend;

namespace Artifact {

namespace {
Q_LOGGING_CATEGORY(softwareInspectorPerfLog, "artifact.softwareinspectorperf")

QString blendLabel(const ArtifactCore::BlendMode mode)
{
    return SoftwareRender::blendModeText(mode);
}

SoftwareRender::CvEffectMode effectFromIndex(const int index)
{
    switch (index) {
    case 1: return SoftwareRender::CvEffectMode::GaussianBlur;
    case 2: return SoftwareRender::CvEffectMode::EdgeOverlay;
    default: return SoftwareRender::CvEffectMode::None;
    }
}

QColor colorForKey(const QString& key)
{
    const uint h = qHash(key);
    return QColor::fromHsv(static_cast<int>(h % 360), 160, 210, 220);
}

QPainter::CompositionMode compositionMode(ArtifactCore::BlendMode mode)
{
    switch (mode) {
    case ArtifactCore::BlendMode::Subtract:  return QPainter::CompositionMode_Difference;
    case ArtifactCore::BlendMode::Add:      return QPainter::CompositionMode_Plus;
    case ArtifactCore::BlendMode::Multiply: return QPainter::CompositionMode_Multiply;
    case ArtifactCore::BlendMode::Screen:   return QPainter::CompositionMode_Screen;
    case ArtifactCore::BlendMode::Overlay:   return QPainter::CompositionMode_Overlay;
    case ArtifactCore::BlendMode::Darken:    return QPainter::CompositionMode_Darken;
    case ArtifactCore::BlendMode::Lighten:   return QPainter::CompositionMode_Lighten;
    case ArtifactCore::BlendMode::ColorDodge:return QPainter::CompositionMode_ColorDodge;
    case ArtifactCore::BlendMode::ColorBurn: return QPainter::CompositionMode_ColorBurn;
    case ArtifactCore::BlendMode::HardLight: return QPainter::CompositionMode_HardLight;
    case ArtifactCore::BlendMode::SoftLight: return QPainter::CompositionMode_SoftLight;
    case ArtifactCore::BlendMode::Difference:return QPainter::CompositionMode_Difference;
    case ArtifactCore::BlendMode::Exclusion: return QPainter::CompositionMode_Exclusion;
    case ArtifactCore::BlendMode::Hue:
    case ArtifactCore::BlendMode::Saturation:
    case ArtifactCore::BlendMode::Color:
    case ArtifactCore::BlendMode::Luminosity:
        return QPainter::CompositionMode_SourceOver;
    case ArtifactCore::BlendMode::Normal:
    default:
        return QPainter::CompositionMode_SourceOver;
    }
}

QPainterPath toQPainterPath(const MaskPath& path)
{
    QPainterPath qpath;
    const int count = path.vertexCount();
    if (count < 2) return qpath;

    const auto v0 = path.vertex(0);
    qpath.moveTo(v0.position);

    for (int i = 0; i < count; ++i) {
        const auto& curr = path.vertex(i);
        const auto& next = path.vertex((i + 1) % count);

        if (i == count - 1 && !path.isClosed()) break;

        if (curr.outTangent.isNull() && next.inTangent.isNull()) {
            qpath.lineTo(next.position);
        } else {
            qpath.cubicTo(curr.position + curr.outTangent,
                          next.position + next.inTangent,
                          next.position);
        }
    }

    if (path.isClosed()) {
        qpath.closeSubpath();
    }
    return qpath;
}

QImage makeCheckerboard(const QSize& size)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(32, 34, 38));
    QPainter painter(&image);
    constexpr int tile = 24;
    for (int y = 0; y < size.height(); y += tile) {
        for (int x = 0; x < size.width(); x += tile) {
            const bool dark = ((x / tile) + (y / tile)) % 2 == 0;
            painter.fillRect(x, y, tile, tile, dark ? QColor(42, 44, 48) : QColor(54, 56, 60));
        }
    }
    return image;
}

QRectF fitCanvasRect(const QSize& outer, const QSize& inner)
{
    const int safeW = std::max(1, inner.width());
    const int safeH = std::max(1, inner.height());
    const qreal sx = static_cast<qreal>(outer.width()) / static_cast<qreal>(safeW);
    const qreal sy = static_cast<qreal>(outer.height()) / static_cast<qreal>(safeH);
    const qreal scale = std::min(sx, sy);
    const qreal width = safeW * scale;
    const qreal height = safeH * scale;
    return QRectF((outer.width() - width) * 0.5, (outer.height() - height) * 0.5, width, height);
}

QSize safePreviewSize(const QLabel* label)
{
    if (!label) {
        return QSize(960, 540);
    }
    const QSize size = label->size();
    return QSize(std::max(320, size.width()), std::max(180, size.height()));
}

struct PreviewNavigationState
{
    float zoom = 1.0f;
    QPointF pan = QPointF(0.0, 0.0);
};

QRectF previewCanvasRect(
    const QSize& previewSize,
    const QSize& canvasSize,
    const PreviewNavigationState& nav)
{
    const QRectF baseRect = fitCanvasRect(previewSize, canvasSize).adjusted(12.0, 12.0, -12.0, -12.0);
    const qreal safeZoom = std::clamp(static_cast<qreal>(nav.zoom), 0.05, 64.0);
    const QSizeF scaled(baseRect.width() * safeZoom, baseRect.height() * safeZoom);
    const QPointF center = baseRect.center() + nav.pan;
    return QRectF(
        center.x() - scaled.width() * 0.5,
        center.y() - scaled.height() * 0.5,
        scaled.width(),
        scaled.height());
}

void resetNavigation(PreviewNavigationState* nav)
{
    if (!nav) {
        return;
    }
    nav->zoom = 1.0f;
    nav->pan = QPointF(0.0, 0.0);
}

bool handleNavigationShortcut(QKeyEvent* event, PreviewNavigationState* nav)
{
    if (!event || !nav) {
        return false;
    }

    switch (event->key()) {
    case Qt::Key_F:
    case Qt::Key_R:
    case Qt::Key_1:
        resetNavigation(nav);
        event->accept();
        return true;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        nav->zoom = std::clamp(nav->zoom * 1.1f, 0.05f, 64.0f);
        event->accept();
        return true;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        nav->zoom = std::clamp(nav->zoom / 1.1f, 0.05f, 64.0f);
        event->accept();
        return true;
    case Qt::Key_Left:
        nav->pan += QPointF(24.0, 0.0);
        event->accept();
        return true;
    case Qt::Key_Right:
        nav->pan += QPointF(-24.0, 0.0);
        event->accept();
        return true;
    case Qt::Key_Up:
        nav->pan += QPointF(0.0, 24.0);
        event->accept();
        return true;
    case Qt::Key_Down:
        nav->pan += QPointF(0.0, -24.0);
        event->accept();
        return true;
    default:
        break;
    }

    return false;
}

bool handleNavigationWheel(QWheelEvent* event, PreviewNavigationState* nav)
{
    if (!event || !nav) {
        return false;
    }
    const float factor = event->angleDelta().y() >= 0 ? 1.1f : (1.0f / 1.1f);
    nav->zoom = std::clamp(nav->zoom * factor, 0.05f, 64.0f);
    event->accept();
    return true;
}

QSize safeCompositionSize(const ArtifactCompositionPtr& composition)
{
    if (!composition) {
        return QSize(1920, 1080);
    }
    const QSize size = composition->settings().compositionSize();
    return QSize(std::max(16, size.width()), std::max(16, size.height()));
}

void collectCompositionEntries(
    ArtifactProjectService* service,
    const QVector<ProjectItem*>& items,
    QVector<QPair<ArtifactCore::CompositionID, QString>>* out)
{
    if (!service || !out) {
        return;
    }
    std::function<void(ProjectItem*)> visit = [&](ProjectItem* item) {
        if (!item) {
            return;
        }
        if (item->type() == eProjectItemType::Composition) {
            const auto* compItem = static_cast<CompositionItem*>(item);
            QString name = item->name.toQString().trimmed();
            const auto found = service->findComposition(compItem->compositionId);
            if (found.success) {
                if (const auto composition = found.ptr.lock()) {
                    const QString liveName = composition->settings().compositionName().toQString().trimmed();
                    if (!liveName.isEmpty()) {
                        name = liveName;
                    }
                }
            }
            out->push_back({compItem->compositionId, name.isEmpty() ? compItem->compositionId.toString() : name});
        }
        for (ProjectItem* child : item->children) {
            visit(child);
        }
    };
    for (ProjectItem* item : items) {
        visit(item);
    }
}

void saveImagePreview(QWidget* owner, const QImage& image, const QString& prefix)
{
    if (image.isNull()) {
        return;
    }
    const QString suggested = QStringLiteral("%1_%2.png")
        .arg(prefix)
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
        owner,
        QStringLiteral("Save Software Render Preview"),
        suggested,
        QStringLiteral("PNG Image (*.png);;All Files (*.*)"));
    if (!path.isEmpty()) {
        image.save(path, "PNG");
    }
}

QImage renderLayerCard(const ArtifactAbstractLayerPtr& layer, const QSize& size, const QString& footer = QString())
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!layer) {
        return image;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF cardRect(20.0, 20.0, std::max(120, size.width() - 40), std::max(100, size.height() - 40));
    const QColor base = colorForKey(layer->layerName());
    QLinearGradient gradient(cardRect.topLeft(), cardRect.bottomRight());
    gradient.setColorAt(0.0, base.lighter(125));
    gradient.setColorAt(1.0, base.darker(135));
    painter.setBrush(gradient);
    painter.setPen(QPen(QColor(235, 235, 235, 210), 2.0));
    painter.drawRoundedRect(cardRect, 14.0, 14.0);

    painter.setPen(QColor(255, 255, 255));
    QFont titleFont = painter.font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(cardRect.adjusted(18.0, 16.0, -18.0, -cardRect.height() + 46.0), layer->layerName());

    QFont bodyFont = painter.font();
    bodyFont.setBold(false);
    bodyFont.setPointSize(10);
    painter.setFont(bodyFont);
    const QString typeName = layer->className().toQString();
    const QString info = QStringLiteral("Blend: %1\nVisible: %2  Locked: %3  Solo: %4  Shy: %5")
        .arg(blendLabel(ArtifactCore::toBlendMode(layer->layerBlendType())))
        .arg(layer->isVisible() ? QStringLiteral("Yes") : QStringLiteral("No"))
        .arg(layer->isLocked() ? QStringLiteral("Yes") : QStringLiteral("No"))
        .arg(layer->isSolo() ? QStringLiteral("Yes") : QStringLiteral("No"))
        .arg(layer->isShy() ? QStringLiteral("Yes") : QStringLiteral("No"));
    painter.drawText(cardRect.adjusted(18.0, 58.0, -18.0, -58.0), Qt::TextWordWrap, typeName + QLatin1Char('\n') + info);

    if (!footer.trimmed().isEmpty()) {
        painter.setPen(QColor(245, 245, 245, 210));
        painter.drawText(cardRect.adjusted(18.0, cardRect.height() - 54.0, -18.0, -16.0), footer);
    }
    return image;
}

QColor toQColor(const FloatColor& color, const float alphaScale = 1.0f)
{
    return QColor::fromRgbF(
        std::clamp(color.r(), 0.0f, 1.0f),
        std::clamp(color.g(), 0.0f, 1.0f),
        std::clamp(color.b(), 0.0f, 1.0f),
        std::clamp(color.a() * alphaScale, 0.0f, 1.0f));
}

QPainter::CompositionMode compositionModeForLayer(const ArtifactAbstractLayerPtr& layer)
{
    return compositionMode(ArtifactCore::toBlendMode(layer ? layer->layerBlendType() : LAYER_BLEND_TYPE::BLEND_NORMAL));
}

QSize safeLayerSize(const ArtifactAbstractLayerPtr& layer, const QSize& fallback = QSize(320, 180))
{
    if (!layer) {
        return fallback;
    }
    const auto source = layer->sourceSize();
    return QSize(std::max(16, source.width), std::max(16, source.height));
}

QImage renderTextLayerSurface(const std::shared_ptr<ArtifactTextLayer>& textLayer, const QSize& size)
{
    if (!textLayer) {
        return {};
    }
    (void)size;
    return textLayer->toQImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QImage renderVideoLayerSurface(const std::shared_ptr<ArtifactVideoLayer>& videoLayer, const QSize& size)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(20, 24, 30));
    if (!videoLayer) {
        return image;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(image.rect(), QLinearGradient(0.0, 0.0, static_cast<qreal>(size.width()), static_cast<qreal>(size.height())));
    painter.fillRect(image.rect(), QColor(34, 40, 48));
    painter.setPen(QPen(QColor(104, 148, 196), 2.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(image.rect().adjusted(8, 8, -8, -8), 12.0, 12.0);
    painter.setPen(QColor(240, 244, 248));
    QFont titleFont = painter.font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(QRect(18, 18, size.width() - 36, 28), Qt::AlignLeft | Qt::AlignVCenter, videoLayer->layerName());

    painter.setPen(QColor(194, 205, 216));
    QFont bodyFont = painter.font();
    bodyFont.setPointSize(10);
    bodyFont.setBold(false);
    painter.setFont(bodyFont);
    const auto info = videoLayer->streamInfo();
    const QString sourceName = QFileInfo(videoLayer->sourcePath()).fileName();
    const QString body = QStringLiteral("%1\n%2x%3  %4 fps")
        .arg(sourceName.isEmpty() ? QStringLiteral("Video layer") : sourceName)
        .arg(info.width > 0 ? info.width : size.width())
        .arg(info.height > 0 ? info.height : size.height())
        .arg(info.frameRate > 0.0 ? QString::number(info.frameRate, 'f', 3) : QStringLiteral("-"));
    painter.drawText(QRect(18, 56, size.width() - 36, size.height() - 74), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, body);
    return image;
}

QImage renderLayerSurface(const ArtifactAbstractLayerPtr& layer)
{
    if (!layer) {
        return {};
    }

    const QSize layerSize = safeLayerSize(layer);

    if (const auto imageLayer = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
        QImage image = imageLayer->toQImage();
        if (!image.isNull()) {
            return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
        // フォールバック: 画像が読み込まれていない場合はプレースホルダーを返す
        QImage placeholder(layerSize, QImage::Format_ARGB32_Premultiplied);
        placeholder.fill(QColor(60, 60, 60));
        QPainter p(&placeholder);
        p.setPen(QColor(200, 100, 100));
        p.drawText(QRect(0, 0, layerSize.width(), layerSize.height()), 
                   Qt::AlignCenter, QStringLiteral("No Image"));
        return placeholder;
    }

    if (const auto svgLayer = std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
        if (svgLayer->isLoaded()) {
            QImage image = svgLayer->toQImage();
            if (!image.isNull()) {
                return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            }
        }
        QImage placeholder(layerSize, QImage::Format_ARGB32_Premultiplied);
        placeholder.fill(QColor(60, 60, 60));
        QPainter p(&placeholder);
        p.setPen(QColor(200, 100, 100));
        p.drawText(QRect(0, 0, layerSize.width(), layerSize.height()),
                   Qt::AlignCenter, QStringLiteral("No SVG"));
        return placeholder;
    }

    if (const auto solidLayer = std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
        QImage image(layerSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(toQColor(solidLayer->color()));
        return image;
    }

    if (const auto solid2DLayer = std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
        QImage image(layerSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(toQColor(solid2DLayer->color()));
        return image;
    }

    if (const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
        return renderTextLayerSurface(textLayer, layerSize);
    }

    if (const auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
        return renderVideoLayerSurface(videoLayer, layerSize);
    }

    return renderLayerCard(layer, layerSize, QStringLiteral("Data-driven fallback"));
}

void drawLayerOnCanvas(QPainter& painter, const ArtifactAbstractLayerPtr& layer, const qreal opacityScale)
{
    if (!layer || !layer->isVisible()) {
        return;
    }

    const QImage surface = renderLayerSurface(layer);
    if (surface.isNull() && !std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer) && !std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
        return;
    }

    // Combine opacity
    const qreal finalOpacity = std::clamp(opacityScale * static_cast<qreal>(layer->opacity()), 0.0, 1.0);

    painter.save();
    
    // 1. Apply Transform
    painter.setTransform(layer->getGlobalTransform(), true);
    painter.setOpacity(finalOpacity);
    
    // 2. Apply Blend Mode
    painter.setCompositionMode(compositionModeForLayer(layer));

    // 3. Apply Masks
    if (layer->hasMasks()) {
        QPainterPath totalClip;
        for (int m = 0; m < layer->maskCount(); ++m) {
            const auto mask = layer->mask(m);
            if (!mask.isEnabled()) continue;
            for (int p = 0; p < mask.maskPathCount(); ++p) {
                totalClip.addPath(toQPainterPath(mask.maskPath(p)));
            }
        }
        if (!totalClip.isEmpty()) {
            painter.setClipPath(totalClip, Qt::IntersectClip);
        }
    }

    // 4. Draw Content
    const auto size = layer->sourceSize();
    if (const auto solidLayer = std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
        painter.fillRect(QRectF(0, 0, size.width, size.height), toQColor(solidLayer->color()));
    } else if (const auto solid2D = std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
        painter.fillRect(QRectF(0, 0, size.width, size.height), toQColor(solid2D->color()));
    } else if (!surface.isNull()) {
        painter.drawImage(QRectF(0.0, 0.0, size.width, size.height), surface);
    }

    painter.restore();
}

QImage renderCompositionCanvas(
    const ArtifactCompositionPtr& composition,
    const std::optional<ArtifactCore::LayerID>& focusLayerId = std::nullopt,
    const bool drawFocusedOnly = false,
    const qreal nonFocusedOpacity = 1.0)
{
    const QSize compSize = safeCompositionSize(composition);
    QImage canvas(compSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(composition ? toQColor(composition->backgroundColor()) : QColor(18, 20, 24));
    if (!composition) {
        return canvas;
    }

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const auto layers = composition->allLayer();
    for (const auto& layer : layers) {
        if (!layer || !layer->isVisible()) {
            continue;
        }
        const bool isFocused = focusLayerId.has_value() && layer->id() == *focusLayerId;
        if (drawFocusedOnly && !isFocused) {
            continue;
        }
        const qreal opacity = focusLayerId.has_value() && !isFocused ? nonFocusedOpacity : 1.0;
        drawLayerOnCanvas(painter, layer, opacity);
    }
    return canvas;
}

QImage fitCanvasImageToPreview(
    const QImage& canvas,
    const QSize& previewSize,
    const PreviewNavigationState& nav)
{
    QImage image(previewSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (canvas.isNull()) {
        return image;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRectF targetRect = previewCanvasRect(previewSize, canvas.size(), nav);
    painter.drawImage(targetRect, canvas, QRectF(0.0, 0.0, canvas.width(), canvas.height()));
    return image;
}

} // namespace

QImage generateCompositionThumbnail(const ArtifactCompositionPtr& composition, const QSize& thumbnailSize)
{
    if (!composition) {
        QImage placeholder(thumbnailSize, QImage::Format_ARGB32_Premultiplied);
        placeholder.fill(QColor(40, 40, 40));
        return placeholder;
    }
    
    // Use the unified rendering logic for consistent thumbnail generation
    const QImage canvas = renderCompositionCanvas(composition);
    
    // Resize to thumbnail size
    return canvas.scaled(thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

namespace {

QString compositionSummaryText(const ArtifactCompositionPtr& composition)
{
    if (!composition) {
        return QStringLiteral("No composition selected");
    }

    const QSize compSize = safeCompositionSize(composition);
    const auto layers = composition->allLayer();
    int visibleCount = 0;
    int solidCount = 0;
    int imageCount = 0;
    int textCount = 0;
    for (const auto& layer : layers) {
        if (!layer) {
            continue;
        }
        if (layer->isVisible()) {
            ++visibleCount;
        }
        if (std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer) || std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
            ++solidCount;
        } else if (std::dynamic_pointer_cast<ArtifactImageLayer>(layer) ||
                   std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
            ++imageCount;
        } else if (std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
            ++textCount;
        }
    }

    return QStringLiteral("%1 | %2x%3 | Layers: %4 | Visible: %5 | Solids: %6 | Images: %7 | Text: %8 | FPS: %9")
        .arg(composition->settings().compositionName().toQString())
        .arg(compSize.width())
        .arg(compSize.height())
        .arg(layers.size())
        .arg(visibleCount)
        .arg(solidCount)
        .arg(imageCount)
        .arg(textCount)
        .arg(QString::number(composition->frameRate().framerate(), 'f', 3));
}

QImage renderCompositionOverlay(
    const ArtifactCompositionPtr& composition,
    const QSize& previewSize,
    const PreviewNavigationState& nav)
{
    QImage image(previewSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!composition) {
        return image;
    }

    const QRectF canvasRect = previewCanvasRect(previewSize, safeCompositionSize(composition), nav);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(120, 180, 255, 180), 1.0, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(canvasRect, 8.0, 8.0);
    painter.drawLine(QPointF(canvasRect.center().x(), canvasRect.top()), QPointF(canvasRect.center().x(), canvasRect.bottom()));
    painter.drawLine(QPointF(canvasRect.left(), canvasRect.center().y()), QPointF(canvasRect.right(), canvasRect.center().y()));
    return image;
}

QImage renderFocusedLayerOverlay(
    const ArtifactCompositionPtr& composition,
    const ArtifactAbstractLayerPtr& layer,
    const QSize& previewSize,
    const PreviewNavigationState& nav)
{
    QImage image(previewSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!composition || !layer) {
        return image;
    }

    const QSize compSize = safeCompositionSize(composition);
    const QRectF canvasRect = previewCanvasRect(previewSize, compSize, nav);
    const QRectF bounds = layer->transformedBoundingBox();
    if (bounds.isEmpty()) {
        return image;
    }

    const qreal sx = canvasRect.width() / static_cast<qreal>(std::max(1, compSize.width()));
    const qreal sy = canvasRect.height() / static_cast<qreal>(std::max(1, compSize.height()));
    const QRectF mapped(
        canvasRect.left() + bounds.left() * sx,
        canvasRect.top() + bounds.top() * sy,
        std::max(1.0, bounds.width() * sx),
        std::max(1.0, bounds.height() * sy));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(255, 215, 96, 220), 2.0, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(mapped, 6.0, 6.0);
    painter.setPen(QColor(255, 244, 208));
    painter.drawText(mapped.adjusted(8.0, 6.0, -8.0, -6.0), Qt::AlignLeft | Qt::AlignTop, layer->layerName());
    return image;
}

QImage mergePreviewImages(std::initializer_list<QImage> images, const QSize& size)
{
    QImage merged(size, QImage::Format_ARGB32_Premultiplied);
    merged.fill(Qt::transparent);

    QPainter painter(&merged);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (const QImage& image : images) {
        if (!image.isNull()) {
            painter.drawImage(QPoint(0, 0), image);
        }
    }
    return merged;
}

QString layerSummaryText(const ArtifactCompositionPtr& composition, const ArtifactAbstractLayerPtr& layer)
{
    if (!layer) {
        return QStringLiteral("No layer selected");
    }

    const auto sourceSize = layer->sourceSize();
    const QRectF bounds = layer->transformedBoundingBox();
    QString sourceLabel = QStringLiteral("%1x%2")
        .arg(std::max(0, sourceSize.width))
        .arg(std::max(0, sourceSize.height));
    if (sourceSize.width <= 0 || sourceSize.height <= 0) {
        sourceLabel = QStringLiteral("-");
    }

    return QStringLiteral("%1 | %2 | Blend=%3 | Visible=%4 | Source=%5 | Bounds=(%6, %7, %8x%9)%10")
        .arg(layer->layerName())
        .arg(layer->className().toQString())
        .arg(blendLabel(ArtifactCore::toBlendMode(layer->layerBlendType())))
        .arg(layer->isVisible() ? QStringLiteral("Yes") : QStringLiteral("No"))
        .arg(sourceLabel)
        .arg(QString::number(bounds.x(), 'f', 1))
        .arg(QString::number(bounds.y(), 'f', 1))
        .arg(QString::number(bounds.width(), 'f', 1))
        .arg(QString::number(bounds.height(), 'f', 1))
        .arg(composition ? QStringLiteral(" | Comp=%1").arg(composition->settings().compositionName().toQString()) : QString());
}

} // namespace

W_OBJECT_IMPL(ArtifactSoftwareCompositionTestWidget)
W_OBJECT_IMPL(ArtifactSoftwareLayerTestWidget)

class ArtifactSoftwareCompositionTestWidget::Impl {
public:
    QWidget* owner_ = nullptr;
    ArtifactProjectService* service_ = nullptr;
    QComboBox* compositionCombo_ = nullptr;
    QCheckBox* followCurrentCompositionCheck_ = nullptr;
    QComboBox* effectCombo_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QLabel* previewLabel_ = nullptr;
    QImage lastImage_;
    PreviewNavigationState nav_;
    bool isPanning_ = false;
    QPointF lastMousePos_;

    ArtifactCompositionPtr selectedComposition() const
    {
        if (!service_ || !compositionCombo_) {
            return {};
        }
        const QString idString = compositionCombo_->currentData().toString();
        if (idString.trimmed().isEmpty()) {
            return service_->currentComposition().lock();
        }
        const auto found = service_->findComposition(ArtifactCore::CompositionID(idString));
        return found.success ? found.ptr.lock() : ArtifactCompositionPtr();
    }

    void setCurrentCompositionSelection(const ArtifactCore::CompositionID& id)
    {
        if (!compositionCombo_ || id.isNil()) {
            return;
        }
        const int index = compositionCombo_->findData(id.toString());
        if (index >= 0) {
            compositionCombo_->setCurrentIndex(index);
        }
    }

    void reloadCompositions()
    {
        if (!service_ || !compositionCombo_) {
            return;
        }
        const QString previous = compositionCombo_->currentData().toString();
        QVector<QPair<ArtifactCore::CompositionID, QString>> entries;
        collectCompositionEntries(service_, service_->projectItems(), &entries);
        QSignalBlocker blocker(compositionCombo_);
        compositionCombo_->clear();
        for (const auto& entry : entries) {
            compositionCombo_->addItem(entry.second, entry.first.toString());
        }
        if (compositionCombo_->count() == 0) {
            compositionCombo_->addItem(QStringLiteral("(No composition)"), QString());
            return;
        }
        int index = compositionCombo_->findData(previous);
        if (index < 0) {
            if (const auto current = service_->currentComposition().lock()) {
                index = compositionCombo_->findData(current->id().toString());
            }
        }
        compositionCombo_->setCurrentIndex(std::max(0, index));
    }

    void refreshPreview()
    {
        if (!owner_ || !owner_->isVisible() || owner_->isHidden() ||
            !owner_->window() || !owner_->window()->isVisible()) {
            return;
        }
        QElapsedTimer timer;
        timer.start();
        const QSize renderSize = safePreviewSize(previewLabel_);
        const ArtifactCompositionPtr composition = selectedComposition();
        const QImage compositionCanvas = renderCompositionCanvas(composition);
        SoftwareRender::CompositeRequest request;
        request.background = makeCheckerboard(renderSize);
        request.foreground = fitCanvasImageToPreview(compositionCanvas, renderSize, nav_);
        request.overlay = renderCompositionOverlay(composition, renderSize, nav_);
        request.outputSize = renderSize;
        request.backend = SoftwareRender::CompositeBackend::OpenCV;
        request.cvEffect = effectFromIndex(effectCombo_ ? effectCombo_->currentIndex() : 0);
        request.overlayOpacity = 0.9f;
        request.blendMode = ArtifactCore::BlendMode::Screen;

        lastImage_ = SoftwareRender::compose(request);
        if (previewLabel_) {
            previewLabel_->setPixmap(QPixmap::fromImage(lastImage_));
        }
        if (summaryLabel_) {
            summaryLabel_->setText(compositionSummaryText(composition));
        }

        const qint64 elapsedMs = timer.elapsed();
        if (elapsedMs >= 8 || (owner_ && owner_->isVisible())) {
            qCDebug(softwareInspectorPerfLog) << "[SoftwareCompositionView][Refresh]"
                                              << "ms=" << elapsedMs
                                              << "visible=" << (owner_ ? owner_->isVisible() : false)
                                              << "hidden=" << (owner_ ? owner_->isHidden() : true)
                                              << "windowVisible=" << (owner_ && owner_->window() ? owner_->window()->isVisible() : false)
                                              << "renderSize=" << renderSize
                                              << "hasComp=" << static_cast<bool>(composition);
        }
    }
};

class ArtifactSoftwareLayerTestWidget::Impl {
public:
    QWidget* owner_ = nullptr;
    ArtifactProjectService* service_ = nullptr;
    QComboBox* compositionCombo_ = nullptr;
    QComboBox* layerCombo_ = nullptr;
    QCheckBox* followCurrentCompositionCheck_ = nullptr;
    QCheckBox* followSelectionCheck_ = nullptr;
    QComboBox* effectCombo_ = nullptr;
    QLabel* infoLabel_ = nullptr;
    QLabel* previewLabel_ = nullptr;
    QImage lastImage_;
    PreviewNavigationState nav_;
    bool isPanning_ = false;
    QPointF lastMousePos_;

    ArtifactCompositionPtr selectedComposition() const;
    ArtifactAbstractLayerPtr selectedLayer() const;
    void setCurrentCompositionSelection(const ArtifactCore::CompositionID& id);
    void setCurrentLayerSelection(const ArtifactCore::LayerID& id);
    void reloadCompositions();
    void reloadLayers();
    void refreshPreview();
};

ArtifactCompositionPtr ArtifactSoftwareLayerTestWidget::Impl::selectedComposition() const
{
    if (!service_ || !compositionCombo_) {
        return {};
    }
    const QString idString = compositionCombo_->currentData().toString();
    if (idString.trimmed().isEmpty()) {
        return service_->currentComposition().lock();
    }
    const auto found = service_->findComposition(ArtifactCore::CompositionID(idString));
    return found.success ? found.ptr.lock() : ArtifactCompositionPtr();
}

ArtifactAbstractLayerPtr ArtifactSoftwareLayerTestWidget::Impl::selectedLayer() const
{
    const ArtifactCompositionPtr composition = selectedComposition();
    if (!composition || !layerCombo_) {
        return {};
    }
    const QString idString = layerCombo_->currentData().toString();
    if (idString.trimmed().isEmpty()) {
        return {};
    }
    return composition->layerById(ArtifactCore::LayerID(idString));
}

void ArtifactSoftwareLayerTestWidget::Impl::setCurrentCompositionSelection(const ArtifactCore::CompositionID& id)
{
    if (!compositionCombo_ || id.isNil()) {
        return;
    }
    const int index = compositionCombo_->findData(id.toString());
    if (index >= 0) {
        compositionCombo_->setCurrentIndex(index);
    }
}

void ArtifactSoftwareLayerTestWidget::Impl::setCurrentLayerSelection(const ArtifactCore::LayerID& id)
{
    if (!layerCombo_ || id.isNil()) {
        return;
    }
    const int index = layerCombo_->findData(id.toString());
    if (index >= 0) {
        layerCombo_->setCurrentIndex(index);
    }
}

void ArtifactSoftwareLayerTestWidget::Impl::reloadCompositions()
{
    if (!service_ || !compositionCombo_) {
        return;
    }
    const QString previous = compositionCombo_->currentData().toString();
    QVector<QPair<ArtifactCore::CompositionID, QString>> entries;
    collectCompositionEntries(service_, service_->projectItems(), &entries);
    QSignalBlocker blocker(compositionCombo_);
    compositionCombo_->clear();
    for (const auto& entry : entries) {
        compositionCombo_->addItem(entry.second, entry.first.toString());
    }
    if (compositionCombo_->count() == 0) {
        compositionCombo_->addItem(QStringLiteral("(No composition)"), QString());
        return;
    }
    int index = compositionCombo_->findData(previous);
    if (index < 0) {
        if (const auto current = service_->currentComposition().lock()) {
            index = compositionCombo_->findData(current->id().toString());
        }
    }
    compositionCombo_->setCurrentIndex(std::max(0, index));
}

void ArtifactSoftwareLayerTestWidget::Impl::reloadLayers()
{
    if (!layerCombo_) {
        return;
    }
    const QString previous = layerCombo_->currentData().toString();
    const ArtifactCompositionPtr composition = selectedComposition();
    QSignalBlocker blocker(layerCombo_);
    layerCombo_->clear();
    if (!composition) {
        layerCombo_->addItem(QStringLiteral("(No layer)"), QString());
        return;
    }
    const auto layers = composition->allLayer();
    for (const auto& layer : layers) {
        if (!layer) {
            continue;
        }
        layerCombo_->addItem(layer->layerName(), layer->id().toString());
    }
    if (layerCombo_->count() == 0) {
        layerCombo_->addItem(QStringLiteral("(No layer)"), QString());
        return;
    }
    const int index = layerCombo_->findData(previous);
    layerCombo_->setCurrentIndex(index >= 0 ? index : 0);
}

void ArtifactSoftwareLayerTestWidget::Impl::refreshPreview()
{
    if (!owner_ || !owner_->isVisible() || owner_->isHidden() ||
        !owner_->window() || !owner_->window()->isVisible()) {
        return;
    }
    QElapsedTimer timer;
    timer.start();
    const QSize renderSize = safePreviewSize(previewLabel_);
    const ArtifactAbstractLayerPtr layer = selectedLayer();
    const ArtifactCompositionPtr composition = selectedComposition();
    const std::optional<ArtifactCore::LayerID> focusLayerId = layer ? std::optional<ArtifactCore::LayerID>(layer->id()) : std::nullopt;
    const QImage contextCanvas = renderCompositionCanvas(composition, focusLayerId, false, 0.18);
    const QImage focusedCanvas = renderCompositionCanvas(composition, focusLayerId, true, 1.0);
    const QImage overlayImage = mergePreviewImages(
        {
            fitCanvasImageToPreview(focusedCanvas, renderSize, nav_),
            renderCompositionOverlay(composition, renderSize, nav_),
            renderFocusedLayerOverlay(composition, layer, renderSize, nav_)
        },
        renderSize);

    SoftwareRender::CompositeRequest request;
    request.background = makeCheckerboard(renderSize);
    request.foreground = fitCanvasImageToPreview(contextCanvas, renderSize, nav_);
    request.overlay = overlayImage;
    request.outputSize = renderSize;
        request.backend = SoftwareRender::CompositeBackend::OpenCV;
    request.cvEffect = effectFromIndex(effectCombo_ ? effectCombo_->currentIndex() : 0);
    request.blendMode = layer ? ArtifactCore::toBlendMode(layer->layerBlendType()) : ArtifactCore::BlendMode::Normal;
    request.overlayOpacity = 1.0f;
    request.useForeground = true;

    lastImage_ = SoftwareRender::compose(request);
    if (previewLabel_) {
        previewLabel_->setPixmap(QPixmap::fromImage(lastImage_));
    }

    if (infoLabel_) {
        infoLabel_->setText(layerSummaryText(composition, layer));
    }

    const qint64 elapsedMs = timer.elapsed();
    if (elapsedMs >= 8 || (owner_ && owner_->isVisible())) {
        qCDebug(softwareInspectorPerfLog) << "[SoftwareLayerView][Refresh]"
                                          << "ms=" << elapsedMs
                                          << "visible=" << (owner_ ? owner_->isVisible() : false)
                                          << "hidden=" << (owner_ ? owner_->isHidden() : true)
                                          << "windowVisible=" << (owner_ && owner_->window() ? owner_->window()->isVisible() : false)
                                          << "renderSize=" << renderSize
                                          << "hasComp=" << static_cast<bool>(composition)
                                          << "hasLayer=" << static_cast<bool>(layer);
    }
}

ArtifactSoftwareCompositionTestWidget::ArtifactSoftwareCompositionTestWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    impl_->owner_ = this;
    impl_->service_ = ArtifactProjectService::instance();

    auto* root = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout();
    impl_->compositionCombo_ = new QComboBox(this);
    impl_->followCurrentCompositionCheck_ = new QCheckBox(QStringLiteral("Follow current composition"), this);
    impl_->followCurrentCompositionCheck_->setChecked(true);
    impl_->effectCombo_ = new QComboBox(this);
    impl_->effectCombo_->addItems(QStringList{QStringLiteral("None"), QStringLiteral("GaussianBlur"), QStringLiteral("EdgeOverlay")});
    auto* saveButton = new QPushButton(QStringLiteral("Save PNG"), this);

    controls->addWidget(new QLabel(QStringLiteral("Composition"), this), 0);
    controls->addWidget(impl_->compositionCombo_, 1);
    controls->addWidget(impl_->followCurrentCompositionCheck_, 0);
    controls->addWidget(new QLabel(QStringLiteral("Effect"), this), 0);
    controls->addWidget(impl_->effectCombo_, 0);
    controls->addWidget(saveButton, 0);
    root->addLayout(controls);

    impl_->summaryLabel_ = new QLabel(QStringLiteral("No composition selected"), this);
    root->addWidget(impl_->summaryLabel_);

    impl_->previewLabel_ = new QLabel(this);
    impl_->previewLabel_->setAlignment(Qt::AlignCenter);
    impl_->previewLabel_->setMinimumSize(720, 405);
    impl_->previewLabel_->setFrameShape(QFrame::StyledPanel);
    root->addWidget(impl_->previewLabel_, 1);

    impl_->reloadCompositions();
    impl_->refreshPreview();

    connect(impl_->compositionCombo_, &QComboBox::currentTextChanged, this, [this](const QString&) {
        impl_->refreshPreview();
    });
    connect(impl_->effectCombo_, &QComboBox::currentTextChanged, this, [this](const QString&) {
        impl_->refreshPreview();
    });
    connect(saveButton, &QPushButton::clicked, this, [this]() {
        saveImagePreview(this, impl_->lastImage_, QStringLiteral("software_composition_test"));
    });

    if (impl_->service_) {
        QObject::connect(impl_->service_, &ArtifactProjectService::projectChanged, this, [this]() {
            impl_->reloadCompositions();
            impl_->refreshPreview();
        });
        QObject::connect(impl_->service_, &ArtifactProjectService::compositionCreated, this, [this](const ArtifactCore::CompositionID& id) {
            impl_->reloadCompositions();
            if (impl_->followCurrentCompositionCheck_ && impl_->followCurrentCompositionCheck_->isChecked()) {
                impl_->setCurrentCompositionSelection(id);
            }
            impl_->refreshPreview();
        });
        QObject::connect(impl_->service_, &ArtifactProjectService::currentCompositionChanged, this, [this](const ArtifactCore::CompositionID& id) {
            if (impl_->followCurrentCompositionCheck_ && impl_->followCurrentCompositionCheck_->isChecked()) {
                impl_->setCurrentCompositionSelection(id);
            }
            impl_->refreshPreview();
        });
        QObject::connect(impl_->service_, &ArtifactProjectService::layerCreated, this, [this](const ArtifactCore::CompositionID&, const ArtifactCore::LayerID&) {
            impl_->refreshPreview();
        });
        QObject::connect(impl_->service_, &ArtifactProjectService::layerRemoved, this, [this](const ArtifactCore::CompositionID&, const ArtifactCore::LayerID&) {
            impl_->refreshPreview();
        });
    }
}

ArtifactSoftwareCompositionTestWidget::~ArtifactSoftwareCompositionTestWidget()
{
    delete impl_;
}

void ArtifactSoftwareCompositionTestWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (impl_) {
        impl_->refreshPreview();
    }
}

void ArtifactSoftwareCompositionTestWidget::keyPressEvent(QKeyEvent* event)
{
    if (impl_ && handleNavigationShortcut(event, &impl_->nav_)) {
        impl_->refreshPreview();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ArtifactSoftwareCompositionTestWidget::wheelEvent(QWheelEvent* event)
{
    if (impl_ && handleNavigationWheel(event, &impl_->nav_)) {
        impl_->refreshPreview();
        return;
    }
    QWidget::wheelEvent(event);
}

void ArtifactSoftwareCompositionTestWidget::mousePressEvent(QMouseEvent* event)
{
    if (!impl_) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::MiddleButton) {
        impl_->isPanning_ = true;
        impl_->lastMousePos_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ArtifactSoftwareCompositionTestWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!impl_ || !impl_->isPanning_) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPointF delta = event->position() - impl_->lastMousePos_;
    impl_->lastMousePos_ = event->position();
    impl_->nav_.pan += delta;
    impl_->refreshPreview();
    event->accept();
}

void ArtifactSoftwareCompositionTestWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (impl_ && impl_->isPanning_ &&
        (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        impl_->isPanning_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

ArtifactSoftwareLayerTestWidget::ArtifactSoftwareLayerTestWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    impl_->owner_ = this;
    impl_->service_ = ArtifactProjectService::instance();

    auto* root = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout();
    impl_->compositionCombo_ = new QComboBox(this);
    impl_->layerCombo_ = new QComboBox(this);
    impl_->followCurrentCompositionCheck_ = new QCheckBox(QStringLiteral("Follow current composition"), this);
    impl_->followCurrentCompositionCheck_->setChecked(true);
    impl_->followSelectionCheck_ = new QCheckBox(QStringLiteral("Follow layer selection"), this);
    impl_->followSelectionCheck_->setChecked(true);
    auto* saveButton = new QPushButton(QStringLiteral("Save PNG"), this);
    topRow->addWidget(new QLabel(QStringLiteral("Composition"), this), 0);
    topRow->addWidget(impl_->compositionCombo_, 1);
    topRow->addWidget(impl_->followCurrentCompositionCheck_, 0);
    topRow->addWidget(new QLabel(QStringLiteral("Layer"), this), 0);
    topRow->addWidget(impl_->layerCombo_, 1);
    topRow->addWidget(impl_->followSelectionCheck_, 0);
    topRow->addWidget(saveButton, 0);
    root->addLayout(topRow);

    auto* controlGrid = new QFormLayout();
    impl_->effectCombo_ = new QComboBox(this);
    impl_->effectCombo_->addItems(QStringList{QStringLiteral("None"), QStringLiteral("GaussianBlur"), QStringLiteral("EdgeOverlay")});

    controlGrid->addRow(QStringLiteral("Effect"), impl_->effectCombo_);
    root->addLayout(controlGrid);

    impl_->infoLabel_ = new QLabel(QStringLiteral("No layer selected"), this);
    root->addWidget(impl_->infoLabel_);

    impl_->previewLabel_ = new QLabel(this);
    impl_->previewLabel_->setAlignment(Qt::AlignCenter);
    impl_->previewLabel_->setMinimumSize(720, 405);
    impl_->previewLabel_->setFrameShape(QFrame::StyledPanel);
    root->addWidget(impl_->previewLabel_, 1);

    impl_->reloadCompositions();
    impl_->reloadLayers();
    impl_->refreshPreview();

    connect(impl_->compositionCombo_, &QComboBox::currentTextChanged, this, [this](const QString&) {
        impl_->reloadLayers();
        impl_->refreshPreview();
    });
    connect(impl_->layerCombo_, &QComboBox::currentTextChanged, this, [this](const QString&) {
        impl_->refreshPreview();
    });
    for (QObject* control : {static_cast<QObject*>(impl_->effectCombo_)}) {
        if (auto* combo = qobject_cast<QComboBox*>(control)) {
            connect(combo, &QComboBox::currentTextChanged, this, [this](const QString&) {
                impl_->refreshPreview();
            });
        }
    }
    connect(saveButton, &QPushButton::clicked, this, [this]() {
        saveImagePreview(this, impl_->lastImage_, QStringLiteral("software_layer_test"));
    });

    if (impl_->service_) {
        QObject::connect(impl_->service_, &ArtifactProjectService::projectChanged, this, [this]() {
            impl_->reloadCompositions();
            impl_->reloadLayers();
            impl_->refreshPreview();
        });
        QObject::connect(impl_->service_, &ArtifactProjectService::currentCompositionChanged, this, [this](const ArtifactCore::CompositionID& id) {
            if (impl_->followCurrentCompositionCheck_ && impl_->followCurrentCompositionCheck_->isChecked()) {
                impl_->setCurrentCompositionSelection(id);
                impl_->reloadLayers();
            }
            impl_->refreshPreview();
        });
        QObject::connect(impl_->service_, &ArtifactProjectService::layerCreated, this, [this](const ArtifactCore::CompositionID&, const ArtifactCore::LayerID&) {
            impl_->reloadLayers();
            impl_->refreshPreview();
        });
        QObject::connect(impl_->service_, &ArtifactProjectService::layerRemoved, this, [this](const ArtifactCore::CompositionID&, const ArtifactCore::LayerID&) {
            impl_->reloadLayers();
            impl_->refreshPreview();
        });
        QObject::connect(impl_->service_, &ArtifactProjectService::layerSelected, this, [this](const ArtifactCore::LayerID& id) {
            if (!impl_->followSelectionCheck_ || !impl_->followSelectionCheck_->isChecked()) {
                return;
            }
            impl_->setCurrentLayerSelection(id);
            impl_->refreshPreview();
        });
    }
}

ArtifactSoftwareLayerTestWidget::~ArtifactSoftwareLayerTestWidget()
{
    delete impl_;
}

void ArtifactSoftwareLayerTestWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (impl_) {
        impl_->refreshPreview();
    }
}

void ArtifactSoftwareLayerTestWidget::keyPressEvent(QKeyEvent* event)
{
    if (impl_ && handleNavigationShortcut(event, &impl_->nav_)) {
        impl_->refreshPreview();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ArtifactSoftwareLayerTestWidget::wheelEvent(QWheelEvent* event)
{
    if (impl_ && handleNavigationWheel(event, &impl_->nav_)) {
        impl_->refreshPreview();
        return;
    }
    QWidget::wheelEvent(event);
}

void ArtifactSoftwareLayerTestWidget::mousePressEvent(QMouseEvent* event)
{
    if (!impl_) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::MiddleButton) {
        impl_->isPanning_ = true;
        impl_->lastMousePos_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ArtifactSoftwareLayerTestWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!impl_ || !impl_->isPanning_) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPointF delta = event->position() - impl_->lastMousePos_;
    impl_->lastMousePos_ = event->position();
    impl_->nav_.pan += delta;
    impl_->refreshPreview();
    event->accept();
}

void ArtifactSoftwareLayerTestWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (impl_ && impl_->isPanning_ &&
        (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        impl_->isPanning_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

} // namespace Artifact
