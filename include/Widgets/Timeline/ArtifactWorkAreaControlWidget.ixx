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

 	
 public: 
 	
 };
	
	

};;