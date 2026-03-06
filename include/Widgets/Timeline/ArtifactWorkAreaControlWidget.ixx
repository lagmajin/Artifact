module;
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Widget.WorkAreaControlWidget;

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
 public:
  explicit WorkAreaControl(QWidget* parent = nullptr);
  ~WorkAreaControl();
  float start{ 0.2f }; // 0..1
  float end{ 0.8f };   // 0..1

  // Property accessors with signals
  void setStart(float s);
  void setEnd(float e);

 public:
   void startChanged(float value) W_SIGNAL(startChanged, value)
   void endChanged(float value) W_SIGNAL(endChanged, value)

 public: 
 
 };
 	
 	

};;
