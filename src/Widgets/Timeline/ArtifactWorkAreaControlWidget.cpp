module;
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>

#include <wobjectimpl.h>
module Artifact.Widget.WorkAreaControlWidget;

namespace Artifact
{
	W_OBJECT_IMPL(WorkAreaControl)
	
 class WorkAreaControl::Impl
 {
 public:
  bool draggingLeft{ false };
  bool draggingRight{ false };
 };



 WorkAreaControl::WorkAreaControl(QWidget* parent /*= nullptr*/):QWidget(parent),impl_(new Impl())
 {
  setMouseTracking(true);
  setMinimumHeight(26);
 }

 WorkAreaControl::~WorkAreaControl()
 {
  delete impl_;
 }

 void WorkAreaControl::paintEvent(QPaintEvent*)
 {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Background
  p.fillRect(rect(), QColor(50, 50, 50));

  // Constrain usable width to keep handles fully inside
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  int usableWidth = width() - handleW;

  int x1 = handleHalfW + static_cast<int>(start * usableWidth);
  int x2 = handleHalfW + static_cast<int>(end * usableWidth);

  // Range strip
  QRect rangeRect(x1, 0, x2 - x1, height());
  QLinearGradient grad(rangeRect.topLeft(), rangeRect.bottomLeft());
  grad.setColorAt(0, QColor(100, 150, 255, 180));
  grad.setColorAt(1, QColor(50, 100, 200, 180));
  p.fillRect(rangeRect, grad);

  // Handles
  p.setBrush(QColor(200, 200, 200));
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(QRectF(x1 - handleHalfW, 0, handleW, height()), 3, 3);
  p.drawRoundedRect(QRectF(x2 - handleHalfW, 0, handleW, height()), 3, 3);
 }

 void WorkAreaControl::mouseMoveEvent(QMouseEvent* ev)
 {
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  int usableWidth = width() - handleW;

  if (impl_->draggingLeft) {
   float newStart = (float(ev->pos().x()) - handleHalfW) / float(usableWidth);
   start = qBound(0.0f, newStart, end - 0.01f);
   update();
  }
  else if (impl_->draggingRight) {
   float newEnd = (float(ev->pos().x()) - handleHalfW) / float(usableWidth);
   end = qBound(start + 0.01f, newEnd, 1.0f);
   update();
  }
 }

 void WorkAreaControl::mouseReleaseEvent(QMouseEvent*)
 {
  impl_->draggingLeft = impl_->draggingRight = false;
 }

 void WorkAreaControl::mousePressEvent(QMouseEvent* ev)
 {
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  int usableWidth = width() - handleW;

  int x1 = handleHalfW + static_cast<int>(start * usableWidth);
  int x2 = handleHalfW + static_cast<int>(end * usableWidth);
  
  if (QRect(x1 - handleHalfW, 0, handleW, height()).contains(ev->pos())) impl_->draggingLeft = true;
  else if (QRect(x2 - handleHalfW, 0, handleW, height()).contains(ev->pos())) impl_->draggingRight = true;
 }

};