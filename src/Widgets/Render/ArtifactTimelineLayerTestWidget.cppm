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
#include <QFrame>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QGroupBox>
#include <QScrollArea>
#include <QTimer>
#include <QDateTime>
#include <wobjectimpl.h>

#include <vector>
#include <memory>

module Artifact.Widgets.TimelineLayerTest;

import Artifact.Render.SoftwareCompositor;
import Artifact.Service.Project;
import Artifact.Project.Items;
import Artifact.Composition.Abstract;
import Artifact.Composition.PlaybackController;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Video;
import Artifact.Layer.Audio;
import Artifact.Layers.SolidImage;
import Layer.Blend;

namespace Artifact {

namespace {

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

QString blendModeName(ArtifactCore::BlendMode mode)
{
    switch (mode) {
    case ArtifactCore::BlendMode::Normal: return "Normal";
    case ArtifactCore::BlendMode::Add: return "Add";
    case ArtifactCore::BlendMode::Multiply: return "Multiply";
    case ArtifactCore::BlendMode::Screen: return "Screen";
    case ArtifactCore::BlendMode::Overlay: return "Overlay";
    case ArtifactCore::BlendMode::Darken: return "Darken";
    case ArtifactCore::BlendMode::Lighten: return "Lighten";
    case ArtifactCore::BlendMode::ColorDodge: return "ColorDodge";
    case ArtifactCore::BlendMode::ColorBurn: return "ColorBurn";
    case ArtifactCore::BlendMode::HardLight: return "HardLight";
    case ArtifactCore::BlendMode::SoftLight: return "SoftLight";
    case ArtifactCore::BlendMode::Difference: return "Difference";
    case ArtifactCore::BlendMode::Exclusion: return "Exclusion";
    default: return "Normal";
    }
}

ArtifactCore::BlendMode blendModeFromIndex(int index)
{
    static const std::vector<ArtifactCore::BlendMode> modes = {
        ArtifactCore::BlendMode::Normal,
        ArtifactCore::BlendMode::Add,
        ArtifactCore::BlendMode::Multiply,
        ArtifactCore::BlendMode::Screen,
        ArtifactCore::BlendMode::Overlay,
        ArtifactCore::BlendMode::Darken,
        ArtifactCore::BlendMode::Lighten,
        ArtifactCore::BlendMode::ColorDodge,
        ArtifactCore::BlendMode::ColorBurn,
        ArtifactCore::BlendMode::HardLight,
        ArtifactCore::BlendMode::SoftLight,
        ArtifactCore::BlendMode::Difference,
        ArtifactCore::BlendMode::Exclusion,
    };
    if (index < 0 || index >= static_cast<int>(modes.size())) {
        return ArtifactCore::BlendMode::Normal;
    }
    return modes[index];
}

QImage renderLayerToImage(
    const std::shared_ptr<ArtifactAbstractLayer>& layer,
    const ArtifactCompositionPtr& composition,
    const QSize& canvasSize)
{
    if (!layer || !composition) {
        QImage result(canvasSize, QImage::Format_ARGB32_Premultiplied);
        result.fill(Qt::transparent);
        return result;
    }

    // ソリッドレイヤーの場合
    if (auto solidLayer = std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
        QImage result(canvasSize, QImage::Format_ARGB32_Premultiplied);
        result.fill(Qt::transparent);
        
        QPainter painter(&result);
        painter.setRenderHint(QPainter::Antialiasing);
        
        const auto color = solidLayer->color();
        const QColor fillColor = QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
        painter.fillRect(result.rect(), fillColor);
        
        // レイヤー名を描画
        painter.setPen(QColor(255, 255, 255));
        painter.setFont(QFont("Segoe UI", 14, QFont::Bold));
        painter.drawText(result.rect(), Qt::AlignCenter, solidLayer->layerName());
        
        return result;
    }
    
    // 画像/ビデオレイヤーの場合（簡易表現）
    QImage result(canvasSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // レイヤーの色を生成（名前でハッシュ）
    const uint hash = qHash(layer->layerName());
    const QColor baseColor = QColor::fromHsv(hash % 360, 160, 200, 180);
    
    // レイヤーの境界ボックスを取得
    const auto inPoint = layer->inPoint();
    const auto outPoint = layer->outPoint();
    const auto startTime = layer->startTime();
    
    // 簡易的な矩形描画
    QRectF rect(50, 50, canvasSize.width() - 100, canvasSize.height() - 100);
    painter.setBrush(baseColor);
    painter.setPen(QPen(baseColor.lighter(150), 3));
    painter.drawRoundedRect(rect, 10, 10);
    
    // レイヤー情報を描画
    painter.setPen(QColor(255, 255, 255));
    painter.setFont(QFont("Segoe UI", 12, QFont::Bold));
    painter.drawText(rect.adjusted(15, 15, -15, -15), Qt::AlignTop | Qt::AlignLeft, layer->layerName());
    
    const QString info = QString("Type: %1\nDuration: %2-%3")
        .arg(layer->className())
        .arg(inPoint.framePosition())
        .arg(outPoint.framePosition());
    
    painter.setFont(QFont("Segoe UI", 10));
    painter.drawText(rect.adjusted(15, 40, -15, -15), Qt::AlignTop | Qt::AlignLeft, info);
    
    return result;
}

} // namespace

W_OBJECT_IMPL(ArtifactTimelineLayerTestWidget)

class ArtifactTimelineLayerTestWidget::Impl {
public:
    ArtifactProjectService* service = nullptr;
    
