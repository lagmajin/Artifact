module;
#include <QLabel>
#include <QHBoxLayout>
#include <wobjectimpl.h>
module Artifact.Timeline.Label;

import std;

namespace Artifact
{
	W_OBJECT_IMPL(ArtifactTimelineBottomLabel)
	
 class ArtifactTimelineBottomLabel::Impl
 {
 private:

 public:
  QLabel* frameRenderingLabel = nullptr;
 };

 ArtifactTimelineBottomLabel::ArtifactTimelineBottomLabel(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {


  auto layout = new QHBoxLayout();


  setLayout(layout);

  setFixedHeight(28);
 }

 ArtifactTimelineBottomLabel::~ArtifactTimelineBottomLabel()
 {

 }
	
}