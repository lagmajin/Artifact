module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QPainter>
#include <QPalette>
#include <cmath>

module Artifact.Timeline.ScaleWidget;

import std;

import Artifact.Project.Manager;
import Widgets.Utils.CSS;


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
  double visibleStartFrame = 0.0;
  double visibleEndFrame = 100.0;
  void draw(QPainter& painter, const QRect& rect)
  {
   painter.save();
   painter.setClipRect(rect);

   const auto& theme = ArtifactCore::currentDCCTheme();
   const QColor background(theme.secondaryBackgroundColor);
   const QColor border(theme.borderColor);
   const QColor majorTick(theme.textColor);
   const QColor minorTick = QColor(theme.textColor).darker(155);
   const QColor labelColor = QColor(theme.textColor).darker(118);
   
   // Background matching AE timeline headers
   painter.fillRect(rect, background);

   QFont font = painter.font();
   font.setFamily("Consolas"); // Use monospace for numbers
   font.setPointSize(8);
   painter.setFont(font);

   const double visibleSpan = std::max(1.0, visibleEndFrame - visibleStartFrame);
   const double pixelsPerFrame = std::max(0.001, static_cast<double>(rect.width()) / visibleSpan);
   const int baseMajorStep = 10;
   const int minorStep = 1;
   const int minLabelPx = 45;
   int majorStep = baseMajorStep;

   while (majorStep * pixelsPerFrame < minLabelPx) {
    majorStep *= 2;
   }

   const int maxFrame = std::max(0, frameCount - 1);
   const int firstFrame = std::clamp(static_cast<int>(std::floor(visibleStartFrame / majorStep)) * majorStep, 0, maxFrame);
   const int lastFrame = std::clamp(static_cast<int>(std::ceil(visibleEndFrame)), firstFrame, maxFrame);

   // Draw ticks and labels
   for (int f = firstFrame; f <= lastFrame; ++f)
   {
    const double normalized = (static_cast<double>(f) - visibleStartFrame) / visibleSpan;
    const int x = rect.left() + static_cast<int>(std::lround(normalized * rect.width()));
    if (x < rect.left()) continue;
    if (x > rect.right()) break;

    if (f % majorStep == 0)
    {
     // Major tick
     painter.setPen(QPen(majorTick, 1));
     painter.drawLine(x, rect.bottom() - 10, x, rect.bottom()); // Tick from bottom up

     // Label
     painter.setPen(labelColor);
     QRect textRect(x + 4, rect.bottom() - 22, 50, 14);
     painter.drawText(textRect, Qt::AlignLeft | Qt::AlignBottom, QString("%1f").arg(f));
    }
    else if (f % minorStep == 0)
    {
     // Minor tick
     int minorHalf = majorStep / 2;
     if (f % minorHalf == 0) {
      painter.setPen(QPen(minorTick, 1));
      painter.drawLine(x, rect.bottom() - 6, x, rect.bottom());
     } else {
      painter.setPen(QPen(minorTick.darker(120), 1));
      painter.drawLine(x, rect.bottom() - 3, x, rect.bottom());
     }
    }
   }

   // Bottom border
   painter.setPen(QPen(border, 1));
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

void TimelineScaleWidget::setFrameCount(int frameCount)
{
 if (!impl_) return;
 const int clamped = std::max(1, frameCount);
 if (impl_->frameCount == clamped) {
  return;
 }
 impl_->frameCount = clamped;
 update();
}

void TimelineScaleWidget::setVisibleRange(double startFrame, double endFrame)
{
 if (!impl_) return;
 const double nextStart = std::max(0.0, startFrame);
 const double nextEnd = std::max(nextStart + 1.0, endFrame);
 if (std::abs(impl_->visibleStartFrame - nextStart) < 0.001 &&
     std::abs(impl_->visibleEndFrame - nextEnd) < 0.001) {
  return;
 }
 impl_->visibleStartFrame = nextStart;
 impl_->visibleEndFrame = nextEnd;
 update();
}

void TimelineScaleWidget::paintEvent(QPaintEvent* event)
{
 QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);
  impl_->draw(p, rect());
  //p.setClipRect(r);
 }

};
