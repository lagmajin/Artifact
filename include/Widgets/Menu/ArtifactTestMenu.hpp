#pragma once
#include <QtWidgets/QMenu>



namespace Artifact {

 class ArtifactRenderTestMenuPrivate;

 class ArtifactRenderTestMenu :public QMenu {
 private:

 public:
  explicit ArtifactRenderTestMenu(QWidget* parent = nullptr);
  ~ArtifactRenderTestMenu();
 signals:
  void serialImageRenderTestRequested();
 public slots:
 };

 class ArtifactEffectTestMenuPrivate;

 class ArtifactEffectTestMenu {
  private:

 public:

 };

 class ArtifactMediaTestMenuPrivate;

 class ArtifactMediaTestMenu :public QMenu {
 private:

 public:
  explicit ArtifactMediaTestMenu(QWidget* parent = nullptr);
  ~ArtifactMediaTestMenu();
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