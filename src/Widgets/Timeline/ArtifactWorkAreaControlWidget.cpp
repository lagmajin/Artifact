module;
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

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
  bool draggingRange{ false };
  float dragGrabRatio{ 0.0f };
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

 void WorkAreaControl::setStart(float s) {
  if (start != s) {
    start = s;
    startChanged(s);
    update();
  }
 }

 void WorkAreaControl::setEnd(float e) {
  if (end != e) {
    end = e;
    endChanged(e);
    update();
  }
 }

 void WorkAreaControl::paintEvent(QPaintEvent*)
 {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Background
  p.fillRect(rect(), QColor(35, 35, 35));

  // Constrain usable width to keep handles fully inside
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  int usableWidth = width() - handleW;

  int x1 = handleHalfW + static_cast<int>(start * usableWidth);
  int x2 = handleHalfW + static_cast<int>(end * usableWidth);

  // Darken outside range
  p.fillRect(0, 0, x1, height(), QColor(0, 0, 0, 100));
  p.fillRect(x2, 0, width() - x2, height(), QColor(0, 0, 0, 100));

  // Range strip
  QRect rangeRect(x1, 0, x2 - x1, height());
  QLinearGradient grad(rangeRect.topLeft(), rangeRect.bottomLeft());
  grad.setColorAt(0, QColor(0, 120, 215, 120));
  grad.setColorAt(1, QColor(0, 80, 180, 120));
  p.fillRect(rangeRect, grad);

  // Bottom border for work area
  p.setPen(QPen(QColor(0, 150, 255), 2));
  p.drawLine(x1, height() - 1, x2, height() - 1);

  // Handles (Blue AE style)
  p.setBrush(QColor(0, 120, 215));
  p.setPen(QPen(Qt::white, 1));
  p.drawRoundedRect(QRectF(x1 - handleHalfW, 2, handleW, height() - 4), 2, 2);
  p.drawRoundedRect(QRectF(x2 - handleHalfW, 2, handleW, height() - 4), 2, 2);
 }

 void WorkAreaControl::mouseMoveEvent(QMouseEvent* ev)
 {
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  const int usableWidth = std::max(1, width() - handleW);

  if (!(ev->buttons() & Qt::LeftButton)) {
   impl_->draggingLeft = impl_->draggingRight = impl_->draggingRange = false;
   return;
  }

  if (impl_->draggingLeft) {
   float newStart = (float(ev->pos().x()) - handleHalfW) / float(usableWidth);
   setStart(qBound(0.0f, newStart, end - 0.01f));
  }
  else if (impl_->draggingRight) {
   float newEnd = (float(ev->pos().x()) - handleHalfW) / float(usableWidth);
   setEnd(qBound(start + 0.01f, newEnd, 1.0f));
  } else if (impl_->draggingRange) {
   const float range = std::max(0.01f, end - start);
   float left = (float(ev->pos().x()) - handleHalfW) / float(usableWidth) - impl_->dragGrabRatio;
   left = qBound(0.0f, left, 1.0f - range);
   setStart(left);
   setEnd(left + range);
  }
 }

 void WorkAreaControl::mouseReleaseEvent(QMouseEvent*)
 {
  impl_->draggingLeft = impl_->draggingRight = impl_->draggingRange = false;
 }

 void WorkAreaControl::mousePressEvent(QMouseEvent* ev)
 {
  if (ev->button() != Qt::LeftButton) return;
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  const int usableWidth = std::max(1, width() - handleW);

  int x1 = handleHalfW + static_cast<int>(start * usableWidth);
  int x2 = handleHalfW + static_cast<int>(end * usableWidth);
  
  if (QRect(x1 - handleHalfW, 0, handleW, height()).contains(ev->pos())) impl_->draggingLeft = true;
  else if (QRect(x2 - handleHalfW, 0, handleW, height()).contains(ev->pos())) impl_->draggingRight = true;
  else if (QRect(x1 + handleHalfW, 0, std::max(0, x2 - x1 - handleW), height()).contains(ev->pos())) {
   impl_->draggingRange = true;
   const float normalizedX = (float(ev->pos().x()) - handleHalfW) / float(usableWidth);
   impl_->dragGrabRatio = normalizedX - start;
  }
 }

};
