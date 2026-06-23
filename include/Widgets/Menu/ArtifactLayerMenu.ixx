module;
#include <utility>
#include <wobjectdefs.h>
#include <QMenu>

export module Artifact.Menu.Layer;




export namespace Artifact {

 enum class LayerCreationPlacementMode {
  CompositionStart,
  Playhead,
  WorkAreaStart,
  SelectedLayerIn,
  SelectedLayerOut,
  AfterSelected,
  BeforeSelected,
  CustomFrame
 };


 class ArtifactLayerMenu:public QMenu {
  W_OBJECT(ArtifactLayerMenu)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactLayerMenu(QWidget* mainWindow = nullptr, QWidget* parent = nullptr);
  ~ArtifactLayerMenu();
  QMenu* newLayerMenu() const;
 //signals
 public :
  void nullLayerCreated()
   W_SIGNAL(nullLayerCreated)
 //slots
 public:

 };








}
