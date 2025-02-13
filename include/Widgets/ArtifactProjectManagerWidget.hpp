#pragma once

#include <QtWidgets/QWidget>


import ArtifactProject;

namespace Artifact {
  
 class ArtifactProjectManagerWidgetPrivate;

 class ArtifactProjectManagerWidget:public QWidget {
  Q_OBJECT
 private:

 protected:
  void dropEvent(QDropEvent* event);
 public:
  explicit ArtifactProjectManagerWidget(QWidget* parent = nullptr);
  ~ArtifactProjectManagerWidget();
 signals:

 public slots:
  void triggerUpdate();
 };







};