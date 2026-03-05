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

module Artifact.Contents.Viewer;

import std;
import Artifact.Preview.Pipeline;
import File.TypeDetector;

namespace Artifact
{
  class ArtifactContentsViewer::Impl
  {
  public:
   Impl(ArtifactContentsViewer* parent);
   ~Impl();
 
   QStackedWidget* stackedWidget;
   QScrollArea* imageScrollArea;
   QLabel* imageLabel;
   QLabel* infoLabel;
   QString currentFilePath;
   ArtifactCore::FileType currentFileType;
   double zoomLevel = 1.0;
   QPoint lastMousePos;
  };

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

   switch (impl_->currentFileType) {
   case ArtifactCore::FileType::Image:
   {
     QPixmap pix(filepath);
     if (!pix.isNull()) {
         impl_->zoomLevel = 1.0;
         impl_->imageLabel->setPixmap(pix);
         impl_->imageLabel->setFixedSize(pix.size());
         impl_->stackedWidget->setCurrentWidget(impl_->imageScrollArea);
     } else {
         impl_->infoLabel->setText("Failed to load image:\n" + filepath);
         impl_->stackedWidget->setCurrentWidget(impl_->infoLabel);
     }
     break;
   }
   case ArtifactCore::FileType::Video:
     impl_->infoLabel->setText("Video Playback coming soon...\n" + filepath);
     impl_->stackedWidget->setCurrentWidget(impl_->infoLabel);
     break;
   default:
     impl_->infoLabel->setText("Unsupported Content:\n" + filepath);
     impl_->stackedWidget->setCurrentWidget(impl_->infoLabel);
     break;
   }
  }

  void ArtifactContentsViewer::wheelEvent(QWheelEvent* event)
  {
      if (impl_->currentFileType == ArtifactCore::FileType::Image && !impl_->imageLabel->pixmap().isNull()) {
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
      if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
          impl_->lastMousePos = event->pos();
          setCursor(Qt::ClosedHandCursor);
          event->accept();
          return;
      }
      QWidget::mousePressEvent(event);
  }

  void ArtifactContentsViewer::mouseMoveEvent(QMouseEvent* event)
  {
      if (event->buttons() & Qt::LeftButton || event->buttons() & Qt::MiddleButton) {
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
  // TODO: Implement playback for supported types
 }

 void ArtifactContentsViewer::pause()
 {
  // TODO: Implement pause for supported types
 }

 void ArtifactContentsViewer::stop()
 {
  // TODO: Implement stop for supported types
 }

 void ArtifactContentsViewer::playRange(int64_t start, int64_t end)
 {
  // TODO: Implement range playback for supported types
 }

};