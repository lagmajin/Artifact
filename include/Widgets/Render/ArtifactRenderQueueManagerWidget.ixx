module;
#include <wobjectdefs.h>
#include <QWidget>



export module Artifact.Widgets.Render.QueueManager;

import std;

export namespace Artifact
{

	
	




 class RenderQueueManagerWidget :public QWidget {
  W_OBJECT(RenderQueueManagerWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:
 	
 public:
  explicit RenderQueueManagerWidget(QWidget* parent = nullptr);
  ~RenderQueueManagerWidget();
  QSize sizeHint() const override;
  void setFloatingMode(bool isFloating);
 };








};