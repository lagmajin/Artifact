#pragma once


#include <QtWidgets/QScrollArea>



namespace Artifact {

 class ArtifactInspectorWidget :public QScrollArea{
 private:

 public:
  explicit ArtifactInspectorWidget(QWidget* parent = nullptr);
  ~ArtifactInspectorWidget();
 };








};