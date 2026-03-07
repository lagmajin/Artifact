module;
#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Timeline.RulerWidget;

import std;

export namespace Artifact
{
  class ArtifactTimelineRulerWidget :public QWidget
  {
   W_OBJECT(ArtifactTimelineRulerWidget)
  private:
  class Impl;
  Impl* impl_;
  float start{ 0.0f }; // 0..1
  float end{ 1.0f };
 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent* ev) override;
  void mouseMoveEvent(QMouseEvent* ev) override;
  void mouseReleaseEvent(QMouseEvent*) override;
 public:
  explicit ArtifactTimelineRulerWidget(QWidget* parent = nullptr);
  ~ArtifactTimelineRulerWidget();

  // Property accessors with signals
  float startValue() const { return start; }
  float endValue() const { return end; }
  void setStart(float s);
  void setEnd(float e);

public:
  void startChanged(float value) W_SIGNAL(startChanged, value)
  void endChanged(float value) W_SIGNAL(endChanged, value)
 };


};
