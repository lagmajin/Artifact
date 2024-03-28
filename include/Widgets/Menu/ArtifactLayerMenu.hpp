#pragma once
#include <QtWidgets/QMenu>






namespace Artifact {


 struct ArtifactLayerMenuPrivate;

 class ArtifactLayerMenu:public QMenu {
 private:

 public:
  explicit ArtifactLayerMenu(QWidget* parent=nullptr);
  ~ArtifactLayerMenu();
  QMenu* newLayerMenu() const;
 };








}