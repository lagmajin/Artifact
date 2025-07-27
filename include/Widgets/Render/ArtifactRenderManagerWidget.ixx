module;

//#include "WickedEngine.h"
#include <wobjectdefs.h>

#include <memory>
#include <QWidget>

export module ArtifactRenderManagerWidget;

import std;

namespace Artifact {

class RendererQueueModel
{
	
};


 class ArtifactRenderManagerWidgetPrivate;

 //class wi::Application;

 class ArtifactRenderManagerWidget :public QWidget{
  //Q_OBJECT
 private:
  //std::unique_ptr<ArtifactRenderManagerWidgetPrivate> pImpl_;
  bool initialized = false;
  //wi::Application* app=nullptr;
 protected:
  bool event(QEvent* e) override;
 public:
  explicit ArtifactRenderManagerWidget(QWidget* parent = nullptr);
  ~ArtifactRenderManagerWidget();
  void clear();
 };











};