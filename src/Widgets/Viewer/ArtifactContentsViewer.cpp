module;

#include <QWidget>
#include <wobjectimpl.h>
module Artifact.Contents.Viewer;

import std;
import Artifact.Preview.Pipeline;

//import Artifact.Preview.Controller;

namespace Artifact
{
 class ArtifactContentsViewer::Impl
 {

 public:
  Impl();
 };

 ArtifactContentsViewer::Impl::Impl()
 {

 }
	
	W_OBJECT_IMPL(ArtifactContentsViewer)

 ArtifactContentsViewer::ArtifactContentsViewer(QWidget* parent/*=nullptr*/) :QWidget(parent), impl_(new Impl())
 {

 }

 ArtifactContentsViewer::~ArtifactContentsViewer()
 {
  delete impl_;
 }

 void ArtifactContentsViewer::setFilePath(const QString& filepath)
 {

 }

 void ArtifactContentsViewer::play()
 {

 }

 void ArtifactContentsViewer::pause()
 {

 }

 void ArtifactContentsViewer::stop()
 {

 }

};