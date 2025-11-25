module;
#include <QWidget>
export module Artifact.Widgets.LayerEditorPanel;

export namespace Artifact {

 class ArtifactLayerEditorPanel final :public QWidget{
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
   explicit ArtifactLayerEditorPanel(QWidget* parent = nullptr);
   ~ArtifactLayerEditorPanel();
 };




};