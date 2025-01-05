#pragma once

#include "WickedEngine.h"
#include <QtWidgets/QWidget>





namespace Artifact {

 class ArtifactRenderManagerWidgetPrivate;

 //class wi::Application;

 class ArtifactRenderManagerWidget :public QWidget{
  Q_OBJECT
 private:
  bool initialized = false;
  wi::Application* app=nullptr;
 protected:
  bool event(QEvent* e) override;
 public:
  explicit ArtifactRenderManagerWidget(QWidget* parent = nullptr);
  ~ArtifactRenderManagerWidget();
  void clear();
 };











};