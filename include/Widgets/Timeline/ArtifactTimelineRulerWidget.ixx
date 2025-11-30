module;
#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Timeline.RulerWidget;

import std;

export namespace Artifact
{
 class ArtifactTimelineRulerWidget :public QWidget
 {
 	
 private:
  class Impl;
  Impl* impl_;
  float start{ 0.2f }; // 0..1
  float end{ 0.8f };
 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent* ev) override;
  void mouseMoveEvent(QMouseEvent* ev) override;
  void mouseReleaseEvent(QMouseEvent*) override;
 public:
  explicit ArtifactTimelineRulerWidget(QWidget* parent = nullptr);
  ~ArtifactTimelineRulerWidget();
 	//signals
 };


};