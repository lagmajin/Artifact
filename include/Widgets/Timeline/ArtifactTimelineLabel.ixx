module;
#include <utility>
#include <wobjectdefs.h>
#include <QLabel>
#include <QString>
export module Artifact.Widgets.Timeline.Label;


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
  void setText(const QString& text);
 };

	
	
	
};
