module;
#include <QLabel>
export module Artifact.Timeline.Label;

export namespace Artifact
{
 class TimelineLabel :public QWidget
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit TimelineLabel(QWidget* parent = nullptr);
  ~TimelineLabel();
 };

	
	
	
};