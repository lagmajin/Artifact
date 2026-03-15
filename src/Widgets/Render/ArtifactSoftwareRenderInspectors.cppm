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
#include <QPixmap>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QDateTime>
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
import Artifact.Layer.Text;
import Artifact.Layer.Video;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Solid2D;
import Layer.Blend;

namespace Artifact {

namespace {

QString blendLabel(const ArtifactCore::BlendMode mode)
{
    return SoftwareRender::blendModeText(mode);
}

SoftwareRender::CompositeBackend backendFromIndex(const int index)
{
    return index == 1 ? SoftwareRender::CompositeBackend::OpenCV : SoftwareRender::CompositeBackend::QtPainter;
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
    case ArtifactCore::BlendMode::Add:      return QPainter::CompositionMode_Plus;
    case ArtifactCore::BlendMode::Multiply: return QPainter::CompositionMode_Multiply;
    case ArtifactCore::BlendMode::Screen:   return QPainter::CompositionMode_Screen;
    case ArtifactCore::BlendMode::Normal:
    default:
        return QPainter::CompositionMode_SourceOver;
    }
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
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!textLayer) {
        return image;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font(textLayer->fontFamily().toQString());
    font.setPointSizeF(std::max(6.0f, textLayer->fontSize()));
    font.setBold(textLayer->isBold());
    font.setItalic(textLayer->isItalic());
    painter.setFont(font);
    painter.setPen(QColor::fromRgbF(
        textLayer->textColor().r(),
        textLayer->textColor().g(),
        textLayer->textColor().b(),
        textLayer->textColor().a()));
    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    option.setAlignment(Qt::AlignLeft | Qt::AlignTop);
    painter.drawText(QRectF(0.0, 0.0, size.width(), size.height()), textLayer->text().toQString(), option);
    return image;
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
    if (surface.isNull()) {
        return;
    }

    painter.save();
    painter.setOpacity(std::clamp(opacityScale, 0.0, 1.0));
    painter.setCompositionMode(compositionModeForLayer(layer));
    painter.setTransform(layer->getGlobalTransform(), true);
    const auto size = safeLayerSize(layer, surface.size());
    painter.drawImage(QRectF(0.0, 0.0, size.width(), size.height()), surface, QRectF(0.0, 0.0, surface.width(), surface.height()));
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
    canvas.fill(QColor(18, 20, 24));
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

QImage fitCanvasImageToPreview(const QImage& canvas, const QSize& previewSize)
{
    QImage image(previewSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (canvas.isNull()) {
        return image;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRectF targetRect = fitCanvasRect(previewSize, canvas.size()).adjusted(12.0, 12.0, -12.0, -12.0);
    painter.drawImage(targetRect, canvas, QRectF(0.0, 0.0, canvas.width(), canvas.height()));
    return image;
}

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
        } else if (std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
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

QImage renderCompositionOverlay(const ArtifactCompositionPtr& composition, const QSize& previewSize)
{
    QImage image(previewSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!composition) {
        return image;
    }

    const QRectF canvasRect = fitCanvasRect(previewSize, safeCompositionSize(composition)).adjusted(12.0, 12.0, -12.0, -12.0);
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
    const QSize& previewSize)
{
    QImage image(previewSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!composition || !layer) {
        return image;
    }

    const QSize compSize = safeCompositionSize(composition);
    const QRectF canvasRect = fitCanvasRect(previewSize, compSize).adjusted(12.0, 12.0, -12.0, -12.0);
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
    ArtifactProjectService* service_ = nullptr;
    QComboBox* compositionCombo_ = nullptr;
    QCheckBox* followCurrentCompositionCheck_ = nullptr;
    QComboBox* backendCombo_ = nullptr;
    QComboBox* effectCombo_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QLabel* previewLabel_ = nullptr;
    QImage lastImage_;

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
        const QSize renderSize = safePreviewSize(previewLabel_);
        const ArtifactCompositionPtr composition = selectedComposition();
        const QImage compositionCanvas = renderCompositionCanvas(composition);
        SoftwareRender::CompositeRequest request;
        request.background = makeCheckerboard(renderSize);
        request.foreground = fitCanvasImageToPreview(compositionCanvas, renderSize);
        request.overlay = renderCompositionOverlay(composition, renderSize);
        request.outputSize = renderSize;
        request.backend = backendFromIndex(backendCombo_ ? backendCombo_->currentIndex() : 0);
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
    }
};

class ArtifactSoftwareLayerTestWidget::Impl {
public:
    ArtifactProjectService* service_ = nullptr;
    QComboBox* compositionCombo_ = nullptr;
    QComboBox* layerCombo_ = nullptr;
    QCheckBox* followCurrentCompositionCheck_ = nullptr;
    QCheckBox* followSelectionCheck_ = nullptr;
    QComboBox* backendCombo_ = nullptr;
    QComboBox* effectCombo_ = nullptr;
    QLabel* infoLabel_ = nullptr;
    QLabel* previewLabel_ = nullptr;
    QImage lastImage_;

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
    const QSize renderSize = safePreviewSize(previewLabel_);
    const ArtifactAbstractLayerPtr layer = selectedLayer();
    const ArtifactCompositionPtr composition = selectedComposition();
    const std::optional<ArtifactCore::LayerID> focusLayerId = layer ? std::optional<ArtifactCore::LayerID>(layer->id()) : std::nullopt;
    const QImage contextCanvas = renderCompositionCanvas(composition, focusLayerId, false, 0.18);
    const QImage focusedCanvas = renderCompositionCanvas(composition, focusLayerId, true, 1.0);
    const QImage overlayImage = mergePreviewImages(
        {
            fitCanvasImageToPreview(focusedCanvas, renderSize),
            renderCompositionOverlay(composition, renderSize),
            renderFocusedLayerOverlay(composition, layer, renderSize)
        },
        renderSize);

    SoftwareRender::CompositeRequest request;
    request.background = makeCheckerboard(renderSize);
    request.foreground = fitCanvasImageToPreview(contextCanvas, renderSize);
    request.overlay = overlayImage;
    request.outputSize = renderSize;
    request.backend = backendFromIndex(backendCombo_ ? backendCombo_->currentIndex() : 0);
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
}

ArtifactSoftwareCompositionTestWidget::ArtifactSoftwareCompositionTestWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    impl_->service_ = ArtifactProjectService::instance();

    auto* root = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout();
    impl_->compositionCombo_ = new QComboBox(this);
    impl_->followCurrentCompositionCheck_ = new QCheckBox(QStringLiteral("Follow current composition"), this);
    impl_->followCurrentCompositionCheck_->setChecked(true);
    impl_->backendCombo_ = new QComboBox(this);
    impl_->backendCombo_->addItems(QStringList{QStringLiteral("QImage/QPainter"), QStringLiteral("OpenCV")});
    impl_->effectCombo_ = new QComboBox(this);
    impl_->effectCombo_->addItems(QStringList{QStringLiteral("None"), QStringLiteral("GaussianBlur"), QStringLiteral("EdgeOverlay")});
    auto* refreshButton = new QPushButton(QStringLiteral("Refresh"), this);
    auto* saveButton = new QPushButton(QStringLiteral("Save PNG"), this);

    controls->addWidget(new QLabel(QStringLiteral("Composition"), this), 0);
    controls->addWidget(impl_->compositionCombo_, 1);
    controls->addWidget(impl_->followCurrentCompositionCheck_, 0);
    controls->addWidget(new QLabel(QStringLiteral("Backend"), this), 0);
    controls->addWidget(impl_->backendCombo_, 0);
    controls->addWidget(new QLabel(QStringLiteral("Effect"), this), 0);
    controls->addWidget(impl_->effectCombo_, 0);
    controls->addWidget(refreshButton, 0);
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
    connect(impl_->backendCombo_, &QComboBox::currentTextChanged, this, [this](const QString&) {
        impl_->refreshPreview();
    });
    connect(impl_->effectCombo_, &QComboBox::currentTextChanged, this, [this](const QString&) {
        impl_->refreshPreview();
    });
    connect(refreshButton, &QPushButton::clicked, this, [this]() {
        impl_->reloadCompositions();
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

ArtifactSoftwareLayerTestWidget::ArtifactSoftwareLayerTestWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    impl_->service_ = ArtifactProjectService::instance();

    auto* root = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout();
    impl_->compositionCombo_ = new QComboBox(this);
    impl_->layerCombo_ = new QComboBox(this);
    impl_->followCurrentCompositionCheck_ = new QCheckBox(QStringLiteral("Follow current composition"), this);
    impl_->followCurrentCompositionCheck_->setChecked(true);
    impl_->followSelectionCheck_ = new QCheckBox(QStringLiteral("Follow layer selection"), this);
    impl_->followSelectionCheck_->setChecked(true);
    auto* refreshButton = new QPushButton(QStringLiteral("Refresh"), this);
    auto* saveButton = new QPushButton(QStringLiteral("Save PNG"), this);
    topRow->addWidget(new QLabel(QStringLiteral("Composition"), this), 0);
    topRow->addWidget(impl_->compositionCombo_, 1);
    topRow->addWidget(impl_->followCurrentCompositionCheck_, 0);
    topRow->addWidget(new QLabel(QStringLiteral("Layer"), this), 0);
    topRow->addWidget(impl_->layerCombo_, 1);
    topRow->addWidget(impl_->followSelectionCheck_, 0);
    topRow->addWidget(refreshButton, 0);
    topRow->addWidget(saveButton, 0);
    root->addLayout(topRow);

    auto* controlGrid = new QFormLayout();
    impl_->backendCombo_ = new QComboBox(this);
    impl_->backendCombo_->addItems(QStringList{QStringLiteral("QImage/QPainter"), QStringLiteral("OpenCV")});
    impl_->effectCombo_ = new QComboBox(this);
    impl_->effectCombo_->addItems(QStringList{QStringLiteral("None"), QStringLiteral("GaussianBlur"), QStringLiteral("EdgeOverlay")});

    controlGrid->addRow(QStringLiteral("Backend"), impl_->backendCombo_);
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
    for (QObject* control : {static_cast<QObject*>(impl_->backendCombo_), static_cast<QObject*>(impl_->effectCombo_)}) {
        if (auto* combo = qobject_cast<QComboBox*>(control)) {
            connect(combo, &QComboBox::currentTextChanged, this, [this](const QString&) {
                impl_->refreshPreview();
            });
        }
    }
    connect(refreshButton, &QPushButton::clicked, this, [this]() {
        impl_->reloadCompositions();
        impl_->reloadLayers();
        impl_->refreshPreview();
    });
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

} // namespace Artifact
