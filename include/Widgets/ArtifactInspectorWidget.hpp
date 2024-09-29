#pragma once


#include <QtWidgets/QScrollArea>

#include <Audio/AudioDllImport.hpp>


namespace Artifact {

 class ArtifactInspectorWidgetPrivate;

 class ArtifactInspectorWidget :public QScrollArea{
 Q_OBJECT
 private:

 protected:
  void update();
 public:
  explicit ArtifactInspectorWidget(QWidget* parent = nullptr);
  ~ArtifactInspectorWidget();
 signals:

 public slots:
  void triggerUpdate();
 };








};