    QWidget* previewHost = nullptr;
    QLabel* previewLabel = nullptr;
    QLabel* infoLabel = nullptr;
    
    QComboBox* compositionCombo = nullptr;
    QCheckBox* followCurrentComposition = nullptr;
    
    struct LayerControl {
        QCheckBox* visibleCheck = nullptr;
        QDoubleSpinBox* opacitySpin = nullptr;
        QComboBox* blendCombo = nullptr;
        LayerID layerId;
        std::shared_ptr<ArtifactAbstractLayer> layerPtr;
    };
    
    std::vector<LayerControl> layerControls;
    QWidget* layerControlsContainer = nullptr;
    QVBoxLayout* layerControlsLayout = nullptr;
    
    QPushButton* refreshButton = nullptr;
    QPushButton* applyButton = nullptr;
    
    QImage compositeImage;
    ArtifactCompositionPtr currentComposition;
    
    void reloadCompositions()
    {
        if (!service || !compositionCombo) return;
        
        const QString previous = compositionCombo->currentData().toString();
        QList<QPair<ArtifactCore::CompositionID, QString>> entries;
        
        auto collectEntries = [&](const auto& items) {
            for (auto* item : items) {
                if (auto* comp = dynamic_cast<ArtifactAbstractComposition*>(item)) {
                    entries.append(qMakePair(comp->id(), comp->settings().compositionName().toQString()));
                }
            }
        };
        
        if (auto project = service->getCurrentProjectSharedPtr()) {
            collectEntries(project->projectItems());
        }
        
        QSignalBlocker blocker(compositionCombo);
        compositionCombo->clear();
        
        if (entries.isEmpty()) {
            compositionCombo->addItem("(No composition)", QString());
            return;
        }
        
        for (const auto& entry : entries) {
            compositionCombo->addItem(entry.second, entry.first.toString());
        }
        
        int index = compositionCombo->findData(previous);
        if (index < 0 && service->currentComposition().lock()) {
            index = compositionCombo->findData(service->currentComposition().lock()->id().toString());
        }
        compositionCombo->setCurrentIndex(std::max(0, index));
    }
    
