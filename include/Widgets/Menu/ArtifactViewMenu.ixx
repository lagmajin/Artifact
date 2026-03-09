module;
#include <QMenu>
#include <wobjectimpl.h>

export module Artifact.Menu.View;
//#pragma once

//#include <QtWidgets/QMenu>




export namespace Artifact {



 class ArtifactViewMenu :public QMenu{
  W_OBJECT(ArtifactViewMenu)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactViewMenu(QWidget*parent=nullptr);
  ~ArtifactViewMenu();
 /*signals:*/
  
 private /*slots*/:

 public /*slots*/:
  void registerView(const QString& name, QWidget* view);
 };







};