module;
#include <utility>

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
  void leaveEvent(QEvent* event) override;
 public:
  explicit WorkAreaControl(QWidget* parent = nullptr);
  ~WorkAreaControl();
  float start{ 0.0f }; // 0..1
  float end{ 1.0f };   // 0..1
  float currentFrame{ 0.0f };
  float totalFrames{ 1.0f };

  // Property accessors with signals
  float startValue() const { return start; }
  float endValue() const { return end; }
  float currentFrameValue() const { return currentFrame; }
  float totalFramesValue() const { return totalFrames; }
  void setStart(float s);
  void setEnd(float e);
  void setCurrentFrame(float frame);
  void setTotalFrames(float frames);
  void setRulerPixelsPerFrame(double ppf);
  void setRulerHorizontalOffset(double offset);

 public:
   void startChanged(float value) W_SIGNAL(startChanged, value)
   void endChanged(float value) W_SIGNAL(endChanged, value)

 public: 
 
 };
 	
 	

};;
