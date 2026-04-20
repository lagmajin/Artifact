module;
#include <utility>

#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Widget.WorkAreaControlWidget;

import Event.Bus;


export namespace Artifact
{
 class WorkAreaControl:public QWidget
 {
 	W_OBJECT(WorkAreaControl)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent* ev) override;
  void mouseMoveEvent(QMouseEvent* ev) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void leaveEvent(QEvent* event) override;
 public:
  explicit WorkAreaControl(QWidget* parent = nullptr);
  ~WorkAreaControl();
  float start{ 0.0f }; // 0..1
  float end{ 1.0f };   // 0..1

  // Property accessors with signals
  float startValue() const { return start; }
  float endValue() const { return end; }
  void setStart(float s);
  void setEnd(float e);

 public:
  void setEventBus(ArtifactCore::EventBus* eventBus);

 public: 
 
 };
 	
 	

};;
