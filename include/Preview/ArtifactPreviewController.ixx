module;
#include <wobjectimpl.h>
#include <QObject>
export module Artifact.Preview.Controller;

export namespace Artifact
{
 class ArtifactPreviewController:public QObject
 {
 	W_OBJECT(ArtifactPreviewController)
 private:
  class Impl;
  Impl* impl_;
 public:
 	ArtifactPreviewController(QObject*parent=nullptr);
    ~ArtifactPreviewController();
 public/*signals*/:
 	
 public /*slots*/:
 	
 };


};