#pragma once

#include <QtWidgets/QMenu>




namespace Artifact {

 class ArtifactViewMenuPrivate;

 class ArtifactViewMenu :public QMenu{
 private:

 public:
  explicit ArtifactViewMenu(QWidget*parent=nullptr);
  ~ArtifactViewMenu();
 };







};