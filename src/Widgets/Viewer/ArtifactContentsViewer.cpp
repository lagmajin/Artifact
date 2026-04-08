module;

#include <QWidget>
#include <QFrame>
#include <QComboBox>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QEvent>
#include <QLabel>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QClipboard>
#include <QToolButton>
#include <QSlider>
#include <QVector3D>
#include <wobjectimpl.h>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QPixmap>
#include <QScrollArea>
#include <QStyle>
#include <QSizePolicy>
#include <QSplitter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QScrollBar>
#include <QDebug>
#include <QFileInfo>
#include <QUrl>
#include <QSignalBlocker>
#include <QSettings>
#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QIODevice>
#include <QTimer>
#include <QTransform>
#include <QVector>
#include <QShortcut>
#include <QKeySequence>

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
#include <limits>
module Artifact.Contents.Viewer;




import Artifact.Preview.Pipeline;
import MediaPlaybackController;
import Artifact.Widgets.AudioPreview;
import Widgets.Utils.CSS;
import File.TypeDetector;
import Artifact.Widgets.ModelViewer;
import Utils.String.UniString;

namespace Artifact
{
  class ArtifactContentsViewer::Impl
  {
  public:
   ArtifactContentsViewer* owner_ = nullptr;
   Impl(ArtifactContentsViewer* parent);
   ~Impl();

   void showInfoMessage(const QString& text);
   void updateHeader();
   void updateViewerBadge();
   void updateSurfaceMeta();
   void updateChannelMetaSurface();
   void loadRecentSources();
   void rememberRecentSource(const QString& filepath);
   void refreshRecentSourceCombo();
   void loadViewerAssignmentState();
   void saveViewerAssignmentState() const;
   void loadCompareSurfaceState();
   void saveCompareSurfaceState() const;
   void updatePlaybackState();
   void updateActionAvailability();
   void updateModeButtons();
   void clearPlaybackRange();
   void setPlaybackRange(int64_t startFrame, int64_t endFrame);
   void resetCurrentMode();
   void ensureVideoWidgets();
   void ensureAudioWidgets();
   void ensureAudioController();
   void releaseAudioPlayback();
   void resetAudioWaveform();
   void appendAudioWaveformSamples(const QByteArray& pcmData);
   void activateAudio(const QString& filepath);
   void pumpAudioPlayback();
   void ensureModelViewer();
   void ensureCompareWidgets();
   void updateCompareSurface();
   void swapCompareSides();
   void assignCompareSource(bool leftSide);
   void updateCompareWipe();
   void updateDisplayedPage();
   void syncModelViewerMode();
   void attachMediaOutputs();
   void detachMediaOutputs();
   void installEventFilters();
   void activateImage(const QString& filepath);
   void activateVideo(const QString& filepath);
   void activateModel(const QString& filepath);
   void openCompareSource(bool leftSide);
   void fitImageToWindow();
   void applyImageTransform();

   static qint64 framesToMs(int64_t frame);
   static QString humanFileSize(qint64 bytes);
   static QString formatDurationMs(qint64 ms);

   QWidget* headerWidget = nullptr;
   QLabel* titleLabel = nullptr;
   QLabel* typeBadgeLabel = nullptr;
   QLabel* viewerBadgeLabel = nullptr;
   QComboBox* viewerAssignmentCombo = nullptr;
   QComboBox* recentSourceCombo = nullptr;
   QLabel* metaLabel = nullptr;
   QLabel* stateLabel = nullptr;
   QLabel* channelMetaLabel = nullptr;
   QLabel* surfaceMetaLabel = nullptr;
   QToolButton* fitButton = nullptr;
   QToolButton* rotateLeftButton = nullptr;
   QToolButton* rotateRightButton = nullptr;
   QToolButton* resetButton = nullptr;
   QToolButton* playButton = nullptr;
   QToolButton* pauseButton = nullptr;
   QToolButton* stopButton = nullptr;
   QToolButton* copyPathButton = nullptr;
   QToolButton* revealButton = nullptr;
   QToolButton* sourceButton = nullptr;
   QToolButton* finalButton = nullptr;
   QToolButton* compareButton = nullptr;
   QToolButton* compareSwapButton = nullptr;
   QToolButton* compareAssignAButton = nullptr;
   QToolButton* compareAssignBButton = nullptr;
   QSlider* compareWipeSlider = nullptr;
   QSplitter* compareSplitter = nullptr;
   QSlider* seekSlider = nullptr;
   QStackedWidget* stackedWidget = nullptr;
   QScrollArea* imageScrollArea = nullptr;
   QLabel* imageLabel = nullptr;
   QVideoWidget* videoWidget = nullptr;
   QMediaPlayer* mediaPlayer = nullptr;
   QAudioOutput* audioOutput = nullptr;
   std::unique_ptr<ArtifactCore::MediaPlaybackController> audioController_;
   QAudioSink* audioSink = nullptr;
   QIODevice* audioSinkDevice = nullptr;
   QTimer* audioPumpTimer = nullptr;
   QWidget* audioPage = nullptr;
   AudioWaveformWidget* audioWaveformWidget = nullptr;
   QLabel* audioWaveformLabel = nullptr;
   QVector<float> audioWaveformSamples;
   QWidget* comparePage = nullptr;
   QLabel* compareSourceLabel = nullptr;
   QLabel* compareFinalLabel = nullptr;
   QLabel* compareSourceHeader = nullptr;
   QLabel* compareFinalHeader = nullptr;
   QLabel* infoLabel = nullptr;
   Artifact3DModelViewer* modelViewer = nullptr;
   bool videoWidgetsReady = false;
   bool modelViewerReady = false;
   bool audioWidgetsReady = false;
  QString currentFilePath;
  ArtifactCore::FileType currentFileType = ArtifactCore::FileType::Unknown;
  ContentsViewerMode currentMode = ContentsViewerMode::Source;
  QStringList recentSourcePaths;
  double zoomLevel = 1.0;
   double rotationDegrees = 0.0;
   bool imageFitMode = true;
   QPixmap originalImage;
   QPoint lastMousePos;
   bool playbackRangeActive = false;
   qint64 playbackRangeStartMs = 0;
   qint64 playbackRangeEndMs = 0;
   qint64 audioPlaybackPositionMs = 0;
   QByteArray audioPendingBuffer;
   bool compareSidesSwapped = false;
   int compareWipePercent = 50;
   int viewerAssignment = 1;
   bool viewerHasFocus = false;
   bool eventFiltersInstalled = false;
   bool hoverProbeValid = false;
   QPoint hoverProbePos;
   QColor hoverProbeColor;
   QString compareSourceAPath;
   QString compareSourceBPath;
  };

  qint64 ArtifactContentsViewer::Impl::framesToMs(int64_t frame)
  {
   constexpr double kDefaultFps = 30.0;
   return static_cast<qint64>(std::llround((static_cast<double>(frame) / kDefaultFps) * 1000.0));
  }

  QString ArtifactContentsViewer::Impl::humanFileSize(qint64 bytes)
  {
   if (bytes < 0) {
    return QStringLiteral("-");
   }
   const double kb = static_cast<double>(bytes) / 1024.0;
   if (kb < 1024.0) {
    return QStringLiteral("%1 KB").arg(kb, 0, 'f', kb < 10.0 ? 1 : 0);
   }
   const double mb = kb / 1024.0;
   if (mb < 1024.0) {
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', mb < 10.0 ? 1 : 0);
   }
   const double gb = mb / 1024.0;
   return QStringLiteral("%1 GB").arg(gb, 0, 'f', gb < 10.0 ? 1 : 0);
  }

  QString ArtifactContentsViewer::Impl::formatDurationMs(qint64 ms)
  {
   if (ms < 0) {
    return QStringLiteral("-");
   }
   const qint64 totalSeconds = ms / 1000;
   const qint64 hours = totalSeconds / 3600;
   const qint64 minutes = (totalSeconds % 3600) / 60;
   const qint64 seconds = totalSeconds % 60;
   if (hours > 0) {
    return QStringLiteral("%1:%2:%3")
        .arg(hours)
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
   }
  return QStringLiteral("%1:%2")
       .arg(minutes, 2, 10, QChar('0'))
       .arg(seconds, 2, 10, QChar('0'));
  }

