module;
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Widgets.CompositionFooter;

export namespace Artifact {


 class ArtifactCompositionViewerFooter :public QWidget{
 	W_OBJECT(ArtifactCompositionViewerFooter)
 private:
  class Impl;
  Impl* impl_;
 protected:
 	
 public:
  explicit ArtifactCompositionViewerFooter(QWidget* parent = nullptr);
  ~ArtifactCompositionViewerFooter();
 public/*signal*/:
  void takeSnapShotRequested() W_SIGNAL(takeSnapShotRequested);
 };


}