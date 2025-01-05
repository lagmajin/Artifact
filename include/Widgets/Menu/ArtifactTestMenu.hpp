#pragma once
#include <QtWidgets/QMenu>



namespace Artifact {

 class ArtifactRenderTestMenu :public QMenu {
 private:

 public:
  explicit ArtifactRenderTestMenu(QWidget* parent = nullptr);
  ~ArtifactRenderTestMenu();
 signals:
  void serialImageRenderTestRequested();
 };

 class ArtifactWidgetTestMenu :public QMenu {

 };


 class ArtifactTestMenu :public QMenu {
 private:

 public:
  explicit ArtifactTestMenu(QWidget* parent = nullptr);
  ~ArtifactTestMenu();
 signals:
  
  
 };










};