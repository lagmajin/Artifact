#pragma once
#include <QWidget>
#include <QMenu>







namespace Artifact {

 class ArtifactCompositionMenuPrivate;

 class ArtifactCompositionMenu:public QMenu {
  Q_OBJECT
 private:

  
 public:
  explicit ArtifactCompositionMenu(QWidget*parent=nullptr);
  ~ArtifactCompositionMenu();
 };









};