module;
#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Timeline.RulerWidget;

export namespace Artifact
{
 class ArtifactTimelineRulerWidget :public QWidget
 {
 private:
  class Impl;
  Impl* impl_;
 protected:
 	
 public:
  explicit ArtifactTimelineRulerWidget(QWidget* parent = nullptr);
  ~ArtifactTimelineRulerWidget();
 };


};