    void reloadLayerControls(ArtifactTimelineLayerTestWidget* parent)
    {
        if (!layerControlsContainer || !layerControlsLayout) return;
        
        // 既存のコントロールをクリア
        QLayoutItem* child;
        while ((child = layerControlsLayout->takeAt(0)) != nullptr) {
            if (child->widget()) {
                child->widget()->deleteLater();
            }
            delete child;
        }
        layerControls.clear();
        
        if (!currentComposition) return;
        
        const auto layers = currentComposition->allLayer();
        if (layers.isEmpty()) return;
        
        // 各レイヤーのコントロールを作成
        for (int i = 0; i < layers.size(); ++i) {
            const auto& layer = layers[i];
            if (!layer) continue;
            
            auto* group = new QGroupBox(QString("Layer %1: %2").arg(i + 1).arg(layer->layerName()));
            auto* layout = new QFormLayout(group);
            layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
            layout->setSpacing(8);
            
            LayerControl control;
            control.layerId = layer->id();
            control.layerPtr = layer;
            
            // Visible
            control.visibleCheck = new QCheckBox("Visible");
            control.visibleCheck->setChecked(layer->isVisible());
            layout->addRow(control.visibleCheck);
            
            // Opacity (SolidImageLayer の場合)
            if (auto solidLayer = std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
                control.opacitySpin = new QDoubleSpinBox();
                control.opacitySpin->setRange(0.0, 1.0);
                control.opacitySpin->setSingleStep(0.05);
                control.opacitySpin->setValue(solidLayer->color().alpha());
                control.opacitySpin->setSuffix(" (Alpha)");
                layout->addRow("Opacity:", control.opacitySpin);
            }
            
            // Blend Mode
            control.blendCombo = new QComboBox();
            const auto currentBlend = ArtifactCore::toBlendMode(layer->layerBlendType());
            for (int bi = 0; bi < 13; ++bi) {
                control.blendCombo->addItem(blendModeName(blendModeFromIndex(bi)));
                if (blendModeFromIndex(bi) == currentBlend) {
                    control.blendCombo->setCurrentIndex(bi);
                }
            }
            layout->addRow("Blend Mode:", control.blendCombo);
            
            layerControlsLayout->addWidget(group);
            layerControls.push_back(control);
            
            // シグナル接続 (Impl:: reloadLayerControls 内なので、parent を使用する)
            QObject::connect(control.visibleCheck, &QCheckBox::stateChanged, 
                parent, [this]() { if (autoRefresh()) updateComposite(); });
            
            if (control.opacitySpin) {
                QObject::connect(control.opacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    parent, [this]() { if (autoRefresh()) updateComposite(); });
            }
            
            QObject::connect(control.blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                parent, [this]() { if (autoRefresh()) updateComposite(); });
        }
        
        layerControlsLayout->addStretch();
    }
    
    bool autoRefresh() const
    {
        // Auto-refresh チェックボックスがあれば確認（現在は常時有効）
        return true;
    }
    
    void updateComposite()
    {
        if (!currentComposition) {
            compositeImage = QImage(800, 600, QImage::Format_ARGB32_Premultiplied);
            compositeImage.fill(QColor(32, 34, 38));
            if (previewLabel) {
                previewLabel->setPixmap(QPixmap::fromImage(compositeImage));
            }
            if (infoLabel) {
                infoLabel->setText("No composition selected");
            }
            return;
        }
        
        const QSize canvasSize(800, 600);
        QImage result = makeCheckerboard(canvasSize);
        
        int visibleLayerCount = 0;
        
        // 各レイヤーを合成
        for (size_t i = 0; i < layerControls.size(); ++i) {
            const auto& control = layerControls[i];
            if (!control.visibleCheck->isChecked()) {
                continue;
            }
            
            ++visibleLayerCount;
            
            // レイヤー画像を生成
            QImage layerImage = renderLayerToImage(control.layerPtr, currentComposition, canvasSize);
            
            // 不透明度を適用（SolidImageLayer の場合）
            if (control.opacitySpin) {
                const double opacity = control.opacitySpin->value();
                if (opacity < 1.0) {
                    QPainter alphaPainter(&layerImage);
                    alphaPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                    alphaPainter.fillRect(layerImage.rect(), QColor(0, 0, 0, static_cast<int>(255 * opacity)));
                }
            }
            
            // ブレンドモードを適用
            const ArtifactCore::BlendMode blendMode = blendModeFromIndex(control.blendCombo->currentIndex());
            
            SoftwareRender::CompositeRequest request;
            request.background = result;
            request.foreground = layerImage;
            request.outputSize = canvasSize;
            request.blendMode = blendMode;
            request.backend = SoftwareRender::CompositeBackend::QtPainter;
            request.useForeground = true;
            
            result = SoftwareRender::compose(request);
        }
        
        compositeImage = std::move(result);
        
        if (previewLabel) {
            previewLabel->setPixmap(QPixmap::fromImage(compositeImage));
        }
        
        if (infoLabel) {
            const QString info = QString("Composition: %1 | Layers: %2 | Visible: %3")
                .arg(currentComposition->settings().compositionName().toQString())
                .arg(currentComposition->allLayer().size())
                .arg(visibleLayerCount);
            infoLabel->setText(info);
        }
    }
    
