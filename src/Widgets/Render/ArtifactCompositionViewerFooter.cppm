module;
#include <QLabel>
#include <QIcon>
#include <QWidget>
#include <QBoxLayout>
#include <QComboBox>
#include <QToolButton>
#include <QSize>
#include <QMetaObject>
#include <wobjectimpl.h>

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




import Thread.PreciseTicker;
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

QString ramPreviewStateNote(Artifact::ArtifactPlaybackService* playback)
{
  if (!playback) {
    return QString();
  }
  const auto currentFrame = playback->currentFrame().framePosition();
  const auto state = playback->ramPreviewFrameState(currentFrame);
  const QString note = Artifact::ramPreviewStatusNote(state);
  if (note == QStringLiteral("-")) {
    return QString();
  }
  return QStringLiteral(" | note %1").arg(note);
}

QString ramPreviewFooterText(Artifact::ArtifactPlaybackService* playback,
                             const float hitRateFallback,
                             const int cachedFrameCountFallback)
{
  if (!playback) {
    return QStringLiteral("RAM: N/A");
  }

  const auto summary = playback->ramPreviewSummary();
  const auto currentFrame = playback->currentFrame().framePosition();
  const float hitRate = summary.hitRate > 0.0f ? summary.hitRate : hitRateFallback;
  const int inRamFrames =
      summary.inRamFrames > 0 ? summary.inRamFrames : cachedFrameCountFallback;
  const QString prioritySuffix = summary.currentPriorityReason.trimmed().isEmpty()
                                     ? QString()
                                     : QStringLiteral(" | prio %1").arg(summary.currentPriorityReason);
  return QStringLiteral("RAM: playable %1/%2 | requested %3 | pending %4 | next %5 | range %6 | progress %7 | failed %8 | inRam %9 | onDisk %10 | readyMissingImage %11 | current %12%13 | hit%14")
      .arg(summary.playableFrames)
      .arg(summary.rangeFrames)
      .arg(summary.requestedFrames)
      .arg(summary.buildQueuePendingFrames)
      .arg(summary.buildQueueNextFrame)
      .arg(summary.buildRangeReady ? QStringLiteral("ready")
                                   : QStringLiteral("building"))
      .arg(QString::number(summary.buildRangeProgress * 100.0f, 'f', 0) + QStringLiteral("%"))
      .arg(summary.failedFrames)
      .arg(inRamFrames)
      .arg(summary.onDiskFrames)
      .arg(summary.readyMissingImageFrames)
      .arg(currentFrame)
      .arg(ramPreviewStateNote(playback) + prioritySuffix)
      .arg(QString::number(hitRate * 100.0f, 'f', 0) + QStringLiteral("%"));
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
  QLabel* ramPreviewLabel = nullptr;
  QLabel* selectionLabel = nullptr;
  QLabel* zoomLabel = nullptr;
  QLabel* mouseLabel = nullptr;
  QLabel* resolutionInfoLabel = nullptr;
  QComboBox* resolutionCombo = nullptr;
  QComboBox* qualityCombo = nullptr;
  double fps_ = 0.0;
  uint64_t memMB_ = 0;
  float ramPreviewHitRate_ = 0.0f;
  int ramPreviewCachedFrameCount_ = 0;
  int ramPreviewRequestedFrameCount_ = 0;
  int ramPreviewPlayableFrameCount_ = 0;
  bool isPlaying_ = false;
  std::unique_ptr<ArtifactCore::PreciseTicker> refreshTimer = nullptr;
 };

 ArtifactCompositionViewerFooter::Impl::Impl()
 {
  pSnapShotButton = new QToolButton();
  pSnapShotButton->setIcon(loadIconWithFallback("MaterialVS/neutral/camera_alt.svg"));
  pSnapShotButton->setIconSize(QSize(20, 20));
  pSnapShotButton->setAutoRaise(true);
  pSnapShotButton->setToolTip("Take Snapshot");
  pSnapShotButton->setAccessibleName(QStringLiteral("Take snapshot"));
  pSnapShotButton->setAccessibleDescription(QStringLiteral("Capture a snapshot of the composition viewer"));
  pShutterButton = new QToolButton();
  pShutterButton->setIcon(loadIconWithFallback("MaterialVS/neutral/videocam.svg"));
  pShutterButton->setIconSize(QSize(20, 20));
  pShutterButton->setAutoRaise(true);
  pShutterButton->setToolTip("Render Current Frame");
  pShutterButton->setAccessibleName(QStringLiteral("Render current frame"));
  pShutterButton->setAccessibleDescription(QStringLiteral("Render the current composition frame"));
  pPlayPauseButton = new QToolButton();
  pPlayPauseButton->setIcon(loadIconWithFallback("MaterialVS/green/play_arrow.svg"));
  pPlayPauseButton->setIconSize(QSize(20, 20));
  pPlayPauseButton->setAutoRaise(true);
  pPlayPauseButton->setToolTip("Play/Pause");
  pPlayPauseButton->setAccessibleName(QStringLiteral("Play or pause"));
  pPlayPauseButton->setAccessibleDescription(QStringLiteral("Play or pause the composition preview"));
  pStopButton = new QToolButton();
  pStopButton->setIcon(loadIconWithFallback("MaterialVS/green/stop.svg"));
  pStopButton->setIconSize(QSize(20, 20));
  pStopButton->setAutoRaise(true);
  pStopButton->setToolTip("Stop");
  pStopButton->setAccessibleName(QStringLiteral("Stop playback"));
  pStopButton->setAccessibleDescription(QStringLiteral("Stop the composition preview"));
  fpsLabel = new QLabel("FPS: N/A");
  memLabel = new QLabel("Mem: N/A");
  ramPreviewLabel = new QLabel("RAM: N/A");
  selectionLabel = new QLabel("");
  zoomLabel = new QLabel("Zoom: 100%");
  mouseLabel = new QLabel("XY: -,-");
  resolutionInfoLabel = new QLabel("Res: -");
  refreshTimer = std::make_unique<ArtifactCore::PreciseTicker>();
 }

 ArtifactCompositionViewerFooter::Impl::~Impl()
 {
  if (refreshTimer) {
   refreshTimer->stop();
  }
  delete pSnapShotButton;
  delete pShutterButton;
  delete pPlayPauseButton;
  delete pStopButton;
  delete fpsLabel;
  delete memLabel;
  delete ramPreviewLabel;
  delete selectionLabel;
  delete zoomLabel;
  delete mouseLabel;
  delete resolutionInfoLabel;
 }

