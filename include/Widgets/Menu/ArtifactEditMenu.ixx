module;
#include <wobjectdefs.h>
#include <QMenu>

export module Artifact.Menu.Edit;


export namespace Artifact {

 class ArtifactEditMenu :public QMenu {
  W_OBJECT(ArtifactEditMenu)
 private:
  class Impl;
  Impl* impl_;
 protected:
  
 public:
  explicit ArtifactEditMenu(QWidget* mainWindow = nullptr, QWidget* parent = nullptr);
  ~ArtifactEditMenu();

  void rebuildMenu();
  W_SLOT(rebuildMenu, ());

 };




};