    void applyChangesToComposition()
    {
        if (!currentComposition || !service) return;
        
        for (auto& control : layerControls) {
            auto layer = control.layerPtr;
            if (!layer) continue;
            
            // Visible 状態を適用
            const bool newVisible = control.visibleCheck->isChecked();
            if (layer->isVisible() != newVisible) {
                layer->setVisible(newVisible);
            }
            
            // Blend Mode を適用
            const ArtifactCore::BlendMode newBlend = blendModeFromIndex(control.blendCombo->currentIndex());
            const auto currentBlend = ArtifactCore::toBlendMode(layer->layerBlendType());
            if (currentBlend != newBlend) {
                layer->setBlendMode(static_cast<LAYER_BLEND_TYPE>(newBlend));
            }
            
            // Opacity を適用（SolidImageLayer の場合）
            if (control.opacitySpin) {
                if (auto solidLayer = std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
                    auto color = solidLayer->color();
                    color.setAlpha(static_cast<float>(control.opacitySpin->value()));
                    solidLayer->setColor(color);
                }
            }
        }
        
        // プロジェクトに変更を通知
        service->projectChanged();
    }
};

ArtifactTimelineLayerTestWidget::ArtifactTimelineLayerTestWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl)
{
    setWindowTitle("Timeline Layer Composite Test");
    resize(1600, 1000);
    
    auto* mainLayout = new QHBoxLayout(this);
    
    // 左側：コントロールパネル
    auto* controlPanel = new QWidget();
    auto* controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setSpacing(12);
    
    // コンポジション選択
    auto* compGroup = new QGroupBox("Composition");
    auto* compLayout = new QVBoxLayout(compGroup);
    
    impl_->compositionCombo = new QComboBox();
    compLayout->addWidget(impl_->compositionCombo);
    
    impl_->followCurrentComposition = new QCheckBox("Follow Current Composition");
    compLayout->addWidget(impl_->followCurrentComposition);
    
    impl_->refreshButton = new QPushButton("Reload Layers (F5)");
    compLayout->addWidget(impl_->refreshButton);
    
    impl_->applyButton = new QPushButton("Apply Changes to Timeline");
    impl_->applyButton->setStyleSheet("background-color: #4CAF50; color: white; padding: 8px;");
    compLayout->addWidget(impl_->applyButton);
    
    controlLayout->addWidget(compGroup);
    
    // レイヤーコントロール（スクロール可能）
    auto* layersGroup = new QGroupBox("Layer Controls");
    auto* layersLayout = new QVBoxLayout(layersGroup);
    
    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    impl_->layerControlsContainer = new QWidget();
    impl_->layerControlsLayout = new QVBoxLayout(impl_->layerControlsContainer);
    
    scrollArea->setWidget(impl_->layerControlsContainer);
    layersLayout->addWidget(scrollArea);
    
    controlLayout->addWidget(layersGroup);
    
    // 右側：プレビュー
    auto* previewGroup = new QGroupBox("Composite Preview");
    auto* previewLayout = new QVBoxLayout(previewGroup);
    
    impl_->infoLabel = new QLabel();
    impl_->infoLabel->setStyleSheet("color: #aaa; font-size: 12px;");
    previewLayout->addWidget(impl_->infoLabel);
    
    impl_->previewHost = new QWidget();
    impl_->previewHost->setMinimumSize(800, 600);
    impl_->previewHost->setStyleSheet("background-color: #1a1a1a;");
    
    auto* previewLayout2 = new QVBoxLayout(impl_->previewHost);
    previewLayout2->setContentsMargins(0, 0, 0, 0);
    
    impl_->previewLabel = new QLabel();
    impl_->previewLabel->setAlignment(Qt::AlignCenter);
    impl_->previewLabel->setMinimumSize(800, 600);
    previewLayout2->addWidget(impl_->previewLabel);
    
    previewLayout->addWidget(impl_->previewHost);
    
    // レイアウト設定
    mainLayout->addWidget(controlPanel, 0);
    mainLayout->addWidget(previewGroup, 1);
    
    // シグナル接続
    QObject::connect(impl_->compositionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this]() {
            const QString idStr = impl_->compositionCombo->currentData().toString();
            if (idStr.isEmpty() || !impl_->service) {
                impl_->currentComposition.reset();
            } else {
                const auto result = impl_->service->findComposition(ArtifactCore::CompositionID(idStr));
                impl_->currentComposition = result.success ? result.ptr.lock() : nullptr;
            }
            impl_->reloadLayerControls(this);
            impl_->updateComposite();
        });
    
