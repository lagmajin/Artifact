#pragma once

#include <QtWidgets/QWidget>


namespace Artifact {
  
 class ArtifactProjectManagerWidgetPrivate;

 class ArtifactProjectManagerWidget:public QWidget {
  Q_OBJECT
 private:

 public:
  explicit ArtifactProjectManagerWidget(QWidget* parent = nullptr);
  ~ArtifactProjectManagerWidget();

 };







};