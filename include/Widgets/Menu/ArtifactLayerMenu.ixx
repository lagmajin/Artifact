module;

#include <wobjectdefs.h>
#include <QMenu>

export module Artifact.Menu.Layer;




export namespace Artifact {



 class ArtifactLayerMenu:public QMenu {
  W_OBJECT(ArtifactLayerMenu)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactLayerMenu(QWidget* parent=nullptr);
  ~ArtifactLayerMenu();
  QMenu* newLayerMenu() const;
 //signals
 public :
  void nullLayerCreated()
   W_SIGNAL(nullLayerCreated)
 public slots:

 };








}