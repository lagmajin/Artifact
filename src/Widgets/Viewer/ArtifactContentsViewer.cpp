module;

#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
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
   Impl(ArtifactContentsViewer* parent);
   ~Impl();

   void showInfoMessage(const QString& text);
   void clearPlaybackRange();
   void setPlaybackRange(int64_t startFrame, int64_t endFrame);
   void resetCurrentMode();
   void activateImage(const QString& filepath);
   void activateVideo(const QString& filepath);
   void activateModel(const QString& filepath);

   static qint64 framesToMs(int64_t frame);

   QStackedWidget* stackedWidget = nullptr;
   QScrollArea* imageScrollArea = nullptr;
   QLabel* imageLabel = nullptr;
   QVideoWidget* videoWidget = nullptr;
   QMediaPlayer* mediaPlayer = nullptr;
   QAudioOutput* audioOutput = nullptr;
   QLabel* infoLabel = nullptr;
   Artifact3DModelViewer* modelViewer = nullptr;
   QString currentFilePath;
   ArtifactCore::FileType currentFileType = ArtifactCore::FileType::Unknown;
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

  void ArtifactContentsViewer::Impl::showInfoMessage(const QString& text)
  {
   infoLabel->setText(text);
   stackedWidget->setCurrentWidget(infoLabel);
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

   if (mediaPlayer) {
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
  }

  void ArtifactContentsViewer::Impl::activateVideo(const QString& filepath)
  {
   QFileInfo info(filepath);
   if (!info.exists()) {
    showInfoMessage("Video file does not exist:\n" + filepath);
    return;
   }

   clearPlaybackRange();
   stackedWidget->setCurrentWidget(videoWidget);
   mediaPlayer->setSource(QUrl::fromLocalFile(info.absoluteFilePath()));
   mediaPlayer->play();
  }

  void ArtifactContentsViewer::Impl::activateModel(const QString& filepath)
  {
   modelViewer->loadModel(ArtifactCore::UniString(filepath.toStdString()));
   stackedWidget->setCurrentWidget(modelViewer);
  }

  ArtifactContentsViewer::Impl::Impl(ArtifactContentsViewer* parent)
   : stackedWidget(new QStackedWidget(parent))
  {
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

   // Video Viewer Setup
   videoWidget = new QVideoWidget(parent);
   videoWidget->setStyleSheet("background-color: #0f0f0f;");

   mediaPlayer = new QMediaPlayer(parent);
   audioOutput = new QAudioOutput(parent);
   audioOutput->setVolume(1.0f);
   mediaPlayer->setAudioOutput(audioOutput);
   mediaPlayer->setVideoOutput(videoWidget);

   QObject::connect(mediaPlayer, &QMediaPlayer::positionChanged, parent, [this](qint64 position) {
    if (!playbackRangeActive) return;
    if (position >= playbackRangeEndMs) {
     mediaPlayer->pause();
     mediaPlayer->setPosition(playbackRangeStartMs);
    }
   });

   QObject::connect(mediaPlayer, &QMediaPlayer::errorOccurred, parent,
    [this](QMediaPlayer::Error error, const QString& errorString) {
     if (error == QMediaPlayer::NoError) return;
     showInfoMessage("Failed to play video:\n" + currentFilePath + "\n" + errorString);
    });

   // Info/Message View Setup
   infoLabel = new QLabel();
   infoLabel->setAlignment(Qt::AlignCenter);
   infoLabel->setWordWrap(true);
   infoLabel->setStyleSheet("color: #888; background-color: #1a1a1a; font-family: 'Segoe UI'; font-size: 14px;");

   // 3D Model Viewer Setup
   modelViewer = new Artifact3DModelViewer(parent);

   stackedWidget->addWidget(imageScrollArea);
   stackedWidget->addWidget(videoWidget);
   stackedWidget->addWidget(infoLabel);
   stackedWidget->addWidget(modelViewer);

   auto layout = new QVBoxLayout(parent);
   layout->setContentsMargins(0, 0, 0, 0);
   layout->setSpacing(0);
   layout->addWidget(stackedWidget);
   parent->setLayout(layout);
  }

 ArtifactContentsViewer::Impl::~Impl()
 {
  // Widgets are parented, will be deleted
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
   return;
  }

  if (impl_->currentFileType == ArtifactCore::FileType::Video) {
   if (impl_->mediaPlayer->source().isEmpty() && !impl_->currentFilePath.isEmpty()) {
    impl_->mediaPlayer->setSource(QUrl::fromLocalFile(impl_->currentFilePath));
   }
   if (impl_->playbackRangeActive) {
    impl_->mediaPlayer->setPosition(impl_->playbackRangeStartMs);
   }
   impl_->stackedWidget->setCurrentWidget(impl_->videoWidget);
   impl_->mediaPlayer->play();
   return;
  }

  impl_->showInfoMessage("Cannot play unsupported content.\n" + impl_->currentFilePath);
}

void ArtifactContentsViewer::pause()
{
  if (impl_->currentFileType == ArtifactCore::FileType::Video) {
   impl_->mediaPlayer->pause();
   return;
  }

  qDebug() << "ArtifactContentsViewer::pause no-op for current type:" << static_cast<int>(impl_->currentFileType);
}

void ArtifactContentsViewer::stop()
{
  if (impl_->currentFileType == ArtifactCore::FileType::Image && !impl_->imageLabel->pixmap().isNull()) {
   impl_->zoomLevel = 1.0;
   impl_->imageLabel->setFixedSize(impl_->imageLabel->pixmap().size());
   return;
  }

  if (impl_->currentFileType == ArtifactCore::FileType::Video) {
   impl_->mediaPlayer->stop();
   if (impl_->playbackRangeActive) {
    impl_->mediaPlayer->setPosition(impl_->playbackRangeStartMs);
   } else {
    impl_->mediaPlayer->setPosition(0);
   }
   return;
  }

  qDebug() << "ArtifactContentsViewer::stop no-op for current type:" << static_cast<int>(impl_->currentFileType);
}

void ArtifactContentsViewer::playRange(int64_t start, int64_t end)
{
  if (impl_->currentFileType == ArtifactCore::FileType::Video) {
   impl_->setPlaybackRange(start, end);
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
}

};
