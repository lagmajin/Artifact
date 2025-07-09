module;

#include <QtWidgets/QWidget>
#include <wobjectdefs.h>

export module ArtifactProjectManagerWidget;

import std;


import Project;

export namespace Artifact {
  
 class ArtifactProjectManagerWidgetPrivate;

 class ArtifactProjectManagerWidget:public QWidget {
  W_OBJECT(ArtifactProjectManagerWidget)
 private:
  class Impl;
  std::unique_ptr<Impl> impl;
 protected:
  void dropEvent(QDropEvent* event);
  void dragEnterEvent(QDragEnterEvent* event);
 public:
  explicit ArtifactProjectManagerWidget(QWidget* parent = nullptr);
  ~ArtifactProjectManagerWidget();
 signals:

 public slots:
  void triggerUpdate();
 };







};