ArtifactCompositionViewerFooter::ArtifactCompositionViewerFooter(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setAccessibleName(QStringLiteral("Composition viewer controls"));
  setAccessibleDescription(QStringLiteral("Choose preview resolution and control composition playback"));
  setMaximumHeight(24);
  auto layout = new QHBoxLayout(this);
  layout->setContentsMargins(6, 0, 6, 0);
  layout->setSpacing(8);

  auto resLabel = new QLabel("Resolution:", this);
  auto font = resLabel->font();
  font.setPointSize(9);
  resLabel->setFont(font);
  layout->addWidget(resLabel);

  impl_->resolutionCombo = new QComboBox(this);
  impl_->resolutionCombo->addItems({ "1920x1080", "1280x720", "800x600" });
  impl_->resolutionCombo->setAccessibleName(QStringLiteral("Preview resolution"));
  impl_->resolutionCombo->setAccessibleDescription(QStringLiteral("Choose the composition preview resolution"));
  layout->addWidget(impl_->resolutionCombo);

  auto *qualityLabel = new QLabel(QStringLiteral("Quality:"), this);
  impl_->qualityCombo = new QComboBox(this);
  impl_->qualityCombo->addItem(QStringLiteral("Draft (1/4)"),
                               static_cast<int>(PreviewQualityPreset::Draft));
  impl_->qualityCombo->addItem(QStringLiteral("Preview (1/2)"),
                               static_cast<int>(PreviewQualityPreset::Preview));
  impl_->qualityCombo->addItem(QStringLiteral("Full"),
                               static_cast<int>(PreviewQualityPreset::Final));
  impl_->qualityCombo->setAccessibleName(QStringLiteral("Preview quality"));
  impl_->qualityCombo->setAccessibleDescription(
      QStringLiteral("Choose the preview render quality"));
  if (auto *service = ArtifactProjectService::instance()) {
    impl_->qualityCombo->setCurrentIndex(
        impl_->qualityCombo->findData(static_cast<int>(service->previewQualityPreset())));
  }
  layout->addWidget(qualityLabel);
  layout->addWidget(impl_->qualityCombo);

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
  impl_->zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  impl_->mouseLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  impl_->resolutionInfoLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  layout->addWidget(impl_->zoomLabel);
  layout->addWidget(impl_->mouseLabel);
  layout->addWidget(impl_->resolutionInfoLabel);
  impl_->selectionLabel->setFixedWidth(200);
  impl_->selectionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  impl_->fpsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  impl_->memLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  impl_->ramPreviewLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  layout->addWidget(impl_->selectionLabel);
  layout->addWidget(impl_->fpsLabel);
  layout->addWidget(impl_->memLabel);
  layout->addWidget(impl_->ramPreviewLabel);

  setLayout(layout);

  // Connections
  connect(impl_->pSnapShotButton, &QToolButton::clicked, this, &ArtifactCompositionViewerFooter::takeSnapShotRequested);
  connect(impl_->qualityCombo, &QComboBox::currentIndexChanged, this,
          [this](int index) {
            if (index < 0 || !impl_->qualityCombo) return;
            if (auto *service = ArtifactProjectService::instance()) {
              service->setPreviewQualityPreset(
                  static_cast<PreviewQualityPreset>(impl_->qualityCombo->itemData(index).toInt()));
            }
          });
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
  impl_->refreshTimer->setInterval(std::chrono::milliseconds(1000));
  impl_->refreshTimer->setCallback([this]() {
    QMetaObject::invokeMethod(this, [this]() {
      auto* playback = ArtifactPlaybackService::instance();
      if (playback) {
        const auto summary = playback->ramPreviewSummary();
        impl_->ramPreviewRequestedFrameCount_ = summary.requestedFrames;
        impl_->ramPreviewPlayableFrameCount_ = summary.playableFrames;
        impl_->ramPreviewCachedFrameCount_ = summary.inRamFrames;
        impl_->ramPreviewHitRate_ = summary.hitRate;
      }
      impl_->fpsLabel->setText(QString("FPS: %1").arg(impl_->fps_ > 0.0 ? QString::number(impl_->fps_, 'f', 1) : QString("N/A")));
      impl_->memLabel->setText(QString("Mem: %1 MB").arg(impl_->memMB_ ? QString::number(impl_->memMB_) : QString("N/A")));
      impl_->ramPreviewLabel->setText(
          ramPreviewFooterText(playback, impl_->ramPreviewHitRate_,
                               impl_->ramPreviewCachedFrameCount_));
    }, Qt::QueuedConnection);
  });
  impl_->refreshTimer->start();
 }

 ArtifactCompositionViewerFooter::~ArtifactCompositionViewerFooter()
 {
  delete impl_;
 }

