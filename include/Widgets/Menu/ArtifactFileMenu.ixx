module;
#include <utility>

#include <wobjectdefs.h>
#include <QMenu>
#include <QWidget>

export module Artifact.Menu.File;


export namespace Artifact {

 class ArtifactFileMenu :public QMenu {
  W_OBJECT(ArtifactFileMenu)
 	private:
  //QScopedPointer<ArtifactFileMenuPrivate> pImpl_;
 class Impl;
 Impl* Impl_;
 public:
  explicit ArtifactFileMenu(QWidget* parent = nullptr);
  ~ArtifactFileMenu();

  void resetRecentFilesMenu();
  /*signals:*/
 public:
  void rebuildMenu();
  W_SLOT(rebuildMenu, ());
 protected:
 public:
  void projectCreateRequested();
  void projectClosed();
  void restartApplication();
  void quitApplication();
  W_SLOT(projectCreateRequested, ());
  W_SLOT(projectClosed, ());
  W_SLOT(restartApplication, ());
  W_SLOT(quitApplication, ());
 };














};
