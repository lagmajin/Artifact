#pragma once

#include "WickedEngine.h"

#include <memory>
#include <QtWidgets/QWidget>





namespace Artifact {

 class ArtifactRenderManagerWidgetPrivate;

 //class wi::Application;

 class ArtifactRenderManagerWidget :public QWidget{
  Q_OBJECT
 private:
  std::unique_ptr<ArtifactDiligentEngineRenderWindowPrivate> pImpl_;
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