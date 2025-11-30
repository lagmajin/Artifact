module;
#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Timeline.ScaleWidget;

export namespace Artifact
{
	
 class TimelineScaleWidget :public QWidget
 {
  W_OBJECT(TimelineScaleWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void paintEvent(QPaintEvent* event) override;
 public:
  explicit TimelineScaleWidget(QWidget* parent = nullptr);
  ~TimelineScaleWidget();
 };

	
	
}