  void ArtifactContentsViewer::Impl::loadRecentSources()
  {
   recentSourcePaths.clear();
   QSettings settings;
   const QStringList stored =
       settings.value(QStringLiteral("ContentsViewer/RecentSourcePaths")).toStringList();
   for (const auto& path : stored) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || recentSourcePaths.contains(trimmed)) {
     continue;
    }
    recentSourcePaths.push_back(trimmed);
    if (recentSourcePaths.size() >= 12) {
     break;
    }
   }
   refreshRecentSourceCombo();
  }

  void ArtifactContentsViewer::Impl::rememberRecentSource(const QString& filepath)
  {
   if (filepath.trimmed().isEmpty()) {
    return;
   }
   const QString normalized = QFileInfo(filepath).absoluteFilePath().trimmed();
   if (normalized.isEmpty()) {
    return;
   }

   recentSourcePaths.removeAll(normalized);
   recentSourcePaths.prepend(normalized);
   while (recentSourcePaths.size() > 12) {
    recentSourcePaths.removeLast();
   }

   QSettings settings;
   settings.setValue(QStringLiteral("ContentsViewer/RecentSourcePaths"), recentSourcePaths);
   settings.setValue(QStringLiteral("ContentsViewer/LastSourcePath"), normalized);
   refreshRecentSourceCombo();
  }

  void ArtifactContentsViewer::Impl::refreshRecentSourceCombo()
  {
   if (!recentSourceCombo) {
    return;
   }

   const QString currentPath = currentFilePath.isEmpty()
                                   ? QString()
                                   : QFileInfo(currentFilePath).absoluteFilePath();
   QSignalBlocker blocker(recentSourceCombo);
   recentSourceCombo->clear();
   recentSourceCombo->setEnabled(!recentSourcePaths.isEmpty());
   if (recentSourcePaths.isEmpty()) {
    recentSourceCombo->addItem(QStringLiteral("Recent Sources"));
    recentSourceCombo->setItemData(0, QString(), Qt::UserRole);
    recentSourceCombo->setCurrentIndex(0);
    recentSourceCombo->setToolTip(QStringLiteral("No recent source files"));
    return;
   }

   for (const auto& path : recentSourcePaths) {
    const QFileInfo info(path);
    const QString label = info.fileName().isEmpty() ? path : info.fileName();
    const int index = recentSourceCombo->count();
    recentSourceCombo->addItem(label, path);
    recentSourceCombo->setItemData(index, path, Qt::ToolTipRole);
    recentSourceCombo->setItemData(index, info.absoluteFilePath(), Qt::UserRole);
   }

   int selectedIndex = -1;
   for (int i = 0; i < recentSourceCombo->count(); ++i) {
    if (recentSourceCombo->itemData(i, Qt::UserRole).toString() == currentPath) {
     selectedIndex = i;
     break;
    }
   }
   recentSourceCombo->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
   recentSourceCombo->setToolTip(QStringLiteral("Recent source files"));
  }

  void ArtifactContentsViewer::Impl::loadCompareSurfaceState()
  {
   QSettings settings;
   compareWipePercent = std::clamp(
       settings.value(QStringLiteral("ContentsViewer/CompareWipePercent"), compareWipePercent).toInt(),
       0, 100);
   compareSidesSwapped = settings.value(
       QStringLiteral("ContentsViewer/CompareSidesSwapped"),
       compareSidesSwapped).toBool();
   compareSourceAPath = settings.value(QStringLiteral("ContentsViewer/CompareSourceAPath")).toString().trimmed();
   compareSourceBPath = settings.value(QStringLiteral("ContentsViewer/CompareSourceBPath")).toString().trimmed();
  }

  void ArtifactContentsViewer::Impl::saveCompareSurfaceState() const
  {
   QSettings settings;
   settings.setValue(QStringLiteral("ContentsViewer/CompareWipePercent"), compareWipePercent);
   settings.setValue(QStringLiteral("ContentsViewer/CompareSidesSwapped"), compareSidesSwapped);
   settings.setValue(QStringLiteral("ContentsViewer/CompareSourceAPath"), compareSourceAPath);
   settings.setValue(QStringLiteral("ContentsViewer/CompareSourceBPath"), compareSourceBPath);
  }

  void ArtifactContentsViewer::Impl::loadViewerAssignmentState()
  {
   QSettings settings;
   viewerAssignment = std::clamp(settings.value(QStringLiteral("ContentsViewer/ViewerAssignment"), viewerAssignment).toInt(), 1, 4);
  }

  void ArtifactContentsViewer::Impl::saveViewerAssignmentState() const
  {
   QSettings settings;
   settings.setValue(QStringLiteral("ContentsViewer/ViewerAssignment"), viewerAssignment);
  }

  void ArtifactContentsViewer::Impl::updateViewerBadge()
  {
   if (!viewerBadgeLabel) {
    return;
   }
   const QString slotLabel = QStringLiteral("Viewer %1").arg(viewerAssignment, 2, 10, QChar('0'));
   viewerBadgeLabel->setText(viewerHasFocus ? QStringLiteral("%1 • Focus").arg(slotLabel) : slotLabel);
   QPalette pal = viewerBadgeLabel->palette();
   const QColor baseColor = QColor(ArtifactCore::currentDCCTheme().textColor);
   pal.setColor(QPalette::WindowText, viewerHasFocus ? baseColor : baseColor.darker(110));
   viewerBadgeLabel->setPalette(pal);
  }

  static qint64 audioBytesToMs(qint64 bytes)
  {
   constexpr qint64 kSampleRate = 44100;
   constexpr qint64 kBytesPerFrame = 4; // 16-bit stereo
   if (bytes <= 0) {
    return 0;
   }
   return (bytes * 1000) / (kSampleRate * kBytesPerFrame);
  }

  void ArtifactContentsViewer::Impl::showInfoMessage(const QString& text)
  {
   infoLabel->setText(text);
   stackedWidget->setCurrentWidget(infoLabel);
   if (titleLabel) {
    titleLabel->setText(QStringLiteral("Contents Viewer"));
   }
   if (typeBadgeLabel) {
    typeBadgeLabel->setText(QStringLiteral("Info"));
   }
   if (metaLabel) {
    metaLabel->setText(QStringLiteral("No content loaded"));
   }
   if (stateLabel) {
    stateLabel->setText(QStringLiteral("State: Idle"));
   }
   updateModeButtons();
   updateSurfaceMeta();
  }

  void ArtifactContentsViewer::Impl::clearPlaybackRange()
  {
   playbackRangeActive = false;
   playbackRangeStartMs = 0;
   playbackRangeEndMs = 0;
  }

  void ArtifactContentsViewer::Impl::setPlaybackRange(int64_t startFrame, int64_t endFrame)
  {
   startFrame = std::max<int64_t>(0, startFrame);
   endFrame = std::max<int64_t>(0, endFrame);
   if (startFrame > endFrame) {
    std::swap(startFrame, endFrame);
   }
   playbackRangeStartMs = framesToMs(startFrame);
   playbackRangeEndMs = framesToMs(endFrame);
   playbackRangeActive = true;
  }

  void ArtifactContentsViewer::Impl::ensureAudioController()
  {
   if (!audioController_) {
    audioController_ = std::make_unique<ArtifactCore::MediaPlaybackController>();
    audioController_->setDecoderBackend(ArtifactCore::DecoderBackend::FFmpeg);
   }
  }

  void ArtifactContentsViewer::Impl::ensureAudioWidgets()
  {
   if (audioPage) {
    return;
   }

   audioPage = new QWidget(owner_);
   auto* layout = new QVBoxLayout(audioPage);
   layout->setContentsMargins(12, 12, 12, 12);
   layout->setSpacing(8);

   audioWaveformLabel = new QLabel(QStringLiteral("Audio waveform preview"), audioPage);
   {
    QFont labelFont = audioWaveformLabel->font();
    labelFont.setPointSize(11);
    labelFont.setWeight(QFont::DemiBold);
    audioWaveformLabel->setFont(labelFont);
    QPalette pal = audioWaveformLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    audioWaveformLabel->setPalette(pal);
   }

   audioWaveformWidget = new AudioWaveformWidget(audioPage);
   audioWaveformWidget->setMinimumHeight(180);

   layout->addWidget(audioWaveformLabel);
   layout->addWidget(audioWaveformWidget, 1);
   stackedWidget->addWidget(audioPage);
  }

  void ArtifactContentsViewer::Impl::resetAudioWaveform()
  {
   audioWaveformSamples.clear();
   if (audioWaveformWidget) {
    audioWaveformWidget->clear();
   }
  }

  void ArtifactContentsViewer::Impl::appendAudioWaveformSamples(const QByteArray& pcmData)
  {
   if (!audioWaveformWidget || pcmData.size() < 4) {
    return;
   }

   constexpr int kBytesPerFrame = 4;
   constexpr int kTargetBins = 48;

   const int frameCount = pcmData.size() / kBytesPerFrame;
   if (frameCount <= 0) {
    return;
   }

   const int framesPerBin = std::max(1, frameCount / kTargetBins);
   const char* raw = pcmData.constData();

   for (int startFrame = 0; startFrame < frameCount; startFrame += framesPerBin) {
    const int endFrame = std::min(startFrame + framesPerBin, frameCount);
    float peak = 0.0f;
    for (int frame = startFrame; frame < endFrame; ++frame) {
     const auto* sample = reinterpret_cast<const qint16*>(raw + frame * kBytesPerFrame);
     const float left = std::abs(static_cast<float>(sample[0]) / 32768.0f);
     const float right = std::abs(static_cast<float>(sample[1]) / 32768.0f);
     peak = std::max(peak, std::max(left, right));
    }
    audioWaveformSamples.append(std::clamp(peak, 0.0f, 1.0f));
   }

   constexpr int kMaxSamples = 4096;
   if (audioWaveformSamples.size() > kMaxSamples) {
    audioWaveformSamples.remove(0, audioWaveformSamples.size() - kMaxSamples);
   }

   audioWaveformWidget->setSamples(audioWaveformSamples, 44100);
   if (!audioWaveformSamples.isEmpty()) {
    audioWaveformWidget->setPosition(audioWaveformSamples.size() - 1);
   }
  }

  void ArtifactContentsViewer::Impl::releaseAudioPlayback()
  {
   audioPendingBuffer.clear();
   audioPlaybackPositionMs = 0;
   resetAudioWaveform();

   if (audioPumpTimer) {
    audioPumpTimer->stop();
   }

   if (audioSink) {
    audioSink->stop();
    audioSink->deleteLater();
    audioSink = nullptr;
   }
   audioSinkDevice = nullptr;

   if (audioController_) {
    audioController_->stop();
    audioController_->closeMedia();
   }
  }

  void ArtifactContentsViewer::Impl::activateAudio(const QString& filepath)
  {
   QFileInfo info(filepath);
   if (!info.exists()) {
    showInfoMessage("Audio file does not exist:\n" + filepath);
    return;
   }

   ensureAudioController();
   if (!audioController_->openMediaFile(info.absoluteFilePath())) {
    showInfoMessage("Failed to load audio:\n" + filepath);
    return;
   }

   ensureAudioWidgets();
   resetAudioWaveform();

   if (audioPumpTimer) {
    audioPumpTimer->stop();
   }

   if (audioSink) {
    audioSink->stop();
    audioSink->deleteLater();
    audioSink = nullptr;
   }
   audioSinkDevice = nullptr;
   audioPendingBuffer.clear();

   QAudioFormat format;
   format.setSampleRate(44100);
   format.setChannelCount(2);
   format.setSampleFormat(QAudioFormat::Int16);

   audioSink = new QAudioSink(format, owner_);
   audioSink->setVolume(1.0f);
   audioSinkDevice = audioSink->start();
   audioSink->suspend();

   if (!audioPumpTimer) {
    audioPumpTimer = new QTimer(owner_);
    QObject::connect(audioPumpTimer, &QTimer::timeout, owner_, [this]() {
     pumpAudioPlayback();
    });
   }
   audioPumpTimer->setInterval(16);

   audioPlaybackPositionMs = 0;
   if (audioWaveformLabel) {
    const auto metadata = audioController_->getMetadata();
    audioWaveformLabel->setText(metadata.formatName.isEmpty()
                                    ? QStringLiteral("Audio waveform preview")
                                    : QStringLiteral("Audio waveform preview  •  %1").arg(metadata.formatName));
   }
   if (infoLabel) {
    infoLabel->setText(QStringLiteral("Audio preview ready\n%1").arg(info.absoluteFilePath()));
   }
   if (audioPage) {
    stackedWidget->setCurrentWidget(audioPage);
   } else if (infoLabel) {
    stackedWidget->setCurrentWidget(infoLabel);
   }
   updateHeader();
   updatePlaybackState();
   updateActionAvailability();
   updateModeButtons();
   if (owner_) {
    owner_->play();
   }
  }

  void ArtifactContentsViewer::Impl::pumpAudioPlayback()
  {
   if (currentFileType != ArtifactCore::FileType::Audio || !audioController_ || !audioSink || !audioSinkDevice) {
    return;
   }

   if (audioSink->state() == QAudio::SuspendedState || audioSink->state() == QAudio::StoppedState) {
    return;
   }

   if (playbackRangeActive && audioPlaybackPositionMs >= playbackRangeEndMs) {
    audioController_->stop();
    audioController_->seek(playbackRangeStartMs);
    audioPendingBuffer.clear();
    audioPlaybackPositionMs = playbackRangeStartMs;
    audioSink->suspend();
    updatePlaybackState();
    return;
   }

   while (audioPendingBuffer.size() < 32768) {
    const QByteArray chunk = audioController_->getNextAudioFrame();
    if (chunk.isEmpty()) {
     break;
    }
    audioPendingBuffer.append(chunk);
   }

   if (audioPendingBuffer.isEmpty()) {
    audioController_->stop();
    audioPlaybackPositionMs = playbackRangeActive ? playbackRangeStartMs : 0;
    audioSink->suspend();
    updatePlaybackState();
    return;
   }

   const qint64 written = audioSinkDevice->write(audioPendingBuffer.constData(),
                                                 static_cast<qint64>(audioPendingBuffer.size()));
   if (written > 0) {
    const QByteArray writtenChunk = audioPendingBuffer.left(static_cast<int>(written));
    audioPendingBuffer.remove(0, static_cast<int>(written));
    audioPlaybackPositionMs += audioBytesToMs(written);
    appendAudioWaveformSamples(writtenChunk);
    if (seekSlider && !seekSlider->isSliderDown()) {
     QSignalBlocker blocker(seekSlider);
     seekSlider->setValue(static_cast<int>(std::clamp<qint64>(audioPlaybackPositionMs, 0, std::numeric_limits<int>::max())));
    }
    updatePlaybackState();
   }
  }

  void ArtifactContentsViewer::Impl::resetCurrentMode()
  {
   clearPlaybackRange();
   currentMode = ContentsViewerMode::Source;
   imageFitMode = true;

   releaseAudioPlayback();

   if (mediaPlayer) {
    QSignalBlocker blocker(mediaPlayer);
    detachMediaOutputs();
    mediaPlayer->stop();
    mediaPlayer->setSource(QUrl());
   }

   if (imageLabel) {
    imageLabel->clear();
    imageLabel->setFixedSize(QSize(0, 0));
   }
   originalImage = QPixmap();

   if (modelViewer) {
    modelViewer->clearModel();
   }

   zoomLevel = 1.0;
   rotationDegrees = 0.0;
   updateHeader();
   updatePlaybackState();
   updateActionAvailability();
   updateModeButtons();
  }

  void ArtifactContentsViewer::Impl::ensureVideoWidgets()
  {
   if (videoWidgetsReady) {
    return;
   }

   videoWidget = new QVideoWidget(owner_);
   {
    QPalette pal = videoWidget->palette();
    pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
    videoWidget->setPalette(pal);
   }
   videoWidget->setAutoFillBackground(true);

   mediaPlayer = new QMediaPlayer(owner_);
   audioOutput = new QAudioOutput(owner_);
   audioOutput->setVolume(1.0f);
   attachMediaOutputs();

   QObject::connect(mediaPlayer, &QMediaPlayer::positionChanged, owner_, [this](qint64 position) {
    if (seekSlider && !seekSlider->isSliderDown()) {
     QSignalBlocker blocker(seekSlider);
     seekSlider->setValue(static_cast<int>(std::clamp<qint64>(position, 0, std::numeric_limits<int>::max())));
    }
    if (playbackRangeActive && position >= playbackRangeEndMs) {
     mediaPlayer->pause();
     mediaPlayer->setPosition(playbackRangeStartMs);
    }
    updatePlaybackState();
   });

   QObject::connect(mediaPlayer, &QMediaPlayer::playbackStateChanged, owner_,
    [this](QMediaPlayer::PlaybackState) {
     updatePlaybackState();
    });

   QObject::connect(mediaPlayer, &QMediaPlayer::durationChanged, owner_,
    [this](qint64 duration) {
     if (seekSlider) {
      QSignalBlocker blocker(seekSlider);
      seekSlider->setRange(0, static_cast<int>(std::clamp<qint64>(duration, 0, std::numeric_limits<int>::max())));
     }
     updateHeader();
     updatePlaybackState();
    });

   QObject::connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged, owner_,
    [this](QMediaPlayer::MediaStatus status) {
     if (status == QMediaPlayer::EndOfMedia) {
      const qint64 resetPosition = playbackRangeActive ? playbackRangeStartMs : 0;
      if (seekSlider) {
       QSignalBlocker blocker(seekSlider);
       seekSlider->setValue(static_cast<int>(std::clamp<qint64>(resetPosition, 0, std::numeric_limits<int>::max())));
      }
      mediaPlayer->setPosition(resetPosition);
     }
     updatePlaybackState();
    });

   QObject::connect(mediaPlayer, &QMediaPlayer::errorOccurred, owner_,
    [this](QMediaPlayer::Error error, const QString& errorString) {
     if (error == QMediaPlayer::NoError) return;
     showInfoMessage("Failed to play video:\n" + currentFilePath + "\n" + errorString);
    });

   stackedWidget->addWidget(videoWidget);
   videoWidgetsReady = true;
  }

  void ArtifactContentsViewer::Impl::ensureModelViewer()
  {
   if (modelViewerReady) {
    return;
   }

   modelViewer = new Artifact3DModelViewer(owner_);
   QObject::connect(modelViewer, &Artifact3DModelViewer::displayModeChanged, owner_, [this](Artifact3DModelViewer::DisplayMode) {
    updateHeader();
    updateSurfaceMeta();
   });
   auto* resetShortcut = new QShortcut(QKeySequence::ZoomReset, owner_);
   resetShortcut->setContext(Qt::WidgetWithChildrenShortcut);
   QObject::connect(resetShortcut, &QShortcut::activated, owner_, [this]() {
    if (currentFileType == ArtifactCore::FileType::Model3D && modelViewer) {
     modelViewer->resetView();
     updateHeader();
     updatePlaybackState();
    }
   });
   stackedWidget->addWidget(modelViewer);
   modelViewerReady = true;
  }

  void ArtifactContentsViewer::Impl::ensureCompareWidgets()
  {
   loadCompareSurfaceState();
   if (comparePage) {
    return;
   }

   comparePage = new QWidget(owner_);
   auto* layout = new QVBoxLayout(comparePage);
   layout->setContentsMargins(12, 12, 12, 12);
   layout->setSpacing(8);

   auto* hintLabel = new QLabel(QStringLiteral("Drag the center divider to wipe compare."), comparePage);
   {
    QFont hintFont = hintLabel->font();
    hintFont.setPointSize(10);
    hintLabel->setFont(hintFont);
    QPalette pal = hintLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(120));
    hintLabel->setPalette(pal);
   }

   auto makePanel = [this](const QString& headerText, QLabel*& headerOut, QLabel*& contentOut) {
    auto* panel = new QWidget(comparePage);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(6);

   headerOut = new QLabel(headerText, panel);
   {
    QFont headerFont = headerOut->font();
    headerFont.setPointSize(11);
    headerFont.setWeight(QFont::DemiBold);
     headerOut->setFont(headerFont);
     QPalette pal = headerOut->palette();
     pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
     headerOut->setPalette(pal);
    }
    headerOut->setCursor(Qt::PointingHandCursor);

    contentOut = new QLabel(panel);
    contentOut->setAlignment(Qt::AlignCenter);
    contentOut->setWordWrap(true);
    contentOut->setMinimumSize(0, 0);
    contentOut->setScaledContents(true);
    contentOut->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    contentOut->setFrameShape(QFrame::StyledPanel);
    contentOut->setFrameShadow(QFrame::Sunken);
    {
     QPalette pal = contentOut->palette();
     pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
     pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
     contentOut->setPalette(pal);
    }
    contentOut->setAutoFillBackground(true);

    panelLayout->addWidget(headerOut, 0, Qt::AlignLeft);
    panelLayout->addWidget(contentOut, 1);
    return panel;
   };

   compareSplitter = new QSplitter(Qt::Horizontal, comparePage);
   compareSplitter->setChildrenCollapsible(false);
   compareSplitter->addWidget(makePanel(QStringLiteral("A"), compareSourceHeader, compareSourceLabel));
   compareSplitter->addWidget(makePanel(QStringLiteral("B"), compareFinalHeader, compareFinalLabel));
   compareSplitter->setStretchFactor(0, 1);
   compareSplitter->setStretchFactor(1, 1);

   compareWipeSlider = new QSlider(Qt::Horizontal, comparePage);
   compareWipeSlider->setRange(0, 100);
   compareWipeSlider->setValue(compareWipePercent);
   compareWipeSlider->setToolTip(QStringLiteral("Wipe compare position"));
   {
    QPalette pal = compareWipeSlider->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    compareWipeSlider->setPalette(pal);
   }
   QObject::connect(compareWipeSlider, &QSlider::valueChanged, comparePage, [this](int value) {
    compareWipePercent = std::clamp(value, 0, 100);
    saveCompareSurfaceState();
    updateCompareWipe();
   });

   layout->addWidget(hintLabel, 0);
  layout->addWidget(compareSplitter, 1);
  layout->addWidget(compareWipeSlider, 0);
  stackedWidget->addWidget(comparePage);
  updateCompareWipe();

  }

  void ArtifactContentsViewer::Impl::updateCompareWipe()
  {
   if (!compareSplitter) {
    return;
   }
   const int total = std::max(1, compareSplitter->width());
   const int left = std::clamp((total * compareWipePercent) / 100, 1, std::max(1, total - 1));
   compareSplitter->setSizes({left, std::max(1, total - left)});
  }

  void ArtifactContentsViewer::Impl::updateCompareSurface()
  {
   if (!comparePage || !compareSourceLabel || !compareFinalLabel) {
    return;
   }

   ArtifactCore::FileTypeDetector detector;
   auto sourcePathForRole = [&](bool leftRole) -> QString {
    const QString assignedPath = leftRole
                                     ? compareSourceAPath
                                     : compareSourceBPath;
    if (!assignedPath.isEmpty()) {
     return assignedPath;
    }
    return currentFilePath;
   };

   const QString logicalLeftPath = compareSidesSwapped ? sourcePathForRole(false) : sourcePathForRole(true);
   const QString logicalRightPath = compareSidesSwapped ? sourcePathForRole(true) : sourcePathForRole(false);
   const QString leftBadge = compareSidesSwapped ? QStringLiteral("B") : QStringLiteral("A");
   const QString rightBadge = compareSidesSwapped ? QStringLiteral("A") : QStringLiteral("B");
   const QString leftTitle = logicalLeftPath.isEmpty()
                                 ? QStringLiteral("Unassigned")
                                 : QFileInfo(logicalLeftPath).fileName();
   const QString rightTitle = logicalRightPath.isEmpty()
                                  ? QStringLiteral("Unassigned")
                                  : QFileInfo(logicalRightPath).fileName();
   const auto leftType = logicalLeftPath.isEmpty() ? ArtifactCore::FileType::Unknown
                                                   : detector.detect(logicalLeftPath);
   const auto rightType = logicalRightPath.isEmpty() ? ArtifactCore::FileType::Unknown
                                                     : detector.detect(logicalRightPath);

   auto applyTextPanel = [&](QLabel* label, const QString& title, const QString& body) {
    if (!label) {
     return;
    }
    label->setPixmap(QPixmap());
    label->setScaledContents(false);
    label->setMinimumSize(1, 1);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    const QString escapedBody = body.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>"));
    label->setText(QStringLiteral("<b>%1</b><br>%2").arg(title, escapedBody));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setToolTip(body);
   };

   auto applyImagePanel = [&](QLabel* label, const QString& path, const QString& title, const QString& role) {
    if (!label) {
     return;
    }
    QPixmap pixmap(path);
    if (pixmap.isNull()) {
     applyTextPanel(label, title, QStringLiteral("%1\n%2").arg(role, path.isEmpty() ? QStringLiteral("No file selected") : path));
     return;
    }
    label->setText({});
    label->setPixmap(pixmap);
    label->setAlignment(Qt::AlignCenter);
    label->setScaledContents(true);
    label->setMinimumSize(1, 1);
    label->setToolTip(path);
    label->setTextInteractionFlags(Qt::NoTextInteraction);
   };

   auto buildFallbackBody = [&](const QString& path, ArtifactCore::FileType type) {
    if (path.isEmpty()) {
     return QStringLiteral("No file selected");
    }
    QFileInfo info(path);
    QStringList lines;
    lines << QStringLiteral("%1").arg(path);
    if (info.exists()) {
     lines.prepend(humanFileSize(info.size()));
    } else {
     lines.prepend(QStringLiteral("Missing"));
    }
    switch (type) {
    case ArtifactCore::FileType::Video:
     lines.prepend(QStringLiteral("Video source"));
     break;
    case ArtifactCore::FileType::Audio:
     lines.prepend(QStringLiteral("Audio source"));
     break;
    case ArtifactCore::FileType::Model3D:
     lines.prepend(QStringLiteral("3D source"));
     break;
    case ArtifactCore::FileType::Image:
     lines.prepend(QStringLiteral("Image source"));
     break;
    default:
     lines.prepend(QStringLiteral("Unsupported source"));
     break;
    }
    return lines.join(QStringLiteral("\n"));
   };

   QLabel* leftLabel = compareSidesSwapped ? compareFinalLabel : compareSourceLabel;
   QLabel* rightLabel = compareSidesSwapped ? compareSourceLabel : compareFinalLabel;
   QLabel* leftHeader = compareSidesSwapped ? compareFinalHeader : compareSourceHeader;
   QLabel* rightHeader = compareSidesSwapped ? compareSourceHeader : compareFinalHeader;

   if (leftHeader) {
    leftHeader->setText(QStringLiteral("%1 · %2").arg(leftBadge, leftTitle.isEmpty() ? QStringLiteral("Unassigned") : leftTitle));
   }
   if (rightHeader) {
    rightHeader->setText(QStringLiteral("%1 · %2").arg(rightBadge, rightTitle.isEmpty() ? QStringLiteral("Unassigned") : rightTitle));
   }

   if (leftType == ArtifactCore::FileType::Image || rightType == ArtifactCore::FileType::Image) {
    if (leftType == ArtifactCore::FileType::Image) {
     applyImagePanel(leftLabel, logicalLeftPath, leftTitle, QStringLiteral("A"));
    } else {
     applyTextPanel(leftLabel, leftTitle, buildFallbackBody(logicalLeftPath, leftType));
    }

    if (rightType == ArtifactCore::FileType::Image) {
     applyImagePanel(rightLabel, logicalRightPath, rightTitle, QStringLiteral("B"));
    } else {
     applyTextPanel(rightLabel, rightTitle, buildFallbackBody(logicalRightPath, rightType));
    }
   } else {
    applyTextPanel(leftLabel, leftTitle, buildFallbackBody(logicalLeftPath, leftType));
    applyTextPanel(rightLabel, rightTitle, buildFallbackBody(logicalRightPath, rightType));
   }
  }

  void ArtifactContentsViewer::Impl::swapCompareSides()
  {
   compareSidesSwapped = !compareSidesSwapped;
   saveCompareSurfaceState();
   updateCompareSurface();
   updateCompareWipe();
  }

  void ArtifactContentsViewer::Impl::assignCompareSource(bool leftSide)
  {
   if (currentFilePath.trimmed().isEmpty()) {
    return;
   }

   const QString normalized = QFileInfo(currentFilePath).absoluteFilePath().trimmed();
   if (normalized.isEmpty()) {
    return;
   }

   if (leftSide) {
    compareSourceAPath = normalized;
   } else {
    compareSourceBPath = normalized;
   }
   saveCompareSurfaceState();
   currentMode = ContentsViewerMode::Compare;
   updateHeader();
   updatePlaybackState();
   updateModeButtons();
   syncModelViewerMode();
  }

  void ArtifactContentsViewer::Impl::updateDisplayedPage()
  {
   if (!stackedWidget) {
    return;
   }

  if (currentMode == ContentsViewerMode::Compare && comparePage) {
   stackedWidget->setCurrentWidget(comparePage);
   updateCompareSurface();
    updateCompareWipe();
   return;
  }

   switch (currentFileType) {
   case ArtifactCore::FileType::Image:
    if (imageScrollArea) {
     stackedWidget->setCurrentWidget(imageScrollArea);
    }
    break;
   case ArtifactCore::FileType::Video:
    if (videoWidget) {
     stackedWidget->setCurrentWidget(videoWidget);
    }
    break;
   case ArtifactCore::FileType::Audio:
    if (audioPage) {
     stackedWidget->setCurrentWidget(audioPage);
    }
    break;
   case ArtifactCore::FileType::Model3D:
    if (modelViewer) {
     stackedWidget->setCurrentWidget(modelViewer);
    }
    break;
   default:
    if (infoLabel) {
     stackedWidget->setCurrentWidget(infoLabel);
    }
    break;
   }
  }

  void ArtifactContentsViewer::Impl::activateImage(const QString& filepath)
  {
   QFileInfo info(filepath);
   if (!info.exists()) {
    showInfoMessage("Image file does not exist:\n" + filepath);
    return;
   }

   QPixmap pixmap(info.absoluteFilePath());
   if (pixmap.isNull()) {
    showInfoMessage("Failed to load image:\n" + filepath);
    return;
   }

   clearPlaybackRange();
   originalImage = pixmap;
   rotationDegrees = 0.0;
   zoomLevel = 1.0;
   imageFitMode = true;
   imageLabel->setPixmap(pixmap);
   imageLabel->setFixedSize(pixmap.size());
   imageScrollArea->setWidgetResizable(false);
   updateDisplayedPage();
   fitImageToWindow();
   if (currentMode == ContentsViewerMode::Compare) {
    updateCompareSurface();
   }
   updateHeader();
   updatePlaybackState();
   updateActionAvailability();
   updateModeButtons();
  }

  void ArtifactContentsViewer::Impl::activateVideo(const QString& filepath)
  {
   QFileInfo info(filepath);
   if (!info.exists()) {
    showInfoMessage("Video file does not exist:\n" + filepath);
    return;
   }

   clearPlaybackRange();
   ensureVideoWidgets();
   attachMediaOutputs();
   updateDisplayedPage();
   mediaPlayer->setSource(QUrl::fromLocalFile(info.absoluteFilePath()));
   mediaPlayer->play();
   updateHeader();
   updatePlaybackState();
   updateActionAvailability();
   updateModeButtons();
  }

  void ArtifactContentsViewer::Impl::activateModel(const QString& filepath)
  {
   ensureModelViewer();
   if (modelViewer) {
    modelViewer->setDisplayMode(Artifact3DModelViewer::DisplayMode::Solid);
   }
   modelViewer->loadModel(ArtifactCore::UniString(filepath.toStdString()));
   syncModelViewerMode();
   updateDisplayedPage();
   updateHeader();
   updatePlaybackState();
   updateActionAvailability();
   updateModeButtons();
  }

  void ArtifactContentsViewer::Impl::fitImageToWindow()
  {
   if (originalImage.isNull() || !imageScrollArea || !imageLabel) {
    return;
   }

   const QPixmap currentPixmap = imageLabel->pixmap();
   const QSize baseSize = currentPixmap.isNull() ? originalImage.size() : currentPixmap.size();
   if (baseSize.isEmpty()) {
    return;
   }

   const QSize viewportSize = imageScrollArea->viewport() ? imageScrollArea->viewport()->size() : QSize();
   if (viewportSize.isEmpty()) {
    zoomLevel = 1.0;
    applyImageTransform();
    return;
   }

   const double scaleX = static_cast<double>(viewportSize.width()) / static_cast<double>(baseSize.width());
   const double scaleY = static_cast<double>(viewportSize.height()) / static_cast<double>(baseSize.height());
   zoomLevel = std::clamp(std::min(scaleX, scaleY), 0.05, 10.0);
   imageFitMode = true;
   applyImageTransform();
  }

  void ArtifactContentsViewer::Impl::applyImageTransform()
  {
   if (originalImage.isNull() || !imageLabel) {
    return;
   }

   QTransform transform;
   transform.rotate(rotationDegrees);
   QPixmap transformed = originalImage.transformed(transform, Qt::SmoothTransformation);
   if (transformed.isNull()) {
    return;
   }

   const QSize targetSize = (transformed.size() * zoomLevel).expandedTo(QSize(1, 1));
   if (targetSize != transformed.size()) {
    transformed = transformed.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
   }

   imageLabel->setPixmap(transformed);
   imageLabel->setFixedSize(transformed.size());
   if (currentMode == ContentsViewerMode::Compare) {
    updateCompareSurface();
   }
  }

  void ArtifactContentsViewer::Impl::syncModelViewerMode()
  {
   if (!modelViewer) {
    return;
   }

   switch (currentMode) {
   case ContentsViewerMode::Final:
    modelViewer->setDisplayMode(Artifact3DModelViewer::DisplayMode::Solid);
    break;
   case ContentsViewerMode::Compare:
    modelViewer->setDisplayMode(Artifact3DModelViewer::DisplayMode::SolidWithWire);
    break;
   case ContentsViewerMode::Source:
   default:
    modelViewer->setDisplayMode(Artifact3DModelViewer::DisplayMode::Solid);
    break;
   }
  }

  void ArtifactContentsViewer::Impl::updateHeader()
  {
   if (!titleLabel || !typeBadgeLabel || !metaLabel || !stateLabel) {
    return;
   }

   const QFileInfo info(currentFilePath);
   const QString baseName = info.exists() ? info.fileName() : QFileInfo(currentFilePath).fileName();
   titleLabel->setText(baseName.isEmpty() ? QStringLiteral("(untitled)") : baseName);

   switch (currentFileType) {
   case ArtifactCore::FileType::Image:
    typeBadgeLabel->setText(QStringLiteral("Image"));
    break;
   case ArtifactCore::FileType::Video:
    typeBadgeLabel->setText(QStringLiteral("Video"));
    break;
   case ArtifactCore::FileType::Audio:
    typeBadgeLabel->setText(QStringLiteral("Audio"));
    break;
   case ArtifactCore::FileType::Model3D:
    typeBadgeLabel->setText(QStringLiteral("3D Model"));
    break;
   default:
    typeBadgeLabel->setText(QStringLiteral("Unknown"));
    break;
   }
   if (currentMode == ContentsViewerMode::Source) {
    typeBadgeLabel->setText(typeBadgeLabel->text() + QStringLiteral(" / Source"));
   } else if (currentMode == ContentsViewerMode::Final) {
    typeBadgeLabel->setText(typeBadgeLabel->text() + QStringLiteral(" / Final"));
   } else if (currentMode == ContentsViewerMode::Compare) {
    typeBadgeLabel->setText(typeBadgeLabel->text() + QStringLiteral(" / Compare"));
   }
   updateViewerBadge();

   QStringList metaParts;
   if (info.exists()) {
    metaParts << humanFileSize(info.size());
    metaParts << info.absoluteFilePath();
   } else if (!currentFilePath.isEmpty()) {
    metaParts << QStringLiteral("Missing");
    metaParts << currentFilePath;
   } else {
    metaParts << QStringLiteral("No file selected");
   }

   if (currentFileType == ArtifactCore::FileType::Image && !originalImage.isNull()) {
    metaParts.prepend(QStringLiteral("%1x%2")
                          .arg(originalImage.width())
                          .arg(originalImage.height()));
   } else if (currentFileType == ArtifactCore::FileType::Video && mediaPlayer) {
    const qint64 duration = mediaPlayer->duration();
    if (duration > 0) {
     metaParts.prepend(QStringLiteral("Duration %1").arg(formatDurationMs(duration)));
    }
   } else if (currentFileType == ArtifactCore::FileType::Audio && audioController_ && audioController_->isMediaOpen()) {
    const qint64 duration = audioController_->getDuration();
    if (duration > 0) {
     metaParts.prepend(QStringLiteral("Duration %1").arg(formatDurationMs(duration)));
    }
    const auto metadata = audioController_->getMetadata();
    if (!metadata.formatName.isEmpty()) {
     metaParts.prepend(metadata.formatName);
    }
    if (!metadata.streams.empty()) {
     for (const auto& stream : metadata.streams) {
      if (stream.type == ArtifactCore::MediaType::Audio) {
       QStringList audioBits;
       if (stream.audioCodec.sampleRate > 0) {
        audioBits << QStringLiteral("%1 Hz").arg(stream.audioCodec.sampleRate);
       }
       if (stream.audioCodec.channels > 0) {
        audioBits << QStringLiteral("%1 ch").arg(stream.audioCodec.channels);
       }
       if (!stream.audioCodec.codecName.isEmpty()) {
        audioBits << stream.audioCodec.codecName;
       }
       if (!audioBits.isEmpty()) {
        metaParts.prepend(audioBits.join(QStringLiteral(" / ")));
       }
       break;
      }
     }
    }
   } else if (currentFileType == ArtifactCore::FileType::Model3D) {
    const QString suffix = info.suffix().toUpper();
    if (modelViewer && modelViewer->hasModel()) {
     metaParts.prepend(QStringLiteral("Preview"));
     metaParts.prepend(QStringLiteral("%1v / %2p")
                           .arg(modelViewer->vertexCount())
                           .arg(modelViewer->polygonCount()));
     if (!suffix.isEmpty()) {
      metaParts.prepend(QStringLiteral("%1").arg(suffix));
     }
    } else if (modelViewer) {
     metaParts.prepend(QStringLiteral("Preview unavailable"));
     if (!suffix.isEmpty()) {
      metaParts.prepend(QStringLiteral("%1").arg(suffix));
     }
     if (!modelViewer->backendName().isEmpty() && modelViewer->backendName() != QStringLiteral("none")) {
      metaParts.prepend(modelViewer->backendName());
     }
    } else {
     metaParts.prepend(QStringLiteral("Preview unavailable"));
    }
    if (modelViewer && modelViewer->displayMode() == Artifact3DModelViewer::DisplayMode::Wireframe) {
     metaParts.prepend(QStringLiteral("Wireframe"));
    } else if (modelViewer && modelViewer->displayMode() == Artifact3DModelViewer::DisplayMode::SolidWithWire) {
     metaParts.prepend(QStringLiteral("Solid+Wire"));
    } else if (modelViewer) {
     metaParts.prepend(QStringLiteral("Solid"));
    }
   }

   metaLabel->setText(metaParts.join(QStringLiteral(" | ")));
   updateSurfaceMeta();
  }

  void ArtifactContentsViewer::Impl::openCompareSource(bool leftSide)
  {
   const QString path = leftSide ? compareSourceAPath : compareSourceBPath;
   if (path.trimmed().isEmpty() || !owner_) {
    return;
   }
   owner_->setFilePath(path);
   owner_->setViewerMode(ContentsViewerMode::Source);
  }

  void ArtifactContentsViewer::Impl::updateSurfaceMeta()
  {
   if (!surfaceMetaLabel) {
    return;
   }

   QStringList chips;
   chips << QStringLiteral("Viewer");
   chips << QStringLiteral("Slot %1").arg(viewerAssignment, 2, 10, QChar('0'));
   switch (currentFileType) {
   case ArtifactCore::FileType::Image:
    chips << QStringLiteral("Image");
    if (!originalImage.isNull()) {
     chips << QStringLiteral("%1x%2").arg(originalImage.width()).arg(originalImage.height());
     chips << QStringLiteral("Zoom %1%").arg(static_cast<int>(std::round(zoomLevel * 100.0)));
     chips << QStringLiteral("Rotate %1°").arg(static_cast<int>(std::round(rotationDegrees)));
    }
    break;
   case ArtifactCore::FileType::Video:
    chips << QStringLiteral("Video");
    if (mediaPlayer) {
     const qreal speed = mediaPlayer->playbackRate();
     chips << QStringLiteral("%1 / %2")
                 .arg(formatDurationMs(mediaPlayer->position()))
                 .arg(formatDurationMs(mediaPlayer->duration()));
     chips << QStringLiteral("Speed x%1").arg(speed, 0, 'f', speed >= 1.0 ? 1 : 2);
     chips << (playbackRangeActive
                   ? QStringLiteral("Range %1-%2").arg(formatDurationMs(playbackRangeStartMs),
                                                         formatDurationMs(playbackRangeEndMs))
                   : QStringLiteral("Range Off"));
    }
    break;
   case ArtifactCore::FileType::Audio:
    chips << QStringLiteral("Audio");
    if (audioController_) {
     const qint64 position = audioPlaybackPositionMs;
     const qint64 duration = audioController_->getDuration();
     if (duration > 0) {
      chips << QStringLiteral("%1 / %2")
                  .arg(formatDurationMs(position))
                  .arg(formatDurationMs(duration));
     }
     chips << (audioController_->getDecoderBackend() == ArtifactCore::DecoderBackend::MediaFoundation
                   ? QStringLiteral("MediaFoundation")
                   : QStringLiteral("FFmpeg"));
     chips << (playbackRangeActive
                   ? QStringLiteral("Range %1-%2").arg(formatDurationMs(playbackRangeStartMs),
                                                         formatDurationMs(playbackRangeEndMs))
                   : QStringLiteral("Range Off"));
    }
    break;
   case ArtifactCore::FileType::Model3D:
    chips << QStringLiteral("3D Preview");
    if (modelViewer && modelViewer->hasModel()) {
     chips << modelViewer->backendName();
     switch (modelViewer->displayMode()) {
     case Artifact3DModelViewer::DisplayMode::Wireframe:
      chips << QStringLiteral("Wireframe");
      break;
     case Artifact3DModelViewer::DisplayMode::SolidWithWire:
      chips << QStringLiteral("Solid+Wire");
      break;
     case Artifact3DModelViewer::DisplayMode::Solid:
      chips << QStringLiteral("Solid");
      break;
     }
     chips << QStringLiteral("%1v / %2p")
                 .arg(modelViewer->vertexCount())
                 .arg(modelViewer->polygonCount());
     chips << QStringLiteral("Zoom %1%").arg(static_cast<int>(std::round(modelViewer->zoomFactor() * 100.0f)));
     chips << QStringLiteral("Yaw %1°").arg(static_cast<int>(std::round(modelViewer->cameraYaw())));
     chips << QStringLiteral("Pitch %1°").arg(static_cast<int>(std::round(modelViewer->cameraPitch())));
    } else if (modelViewer) {
     chips << QStringLiteral("Preview unavailable");
     chips << modelViewer->backendName();
     if (!modelViewer->lastErrorText().isEmpty()) {
      chips << modelViewer->lastErrorText();
     }
    } else {
     chips << QStringLiteral("Preview unavailable");
    }
    break;
   default:
    chips << QStringLiteral("Unknown");
    break;
   }

   if (currentMode == ContentsViewerMode::Source) {
    chips << QStringLiteral("Source");
   } else if (currentMode == ContentsViewerMode::Final) {
    chips << QStringLiteral("Final");
   } else if (currentMode == ContentsViewerMode::Compare) {
    chips << QStringLiteral("Compare");
    if (!compareSourceAPath.isEmpty()) {
     chips << QStringLiteral("A %1").arg(QFileInfo(compareSourceAPath).fileName());
    }
    if (!compareSourceBPath.isEmpty()) {
     chips << QStringLiteral("B %1").arg(QFileInfo(compareSourceBPath).fileName());
    }
   }

   surfaceMetaLabel->setText(chips.join(QStringLiteral("  •  ")));
   if (currentMode == ContentsViewerMode::Compare) {
    surfaceMetaLabel->setToolTip(QStringLiteral("Compare mode. Click the A/B chips to reopen their sources, or use Tab to swap sides."));
   } else {
    surfaceMetaLabel->setToolTip(QStringLiteral("Viewer state summary"));
   }
   updateChannelMetaSurface();
  }

  void ArtifactContentsViewer::Impl::updateChannelMetaSurface()
  {
   if (!channelMetaLabel) {
    return;
   }

   QStringList chips;
   switch (currentFileType) {
   case ArtifactCore::FileType::Image:
    chips << QStringLiteral("RGBA");
    chips << QStringLiteral("RGB");
    chips << QStringLiteral("Alpha");
    chips << QStringLiteral("Luma");
    if (hoverProbeValid) {
     chips << QStringLiteral("Probe %1,%2")
                 .arg(hoverProbePos.x())
                 .arg(hoverProbePos.y());
     chips << QStringLiteral("#%1%2%3%4")
                 .arg(hoverProbeColor.red(), 2, 16, QChar('0'))
                 .arg(hoverProbeColor.green(), 2, 16, QChar('0'))
                 .arg(hoverProbeColor.blue(), 2, 16, QChar('0'))
                 .arg(hoverProbeColor.alpha(), 2, 16, QChar('0'))
                 .toUpper();
    } else {
     chips << QStringLiteral("Hover to probe pixels");
    }
    break;
   case ArtifactCore::FileType::Video:
    chips << QStringLiteral("RGBA");
    chips << QStringLiteral("Audio");
    chips << QStringLiteral("Timeline");
    chips << QStringLiteral("Cursor Sample");
    break;
   case ArtifactCore::FileType::Audio:
    chips << QStringLiteral("L/R");
    chips << QStringLiteral("Waveform");
    chips << QStringLiteral("Peak");
    chips << QStringLiteral("Cursor Sample");
    break;
  case ArtifactCore::FileType::Model3D:
   chips << QStringLiteral("World XYZ");
   chips << QStringLiteral("Camera");
   chips << QStringLiteral("Orbit");
   chips << QStringLiteral("Dolly");
    if (modelViewer) {
     chips << QStringLiteral("Zoom %1%").arg(static_cast<int>(std::round(modelViewer->zoomFactor() * 100.0f)));
     chips << QStringLiteral("Yaw %1°").arg(static_cast<int>(std::round(modelViewer->cameraYaw())));
     chips << QStringLiteral("Pitch %1°").arg(static_cast<int>(std::round(modelViewer->cameraPitch())));
    }
   break;
   default:
    chips << QStringLiteral("No data");
    chips << QStringLiteral("Unsupported");
    break;
   }

   if (currentMode == ContentsViewerMode::Compare) {
    chips << QStringLiteral("A/B compare");
   }

   channelMetaLabel->setText(chips.join(QStringLiteral("  •  ")));
   if (currentMode == ContentsViewerMode::Compare) {
    channelMetaLabel->setToolTip(QStringLiteral("Compare routing is active. Hover probe stays tied to the current source."));
   } else {
    channelMetaLabel->setToolTip(QStringLiteral("Channel and probe metadata"));
   }
  }

  void ArtifactContentsViewer::Impl::updatePlaybackState()
  {
   if (!stateLabel) {
    return;
   }

   QString stateText = QStringLiteral("Idle");
  if (currentFileType == ArtifactCore::FileType::Image) {
    stateText = QStringLiteral("Image preview");
   } else if (currentFileType == ArtifactCore::FileType::Video && mediaPlayer) {
    switch (mediaPlayer->playbackState()) {
    case QMediaPlayer::PlayingState:
     stateText = QStringLiteral("Playing");
     break;
    case QMediaPlayer::PausedState:
     stateText = QStringLiteral("Paused");
     break;
    case QMediaPlayer::StoppedState:
    default:
     stateText = QStringLiteral("Stopped");
     break;
    }
    if (playbackRangeActive) {
     stateText += QStringLiteral(" | Range %1-%2")
                     .arg(formatDurationMs(playbackRangeStartMs))
                     .arg(formatDurationMs(playbackRangeEndMs));
    }
    const qint64 position = mediaPlayer->position();
    const qint64 duration = mediaPlayer->duration();
    if (duration > 0) {
     stateText += QStringLiteral(" | %1 / %2")
                     .arg(formatDurationMs(position))
                     .arg(formatDurationMs(duration));
    }
   } else if (currentFileType == ArtifactCore::FileType::Audio && audioController_ && audioController_->isMediaOpen()) {
    switch (audioController_->getState()) {
    case ArtifactCore::PlaybackState::Playing:
     stateText = QStringLiteral("Playing");
     break;
    case ArtifactCore::PlaybackState::Paused:
     stateText = QStringLiteral("Paused");
     break;
    case ArtifactCore::PlaybackState::Stopped:
    default:
     stateText = QStringLiteral("Stopped");
     break;
    }
    if (playbackRangeActive) {
     stateText += QStringLiteral(" | Range %1-%2")
                     .arg(formatDurationMs(playbackRangeStartMs))
                     .arg(formatDurationMs(playbackRangeEndMs));
    }
    const qint64 duration = audioController_->getDuration();
    if (duration > 0) {
     stateText += QStringLiteral(" | %1 / %2")
                     .arg(formatDurationMs(audioPlaybackPositionMs))
                     .arg(formatDurationMs(duration));
    }
  } else if (currentFileType == ArtifactCore::FileType::Model3D) {
   stateText = QStringLiteral("3D preview");
    if (modelViewer && modelViewer->hasModel()) {
     switch (modelViewer->displayMode()) {
     case Artifact3DModelViewer::DisplayMode::Wireframe:
      stateText += QStringLiteral(" | Wireframe");
      break;
     case Artifact3DModelViewer::DisplayMode::SolidWithWire:
      stateText += QStringLiteral(" | Solid+Wire");
      break;
     case Artifact3DModelViewer::DisplayMode::Solid:
     default:
      stateText += QStringLiteral(" | Solid");
      break;
     }
    } else if (modelViewer) {
     stateText += QStringLiteral(" | Preview unavailable");
     if (!modelViewer->backendName().isEmpty() && modelViewer->backendName() != QStringLiteral("none")) {
      stateText += QStringLiteral(" | %1").arg(modelViewer->backendName());
     }
     if (!modelViewer->lastErrorText().isEmpty()) {
      stateText += QStringLiteral(" | %1").arg(modelViewer->lastErrorText());
     }
    }
   }
   if (currentMode == ContentsViewerMode::Source) {
    stateText += QStringLiteral(" | Source");
   } else if (currentMode == ContentsViewerMode::Final) {
    stateText += QStringLiteral(" | Final");
   } else if (currentMode == ContentsViewerMode::Compare) {
    stateText += QStringLiteral(" | Compare");
   }

   stateLabel->setText(QStringLiteral("State: %1").arg(stateText));

   const bool isVideo = currentFileType == ArtifactCore::FileType::Video;
   const bool isAudio = currentFileType == ArtifactCore::FileType::Audio;
   const bool audioReady = isAudio && audioController_ && audioController_->isMediaOpen();
   if (playButton) {
    playButton->setEnabled(isVideo || audioReady);
   }
   if (pauseButton) {
    pauseButton->setEnabled((isVideo && mediaPlayer && mediaPlayer->playbackState() == QMediaPlayer::PlayingState)
                             || (audioReady && audioController_->getState() == ArtifactCore::PlaybackState::Playing));
   }
   if (stopButton) {
    stopButton->setEnabled((isVideo && mediaPlayer && mediaPlayer->playbackState() != QMediaPlayer::StoppedState)
                            || (audioReady && audioController_->getState() != ArtifactCore::PlaybackState::Stopped));
   }
   if (copyPathButton) {
    copyPathButton->setEnabled(!currentFilePath.isEmpty());
   }
   if (revealButton) {
    revealButton->setEnabled(!currentFilePath.isEmpty());
   }
   if (seekSlider) {
    const bool isSeekable = isVideo || audioReady;
    seekSlider->setEnabled(isSeekable);
    seekSlider->setVisible(isSeekable);
    if (isVideo) {
     if (mediaPlayer) {
      QSignalBlocker blocker(seekSlider);
      seekSlider->setRange(0, static_cast<int>(std::clamp<qint64>(mediaPlayer->duration(), 0, std::numeric_limits<int>::max())));
      seekSlider->setValue(static_cast<int>(std::clamp<qint64>(mediaPlayer->position(), 0, std::numeric_limits<int>::max())));
     }
    } else if (isAudio) {
     if (audioReady) {
      QSignalBlocker blocker(seekSlider);
      seekSlider->setRange(0, static_cast<int>(std::clamp<qint64>(audioController_->getDuration(), 0, std::numeric_limits<int>::max())));
      seekSlider->setValue(static_cast<int>(std::clamp<qint64>(audioPlaybackPositionMs, 0, std::numeric_limits<int>::max())));
     }
    } else {
     QSignalBlocker blocker(seekSlider);
     seekSlider->setRange(0, 0);
     seekSlider->setValue(0);
    }
   }
   if (fitButton) {
    fitButton->setEnabled(true);
   }
   if (resetButton) {
    resetButton->setEnabled(currentFileType == ArtifactCore::FileType::Image
                            || currentFileType == ArtifactCore::FileType::Video
                            || currentFileType == ArtifactCore::FileType::Audio
                            || currentFileType == ArtifactCore::FileType::Model3D);
   }
   if (rotateLeftButton) {
    rotateLeftButton->setEnabled(currentFileType == ArtifactCore::FileType::Image);
   }
   if (rotateRightButton) {
    rotateRightButton->setEnabled(currentFileType == ArtifactCore::FileType::Image);
   }
   if (currentMode == ContentsViewerMode::Compare) {
    updateCompareSurface();
   }
   updateSurfaceMeta();
  }

  void ArtifactContentsViewer::Impl::updateActionAvailability()
  {
   const bool isModel = currentFileType == ArtifactCore::FileType::Model3D;
   if (fitButton) {
    if (isModel) {
     fitButton->setText(QStringLiteral("Reset 3D"));
     fitButton->setToolTip(QStringLiteral("Reset 3D view"));
    } else {
     fitButton->setText(QStringLiteral("Fit"));
     fitButton->setToolTip(QStringLiteral("Fit view"));
    }
   }
   if (sourceButton) sourceButton->setVisible(!isModel);
   if (finalButton) finalButton->setVisible(!isModel);
   if (compareButton) compareButton->setVisible(!isModel);
   if (compareSwapButton) compareSwapButton->setVisible(!isModel && currentMode == ContentsViewerMode::Compare);
   if (compareAssignAButton) compareAssignAButton->setVisible(!isModel && currentMode == ContentsViewerMode::Compare);
   if (compareAssignBButton) compareAssignBButton->setVisible(!isModel && currentMode == ContentsViewerMode::Compare);
   if (compareWipeSlider) compareWipeSlider->setVisible(!isModel && currentMode == ContentsViewerMode::Compare);
   if (fitButton) fitButton->setVisible(isModel || currentFileType == ArtifactCore::FileType::Image);
   if (rotateLeftButton) rotateLeftButton->setVisible(currentFileType == ArtifactCore::FileType::Image);
   if (rotateRightButton) rotateRightButton->setVisible(currentFileType == ArtifactCore::FileType::Image);
   updatePlaybackState();
  }

  void ArtifactContentsViewer::Impl::updateModeButtons()
  {
  if (sourceButton) sourceButton->setChecked(currentMode == ContentsViewerMode::Source);
  if (finalButton) finalButton->setChecked(currentMode == ContentsViewerMode::Final);
  if (compareButton) compareButton->setChecked(currentMode == ContentsViewerMode::Compare);
  if (compareSwapButton) compareSwapButton->setEnabled(currentMode == ContentsViewerMode::Compare);
  if (compareAssignAButton) compareAssignAButton->setEnabled(currentMode == ContentsViewerMode::Compare && !currentFilePath.isEmpty());
  if (compareAssignBButton) compareAssignBButton->setEnabled(currentMode == ContentsViewerMode::Compare && !currentFilePath.isEmpty());
  updateDisplayedPage();
  syncModelViewerMode();
  updateActionAvailability();
 }

  ArtifactContentsViewer::Impl::Impl(ArtifactContentsViewer* parent)
   : owner_(parent)
   , stackedWidget(new QStackedWidget(parent))
  {
   headerWidget = new QWidget(parent);
   auto* headerLayout = new QVBoxLayout(headerWidget);
   headerLayout->setContentsMargins(8, 4, 8, 4);
   headerLayout->setSpacing(3);

   auto* infoRow = new QHBoxLayout();
   infoRow->setContentsMargins(0, 0, 0, 0);
   infoRow->setSpacing(6);

   auto* textColumn = new QVBoxLayout();
   textColumn->setContentsMargins(0, 0, 0, 0);
   textColumn->setSpacing(1);

   titleLabel = new QLabel(QStringLiteral("Contents Viewer"), headerWidget);
   {
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setWeight(QFont::DemiBold);
    titleLabel->setFont(titleFont);
    QPalette pal = titleLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    titleLabel->setPalette(pal);
   }
   recentSourceCombo = new QComboBox(headerWidget);
   {
    QFont comboFont = recentSourceCombo->font();
    comboFont.setPointSize(10);
    recentSourceCombo->setFont(comboFont);
    recentSourceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    recentSourceCombo->setEditable(false);
    recentSourceCombo->setMinimumWidth(200);
    recentSourceCombo->setMaximumWidth(280);
    recentSourceCombo->setToolTip(QStringLiteral("Recent source files"));
    QPalette pal = recentSourceCombo->palette();
    pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
    pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
    pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
    pal.setColor(QPalette::ButtonText, QColor(ArtifactCore::currentDCCTheme().textColor));
    recentSourceCombo->setPalette(pal);
   }
   typeBadgeLabel = new QLabel(QStringLiteral("Idle"), headerWidget);
   {
    QFont badgeFont = typeBadgeLabel->font();
    badgeFont.setPointSize(10);
    typeBadgeLabel->setFont(badgeFont);
    QPalette pal = typeBadgeLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    typeBadgeLabel->setPalette(pal);
   }
   viewerBadgeLabel = new QLabel(QStringLiteral("Viewer 01"), headerWidget);
   {
    QFont badgeFont = viewerBadgeLabel->font();
    badgeFont.setPointSize(10);
    badgeFont.setWeight(QFont::DemiBold);
    viewerBadgeLabel->setFont(badgeFont);
    QPalette pal = viewerBadgeLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    viewerBadgeLabel->setPalette(pal);
   }
   viewerAssignmentCombo = new QComboBox(headerWidget);
   {
    QFont comboFont = viewerAssignmentCombo->font();
    comboFont.setPointSize(10);
    viewerAssignmentCombo->setFont(comboFont);
    viewerAssignmentCombo->setEditable(false);
    viewerAssignmentCombo->setMinimumWidth(120);
    viewerAssignmentCombo->setMaximumWidth(140);
    viewerAssignmentCombo->setToolTip(QStringLiteral("Viewer assignment (Ctrl+1..4)"));
    for (int i = 1; i <= 4; ++i) {
     viewerAssignmentCombo->addItem(QStringLiteral("Viewer %1").arg(i, 2, 10, QChar('0')), i);
    }
    QPalette pal = viewerAssignmentCombo->palette();
    pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
    pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
    pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
    pal.setColor(QPalette::ButtonText, QColor(ArtifactCore::currentDCCTheme().textColor));
    viewerAssignmentCombo->setPalette(pal);
   }
   metaLabel = new QLabel(QStringLiteral("No file selected"), headerWidget);
   {
    QFont metaFont = metaLabel->font();
    metaFont.setPointSize(10);
    metaLabel->setFont(metaFont);
    QPalette pal = metaLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(125));
    metaLabel->setPalette(pal);
   }
   metaLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
   stateLabel = new QLabel(QStringLiteral("State: Idle"), headerWidget);
   {
    QFont stateFont = stateLabel->font();
    stateFont.setPointSize(10);
    stateLabel->setFont(stateFont);
    QPalette pal = stateLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(120));
    stateLabel->setPalette(pal);
   }
   channelMetaLabel = new QLabel(QStringLiteral("RGBA • Hover to probe pixels"), headerWidget);
   {
    QFont channelFont = channelMetaLabel->font();
    channelFont.setPointSize(10);
    channelMetaLabel->setFont(channelFont);
    QPalette pal = channelMetaLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(110));
    channelMetaLabel->setPalette(pal);
   }
   channelMetaLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

   auto* titleRow = new QHBoxLayout();
   titleRow->setContentsMargins(0, 0, 0, 0);
   titleRow->setSpacing(8);
   titleRow->addWidget(titleLabel, 1);
   titleRow->addWidget(recentSourceCombo, 0, Qt::AlignRight);
   titleRow->addWidget(viewerAssignmentCombo, 0, Qt::AlignRight);

   textColumn->addLayout(titleRow);
   textColumn->addWidget(metaLabel);
   textColumn->addWidget(stateLabel);
   textColumn->addWidget(channelMetaLabel);

   auto* badgeColumn = new QVBoxLayout();
   badgeColumn->setContentsMargins(0, 0, 0, 0);
   badgeColumn->setSpacing(2);
   badgeColumn->addWidget(viewerBadgeLabel, 0, Qt::AlignLeft);
   badgeColumn->addWidget(typeBadgeLabel, 0, Qt::AlignLeft);
   badgeColumn->addStretch(1);

   auto* buttonRow = new QHBoxLayout();
   buttonRow->setContentsMargins(0, 0, 0, 0);
   buttonRow->setSpacing(2);

   auto createButton = [parent](const QString& text, const QString& tooltip) {
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tooltip);
    button->setAutoRaise(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(22);
    button->setMinimumWidth(0);
    return button;
   };

   fitButton = createButton(QStringLiteral("Fit"), QStringLiteral("Fit view"));
   rotateLeftButton = createButton(QStringLiteral("⟲"), QStringLiteral("Rotate left"));
   rotateRightButton = createButton(QStringLiteral("⟳"), QStringLiteral("Rotate right"));
   resetButton = createButton(QStringLiteral("Reset"), QStringLiteral("Reset view state"));
   playButton = createButton(QStringLiteral("Play"), QStringLiteral("Play media"));
   pauseButton = createButton(QStringLiteral("Pause"), QStringLiteral("Pause media"));
   stopButton = createButton(QStringLiteral("Stop"), QStringLiteral("Stop media"));
   copyPathButton = createButton(QStringLiteral("Copy"), QStringLiteral("Copy file path"));
   revealButton = createButton(QStringLiteral("Open"), QStringLiteral("Open containing folder"));
   sourceButton = createButton(QStringLiteral("Source"), QStringLiteral("Show source view"));
   finalButton = createButton(QStringLiteral("Final"), QStringLiteral("Show final output view"));
   compareButton = createButton(QStringLiteral("Compare"), QStringLiteral("Show compare view"));
   compareSwapButton = createButton(QStringLiteral("Swap"), QStringLiteral("Swap compare sides"));
   sourceButton->setCheckable(true);
   finalButton->setCheckable(true);
   compareButton->setCheckable(true);
   seekSlider = new QSlider(Qt::Horizontal, parent);
   seekSlider->setRange(0, 0);
   seekSlider->setSingleStep(1000);
   seekSlider->setPageStep(5000);
   seekSlider->setTracking(true);
   seekSlider->setVisible(false);
   seekSlider->setToolTip(QStringLiteral("Scrub media position"));

   buttonRow->addWidget(fitButton);
   buttonRow->addWidget(rotateLeftButton);
   buttonRow->addWidget(rotateRightButton);
   buttonRow->addWidget(resetButton);
   buttonRow->addSpacing(4);
   buttonRow->addWidget(playButton);
   buttonRow->addWidget(pauseButton);
   buttonRow->addWidget(stopButton);
   buttonRow->addSpacing(4);
   buttonRow->addWidget(copyPathButton);
   buttonRow->addWidget(revealButton);
   buttonRow->addSpacing(4);
   buttonRow->addWidget(sourceButton);
   buttonRow->addWidget(finalButton);
   buttonRow->addWidget(compareButton);
   buttonRow->addWidget(compareSwapButton);
   compareAssignAButton = createButton(QStringLiteral("A"), QStringLiteral("Assign current source to compare A"));
   compareAssignBButton = createButton(QStringLiteral("B"), QStringLiteral("Assign current source to compare B"));
   buttonRow->addWidget(compareAssignAButton);
   buttonRow->addWidget(compareAssignBButton);

   auto* surfaceMetaRow = new QHBoxLayout();
   surfaceMetaRow->setContentsMargins(0, 0, 0, 0);
   surfaceMetaRow->setSpacing(6);
   surfaceMetaLabel = new QLabel(QStringLiteral("Viewer • Idle"), headerWidget);
   {
    QFont surfaceFont = surfaceMetaLabel->font();
    surfaceFont.setPointSize(10);
    surfaceMetaLabel->setFont(surfaceFont);
    QPalette pal = surfaceMetaLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    surfaceMetaLabel->setPalette(pal);
   }
   surfaceMetaRow->addWidget(surfaceMetaLabel, 1);

   infoRow->addLayout(textColumn, 1);
   infoRow->addLayout(badgeColumn, 0);
   headerLayout->addLayout(infoRow);
   headerLayout->addLayout(buttonRow);
   headerLayout->addLayout(surfaceMetaRow);

    QObject::connect(fitButton, &QToolButton::clicked, parent, [this]() {
     if (currentFileType == ArtifactCore::FileType::Image) {
      fitImageToWindow();
     } else if (currentFileType == ArtifactCore::FileType::Model3D && modelViewer) {
      modelViewer->resetView();
     }
     updateHeader();
     updatePlaybackState();
    });

   QObject::connect(rotateLeftButton, &QToolButton::clicked, parent, [this]() {
    if (currentFileType == ArtifactCore::FileType::Image) {
     if (owner_) {
      owner_->rotateLeft();
     }
     updateHeader();
     updatePlaybackState();
    }
   });

   QObject::connect(rotateRightButton, &QToolButton::clicked, parent, [this]() {
    if (currentFileType == ArtifactCore::FileType::Image) {
     if (owner_) {
      owner_->rotateRight();
     }
     updateHeader();
     updatePlaybackState();
    }
   });

   QObject::connect(resetButton, &QToolButton::clicked, parent, [this]() {
    switch (currentFileType) {
    case ArtifactCore::FileType::Image:
     zoomLevel = 1.0;
     rotationDegrees = 0.0;
     if (imageLabel && !originalImage.isNull()) {
      imageLabel->setPixmap(originalImage);
      imageLabel->setFixedSize(originalImage.size());
     }
     if (imageScrollArea) {
      if (auto* hBar = imageScrollArea->horizontalScrollBar()) {
       hBar->setValue(0);
      }
      if (auto* vBar = imageScrollArea->verticalScrollBar()) {
       vBar->setValue(0);
      }
     }
     break;
    case ArtifactCore::FileType::Video:
     if (mediaPlayer) {
      mediaPlayer->stop();
      mediaPlayer->setPosition(0);
     }
     break;
    case ArtifactCore::FileType::Model3D:
     if (modelViewer) {
      modelViewer->resetView();
     }
     break;
    default:
     break;
    }
    updateHeader();
    updatePlaybackState();
    updateActionAvailability();
   });

   QObject::connect(playButton, &QToolButton::clicked, parent, [this]() {
    if (owner_) {
     owner_->play();
    }
    updatePlaybackState();
   });

   QObject::connect(pauseButton, &QToolButton::clicked, parent, [this]() {
    if (owner_) {
     owner_->pause();
    }
    updatePlaybackState();
   });

   QObject::connect(stopButton, &QToolButton::clicked, parent, [this]() {
    if (owner_) {
     owner_->stop();
    }
    updatePlaybackState();
   });

   QObject::connect(copyPathButton, &QToolButton::clicked, parent, [this]() {
    if (currentFilePath.isEmpty()) {
     return;
    }
    QGuiApplication::clipboard()->setText(currentFilePath);
    updateActionAvailability();
   });

   QObject::connect(revealButton, &QToolButton::clicked, parent, [this]() {
    if (currentFilePath.isEmpty()) {
     return;
    }
    const QFileInfo info(currentFilePath);
    const QString target = info.exists() ? info.absoluteFilePath() : currentFilePath;
    const QString folder = QFileInfo(target).absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    updateActionAvailability();
   });

   QObject::connect(recentSourceCombo, qOverload<int>(&QComboBox::activated), parent,
                    [this](int index) {
                     if (!recentSourceCombo || index < 0 || index >= recentSourceCombo->count()) {
                      return;
                     }
                     const QString path =
                         recentSourceCombo->itemData(index, Qt::UserRole).toString();
                     if (path.isEmpty() || path == currentFilePath) {
                      return;
                     }
                     if (owner_) {
                      owner_->setFilePath(path);
                     }
                    });

   QObject::connect(viewerAssignmentCombo, qOverload<int>(&QComboBox::activated), parent,
                    [this](int index) {
                     if (!viewerAssignmentCombo || index < 0 || index >= viewerAssignmentCombo->count()) {
                      return;
                     }
                     const int slot = viewerAssignmentCombo->itemData(index, Qt::UserRole).toInt();
                     if (slot > 0) {
                      if (owner_) {
                       owner_->setViewerAssignment(slot);
                      }
                      updateViewerBadge();
                     }
                    });

   QObject::connect(sourceButton, &QToolButton::clicked, parent, [this]() {
    currentMode = ContentsViewerMode::Source;
    updateHeader();
    updatePlaybackState();
    updateModeButtons();
    syncModelViewerMode();
   });
   QObject::connect(finalButton, &QToolButton::clicked, parent, [this]() {
    currentMode = ContentsViewerMode::Final;
    updateHeader();
    updatePlaybackState();
    updateModeButtons();
    syncModelViewerMode();
   });
   QObject::connect(compareButton, &QToolButton::clicked, parent, [this]() {
    currentMode = ContentsViewerMode::Compare;
    updateHeader();
    updatePlaybackState();
    updateModeButtons();
    syncModelViewerMode();
   });
   QObject::connect(compareSwapButton, &QToolButton::clicked, parent, [this]() {
    swapCompareSides();
   });
   QObject::connect(compareAssignAButton, &QToolButton::clicked, parent, [this]() {
    assignCompareSource(true);
   });
   QObject::connect(compareAssignBButton, &QToolButton::clicked, parent, [this]() {
    assignCompareSource(false);
   });

   auto* compareSwapShortcut = new QShortcut(QKeySequence(Qt::Key_Tab), parent);
   compareSwapShortcut->setContext(Qt::WidgetWithChildrenShortcut);
   compareSwapShortcut->setAutoRepeat(false);
   QObject::connect(compareSwapShortcut, &QShortcut::activated, parent, [this]() {
    if (currentMode == ContentsViewerMode::Compare) {
     swapCompareSides();
    }
   });

   auto installViewerAssignmentShortcut = [parent, this](int slot) {
    auto* shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(slot)), parent);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    shortcut->setAutoRepeat(false);
    QObject::connect(shortcut, &QShortcut::activated, parent, [this, slot]() {
     if (owner_) {
      owner_->setViewerAssignment(slot);
     }
     updateViewerBadge();
     updateSurfaceMeta();
    });
   };
   installViewerAssignmentShortcut(1);
   installViewerAssignmentShortcut(2);
   installViewerAssignmentShortcut(3);
   installViewerAssignmentShortcut(4);

   auto* assignCompareAShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+A")), parent);
   assignCompareAShortcut->setContext(Qt::WidgetWithChildrenShortcut);
   assignCompareAShortcut->setAutoRepeat(false);
   QObject::connect(assignCompareAShortcut, &QShortcut::activated, parent, [this]() {
    if (currentMode == ContentsViewerMode::Compare) {
     assignCompareSource(true);
    }
   });

   auto* assignCompareBShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+B")), parent);
   assignCompareBShortcut->setContext(Qt::WidgetWithChildrenShortcut);
   assignCompareBShortcut->setAutoRepeat(false);
   QObject::connect(assignCompareBShortcut, &QShortcut::activated, parent, [this]() {
    if (currentMode == ContentsViewerMode::Compare) {
     assignCompareSource(false);
    }
   });

   QObject::connect(seekSlider, &QSlider::sliderMoved, parent, [this](int value) {
    if (currentFileType == ArtifactCore::FileType::Video && mediaPlayer) {
     mediaPlayer->setPosition(static_cast<qint64>(value));
     updatePlaybackState();
     return;
    }
    if (currentFileType == ArtifactCore::FileType::Audio && audioController_ && audioController_->isMediaOpen()) {
     const bool wasPlaying = audioController_->getState() == ArtifactCore::PlaybackState::Playing;
     audioController_->seek(static_cast<qint64>(value));
     audioPlaybackPositionMs = static_cast<qint64>(value);
     audioPendingBuffer.clear();
     if (audioSink && audioSink->state() == QAudio::StoppedState) {
      audioSinkDevice = audioSink->start();
     }
     if (audioSink) {
      if (wasPlaying) {
       audioSink->resume();
      } else {
       audioSink->suspend();
      }
     }
     if (audioPumpTimer) {
      if (wasPlaying) {
       audioPumpTimer->start(16);
      } else {
       audioPumpTimer->stop();
      }
     }
     if (wasPlaying) {
      pumpAudioPlayback();
     }
     updatePlaybackState();
     return;
    }
    Q_UNUSED(value);
    updatePlaybackState();
   });

   // Image Viewer Setup
   imageScrollArea = new QScrollArea();
   imageLabel = new QLabel();
   imageLabel->setAlignment(Qt::AlignCenter);
   imageLabel->setScaledContents(true); // Allow manual scaling via setFixedSize
   
   imageScrollArea->setWidget(imageLabel);
   imageScrollArea->setWidgetResizable(false); // We want to control sizing based on zoom
   imageScrollArea->setAlignment(Qt::AlignCenter);
   imageScrollArea->viewport()->setMouseTracking(true);
   imageLabel->setMouseTracking(true);
   {
    QPalette scrollPalette = imageScrollArea->palette();
    scrollPalette.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
    scrollPalette.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
    imageScrollArea->setPalette(scrollPalette);
   }
   imageScrollArea->setAutoFillBackground(true);

   // Info/Message View Setup
   infoLabel = new QLabel();
   infoLabel->setAlignment(Qt::AlignCenter);
   infoLabel->setWordWrap(true);
   {
    QFont infoFont = infoLabel->font();
    infoFont.setPointSize(14);
    infoLabel->setFont(infoFont);
    QPalette pal = infoLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(120));
    infoLabel->setPalette(pal);
   }

   stackedWidget->addWidget(imageScrollArea);
   stackedWidget->addWidget(infoLabel);
   ensureCompareWidgets();
   loadRecentSources();
   loadViewerAssignmentState();
   updateViewerBadge();
   if (viewerAssignmentCombo) {
    QSignalBlocker blocker(viewerAssignmentCombo);
    viewerAssignmentCombo->setCurrentIndex(std::clamp(viewerAssignment, 1, 4) - 1);
   }

   if (compareAssignAButton) {
    compareAssignAButton->setToolTip(QStringLiteral("Assign current source to compare A (Ctrl+Shift+A)"));
   }
   if (compareAssignBButton) {
    compareAssignBButton->setToolTip(QStringLiteral("Assign current source to compare B (Ctrl+Shift+B)"));
   }
   if (compareSwapButton) {
    compareSwapButton->setToolTip(QStringLiteral("Swap compare sides (Tab)"));
   }

   auto layout = new QVBoxLayout(parent);
   layout->setContentsMargins(0, 0, 0, 0);
   layout->setSpacing(0);
   layout->addWidget(headerWidget);
   layout->addWidget(seekSlider);
   layout->addWidget(stackedWidget);
   parent->setLayout(layout);

   updateHeader();
   updatePlaybackState();
   updateActionAvailability();
  }

  ArtifactContentsViewer::Impl::~Impl()
 {
  if (mediaPlayer) {
   QSignalBlocker blocker(mediaPlayer);
   detachMediaOutputs();
   mediaPlayer->stop();
   mediaPlayer->setSource(QUrl());
  }
  // Widgets are parented, will be deleted by the QObject hierarchy.
 }

  void ArtifactContentsViewer::Impl::attachMediaOutputs()
  {
   if (!mediaPlayer) {
    return;
   }
   mediaPlayer->setAudioOutput(audioOutput);
   mediaPlayer->setVideoOutput(videoWidget);
  }

  void ArtifactContentsViewer::Impl::detachMediaOutputs()
  {
   if (!mediaPlayer) {
    return;
   }
   mediaPlayer->setAudioOutput(nullptr);
   mediaPlayer->setVideoOutput(nullptr);
  }

  void ArtifactContentsViewer::Impl::installEventFilters()
  {
   if (eventFiltersInstalled) {
    return;
   }

   if (imageScrollArea && imageScrollArea->viewport()) {
    imageScrollArea->viewport()->installEventFilter(owner_);
   }
   if (imageLabel) {
    imageLabel->installEventFilter(owner_);
   }
   if (compareSourceHeader) {
    compareSourceHeader->installEventFilter(owner_);
   }
   if (compareFinalHeader) {
    compareFinalHeader->installEventFilter(owner_);
   }

   eventFiltersInstalled = true;
  }

  bool ArtifactContentsViewer::eventFilter(QObject* watched, QEvent* event)
  {
   if (!impl_) {
    return QWidget::eventFilter(watched, event);
   }

   if ((watched == impl_->compareSourceHeader || watched == impl_->compareFinalHeader) &&
       event->type() == QEvent::MouseButtonRelease) {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
     const bool isLeft = watched == impl_->compareSourceHeader;
     impl_->openCompareSource(isLeft);
     return true;
    }
   }

   if (impl_->currentFileType == ArtifactCore::FileType::Image && impl_->imageLabel && impl_->imageScrollArea) {
    const bool matchesViewport = watched == impl_->imageScrollArea->viewport();
    const bool matchesLabel = watched == impl_->imageLabel;
    if (matchesViewport || matchesLabel) {
     switch (event->type()) {
     case QEvent::MouseMove: {
      auto* mouseEvent = static_cast<QMouseEvent*>(event);
      const QPoint localPos = matchesLabel
                                  ? mouseEvent->position().toPoint()
                                  : impl_->imageLabel->mapFrom(impl_->imageScrollArea->viewport(),
                                                              mouseEvent->position().toPoint());
      const QPixmap pixmap = impl_->imageLabel->pixmap();
      if (!pixmap.isNull()) {
       const QPoint clampedPos(
           std::clamp(localPos.x(), 0, std::max(0, pixmap.width() - 1)),
           std::clamp(localPos.y(), 0, std::max(0, pixmap.height() - 1)));
       if (clampedPos.x() >= 0 && clampedPos.y() >= 0) {
        impl_->hoverProbeValid = true;
        impl_->hoverProbePos = clampedPos;
        impl_->hoverProbeColor = pixmap.toImage().pixelColor(clampedPos);
        impl_->updateChannelMetaSurface();
       }
      }
      break;
     }
     case QEvent::Leave:
      impl_->hoverProbeValid = false;
      impl_->updateChannelMetaSurface();
      break;
     default:
      break;
     }
    }
   }

   return QWidget::eventFilter(watched, event);
  }
	
	W_OBJECT_IMPL(ArtifactContentsViewer)

