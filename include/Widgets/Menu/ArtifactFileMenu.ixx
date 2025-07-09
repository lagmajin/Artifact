module;
#include <QMenu>
#include <QWidget>

#include <wobjectdefs.h>
export module Menu:File;


export namespace Artifact {

 class  ArtifactFileMenuPrivate;

 class ArtifactFileMenu :public QMenu {
  W_OBJECT(ArtifactFileMenu)
 	private:
  //QScopedPointer<ArtifactFileMenuPrivate> pImpl_;
 class Impl;
 Impl* Impl_;
 public:
  explicit ArtifactFileMenu(QWidget* parent = nullptr);
  ~ArtifactFileMenu();

 signals:
 protected:
  void rebuildMenu();
  W_SLOT(rebuildMenu, ());
 public:
  void projectCreateRequested();
  void projectClosed();
  void quitApplication();
  W_SLOT(projectCreateRequested, ());
  W_SLOT(projectClosed, ());
  W_SLOT(quitApplication, ());
 };














};