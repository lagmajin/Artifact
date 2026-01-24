module;

#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <wobjectimpl.h>
#include <QVBoxLayout>
#include <QPixmap>

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
  QLabel* imageViewer;
  QString currentFilePath;
  ArtifactCore::FileType currentFileType;
 };

 ArtifactContentsViewer::Impl::Impl(ArtifactContentsViewer* parent)
  : stackedWidget(new QStackedWidget(parent)),
    imageViewer(new QLabel)
 {
  stackedWidget->addWidget(imageViewer);

  auto layout = new QVBoxLayout(parent);
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
    impl_->imageViewer->setPixmap(QPixmap(filepath));
    impl_->stackedWidget->setCurrentWidget(impl_->imageViewer);
    break;
  case ArtifactCore::FileType::Video:
    impl_->imageViewer->setText("Video playback not yet supported. File: " + filepath);
    impl_->stackedWidget->setCurrentWidget(impl_->imageViewer);
    break;
  // case ArtifactCore::FileType::Model: // Future
  //   impl_->modelViewer->loadModel(filepath);
  //   impl_->stackedWidget->setCurrentWidget(impl_->modelViewer);
  //   break;
  default:
    impl_->imageViewer->setText("Unsupported file type: " + filepath);
    impl_->stackedWidget->setCurrentWidget(impl_->imageViewer);
    break;
  }
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