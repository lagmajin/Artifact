module;

#include <QWidget>
#include <QFrame>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QClipboard>
#include <QToolButton>
#include <QSlider>
#include <wobjectimpl.h>
#include <QVBoxLayout>
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
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
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
   void updatePlaybackState();
   void updateActionAvailability();
   void updateModeButtons();
   void clearPlaybackRange();
   void setPlaybackRange(int64_t startFrame, int64_t endFrame);
   void resetCurrentMode();
   void ensureVideoWidgets();
   void ensureModelViewer();
   void syncModelViewerMode();
   void attachMediaOutputs();
   void detachMediaOutputs();
   void activateImage(const QString& filepath);
   void activateVideo(const QString& filepath);
   void activateModel(const QString& filepath);

   static qint64 framesToMs(int64_t frame);
   static QString humanFileSize(qint64 bytes);
   static QString formatDurationMs(qint64 ms);

   QWidget* headerWidget = nullptr;
   QLabel* titleLabel = nullptr;
   QLabel* typeBadgeLabel = nullptr;
   QLabel* metaLabel = nullptr;
   QLabel* stateLabel = nullptr;
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
   QLabel* infoLabel = nullptr;
   Artifact3DModelViewer* modelViewer = nullptr;
   bool videoWidgetsReady = false;
   bool modelViewerReady = false;
   QString currentFilePath;
   ArtifactCore::FileType currentFileType = ArtifactCore::FileType::Unknown;
   ContentsViewerMode currentMode = ContentsViewerMode::Source;
   double zoomLevel = 1.0;
   double rotationDegrees = 0.0;
   QPixmap originalImage;
   QPoint lastMousePos;
   bool playbackRangeActive = false;
   qint64 playbackRangeStartMs = 0;
   qint64 playbackRangeEndMs = 0;
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

  void ArtifactContentsViewer::Impl::resetCurrentMode()
  {
   clearPlaybackRange();
   currentMode = ContentsViewerMode::Source;

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
   videoWidget->setStyleSheet("background-color: #0f0f0f;");

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
   QPixmap pix(filepath);
   if (pix.isNull()) {
    showInfoMessage("Failed to load image:\n" + filepath);
    return;
   }

   zoomLevel = 1.0;
   rotationDegrees = 0.0;
   originalImage = pix;
   imageLabel->setPixmap(originalImage);
   imageLabel->setFixedSize(originalImage.size());
   stackedWidget->setCurrentWidget(imageScrollArea);
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

   if (playButton) {
    playButton->setEnabled(currentFileType == ArtifactCore::FileType::Video);
   }
   if (pauseButton) {
    pauseButton->setEnabled(currentFileType == ArtifactCore::FileType::Video && mediaPlayer && mediaPlayer->playbackState() == QMediaPlayer::PlayingState);
   }
   if (stopButton) {
    stopButton->setEnabled(currentFileType == ArtifactCore::FileType::Video && mediaPlayer && mediaPlayer->playbackState() != QMediaPlayer::StoppedState);
   }
   if (copyPathButton) {
    copyPathButton->setEnabled(!currentFilePath.isEmpty());
   }
   if (revealButton) {
    revealButton->setEnabled(!currentFilePath.isEmpty());
   }
   if (seekSlider) {
    const bool isVideo = currentFileType == ArtifactCore::FileType::Video;
    seekSlider->setEnabled(isVideo);
    seekSlider->setVisible(isVideo);
    if (!isVideo) {
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
                            || currentFileType == ArtifactCore::FileType::Model3D);
   }
   if (rotateLeftButton) {
    rotateLeftButton->setEnabled(currentFileType == ArtifactCore::FileType::Image);
   }
   if (rotateRightButton) {
    rotateRightButton->setEnabled(currentFileType == ArtifactCore::FileType::Image);
   }
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
   auto* headerLayout = new QHBoxLayout(headerWidget);
   headerLayout->setContentsMargins(10, 8, 10, 8);
   headerLayout->setSpacing(8);

   auto* textColumn = new QVBoxLayout();
   textColumn->setContentsMargins(0, 0, 0, 0);
   textColumn->setSpacing(2);

   titleLabel = new QLabel(QStringLiteral("Contents Viewer"), headerWidget);
   titleLabel->setStyleSheet("font-size: 15px; font-weight: 600; color: #f0f0f0;");
   typeBadgeLabel = new QLabel(QStringLiteral("Idle"), headerWidget);
   typeBadgeLabel->setStyleSheet(R"(
     QLabel {
       background: #3a3a3a;
       color: #f0f0f0;
       border-radius: 9px;
       padding: 2px 8px;
       font-size: 11px;
     }
   )");
   metaLabel = new QLabel(QStringLiteral("No file selected"), headerWidget);
   metaLabel->setStyleSheet("color: #a8a8a8; font-size: 11px;");
   metaLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
   stateLabel = new QLabel(QStringLiteral("State: Idle"), headerWidget);
   stateLabel->setStyleSheet("color: #c6c6c6; font-size: 11px;");

   textColumn->addWidget(titleLabel);
   textColumn->addWidget(metaLabel);
   textColumn->addWidget(stateLabel);

   auto* badgeColumn = new QVBoxLayout();
   badgeColumn->setContentsMargins(0, 0, 0, 0);
   badgeColumn->setSpacing(4);
   badgeColumn->addWidget(typeBadgeLabel, 0, Qt::AlignLeft);
   badgeColumn->addStretch(1);

   auto* buttonRow = new QHBoxLayout();
   buttonRow->setContentsMargins(0, 0, 0, 0);
   buttonRow->setSpacing(4);

   auto createButton = [parent](const QString& text, const QString& tooltip) {
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tooltip);
    button->setAutoRaise(true);
    button->setCursor(Qt::PointingHandCursor);
    return button;
   };

   fitButton = createButton(QStringLiteral("Fit"), QStringLiteral("Fit view"));
   rotateLeftButton = createButton(QStringLiteral("⟲"), QStringLiteral("Rotate left"));
   rotateRightButton = createButton(QStringLiteral("⟳"), QStringLiteral("Rotate right"));
   resetButton = createButton(QStringLiteral("Reset"), QStringLiteral("Reset view state"));
   playButton = createButton(QStringLiteral("Play"), QStringLiteral("Play video"));
   pauseButton = createButton(QStringLiteral("Pause"), QStringLiteral("Pause video"));
   stopButton = createButton(QStringLiteral("Stop"), QStringLiteral("Stop video"));
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
   seekSlider->setToolTip(QStringLiteral("Scrub video position"));
   seekSlider->setStyleSheet(R"(
    QSlider::groove:horizontal {
      height: 4px;
      background: #2a2a2a;
      border-radius: 2px;
    }
    QSlider::handle:horizontal {
      width: 12px;
      margin: -5px 0;
      border-radius: 6px;
      background: #d0d0d0;
    }
    QSlider::sub-page:horizontal {
      background: #6b8cff;
      border-radius: 2px;
    }
   )");

   buttonRow->addWidget(fitButton);
   buttonRow->addWidget(rotateLeftButton);
   buttonRow->addWidget(rotateRightButton);
   buttonRow->addWidget(resetButton);
   buttonRow->addSpacing(8);
   buttonRow->addWidget(playButton);
   buttonRow->addWidget(pauseButton);
   buttonRow->addWidget(stopButton);
   buttonRow->addSpacing(8);
   buttonRow->addWidget(copyPathButton);
   buttonRow->addWidget(revealButton);
   buttonRow->addSpacing(8);
   buttonRow->addWidget(sourceButton);
   buttonRow->addWidget(finalButton);
   buttonRow->addWidget(compareButton);

   headerLayout->addLayout(textColumn, 1);
   headerLayout->addLayout(badgeColumn, 0);
   headerLayout->addLayout(buttonRow, 0);

   QObject::connect(fitButton, &QToolButton::clicked, parent, [this]() {
    if (currentFileType == ArtifactCore::FileType::Image || currentFileType == ArtifactCore::FileType::Model3D) {
     if (currentFileType == ArtifactCore::FileType::Image) {
      zoomLevel = 1.0;
      rotationDegrees = 0.0;
      if (imageLabel && !originalImage.isNull()) {
       imageLabel->setPixmap(originalImage);
       imageLabel->setFixedSize(originalImage.size());
      }
     } else if (modelViewer) {
      modelViewer->resetView();
     }
     updateHeader();
     updatePlaybackState();
    }
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
    if (currentFileType != ArtifactCore::FileType::Video || !mediaPlayer) {
     return;
    }
    mediaPlayer->setPosition(static_cast<qint64>(value));
    updatePlaybackState();
   });

   // Image Viewer Setup
   imageScrollArea = new QScrollArea();
   imageLabel = new QLabel();
   imageLabel->setAlignment(Qt::AlignCenter);
   imageLabel->setStyleSheet("background: transparent;");
   imageLabel->setScaledContents(true); // Allow manual scaling via setFixedSize
   
   imageScrollArea->setWidget(imageLabel);
   imageScrollArea->setWidgetResizable(false); // We want to control sizing based on zoom
   imageScrollArea->setAlignment(Qt::AlignCenter);
   imageScrollArea->setStyleSheet(R"(
     QScrollArea {
       background-color: #1a1a1a;
       border: none;
     }
     QScrollBar:vertical {
       background: #111;
       width: 10px;
       margin: 0px;
     }
     QScrollBar::handle:vertical {
       background: #333;
       min-height: 20px;
       border-radius: 5px;
     }
     QScrollBar:horizontal {
       background: #111;
       height: 10px;
       margin: 0px;
     }
     QScrollBar::handle:horizontal {
       background: #333;
       min-width: 20px;
       border-radius: 5px;
     }
    )");

   // Info/Message View Setup
   infoLabel = new QLabel();
   infoLabel->setAlignment(Qt::AlignCenter);
   infoLabel->setWordWrap(true);
   infoLabel->setStyleSheet("color: #888; background-color: #1a1a1a; font-family: 'Segoe UI'; font-size: 14px;");

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
          !impl_->imageLabel->pixmap().isNull()) {
          const double scaleFactor = 1.15;
          if (event->angleDelta().y() > 0) {
              impl_->zoomLevel *= scaleFactor;
          } else {
              impl_->zoomLevel /= scaleFactor;
          }
          
          impl_->zoomLevel = qBound(0.05, impl_->zoomLevel, 10.0);
          QSize newSize = impl_->imageLabel->pixmap().size() * impl_->zoomLevel;
          impl_->imageLabel->setFixedSize(newSize);
          event->accept();
          return;
      }
      QWidget::wheelEvent(event);
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

  qDebug() << "ArtifactContentsViewer::pause no-op for current type:" << static_cast<int>(impl_->currentFileType);
}

void ArtifactContentsViewer::stop()
{
  if (impl_->currentFileType == ArtifactCore::FileType::Image && !impl_->imageLabel->pixmap().isNull()) {
   impl_->zoomLevel = 1.0;
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

  play();
}

void ArtifactContentsViewer::rotateLeft()
{
 if (impl_->currentFileType != ArtifactCore::FileType::Image || impl_->originalImage.isNull()) {
  return;
 }

 impl_->rotationDegrees -= 90.0;
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
 impl_->imageLabel->setPixmap(impl_->originalImage);
 impl_->imageLabel->setFixedSize(impl_->originalImage.size());
 impl_->updateHeader();
}

};