void ArtifactCompositionViewerFooter::setZoomLevel(float zoomPercent)
{
  if (!impl_ || !impl_->zoomLabel) return;
  const float safeZoom = std::isfinite(zoomPercent)
      ? std::max(0.0f, zoomPercent) : 100.0f;
  impl_->zoomLabel->setText(
      QStringLiteral("Zoom: %1%").arg(QString::number(safeZoom, 'f', 1)));
}

void ArtifactCompositionViewerFooter::setMouseCoordinates(int x, int y)
{
  if (!impl_ || !impl_->mouseLabel) return;
  impl_->mouseLabel->setText(QStringLiteral("XY: %1,%2").arg(x).arg(y));
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

void ArtifactCompositionViewerFooter::setRamPreviewStats(float hitRate, int cachedFrameCount)
{
  if (!impl_) return;
  impl_->ramPreviewHitRate_ = std::clamp(hitRate, 0.0f, 1.0f);
  impl_->ramPreviewCachedFrameCount_ = std::max(0, cachedFrameCount);
  auto* playback = ArtifactPlaybackService::instance();
  if (playback) {
    const auto summary = playback->ramPreviewSummary();
    impl_->ramPreviewRequestedFrameCount_ = summary.requestedFrames;
    impl_->ramPreviewPlayableFrameCount_ = summary.playableFrames;
    impl_->ramPreviewCachedFrameCount_ = summary.inRamFrames;
    impl_->ramPreviewHitRate_ = summary.hitRate;
  }
  impl_->ramPreviewLabel->setText(
      ramPreviewFooterText(playback, impl_->ramPreviewHitRate_,
                           impl_->ramPreviewCachedFrameCount_));
}

void ArtifactCompositionViewerFooter::setSelectedLayerInfo(const QString& layerInfo)
{
  if (!impl_) return;
  impl_->selectionLabel->setText(layerInfo);
}

void ArtifactCompositionViewerFooter::setResolutionInfo(uint32_t width, uint32_t height)
{
  if (!impl_) return;
  if (width == 0 || height == 0) {
    impl_->resolutionInfoLabel->setText(QStringLiteral("Res: -"));
    return;
  }
  const QString resolution = QStringLiteral("%1x%2").arg(width).arg(height);
  if (impl_->resolutionCombo) {
    const int index = impl_->resolutionCombo->findText(resolution);
    if (index >= 0) {
      impl_->resolutionCombo->setCurrentIndex(index);
    }
  }
  impl_->resolutionInfoLabel->setText(
      QStringLiteral("Res: %1×%2").arg(width).arg(height));
}

};
