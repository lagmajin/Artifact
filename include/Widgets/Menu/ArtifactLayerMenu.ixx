module;
#include <QMenu>

export module Artifact.Menu.Layer;




export namespace Artifact {



 class ArtifactLayerMenu:public QMenu {
  //Q_OBJECT
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactLayerMenu(QWidget* parent=nullptr);
  ~ArtifactLayerMenu();
  QMenu* newLayerMenu() const;
 signals:

 public slots:

 };








}