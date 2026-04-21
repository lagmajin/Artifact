module;
#include <utility>

#include <QMouseEvent>
#include <QPaintEvent>
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Timeline.NavigatorWidget;


export namespace Artifact
{
 class ArtifactTimelineNavigatorWidget : public QWidget
 {
  W_OBJECT(ArtifactTimelineNavigatorWidget)

 private:
  class Impl;
  Impl* impl_;
  float start{ 0.0f };
  float end{ 1.0f };
  double currentFrame_{ -1.0 };

 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent* ev) override;
  void mouseMoveEvent(QMouseEvent* ev) override;
  void mouseReleaseEvent(QMouseEvent*) override;

 public:
  explicit ArtifactTimelineNavigatorWidget(QWidget* parent = nullptr);
  ~ArtifactTimelineNavigatorWidget();

  float startValue() const { return start; }
  float endValue() const { return end; }
  int totalFrames() const;
  void setStart(float s);
  void setEnd(float e);
  void setCurrentFrame(double frame);
  void setTotalFrames(int totalFrames);

  void startChanged(float value) W_SIGNAL(startChanged, value)
  void endChanged(float value) W_SIGNAL(endChanged, value)
 };
}