    QObject::connect(impl_->refreshButton, &QPushButton::clicked, this, [this]() {
        impl_->reloadCompositions();
        const QString idStr = impl_->compositionCombo->currentData().toString();
        if (!idStr.isEmpty() && impl_->service) {
            const auto result = impl_->service->findComposition(ArtifactCore::CompositionID(idStr));
            impl_->currentComposition = result.success ? result.ptr.lock() : nullptr;
        }
        impl_->reloadLayerControls(this);
        impl_->updateComposite();
    });
    
    QObject::connect(impl_->applyButton, &QPushButton::clicked, this, [this]() {
        impl_->applyChangesToComposition();
        impl_->updateComposite();
    });
    
    // サービスの取得
    impl_->service = ArtifactProjectService::instance();
    
    if (impl_->service) {
        // 現在のコンポジションを監視
        QObject::connect(impl_->service, &ArtifactProjectService::layerCreated, this, [this]() {
            if (impl_->followCurrentComposition->isChecked()) {
                impl_->reloadCompositions();
                impl_->reloadLayerControls(this);
                impl_->updateComposite();
            }
        });
        
        QObject::connect(impl_->service, &ArtifactProjectService::layerRemoved, this, [this]() {
            if (impl_->followCurrentComposition->isChecked()) {
                impl_->reloadLayerControls(this);
                impl_->updateComposite();
            }
        });
        
        impl_->reloadCompositions();
        
        // 初期コンポジションを選択
        const QString idStr = impl_->compositionCombo->currentData().toString();
        if (!idStr.isEmpty()) {
            const auto result = impl_->service->findComposition(ArtifactCore::CompositionID(idStr));
            impl_->currentComposition = result.success ? result.ptr.lock() : nullptr;
        }
        impl_->reloadLayerControls(this);
    }
    
    impl_->updateComposite();
}

ArtifactTimelineLayerTestWidget::~ArtifactTimelineLayerTestWidget()
{
    delete impl_;
}

void ArtifactTimelineLayerTestWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F5) {
        impl_->reloadCompositions();
        impl_->reloadLayerControls(this);
        impl_->updateComposite();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace Artifact
