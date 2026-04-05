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
#include <QSpinBox>
#include <QFrame>
#include <QPainter>
#include <QPalette>
#include <QColor>
#include <QLinearGradient>
#include <QPixmap>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QGroupBox>
#include <QGridLayout>
#include <QTimer>
#include <wobjectimpl.h>

#include <vector>
#include <memory>

module Artifact.Widgets.LayerCompositeTest;

import Artifact.Render.SoftwareCompositor;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Layer.Blend;
import Widgets.Utils.CSS;

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

QImage createTestPattern(const QSize& size, const QColor& color)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // グラデーション背景
    QLinearGradient grad(0, 0, size.width(), size.height());
    grad.setColorAt(0, color.lighter(120));
    grad.setColorAt(1, color.darker(120));
    painter.fillRect(image.rect(), grad);
    
    // パターン描画
    painter.setPen(QPen(Qt::white, 2));
    const int gridSize = 40;
    for (int y = 0; y < size.height(); y += gridSize) {
        painter.drawLine(0, y, size.width(), y);
    }
    for (int x = 0; x < size.width(); x += gridSize) {
        painter.drawLine(x, 0, x, size.height());
    }
    
    // 枠線
    painter.drawRect(image.rect().adjusted(5, 5, -5, -5));
    
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

} // namespace

class ArtifactLayerCompositeTestWidget::Impl {
public:
    QWidget* previewHost = nullptr;
    QLabel* previewLabel = nullptr;
    
    // レイヤー設定コントロール
    struct LayerControls {
        QCheckBox* visibleCheck = nullptr;
        QDoubleSpinBox* opacitySpin = nullptr;
        QComboBox* blendCombo = nullptr;
        QCheckBox* enableCheck = nullptr;
        int layerIndex = 0;
    };
    
    std::vector<LayerControls> layerControls;
    
    // 共通設定
    QPushButton* refreshButton = nullptr;
    QPushButton* animateButton = nullptr;
    QCheckBox* autoRefreshCheck = nullptr;
    
    // テストパターン設定
    std::vector<QColor> layerColors = {
        QColor(255, 100, 100),  // Layer 1: Red
        QColor(100, 255, 100),  // Layer 2: Green
        QColor(100, 100, 255),  // Layer 3: Blue
        QColor(255, 255, 100),  // Layer 4: Yellow
    };
    
    std::vector<double> layerOpacities = {1.0, 1.0, 1.0, 1.0};
    std::vector<bool> layerVisible = {true, true, true, true};
    std::vector<ArtifactCore::BlendMode> layerBlendModes = {
        ArtifactCore::BlendMode::Normal,
        ArtifactCore::BlendMode::Normal,
        ArtifactCore::BlendMode::Normal,
        ArtifactCore::BlendMode::Normal,
    };
    
    QTimer* animationTimer = nullptr;
    float animationTime = 0.0f;
    bool isAnimating = false;
    
    QImage compositeImage;
    
    void updateComposite()
    {
        const QSize canvasSize(800, 600);
        
        // 背景を作成
        QImage result = makeCheckerboard(canvasSize);
        
        // 各レイヤーを合成
        for (size_t i = 0; i < layerControls.size(); ++i) {
            if (!layerVisible[i]) {
                continue;
            }
            
            // テストパターン画像を作成
            QImage layerImage = createTestPattern(canvasSize, layerColors[i]);
            
            // 不透明度を適用
            if (layerOpacities[i] < 1.0) {
                QPainter alphaPainter(&layerImage);
                alphaPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                alphaPainter.fillRect(layerImage.rect(), QColor(0, 0, 0, static_cast<int>(255 * layerOpacities[i])));
            }
            
            // ブレンドモードを適用
            SoftwareRender::CompositeRequest request;
            request.background = result;
            request.foreground = layerImage;
            request.outputSize = canvasSize;
            request.blendMode = layerBlendModes[i];
            request.overlayOpacity = layerOpacities[i];
            request.backend = SoftwareRender::CompositeBackend::OpenCV;
            request.useForeground = true;
            
            result = SoftwareRender::compose(request);
        }
        
        compositeImage = std::move(result);
        
        if (previewLabel) {
            previewLabel->setPixmap(QPixmap::fromImage(compositeImage));
        }
    }
};

