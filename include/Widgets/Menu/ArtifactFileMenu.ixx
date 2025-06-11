module;
#include <QMenu>
#include <QWidget>
export module Menu:File;

//#pragma once


//#include <QtCore/QScopedPointer>
//#include <QtWidgets/QMenu>





export namespace Artifact {

 class  ArtifactFileMenuPrivate;

 class ArtifactFileMenu :public QMenu {
 private:
  QScopedPointer<ArtifactFileMenuPrivate> pImpl_;
 public:
  explicit ArtifactFileMenu(QWidget* parent = nullptr);
  ~ArtifactFileMenu();

 signals:

 public slots:
  void projectCreated();
  void projectClosed();
 };














};