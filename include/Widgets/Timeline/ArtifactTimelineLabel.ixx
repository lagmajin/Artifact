module;
#include <wobjectdefs.h>
#include <QLabel>
export module Artifact.Timeline.Label;

export namespace Artifact
{
 class ArtifactTimelineBottomLabel :public QWidget
 {
 	W_OBJECT(ArtifactTimelineBottomLabel)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactTimelineBottomLabel(QWidget* parent = nullptr);
  ~ArtifactTimelineBottomLabel();
 };

	
	
	
};