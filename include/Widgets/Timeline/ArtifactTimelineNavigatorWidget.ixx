module;
#include <utility>

#include <QMouseEvent>
#include <QPaintEvent>
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Timeline.NavigatorWidget;

import Event.Bus;


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
  void setTotalFrames(int totalFrames);
  void setEventBus(ArtifactCore::EventBus* eventBus);
 };
}
