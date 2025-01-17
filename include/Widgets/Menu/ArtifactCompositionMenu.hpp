#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QtWidgets>
#include <Qtwidgets/QMenu>







namespace Artifact {

 class ArtifactCompositionMenuPrivate;

 class ArtifactCompositionMenu:public QMenu {
  Q_OBJECT
 private:
  QScopedPointer<ArtifactCompositionMenuPrivate> pImpl_;
  
 public:
  explicit ArtifactCompositionMenu(QWidget*parent=nullptr);
  ~ArtifactCompositionMenu();
 };









};