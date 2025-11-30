module;
#include <QWidget>

module Artifact.Timeline.RulerWidget;



namespace Artifact
{
 class ArtifactTimelineRulerWidget::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactTimelineRulerWidget::Impl::Impl()
 {

 }

 ArtifactTimelineRulerWidget::Impl::~Impl()
 {

 }

 ArtifactTimelineRulerWidget::ArtifactTimelineRulerWidget(QWidget* parent /*= nullptr*/):QWidget(parent)
 {
  setMinimumHeight(18);
 }

 ArtifactTimelineRulerWidget::~ArtifactTimelineRulerWidget()
 {

 }

};