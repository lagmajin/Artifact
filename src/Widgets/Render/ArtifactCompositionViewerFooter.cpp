module;
#include <QLabel>
#include <QIcon>
#include <QWidget>
#include <QBoxLayout>
#include <QComboBox>
#include <QToolButton>
#include <QSize>
#include <wobjectimpl.h>
#include <QTimer>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Widgets.CompositionFooter;




import Utils.Path;
import Artifact.Service.Project;
import Artifact.Service.Playback;


namespace {
QIcon loadIconWithFallback(const QString& fileName)
{
  const QString resourcePath = ArtifactCore::resolveIconResourcePath(fileName);
  QIcon icon(resourcePath);
  if (!icon.isNull()) {
    return icon;
  }
  return QIcon(ArtifactCore::resolveIconPath(fileName));
}
}


namespace Artifact {

 W_OBJECT_IMPL(ArtifactCompositionViewerFooter)

 class ArtifactCompositionViewerFooter::Impl
 {
 private:
 public:
  Impl();
  ~Impl();
  QLabel* label = nullptr;
  QToolButton* pSnapShotButton = nullptr;
  QToolButton* pShutterButton = nullptr;
  QToolButton* pPlayPauseButton = nullptr;
  QToolButton* pStopButton = nullptr;
  QLabel* fpsLabel = nullptr;
  QLabel* memLabel = nullptr;
  QLabel* selectionLabel = nullptr;
  double fps_ = 0.0;
  uint64_t memMB_ = 0;
  bool isPlaying_ = false;
  QTimer* refreshTimer = nullptr;
 };

 ArtifactCompositionViewerFooter::Impl::Impl()
 {
  pSnapShotButton = new QToolButton();
  pSnapShotButton->setIcon(loadIconWithFallback("MaterialVS/neutral/camera_alt.svg"));
  pSnapShotButton->setIconSize(QSize(20, 20));
  pSnapShotButton->setAutoRaise(true);
  pSnapShotButton->setToolTip("Take Snapshot");
  pShutterButton = new QToolButton();
  pShutterButton->setIcon(loadIconWithFallback("MaterialVS/neutral/videocam.svg"));
  pShutterButton->setIconSize(QSize(20, 20));
  pShutterButton->setAutoRaise(true);
  pShutterButton->setToolTip("Render Current Frame");
  pPlayPauseButton = new QToolButton();
  pPlayPauseButton->setIcon(loadIconWithFallback("MaterialVS/green/play_arrow.svg"));
  pPlayPauseButton->setIconSize(QSize(20, 20));
  pPlayPauseButton->setAutoRaise(true);
  pPlayPauseButton->setToolTip("Play/Pause");
  pStopButton = new QToolButton();
  pStopButton->setIcon(loadIconWithFallback("MaterialVS/green/stop.svg"));
  pStopButton->setIconSize(QSize(20, 20));
  pStopButton->setAutoRaise(true);
  pStopButton->setToolTip("Stop");
  fpsLabel = new QLabel("FPS: N/A");
  memLabel = new QLabel("Mem: N/A");
  selectionLabel = new QLabel("");
  refreshTimer = new QTimer();
 }

 ArtifactCompositionViewerFooter::Impl::~Impl()
 {
  if (refreshTimer) {
   refreshTimer->stop();
   delete refreshTimer;
  }
  delete pSnapShotButton;
  delete pShutterButton;
  delete pPlayPauseButton;
  delete pStopButton;
  delete fpsLabel;
  delete memLabel;
  delete selectionLabel;
 }

