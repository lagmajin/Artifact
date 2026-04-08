module;
#include <utility>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QImage>
#include <QPainter>
#include <QKeyEvent>
#include <QApplication>
#include <QStatusBar>
#include <QStyle>
#include <QDebug>
#include <QIcon>
#include <QFont>
#include <QGraphicsOpacityEffect>
#include <QPalette>
#include <QColor>
#include <wobjectimpl.h>

module Artifact.Widgets.SecondaryPreviewWindow;

import Widgets.Utils.CSS;

namespace Artifact {

	W_OBJECT_IMPL(ArtifactSecondaryPreviewWindow)

class ArtifactSecondaryPreviewWindow::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    QImage currentImage_;
    QString compName_;
    int64_t currentFrame_ = 0;
    int64_t totalFrames_ = 100;
    bool fullscreen_ = false;
    bool autoUpdate_ = true;
    int updateRate_ = 30;
    int currentScreenIndex_ = 0;

    // OSD
    QString osdText_;
    QTimer* osdTimer_ = nullptr;
    QGraphicsOpacityEffect* osdOpacity_ = nullptr;
    bool osdVisible_ = false;

    // Update timer for auto-refresh
    QTimer* updateTimer_ = nullptr;

    // UI elements
    QLabel* previewLabel_ = nullptr;
    QLabel* osdLabel_ = nullptr;
    QWidget* osdContainer_ = nullptr;
    QStatusBar* statusBar_ = nullptr;

    void showOSD(const QString& text) {
        if (!osdLabel_) return;
        osdText_ = text;
        osdLabel_->setText(text);
        osdLabel_->show();
        osdVisible_ = true;
        if (osdOpacity_) osdOpacity_->setOpacity(1.0f);
        if (osdTimer_) {
            osdTimer_->stop();
            osdTimer_->start(3000);
        }
    }

    void hideOSD() {
        if (osdLabel_) osdLabel_->hide();
        osdVisible_ = false;
    }

    void updateStatusBar() {
        if (!statusBar_) return;
        statusBar_->showMessage(
            QString("%1 | Frame %2 / %3")
                .arg(compName_)
                .arg(currentFrame_)
                .arg(totalFrames_));
    }

    QSize scaledImageSize(const QSize& imageSize, const QSize& availableSize) const {
        if (imageSize.isEmpty() || availableSize.isEmpty()) return imageSize;
        return QSize(imageSize.width(), imageSize.height())
            .scaled(availableSize.width(), availableSize.height(), Qt::KeepAspectRatio);
    }
};

ArtifactSecondaryPreviewWindow::ArtifactSecondaryPreviewWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
    , impl_(new Impl())
{
    setWindowTitle("Secondary Preview");
    setMinimumSize(320, 240);
    resize(800, 600);

    const QColor background = QColor(ArtifactCore::currentDCCTheme().backgroundColor);
    const QColor surface = QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor);
    const QColor text = QColor(ArtifactCore::currentDCCTheme().textColor);

    // Main layout
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setAutoFillBackground(true);
    QPalette widgetPalette = palette();
    widgetPalette.setColor(QPalette::Window, background);
    widgetPalette.setColor(QPalette::WindowText, text);
    setPalette(widgetPalette);

    // Preview label (centered)
    impl_->previewLabel_ = new QLabel(this);
    impl_->previewLabel_->setAlignment(Qt::AlignCenter);
    {
        QPalette pal = impl_->previewLabel_->palette();
        pal.setColor(QPalette::Window, surface);
        pal.setColor(QPalette::WindowText, text);
        impl_->previewLabel_->setAutoFillBackground(true);
        impl_->previewLabel_->setPalette(pal);
    }
    mainLayout->addWidget(impl_->previewLabel_);

    // OSD container (overlay)
    impl_->osdContainer_ = new QWidget(this);
    impl_->osdContainer_->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* osdLayout = new QVBoxLayout(impl_->osdContainer_);
    osdLayout->setContentsMargins(16, 16, 16, 16);
    osdLayout->addStretch();

    impl_->osdLabel_ = new QLabel(this);
    {
        QFont font(QStringLiteral("Consolas"));
        font.setStyleHint(QFont::Monospace);
        font.setPointSize(14);
        impl_->osdLabel_->setFont(font);
        QPalette pal = impl_->osdLabel_->palette();
        pal.setColor(QPalette::Window, QColor(0, 0, 0, 180));
        pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#FFFFFF")));
        impl_->osdLabel_->setAutoFillBackground(true);
        impl_->osdLabel_->setPalette(pal);
    }
    impl_->osdLabel_->hide();
    osdLayout->addWidget(impl_->osdLabel_, 0, Qt::AlignRight);

    impl_->osdOpacity_ = new QGraphicsOpacityEffect(impl_->osdLabel_);
    impl_->osdLabel_->setGraphicsEffect(impl_->osdOpacity_);

    // Status bar
    impl_->statusBar_ = new QStatusBar(this);
    impl_->statusBar_->setSizeGripEnabled(false);
    {
        QPalette pal = impl_->statusBar_->palette();
        pal.setColor(QPalette::Window, surface);
        pal.setColor(QPalette::WindowText, text);
        impl_->statusBar_->setPalette(pal);
        impl_->statusBar_->setAutoFillBackground(true);
    }
    mainLayout->addWidget(impl_->statusBar_);

    // OSD fade-out timer
    impl_->osdTimer_ = new QTimer(this);
    impl_->osdTimer_->setSingleShot(true);
    connect(impl_->osdTimer_, &QTimer::timeout, this, [this]() {
        if (impl_->osdOpacity_) {
            impl_->osdOpacity_->setOpacity(0.0f);
        }
        impl_->osdVisible_ = false;
    });

    // Auto-update timer
    impl_->updateTimer_ = new QTimer(this);
    impl_->updateTimer_->setInterval(1000 / impl_->updateRate_);
    // Update timer is controlled externally via setAutoUpdate

    // Show initial OSD
    impl_->showOSD("Secondary Preview Ready");
    impl_->updateStatusBar();

    // Set window icon from application
    setWindowIcon(QApplication::windowIcon());
}

