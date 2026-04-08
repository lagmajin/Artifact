module;
#include <wobjectimpl.h>
#include <QBoxLayout>
#include <QWidget>
#include <utility>
export module Artifact.Widgets.LayerEditorPanel;

export namespace Artifact {

 class ArtifactLayerEditorPanel final :public QWidget{
 
 	private:
  class Impl;
  Impl* impl_;
 protected:
  void closeEvent(QCloseEvent* event) override;
 public:
   explicit ArtifactLayerEditorPanel(QWidget* parent = nullptr);
   ~ArtifactLayerEditorPanel();

   QSize sizeHint() const override;

 };




};
W_REGISTER_ARGTYPE(QPoint)
