#pragma once


#include <QtWidgets/QMenu>




namespace Artifact {

 class ArtifactImageProcessingTestMenuPrivate;

 class ArtifactImageProcessingTestMenu :public QMenu {
  Q_OBJECT
 private:
  QScopedPointer<ArtifactImageProcessingTestMenuPrivate> pImpl_;
 public:
  explicit ArtifactImageProcessingTestMenu(QWidget* parent = nullptr);
  ~ArtifactImageProcessingTestMenu();

 public slots:
  void imageProcessingTest();
 };





};