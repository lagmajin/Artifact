module;
#include <utility>
#include <QMenu>
#include <wobjectimpl.h>
export module Artifact.Menu.View;

import Artifact.MainWindow;

export namespace Artifact {

 class ArtifactViewMenu :public QMenu{
  W_OBJECT(ArtifactViewMenu)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactViewMenu(QWidget*parent=nullptr);
  ~ArtifactViewMenu();

  void setMainWindow(ArtifactMainWindow* mw);

 public /*slots*/:
  void registerView(const QString& name, QWidget* view);
  };

}
