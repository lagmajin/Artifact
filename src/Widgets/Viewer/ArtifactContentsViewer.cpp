module;

#include <QWidget>
#include <QFrame>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QClipboard>
#include <QToolButton>
#include <QSlider>
#include <wobjectimpl.h>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QPixmap>
#include <QScrollArea>
#include <QStyle>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QDebug>
#include <QFileInfo>
#include <QUrl>
#include <QSignalBlocker>
#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QIODevice>
#include <QTimer>
#include <QTransform>

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
   void updateSurfaceMeta();
   void updatePlaybackState();
   void updateActionAvailability();
   void updateModeButtons();
   void clearPlaybackRange();
   void setPlaybackRange(int64_t startFrame, int64_t endFrame);
   void resetCurrentMode();
   void ensureVideoWidgets();
   void ensureAudioController();
   void releaseAudioPlayback();
   void activateAudio(const QString& filepath);
   void pumpAudioPlayback();
   void ensureModelViewer();
   void syncModelViewerMode();
   void attachMediaOutputs();
   void detachMediaOutputs();
   void activateImage(const QString& filepath);
   void activateVideo(const QString& filepath);
   void activateModel(const QString& filepath);
   void fitImageToWindow();
   void applyImageTransform();

   static qint64 framesToMs(int64_t frame);
   static QString humanFileSize(qint64 bytes);
   static QString formatDurationMs(qint64 ms);

   QWidget* headerWidget = nullptr;
   QLabel* titleLabel = nullptr;
   QLabel* typeBadgeLabel = nullptr;
   QLabel* viewerBadgeLabel = nullptr;
   QLabel* metaLabel = nullptr;
   QLabel* stateLabel = nullptr;
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
   QLabel* infoLabel = nullptr;
   Artifact3DModelViewer* modelViewer = nullptr;
   bool videoWidgetsReady = false;
   bool modelViewerReady = false;
   bool audioWidgetsReady = false;
   QString currentFilePath;
   ArtifactCore::FileType currentFileType = ArtifactCore::FileType::Unknown;
   ContentsViewerMode currentMode = ContentsViewerMode::Source;
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

  void ArtifactContentsViewer::Impl::releaseAudioPlayback()
  {
   audioPendingBuffer.clear();
   audioPlaybackPositionMs = 0;

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
   if (infoLabel) {
    infoLabel->setText(QStringLiteral("Audio preview ready\n%1").arg(info.absoluteFilePath()));
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
    audioPendingBuffer.remove(0, static_cast<int>(written));
    audioPlaybackPositionMs += audioBytesToMs(written);
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
   stackedWidget->addWidget(modelViewer);
   modelViewerReady = true;
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
   stackedWidget->setCurrentWidget(imageScrollArea);
   fitImageToWindow();
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
   stackedWidget->setCurrentWidget(videoWidget);
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
   modelViewer->loadModel(ArtifactCore::UniString(filepath.toStdString()));
   syncModelViewerMode();
   stackedWidget->setCurrentWidget(modelViewer);
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
    modelViewer->setDisplayMode(Artifact3DModelViewer::DisplayMode::Wireframe);
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
    if (!suffix.isEmpty()) {
     metaParts.prepend(QStringLiteral("%1 model").arg(suffix));
    } else {
     metaParts.prepend(QStringLiteral("3D viewer"));
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

  void ArtifactContentsViewer::Impl::updateSurfaceMeta()
  {
   if (!surfaceMetaLabel) {
    return;
   }

   QStringList chips;
   chips << QStringLiteral("Viewer");
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
    chips << QStringLiteral("3D");
    if (modelViewer) {
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
   }

   surfaceMetaLabel->setText(chips.join(QStringLiteral("  •  ")));
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
    stateText = QStringLiteral("3D model inspect");
    if (modelViewer) {
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
   updateSurfaceMeta();
  }

  void ArtifactContentsViewer::Impl::updateActionAvailability()
  {
   const bool isModel = currentFileType == ArtifactCore::FileType::Model3D;
   if (sourceButton) sourceButton->setVisible(!isModel);
   if (finalButton) finalButton->setVisible(!isModel);
   if (compareButton) compareButton->setVisible(!isModel);
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
   syncModelViewerMode();
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

   textColumn->addWidget(titleLabel);
   textColumn->addWidget(metaLabel);
   textColumn->addWidget(stateLabel);

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
	
	W_OBJECT_IMPL(ArtifactContentsViewer)

 ArtifactContentsViewer::ArtifactContentsViewer(QWidget* parent/*=nullptr*/) :QWidget(parent), impl_(new Impl(this))
 {

 }

 ArtifactContentsViewer::~ArtifactContentsViewer()
 {
  delete impl_;
 }

  void ArtifactContentsViewer::setFilePath(const QString& filepath)
  {
   impl_->currentFilePath = filepath;
   ArtifactCore::FileTypeDetector detector;
   impl_->currentFileType = detector.detect(filepath);
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
      if (impl_ && impl_->currentFileType == ArtifactCore::FileType::Image &&
          impl_->stackedWidget && impl_->stackedWidget->currentWidget() == impl_->imageScrollArea &&
          impl_->imageFitMode) {
          impl_->fitImageToWindow();
      }
      QWidget::resizeEvent(event);
  }

  void ArtifactContentsViewer::mousePressEvent(QMouseEvent* event)
  {
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
   if (impl_->stackedWidget && impl_->infoLabel) {
    impl_->stackedWidget->setCurrentWidget(impl_->infoLabel);
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
   if (impl_->seekSlider) {
    QSignalBlocker blocker(impl_->seekSlider);
    impl_->seekSlider->setValue(static_cast<int>(std::clamp<qint64>(impl_->audioPlaybackPositionMs, 0, std::numeric_limits<int>::max())));
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
}

};
