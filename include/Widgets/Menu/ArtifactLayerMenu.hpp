#pragma once
#include <QtWidgets/QMenu>






namespace Artifact {



 class ArtifactLayerMenuPrivate;

 class ArtifactLayerMenu:public QMenu {
  Q_OBJECT
 private:

 public:
  explicit ArtifactLayerMenu(QWidget* parent=nullptr);
  ~ArtifactLayerMenu();
  QMenu* newLayerMenu() const;
 signals:

 public slots:

 };








}