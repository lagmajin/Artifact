module;
#include <QLabel>
#include <QHBoxLayout>
module Artifact.Timeline.Label;


namespace Artifact
{
 class TimelineLabel::Impl
 {
 private:

 public:
  QLabel* frameRenderingLabel = nullptr;
 };

 TimelineLabel::TimelineLabel(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {


  auto layout = new QHBoxLayout();


  setLayout(layout);

  setFixedHeight(28);
 }

 TimelineLabel::~TimelineLabel()
 {

 }
	
}