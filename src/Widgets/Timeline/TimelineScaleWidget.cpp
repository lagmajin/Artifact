module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QPainter>

module Artifact.Timeline.ScaleWidget;

import Artifact.Project.Manager;


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
   painter.save();
   painter.setClipRect(rect);
   painter.fillRect(rect, Qt::black);
   painter.setPen(Qt::white);

   const int baseMajorStep = 10;
   const int minorStep = 1;

   int left = rect.left();
   int right = rect.right();

   // --- 文字を描ける最小ピクセル幅を定義 ---
   const int minLabelPx = 30;       // 30px 以下なら文字を描かない
   int majorStep = baseMajorStep;

   // majorStep を拡大していって「文字が描ける幅」を確保
   while (majorStep * zoom < minLabelPx)
	majorStep *= 2;

   // 描画ループ
   for (int f = 0; f <= frameCount; ++f)
   {
	int x = left + static_cast<int>(f * zoom);
	if (x > right) break;

	if (f % majorStep == 0)
	{
	 painter.drawLine(x, rect.top(), x, rect.top() + 15);
	 // 文字が入る範囲を確保
	 QRect textRect(x + 2, rect.top() + 2, majorStep * zoom, 14);
	 painter.drawText(textRect,
	  Qt::AlignLeft | Qt::AlignVCenter,
	  QString::number(f));
	}
	else if (f % minorStep == 0)
	{
	 painter.drawLine(x, rect.top(), x, rect.top() + 7);
	}
   }

   painter.restore();
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
   setMinimumHeight(40);
 }

 TimelineScaleWidget::~TimelineScaleWidget()
 {
  delete impl_;
 }

 void TimelineScaleWidget::paintEvent(QPaintEvent* event)
 {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);
  impl_->draw(p, rect());
  //p.setClipRect(r);
 }

};