 ArtifactCompositionViewerFooter::ArtifactCompositionViewerFooter(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setMaximumHeight(24);
  auto layout = new QHBoxLayout(this);
  layout->setContentsMargins(6, 0, 6, 0);
  layout->setSpacing(8);

  auto resLabel = new QLabel("Resolution:", this);
  auto font = resLabel->font();
  font.setPointSize(9);
  resLabel->setFont(font);
  layout->addWidget(resLabel);

  auto resCombo = new QComboBox(this);
  resCombo->addItems({ "1920x1080", "1280x720", "800x600" });
  layout->addWidget(resCombo);

  // Playback controls
  impl_->pPlayPauseButton->setToolTip("Play/Pause");
  layout->addWidget(impl_->pPlayPauseButton);
  impl_->pStopButton->setToolTip("Stop");
  layout->addWidget(impl_->pStopButton);

  // Snapshot
  layout->addWidget(impl_->pSnapShotButton);

  // Spacer
  layout->addStretch();

  // Status labels (right aligned)
  impl_->selectionLabel->setFixedWidth(200);
  impl_->selectionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  impl_->fpsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  impl_->memLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  layout->addWidget(impl_->selectionLabel);
  layout->addWidget(impl_->fpsLabel);
  layout->addWidget(impl_->memLabel);

  setLayout(layout);
  setStyleSheet(R"(
    QToolButton {
      background: transparent;
      border: 1px solid transparent;
      border-radius: 4px;
      padding: 2px;
    }
    QToolButton:hover {
      background-color: rgba(255, 255, 255, 0.08);
      border-color: rgba(255, 255, 255, 0.12);
    }
    QToolButton:pressed {
      background-color: rgba(255, 255, 255, 0.16);
    }
  )");

  // Connections
  connect(impl_->pSnapShotButton, &QToolButton::clicked, this, &ArtifactCompositionViewerFooter::takeSnapShotRequested);
  connect(impl_->pPlayPauseButton, &QToolButton::clicked, this, [this]() {
    impl_->isPlaying_ = !impl_->isPlaying_;
    // update icon
    if (impl_->isPlaying_) {
      impl_->pPlayPauseButton->setIcon(loadIconWithFallback("MaterialVS/green/pause.svg"));
    } else {
      impl_->pPlayPauseButton->setIcon(loadIconWithFallback("MaterialVS/green/play_arrow.svg"));
    }
    Q_EMIT playPauseToggled(impl_->isPlaying_);
  });

  connect(impl_->pStopButton, &QToolButton::clicked, this, [this]() {
    if (auto* playback = ArtifactPlaybackService::instance()) {
      playback->stop();
    }
    impl_->isPlaying_ = false;
    if (impl_->pPlayPauseButton) {
      impl_->pPlayPauseButton->setIcon(loadIconWithFallback("MaterialVS/green/play_arrow.svg"));
    }
    Q_EMIT stopRequested();
    Q_EMIT playPauseToggled(false);
  });

  // Periodic refresh to update displayed FPS/Mem if set externally
  connect(impl_->refreshTimer, &QTimer::timeout, this, [this]() {
    impl_->fpsLabel->setText(QString("FPS: %1").arg(impl_->fps_ > 0.0 ? QString::number(impl_->fps_, 'f', 1) : QString("N/A")));
    impl_->memLabel->setText(QString("Mem: %1 MB").arg(impl_->memMB_ ? QString::number(impl_->memMB_) : QString("N/A")));
  });
  impl_->refreshTimer->start(1000);
 }

 ArtifactCompositionViewerFooter::~ArtifactCompositionViewerFooter()
 {
  delete impl_;
 }

void ArtifactCompositionViewerFooter::setZoomLevel(float zoomPercent)
{
  Q_UNUSED(zoomPercent);
  // For now no-op; could display zoom in future
}

void ArtifactCompositionViewerFooter::setMouseCoordinates(int x, int y)
{
  Q_UNUSED(x);
  Q_UNUSED(y);
  // Could display mouse coordinates in selectionLabel if required
}

void ArtifactCompositionViewerFooter::setFPS(double fps)
{
  if (!impl_) return;
  impl_->fps_ = fps;
  impl_->fpsLabel->setText(QString("FPS: %1").arg(fps > 0.0 ? QString::number(fps, 'f', 1) : QString("N/A")));
}

void ArtifactCompositionViewerFooter::setMemoryUsage(uint64_t memoryMB)
{
  if (!impl_) return;
  impl_->memMB_ = memoryMB;
  impl_->memLabel->setText(QString("Mem: %1 MB").arg(memoryMB ? QString::number(memoryMB) : QString("N/A")));
}

void ArtifactCompositionViewerFooter::setSelectedLayerInfo(const QString& layerInfo)
{
  if (!impl_) return;
  impl_->selectionLabel->setText(layerInfo);
}

void ArtifactCompositionViewerFooter::setResolutionInfo(uint32_t width, uint32_t height)
{
  if (!impl_) return;
  Q_UNUSED(width);
  Q_UNUSED(height);
  // resolution currently managed by combo box; could update label if needed
}

};
