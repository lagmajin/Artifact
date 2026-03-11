module;
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
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
import Layer.Blend;

namespace Artifact {

namespace {

QString blendLabel(const ArtifactCore::BlendMode mode)
{
    return SoftwareRender::blendModeText(mode);
}

ArtifactCore::BlendMode blendModeFromText(const QString& text)
{
    if (text == QStringLiteral("Add")) {
        return ArtifactCore::BlendMode::Add;
    }
    if (text == QStringLiteral("Multiply")) {
        return ArtifactCore::BlendMode::Multiply;
    }
    if (text == QStringLiteral("Screen")) {
        return ArtifactCore::BlendMode::Screen;
    }
    return ArtifactCore::BlendMode::Normal;
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

QImage renderCompositionForeground(
    const ArtifactCompositionPtr& composition,
    const QSize& previewSize,
    QString* summaryText)
{
    QImage image(previewSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    if (!composition) {
        if (summaryText) {
            *summaryText = QStringLiteral("No composition");
        }
        return image;
    }

    const QSize compSize = safeCompositionSize(composition);
    const QRectF canvasRect = fitCanvasRect(previewSize, compSize).adjusted(12.0, 12.0, -12.0, -12.0);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(96, 104, 120), 1.0));
    painter.setBrush(QColor(18, 20, 24, 70));
    painter.drawRoundedRect(canvasRect, 8.0, 8.0);

    const auto layers = composition->allLayer();
    int visibleCount = 0;
    for (int i = 0; i < layers.size(); ++i) {
        const auto& layer = layers.at(i);
        if (!layer || !layer->isVisible()) {
            continue;
        }
        ++visibleCount;
        QRectF layerRect = layer->transformedBoundingBox();
        if (layerRect.isEmpty()) {
            const qreal width = std::max(160.0, canvasRect.width() * 0.42);
            layerRect = QRectF(
                canvasRect.left() + 22.0 + (i * 18.0),
                canvasRect.top() + 22.0 + (i * 16.0),
                width,
                44.0 + ((i % 3) * 8.0));
        } else {
            const qreal sx = canvasRect.width() / static_cast<qreal>(compSize.width());
            const qreal sy = canvasRect.height() / static_cast<qreal>(compSize.height());
            layerRect = QRectF(
                canvasRect.left() + (layerRect.left() * sx),
                canvasRect.top() + (layerRect.top() * sy),
                std::max(24.0, layerRect.width() * sx),
                std::max(18.0, layerRect.height() * sy));
        }

        const QColor base = colorForKey(layer->layerName() + QString::number(i));
        painter.setBrush(base);
        painter.setPen(QPen(base.lighter(160), 1.5));
        painter.drawRoundedRect(layerRect, 6.0, 6.0);
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(layerRect.adjusted(10.0, 6.0, -10.0, -6.0), Qt::AlignLeft | Qt::AlignVCenter, layer->layerName());
    }

    if (summaryText) {
        *summaryText = QStringLiteral("%1 | %2x%3 | Layers: %4 | Visible: %5 | FPS: %6")
            .arg(composition->settings().compositionName().toQString())
            .arg(compSize.width())
            .arg(compSize.height())
            .arg(layers.size())
            .arg(visibleCount)
            .arg(QString::number(composition->frameRate().framerate(), 'f', 3));
    }
    return image;
}

QImage renderCompositionOverlay(const ArtifactCompositionPtr& composition, const QSize& previewSize)
{
    QImage image(previewSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!composition) {
        return image;
    }

    const QSize compSize = safeCompositionSize(composition);
    const QRectF canvasRect = fitCanvasRect(previewSize, compSize).adjusted(12.0, 12.0, -12.0, -12.0);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(120, 180, 255, 180), 1.0, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(canvasRect, 8.0, 8.0);
    painter.drawLine(QPointF(canvasRect.center().x(), canvasRect.top()), QPointF(canvasRect.center().x(), canvasRect.bottom()));
    painter.drawLine(QPointF(canvasRect.left(), canvasRect.center().y()), QPointF(canvasRect.right(), canvasRect.center().y()));
    return image;
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
        QString summary;
        SoftwareRender::CompositeRequest request;
        request.background = makeCheckerboard(renderSize);
        request.foreground = renderCompositionForeground(composition, renderSize, &summary);
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
            summaryLabel_->setText(summary.isEmpty() ? QStringLiteral("No composition selected") : summary);
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
    QComboBox* blendCombo_ = nullptr;
    QDoubleSpinBox* opacitySpin_ = nullptr;
    QDoubleSpinBox* offsetXSpin_ = nullptr;
    QDoubleSpinBox* offsetYSpin_ = nullptr;
    QDoubleSpinBox* scaleSpin_ = nullptr;
    QDoubleSpinBox* rotationSpin_ = nullptr;
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

    SoftwareRender::CompositeRequest request;
    request.background = makeCheckerboard(renderSize);
    request.foreground = renderCompositionOverlay(composition, renderSize);
    request.overlay = renderLayerCard(
        layer,
        QSize(std::max(240, renderSize.width() / 2), std::max(180, renderSize.height() / 2)),
        QStringLiteral("Sandbox only: this does not write back to the project"));
    request.outputSize = renderSize;
    request.backend = backendFromIndex(backendCombo_ ? backendCombo_->currentIndex() : 0);
    request.cvEffect = effectFromIndex(effectCombo_ ? effectCombo_->currentIndex() : 0);
    request.blendMode = blendModeFromText(blendCombo_ ? blendCombo_->currentText() : QStringLiteral("Normal"));
    request.overlayOpacity = static_cast<float>(opacitySpin_ ? opacitySpin_->value() : 1.0);
    request.overlayOffset = QPointF(
        offsetXSpin_ ? offsetXSpin_->value() : 0.0,
        offsetYSpin_ ? offsetYSpin_->value() : 0.0);
    request.overlayScale = static_cast<float>(scaleSpin_ ? scaleSpin_->value() : 1.0);
    request.overlayRotationDeg = static_cast<float>(rotationSpin_ ? rotationSpin_->value() : 0.0);
    request.useForeground = true;

    lastImage_ = SoftwareRender::compose(request);
    if (previewLabel_) {
        previewLabel_->setPixmap(QPixmap::fromImage(lastImage_));
    }

    if (infoLabel_) {
        if (!layer) {
            infoLabel_->setText(QStringLiteral("No layer selected"));
        } else {
            infoLabel_->setText(QStringLiteral("%1 | %2 | Blend=%3 | Visible=%4")
                .arg(layer->layerName())
                .arg(layer->className().toQString())
                .arg(blendLabel(ArtifactCore::toBlendMode(layer->layerBlendType())))
                .arg(layer->isVisible() ? QStringLiteral("Yes") : QStringLiteral("No")));
        }
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
    impl_->blendCombo_ = new QComboBox(this);
    impl_->blendCombo_->addItems(QStringList{
        QStringLiteral("Normal"),
        QStringLiteral("Add"),
        QStringLiteral("Multiply"),
        QStringLiteral("Screen")
    });
    impl_->opacitySpin_ = new QDoubleSpinBox(this);
    impl_->opacitySpin_->setRange(0.0, 1.0);
    impl_->opacitySpin_->setDecimals(2);
    impl_->opacitySpin_->setSingleStep(0.05);
    impl_->opacitySpin_->setValue(0.85);
    impl_->offsetXSpin_ = new QDoubleSpinBox(this);
    impl_->offsetYSpin_ = new QDoubleSpinBox(this);
    impl_->scaleSpin_ = new QDoubleSpinBox(this);
    impl_->rotationSpin_ = new QDoubleSpinBox(this);
    for (QDoubleSpinBox* spin : {impl_->offsetXSpin_, impl_->offsetYSpin_}) {
        spin->setRange(-800.0, 800.0);
        spin->setDecimals(1);
        spin->setSingleStep(10.0);
    }
    impl_->scaleSpin_->setRange(0.10, 4.0);
    impl_->scaleSpin_->setDecimals(2);
    impl_->scaleSpin_->setSingleStep(0.05);
    impl_->scaleSpin_->setValue(1.0);
    impl_->rotationSpin_->setRange(-180.0, 180.0);
    impl_->rotationSpin_->setDecimals(1);
    impl_->rotationSpin_->setSingleStep(5.0);

    controlGrid->addRow(QStringLiteral("Backend"), impl_->backendCombo_);
    controlGrid->addRow(QStringLiteral("Effect"), impl_->effectCombo_);
    controlGrid->addRow(QStringLiteral("Blend"), impl_->blendCombo_);
    controlGrid->addRow(QStringLiteral("Opacity"), impl_->opacitySpin_);
    controlGrid->addRow(QStringLiteral("Offset X"), impl_->offsetXSpin_);
    controlGrid->addRow(QStringLiteral("Offset Y"), impl_->offsetYSpin_);
    controlGrid->addRow(QStringLiteral("Scale"), impl_->scaleSpin_);
    controlGrid->addRow(QStringLiteral("Rotation"), impl_->rotationSpin_);
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
    for (QObject* control : {static_cast<QObject*>(impl_->backendCombo_), static_cast<QObject*>(impl_->effectCombo_), static_cast<QObject*>(impl_->blendCombo_),
                             static_cast<QObject*>(impl_->opacitySpin_), static_cast<QObject*>(impl_->offsetXSpin_), static_cast<QObject*>(impl_->offsetYSpin_),
                             static_cast<QObject*>(impl_->scaleSpin_), static_cast<QObject*>(impl_->rotationSpin_)}) {
        if (auto* combo = qobject_cast<QComboBox*>(control)) {
            connect(combo, &QComboBox::currentTextChanged, this, [this](const QString&) {
                impl_->refreshPreview();
            });
        } else if (auto* spin = qobject_cast<QDoubleSpinBox*>(control)) {
            connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
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