ArtifactLayerCompositeTestWidget::ArtifactLayerCompositeTestWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl)
{
    setWindowTitle("Layer Composite Test");
    resize(1400, 900);
    
    auto* mainLayout = new QHBoxLayout(this);
    
    // 左側：コントロールパネル
    auto* controlPanel = new QWidget();
    auto* controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setSpacing(15);
    
    // 共通設定
    auto* commonGroup = new QGroupBox("Common Settings");
    auto* commonLayout = new QFormLayout(commonGroup);
    
    impl_->autoRefreshCheck = new QCheckBox("Auto Refresh");
    impl_->autoRefreshCheck->setChecked(true);
    commonLayout->addRow(impl_->autoRefreshCheck);
    
    impl_->refreshButton = new QPushButton("Refresh (F5)");
    impl_->animateButton = new QPushButton("Start Animation");
    commonLayout->addRow(impl_->refreshButton);
    commonLayout->addRow(impl_->animateButton);
    
    controlLayout->addWidget(commonGroup);
    
    // 各レイヤーのコントロール
    for (int i = 0; i < 4; ++i) {
        auto* layerGroup = new QGroupBox(QString("Layer %1").arg(i + 1));
        auto* layerLayout = new QFormLayout(layerGroup);
        
        Impl::LayerControls controls;
        
        // Visible
        controls.visibleCheck = new QCheckBox("Visible");
        controls.visibleCheck->setChecked(true);
        layerLayout->addRow(controls.visibleCheck);
        
        // Opacity
        controls.opacitySpin = new QDoubleSpinBox();
        controls.opacitySpin->setRange(0.0, 1.0);
        controls.opacitySpin->setSingleStep(0.05);
        controls.opacitySpin->setValue(1.0);
        controls.opacitySpin->setSuffix(" (0-1)");
        layerLayout->addRow("Opacity:", controls.opacitySpin);
        
        // Blend Mode
        controls.blendCombo = new QComboBox();
        for (int bi = 0; bi < 13; ++bi) {
            controls.blendCombo->addItem(blendModeName(blendModeFromIndex(bi)));
        }
        layerLayout->addRow("Blend Mode:", controls.blendCombo);
        
        layerLayout->addRow(new QLabel("")); // Spacer
        
        controlLayout->addWidget(layerGroup);
        
        // シグナル接続
        QObject::connect(controls.visibleCheck, &QCheckBox::stateChanged, this, [this, i]() {
            impl_->layerVisible[i] = impl_->layerControls[i].visibleCheck->isChecked();
            if (impl_->autoRefreshCheck->isChecked()) {
                impl_->updateComposite();
            }
        });
        
        QObject::connect(controls.opacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, [this, i]() {
            impl_->layerOpacities[i] = impl_->layerControls[i].opacitySpin->value();
            if (impl_->autoRefreshCheck->isChecked()) {
                impl_->updateComposite();
            }
        });
        
        QObject::connect(controls.blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, i]() {
            impl_->layerBlendModes[i] = blendModeFromIndex(impl_->layerControls[i].blendCombo->currentIndex());
            if (impl_->autoRefreshCheck->isChecked()) {
                impl_->updateComposite();
            }
        });
        
        impl_->layerControls.push_back(controls);
    }
    
    controlLayout->addStretch();
    
    // 右側：プレビュー
    auto* previewGroup = new QGroupBox("Composite Preview");
    auto* previewLayout = new QVBoxLayout(previewGroup);
    
    impl_->previewHost = new QWidget();
    impl_->previewHost->setMinimumSize(800, 600);
    impl_->previewHost->setAutoFillBackground(true);
    {
        QPalette pal = impl_->previewHost->palette();
        pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
        impl_->previewHost->setPalette(pal);
    }
    
    auto* previewLayout2 = new QVBoxLayout(impl_->previewHost);
    previewLayout2->setContentsMargins(0, 0, 0, 0);
    
    impl_->previewLabel = new QLabel();
    impl_->previewLabel->setAlignment(Qt::AlignCenter);
    impl_->previewLabel->setMinimumSize(800, 600);
    previewLayout2->addWidget(impl_->previewLabel);
    
    previewLayout->addWidget(impl_->previewHost);
    
    // レイアウト設定
    mainLayout->addWidget(controlPanel);
    mainLayout->addWidget(previewGroup, 1);
    
    // シグナル接続
    QObject::connect(impl_->refreshButton, &QPushButton::clicked, this, [this]() {
        impl_->updateComposite();
    });
    
    QObject::connect(impl_->animateButton, &QPushButton::clicked, this, [this]() {
        impl_->isAnimating = !impl_->isAnimating;
        if (impl_->isAnimating) {
            impl_->animationTime = 0.0f;
            if (!impl_->animationTimer) {
                impl_->animationTimer = new QTimer(this);
                QObject::connect(impl_->animationTimer, &QTimer::timeout, this, [this]() {
                    impl_->animationTime += 0.05f;
                    
                    // 不透明度をアニメーション
                    for (size_t i = 0; i < impl_->layerControls.size(); ++i) {
                        const double newOpacity = 0.5 + 0.5 * std::sin(impl_->animationTime + i * 0.5);
                        impl_->layerControls[i].opacitySpin->setValue(newOpacity);
                        impl_->layerOpacities[i] = newOpacity;
                    }
                    
                    impl_->updateComposite();
                });
            }
            impl_->animationTimer->start(30);
            impl_->animateButton->setText("Stop Animation");
        } else {
            if (impl_->animationTimer) {
                impl_->animationTimer->stop();
            }
            impl_->animateButton->setText("Start Animation");
        }
    });
    
    // 初期合成
    impl_->updateComposite();
    
    // Playback サービスの接続
    if (auto* playbackService = ArtifactPlaybackService::instance()) {
        QObject::connect(playbackService, &ArtifactPlaybackService::frameChanged, this, [this]() {
            impl_->updateComposite();
        });
    }
}

ArtifactLayerCompositeTestWidget::~ArtifactLayerCompositeTestWidget()
{
    delete impl_;
}

void ArtifactLayerCompositeTestWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F5) {
        impl_->updateComposite();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space) {
        impl_->animateButton->click();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

W_OBJECT_IMPL(ArtifactLayerCompositeTestWidget);

} // namespace Artifact
