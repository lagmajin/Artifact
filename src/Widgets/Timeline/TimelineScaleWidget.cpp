module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QPainter>

module Artifact.Timeline.ScaleWidget;



namespace Artifact
{
 W_OBJECT_IMPL(TimelineScaleWidget)

  class TimelineScaleWidget::Impl
 {
 private:
 	
 public:
  Impl();
  ~Impl();
  int frameCount = 1000;
  float zoom = 1.0f; // ピクセル/フレーム
  void draw(QPainter& painter, const QRect& rect)
  {
   const int majorStep = 10;
   const int minorStep = 1;

   painter.fillRect(rect, Qt::black);
   painter.setPen(Qt::white);

   for (int f = 0; f <= frameCount; ++f)
   {
	int x = static_cast<int>(f * zoom);
	if (x > rect.width()) break;

	if (f % majorStep == 0)
	{
	 painter.drawLine(x, 0, x, 15); // major
	 painter.drawText(x + 2, 12, QString::number(f));
	}
	else if (f % minorStep == 0)
	{
	 painter.drawLine(x, 0, x, 7); // minor
	}
   }
  }
 };

 TimelineScaleWidget::Impl::Impl()
 {

 }

 TimelineScaleWidget::Impl::~Impl()
 {

 }

  TimelineScaleWidget::TimelineScaleWidget(QWidget* parent/*=nullptr*/) :QWidget(parent),impl_(new Impl())
 {

 }

 TimelineScaleWidget::~TimelineScaleWidget()
 {
  delete impl_;
 }

 void TimelineScaleWidget::paintEvent(QPaintEvent* event)
 {
  QPainter p(this);
  impl_->draw(p, rect());
 }

};