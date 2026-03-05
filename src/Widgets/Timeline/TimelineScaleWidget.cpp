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
  float zoom = 1.0f; // sNZ/t[
  void draw(QPainter& painter, const QRect& rect)
  {
   painter.save();
   painter.setClipRect(rect);
   
   // Background matching AE timeline headers
   painter.fillRect(rect, QColor(40, 40, 40));

   QFont font = painter.font();
   font.setFamily("Consolas"); // Use monospace for numbers
   font.setPointSize(8);
   painter.setFont(font);

   const int baseMajorStep = 10;
   const int minorStep = 1;

   int left = rect.left();
   int right = rect.right();

   // --- Calculate steps ---
   const int minLabelPx = 45;       // Space for text
   int majorStep = baseMajorStep;

   while (majorStep * zoom < minLabelPx) {
    majorStep *= 2;
   }

   // Draw ticks and labels
   for (int f = 0; f <= frameCount; ++f)
   {
    int x = left + static_cast<int>(f * zoom);
    if (x < left) continue;
    if (x > right) break;

    if (f % majorStep == 0)
    {
     // Major tick
     painter.setPen(QPen(QColor(150, 150, 150), 1));
     painter.drawLine(x, rect.bottom() - 10, x, rect.bottom()); // Tick from bottom up

     // Label
     painter.setPen(QColor(200, 200, 200));
     QRect textRect(x + 4, rect.bottom() - 22, 50, 14);
     painter.drawText(textRect, Qt::AlignLeft | Qt::AlignBottom, QString("%1f").arg(f));
    }
    else if (f % minorStep == 0)
    {
     // Minor tick
     int minorHalf = majorStep / 2;
     if (f % minorHalf == 0) {
      painter.setPen(QPen(QColor(110, 110, 110), 1));
      painter.drawLine(x, rect.bottom() - 6, x, rect.bottom());
     } else {
      painter.setPen(QPen(QColor(80, 80, 80), 1));
      painter.drawLine(x, rect.bottom() - 3, x, rect.bottom());
     }
    }
   }

   // Bottom border
   painter.setPen(QPen(QColor(20, 20, 20), 1));
   painter.drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());

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
   setMinimumHeight(24);
   setMaximumHeight(24);
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