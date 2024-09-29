#pragma once

#include <QtWidgets/QWidget>


namespace Artifact {
  
 class ArtifactProjectManagerWidgetPrivate;

 class ArtifactProjectManagerWidget:public QWidget {
  Q_OBJECT
 private:

 protected:
 public:
  explicit ArtifactProjectManagerWidget(QWidget* parent = nullptr);
  ~ArtifactProjectManagerWidget();
 public slots:
  void triggerUpdate();
 };







};