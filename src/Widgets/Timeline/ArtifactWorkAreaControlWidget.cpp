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

  // ”wŒi
  p.fillRect(rect(), QColor(50, 50, 50));

  int x1 = start * width();
  int x2 = end * width();

  // ”ÍˆÍ
  QRect rangeRect(x1, 0, x2 - x1, height());
  QLinearGradient grad(rangeRect.topLeft(), rangeRect.bottomLeft());
  grad.setColorAt(0, QColor(100, 150, 255, 180));
  grad.setColorAt(1, QColor(50, 100, 200, 180));
  p.fillRect(rangeRect, grad);

  // ƒnƒ“ƒhƒ‹
  p.setBrush(QColor(200, 200, 200));
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(QRectF(x1 - 6, 0, 12, height()), 3, 3);
  p.drawRoundedRect(QRectF(x2 - 6, 0, 12, height()), 3, 3);
 }

 void WorkAreaControl::mouseMoveEvent(QMouseEvent* ev)
 {
  if (impl_->draggingLeft) {
   start = qBound(0.0f, float(ev->pos().x()) / width(), end - 0.01f);
   update();
  }
  else if (impl_->draggingRight) {
   end = qBound(start + 0.01f, float(ev->pos().x()) / width(), 1.0f);
   update();
  }
 }

 void WorkAreaControl::mouseReleaseEvent(QMouseEvent*)
 {
  impl_->draggingLeft = impl_->draggingRight = false;
 }

 void WorkAreaControl::mousePressEvent(QMouseEvent* ev)
 {
  int x1 = start * width();
  int x2 = end * width();
  if (QRect(x1 - 5, 0, 10, height()).contains(ev->pos())) impl_->draggingLeft = true;
  else if (QRect(x2 - 5, 0, 10, height()).contains(ev->pos())) impl_->draggingRight = true;
 }

};