ArtifactSecondaryPreviewWindow::~ArtifactSecondaryPreviewWindow() {
    delete impl_;
}

void ArtifactSecondaryPreviewWindow::updatePreviewImage(const QImage& image) {
    if (image.isNull()) return;
    impl_->currentImage_ = image;

    // Scale image to fit available space
    QSize availableSize = impl_->previewLabel_->size();
    QSize scaledSize = impl_->scaledImageSize(image.size(), availableSize);

    if (!scaledSize.isEmpty()) {
        QPixmap pixmap = QPixmap::fromImage(image.scaled(
            scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        impl_->previewLabel_->setPixmap(pixmap);
    }
}

void ArtifactSecondaryPreviewWindow::updateFrameInfo(int64_t frame, int64_t totalFrames, const QString& compName) {
    impl_->currentFrame_ = frame;
    impl_->totalFrames_ = totalFrames;
    impl_->compName_ = compName;
    impl_->updateStatusBar();
}

void ArtifactSecondaryPreviewWindow::showOnScreen(int screenIndex) {
    auto screens = QGuiApplication::screens();
    if (screenIndex < 0 || screenIndex >= screens.size()) return;

    QScreen* screen = screens[screenIndex];
    impl_->currentScreenIndex_ = screenIndex;

    QRect geo = screen->availableGeometry();
    setGeometry(geo);
    show();
    raise();
    activateWindow();

    impl_->showOSD(QString("Preview on %1 (%2x%3)")
        .arg(screen->name())
        .arg(geo.width())
        .arg(geo.height()));
}

void ArtifactSecondaryPreviewWindow::setFullscreen(bool fullscreen) {
    if (impl_->fullscreen_ == fullscreen) return;
    impl_->fullscreen_ = fullscreen;

    if (fullscreen) {
        auto screens = QGuiApplication::screens();
        if (impl_->currentScreenIndex_ < screens.size()) {
            setGeometry(screens[impl_->currentScreenIndex_]->geometry());
        }
        showFullScreen();
    } else {
        showNormal();
        auto screens = QGuiApplication::screens();
        if (impl_->currentScreenIndex_ < screens.size()) {
            setGeometry(screens[impl_->currentScreenIndex_]->availableGeometry());
        }
    }

    emit fullscreenToggled(fullscreen);
    impl_->showOSD(fullscreen ? "Fullscreen Mode (ESC to exit)" : "Windowed Mode");
}

bool ArtifactSecondaryPreviewWindow::isFullscreen() const {
    return impl_->fullscreen_;
}

void ArtifactSecondaryPreviewWindow::setAutoUpdate(bool enabled) {
    impl_->autoUpdate_ = enabled;
    if (enabled) {
        impl_->updateTimer_->start();
    } else {
        impl_->updateTimer_->stop();
    }
}

bool ArtifactSecondaryPreviewWindow::autoUpdate() const {
    return impl_->autoUpdate_;
}

void ArtifactSecondaryPreviewWindow::setUpdateRate(int fps) {
    impl_->updateRate_ = std::max(1, std::min(120, fps));
    impl_->updateTimer_->setInterval(1000 / impl_->updateRate_);
}

int ArtifactSecondaryPreviewWindow::updateRate() const {
    return impl_->updateRate_;
}

void ArtifactSecondaryPreviewWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        if (impl_->fullscreen_) {
            setFullscreen(false);
        } else {
            close();
        }
        return;
    }
    if (event->key() == Qt::Key_F11) {
        setFullscreen(!impl_->fullscreen_);
        return;
    }
    QWidget::keyPressEvent(event);
}

void ArtifactSecondaryPreviewWindow::closeEvent(QCloseEvent* event) {
    emit closed();
    QWidget::closeEvent(event);
}

void ArtifactSecondaryPreviewWindow::paintEvent(QPaintEvent* event) {
    // Draw checkerboard background if no image
    if (impl_->currentImage_.isNull()) {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0x1a, 0x1a, 0x1a));
    }
    QWidget::paintEvent(event);
}

} // namespace Artifact
