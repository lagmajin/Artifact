#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QMenu>





namespace Artifact {

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