ArtifactContentsViewer::ArtifactContentsViewer(QWidget* parent/*=nullptr*/) :QWidget(parent), impl_(new Impl(this))
{
 setFocusPolicy(Qt::StrongFocus);
 impl_->installEventFilters();
}

 ArtifactContentsViewer::~ArtifactContentsViewer()
 {
  delete impl_;
 }

  void ArtifactContentsViewer::setFilePath(const QString& filepath)
  {
   impl_->currentFilePath = filepath;
   impl_->rememberRecentSource(filepath);
   ArtifactCore::FileTypeDetector detector;
   impl_->currentFileType = detector.detect(filepath);
   impl_->hoverProbeValid = false;
   impl_->resetCurrentMode();

   switch (impl_->currentFileType) {
   case ArtifactCore::FileType::Image:
    impl_->activateImage(filepath);
    break;
   case ArtifactCore::FileType::Video:
    impl_->activateVideo(filepath);
    break;
   case ArtifactCore::FileType::Audio:
    impl_->activateAudio(filepath);
    break;
   case ArtifactCore::FileType::Model3D:
    impl_->activateModel(filepath);
    break;
   default:
    impl_->showInfoMessage("Unsupported Content:\n" + filepath);
    break;
   }
   impl_->updateHeader();
   impl_->updatePlaybackState();
   impl_->updateActionAvailability();
  }

  void ArtifactContentsViewer::setViewerMode(ContentsViewerMode mode)
  {
   if (!impl_) {
    return;
   }
   impl_->currentMode = mode;
   impl_->syncModelViewerMode();
   impl_->updateHeader();
   impl_->updatePlaybackState();
   impl_->updateActionAvailability();
   impl_->updateModeButtons();
  }

  void ArtifactContentsViewer::setViewerAssignment(int viewerIndex)
  {
   if (!impl_) {
    return;
   }
   impl_->viewerAssignment = std::clamp(viewerIndex, 1, 4);
   impl_->saveViewerAssignmentState();
   if (impl_->viewerAssignmentCombo) {
    QSignalBlocker blocker(impl_->viewerAssignmentCombo);
    impl_->viewerAssignmentCombo->setCurrentIndex(impl_->viewerAssignment - 1);
   }
   impl_->updateViewerBadge();
   impl_->updateSurfaceMeta();
  }

  int ArtifactContentsViewer::viewerAssignment() const
  {
   return impl_ ? impl_->viewerAssignment : 0;
  }

  void ArtifactContentsViewer::assignCompareSourceA()
  {
   if (impl_) {
    impl_->assignCompareSource(true);
   }
  }

  void ArtifactContentsViewer::assignCompareSourceB()
  {
   if (impl_) {
    impl_->assignCompareSource(false);
   }
  }

  void ArtifactContentsViewer::focusInEvent(QFocusEvent* event)
  {
   if (impl_) {
    impl_->viewerHasFocus = true;
    impl_->updateViewerBadge();
   }
   QWidget::focusInEvent(event);
  }

  void ArtifactContentsViewer::focusOutEvent(QFocusEvent* event)
  {
   if (impl_) {
    impl_->viewerHasFocus = false;
    impl_->updateViewerBadge();
   }
   QWidget::focusOutEvent(event);
  }

  void ArtifactContentsViewer::wheelEvent(QWheelEvent* event)
  {
      if (impl_->currentFileType == ArtifactCore::FileType::Image &&
          impl_->stackedWidget->currentWidget() == impl_->imageScrollArea &&
          !impl_->originalImage.isNull()) {
          const double scaleFactor = 1.15;
          if (event->angleDelta().y() > 0) {
              impl_->zoomLevel *= scaleFactor;
          } else {
              impl_->zoomLevel /= scaleFactor;
          }
          
          impl_->zoomLevel = qBound(0.05, impl_->zoomLevel, 10.0);
          impl_->imageFitMode = false;
          impl_->applyImageTransform();
          event->accept();
          return;
      }
      QWidget::wheelEvent(event);
  }

  void ArtifactContentsViewer::resizeEvent(QResizeEvent* event)
  {
      if (impl_ && impl_->currentFileType == ArtifactCore::FileType::Image && impl_->imageFitMode) {
          if (impl_->currentMode == ContentsViewerMode::Compare) {
              impl_->fitImageToWindow();
              impl_->updateCompareSurface();
          } else if (impl_->stackedWidget && impl_->stackedWidget->currentWidget() == impl_->imageScrollArea) {
              impl_->fitImageToWindow();
          }
      }
      QWidget::resizeEvent(event);
  }

  void ArtifactContentsViewer::mousePressEvent(QMouseEvent* event)
  {
      setFocus(Qt::MouseFocusReason);
      if (impl_->currentFileType == ArtifactCore::FileType::Image &&
          impl_->stackedWidget->currentWidget() == impl_->imageScrollArea &&
          (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
          impl_->lastMousePos = event->pos();
          setCursor(Qt::ClosedHandCursor);
          event->accept();
          return;
      }
      QWidget::mousePressEvent(event);
  }

  void ArtifactContentsViewer::mouseMoveEvent(QMouseEvent* event)
  {
      if (impl_->currentFileType == ArtifactCore::FileType::Image &&
          impl_->stackedWidget->currentWidget() == impl_->imageScrollArea &&
          (event->buttons() & Qt::LeftButton || event->buttons() & Qt::MiddleButton)) {
          QPoint delta = event->pos() - impl_->lastMousePos;
          impl_->lastMousePos = event->pos();
          
          auto* hBar = impl_->imageScrollArea->horizontalScrollBar();
          auto* vBar = impl_->imageScrollArea->verticalScrollBar();
          hBar->setValue(hBar->value() - delta.x());
          vBar->setValue(vBar->value() - delta.y());
          event->accept();
          return;
      }
      QWidget::mouseMoveEvent(event);
  }

  void ArtifactContentsViewer::mouseReleaseEvent(QMouseEvent* event)
  {
      setCursor(Qt::ArrowCursor);
      QWidget::mouseReleaseEvent(event);
  }

void ArtifactContentsViewer::play()
{
  if (impl_->currentFileType == ArtifactCore::FileType::Image) {
   qDebug() << "ArtifactContentsViewer::play image (static preview):" << impl_->currentFilePath;
   impl_->updatePlaybackState();
   return;
  }

  if (impl_->currentFileType == ArtifactCore::FileType::Video) {
   impl_->ensureVideoWidgets();
   impl_->attachMediaOutputs();
   if (impl_->mediaPlayer->source().isEmpty() && !impl_->currentFilePath.isEmpty()) {
    impl_->mediaPlayer->setSource(QUrl::fromLocalFile(impl_->currentFilePath));
   }
   if (impl_->playbackRangeActive) {
    impl_->mediaPlayer->setPosition(impl_->playbackRangeStartMs);
   }
   impl_->stackedWidget->setCurrentWidget(impl_->videoWidget);
   impl_->mediaPlayer->play();
   impl_->updatePlaybackState();
   return;
  }

  if (impl_->currentFileType == ArtifactCore::FileType::Audio) {
   if (!impl_->audioController_ || !impl_->audioController_->isMediaOpen()) {
    impl_->showInfoMessage("Cannot play unsupported content.\n" + impl_->currentFilePath);
    return;
   }
   if (impl_->audioController_->getState() == ArtifactCore::PlaybackState::Stopped) {
    impl_->audioController_->seek(impl_->playbackRangeActive ? impl_->playbackRangeStartMs : 0);
   }
   if (impl_->audioSink && impl_->audioSink->state() == QAudio::SuspendedState) {
    impl_->audioSink->resume();
   }
   if (impl_->audioSink && impl_->audioSink->state() == QAudio::StoppedState) {
    impl_->audioSinkDevice = impl_->audioSink->start();
   }
   impl_->audioController_->play();
   if (impl_->audioPumpTimer) {
    impl_->audioPumpTimer->start(16);
   }
   if (impl_->stackedWidget && impl_->audioPage) {
    impl_->stackedWidget->setCurrentWidget(impl_->audioPage);
   }
   impl_->pumpAudioPlayback();
   impl_->updatePlaybackState();
   return;
  }

  impl_->showInfoMessage("Cannot play unsupported content.\n" + impl_->currentFilePath);
}

void ArtifactContentsViewer::pause()
{
  if (impl_->currentFileType == ArtifactCore::FileType::Video) {
   impl_->attachMediaOutputs();
   impl_->mediaPlayer->pause();
   impl_->updatePlaybackState();
   return;
  }

  if (impl_->currentFileType == ArtifactCore::FileType::Audio) {
   if (!impl_->audioController_ || !impl_->audioController_->isMediaOpen()) {
    return;
   }
   if (impl_->audioPumpTimer) {
    impl_->audioPumpTimer->stop();
   }
   if (impl_->audioSink) {
    impl_->audioSink->suspend();
   }
   if (impl_->audioController_) {
    impl_->audioController_->pause();
   }
   if (impl_->stackedWidget && impl_->audioPage) {
    impl_->stackedWidget->setCurrentWidget(impl_->audioPage);
   }
   impl_->updatePlaybackState();
   return;
  }

  qDebug() << "ArtifactContentsViewer::pause no-op for current type:" << static_cast<int>(impl_->currentFileType);
}

void ArtifactContentsViewer::stop()
{
  if (impl_->currentFileType == ArtifactCore::FileType::Image && !impl_->imageLabel->pixmap().isNull()) {
   impl_->zoomLevel = 1.0;
   impl_->imageFitMode = true;
   impl_->imageLabel->setFixedSize(impl_->imageLabel->pixmap().size());
   impl_->updateHeader();
   impl_->updatePlaybackState();
   return;
  }

  if (impl_->currentFileType == ArtifactCore::FileType::Video) {
   impl_->attachMediaOutputs();
   impl_->mediaPlayer->stop();
   if (impl_->playbackRangeActive) {
    impl_->mediaPlayer->setPosition(impl_->playbackRangeStartMs);
   } else {
    impl_->mediaPlayer->setPosition(0);
   }
   impl_->updatePlaybackState();
   return;
  }

  if (impl_->currentFileType == ArtifactCore::FileType::Audio) {
   if (!impl_->audioController_ || !impl_->audioController_->isMediaOpen()) {
    return;
   }
   if (impl_->audioPumpTimer) {
    impl_->audioPumpTimer->stop();
   }
   if (impl_->audioSink) {
    impl_->audioSink->stop();
   }
   if (impl_->audioController_) {
    impl_->audioController_->stop();
    impl_->audioController_->seek(impl_->playbackRangeActive ? impl_->playbackRangeStartMs : 0);
   }
   impl_->audioPendingBuffer.clear();
   impl_->audioPlaybackPositionMs = impl_->playbackRangeActive ? impl_->playbackRangeStartMs : 0;
   impl_->resetAudioWaveform();
   if (impl_->seekSlider) {
    QSignalBlocker blocker(impl_->seekSlider);
    impl_->seekSlider->setValue(static_cast<int>(std::clamp<qint64>(impl_->audioPlaybackPositionMs, 0, std::numeric_limits<int>::max())));
   }
   if (impl_->stackedWidget && impl_->audioPage) {
    impl_->stackedWidget->setCurrentWidget(impl_->audioPage);
   }
   impl_->updatePlaybackState();
   return;
  }

  qDebug() << "ArtifactContentsViewer::stop no-op for current type:" << static_cast<int>(impl_->currentFileType);
}

void ArtifactContentsViewer::playRange(int64_t start, int64_t end)
{
  if (impl_->currentFileType == ArtifactCore::FileType::Video) {
   impl_->ensureVideoWidgets();
   impl_->setPlaybackRange(start, end);
   impl_->attachMediaOutputs();
   if (impl_->mediaPlayer->source().isEmpty() && !impl_->currentFilePath.isEmpty()) {
    impl_->mediaPlayer->setSource(QUrl::fromLocalFile(impl_->currentFilePath));
   }
   impl_->stackedWidget->setCurrentWidget(impl_->videoWidget);
   impl_->mediaPlayer->setPosition(impl_->playbackRangeStartMs);
   impl_->mediaPlayer->play();
   return;
  }

  if (impl_->currentFileType == ArtifactCore::FileType::Audio) {
   if (!impl_->audioController_ || !impl_->audioController_->isMediaOpen()) {
    return;
   }
   impl_->setPlaybackRange(start, end);
   if (impl_->audioController_) {
    impl_->audioController_->seek(impl_->playbackRangeStartMs);
   }
   play();
   return;
  }

  play();
}

void ArtifactContentsViewer::rotateLeft()
{
 if (impl_->currentFileType != ArtifactCore::FileType::Image || impl_->originalImage.isNull()) {
  return;
 }

 impl_->rotationDegrees -= 90.0;
 impl_->imageFitMode = false;
 QTransform transform;
 transform.rotate(impl_->rotationDegrees);
 const QPixmap rotated = impl_->originalImage.transformed(transform, Qt::SmoothTransformation);
 impl_->imageLabel->setPixmap(rotated);
 impl_->imageLabel->setFixedSize(rotated.size() * impl_->zoomLevel);
 impl_->updateHeader();
 if (impl_->currentMode == ContentsViewerMode::Compare) {
  impl_->updateCompareSurface();
 }
}

void ArtifactContentsViewer::rotateRight()
{
 if (impl_->currentFileType != ArtifactCore::FileType::Image || impl_->originalImage.isNull()) {
  return;
 }

 impl_->rotationDegrees += 90.0;
 impl_->imageFitMode = false;
 QTransform transform;
 transform.rotate(impl_->rotationDegrees);
 const QPixmap rotated = impl_->originalImage.transformed(transform, Qt::SmoothTransformation);
 impl_->imageLabel->setPixmap(rotated);
 impl_->imageLabel->setFixedSize(rotated.size() * impl_->zoomLevel);
 impl_->updateHeader();
 if (impl_->currentMode == ContentsViewerMode::Compare) {
  impl_->updateCompareSurface();
 }
}

void ArtifactContentsViewer::resetView()
{
 if (impl_->currentFileType != ArtifactCore::FileType::Image || impl_->originalImage.isNull()) {
  return;
 }

 impl_->zoomLevel = 1.0;
 impl_->rotationDegrees = 0.0;
 impl_->imageFitMode = true;
 impl_->imageLabel->setPixmap(impl_->originalImage);
 impl_->imageLabel->setFixedSize(impl_->originalImage.size());
 impl_->updateHeader();
 if (impl_->currentMode == ContentsViewerMode::Compare) {
  impl_->updateCompareSurface();
 }
}

};
