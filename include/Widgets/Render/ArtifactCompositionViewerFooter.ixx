module;
#include <QWidget>
export module Artifact.Widgets.CompositionFooter;

export namespace Artifact {


 class ArtifactCompositionViewerFooter :public QWidget{
 private:

 public:
  explicit ArtifactCompositionViewerFooter(QWidget* parent = nullptr);
  ~ArtifactCompositionViewerFooter();

 };


}