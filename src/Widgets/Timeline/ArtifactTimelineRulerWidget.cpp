module;
#include <QPainter>
#include <QMouseEvent>
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
  bool draggingLeft{ false };
  bool draggingRight{ false };
  float start{ 0.2f }; // 0..1
  float end{ 0.8f };
 };

 ArtifactTimelineRulerWidget::Impl::Impl()
 {

 }

 ArtifactTimelineRulerWidget::Impl::~Impl()
 {

 }

 ArtifactTimelineRulerWidget::ArtifactTimelineRulerWidget(QWidget* parent /*= nullptr*/):QWidget(parent),impl_(new Impl())
 {
  setMinimumHeight(16);
  setMouseTracking(true);

 }

 ArtifactTimelineRulerWidget::~ArtifactTimelineRulerWidget()
 {
   delete impl_;
 }

 void ArtifactTimelineRulerWidget::setStart(float s) {
  if (start != s) {
    start = s;
    startChanged(s);
    update();
  }
 }

 void ArtifactTimelineRulerWidget::setEnd(float e) {
  if (end != e) {
    end = e;
    endChanged(e);
    update();
  }
 }

  void ArtifactTimelineRulerWidget::paintEvent(QPaintEvent*)
  {
   QPainter p(this);
   p.setRenderHint(QPainter::Antialiasing);

   // Background
   p.fillRect(rect(), QColor(35, 35, 35));

   // Draw Ticks (Temporal markers)
   p.setPen(QColor(80, 80, 80));
   for (int x = 0; x < width(); x += 20) {
       int h = (x % 100 == 0) ? height() : (x % 50 == 0) ? height() / 2 : height() / 4;
       p.drawLine(x, height() - h, x, height());
   }

   const int handleHalfW = 6;
   const int handleW = handleHalfW * 2;
   int usableWidth = width() - handleW;

   int x1 = handleHalfW + static_cast<int>(start * usableWidth);
   int x2 = handleHalfW + static_cast<int>(end * usableWidth);

   // Darken outside range
   p.fillRect(0, 0, x1, height(), QColor(0, 0, 0, 100));
   p.fillRect(x2, 0, width() - x2, height(), QColor(0, 0, 0, 100));

   // Range strip (Work Area)
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

 void ArtifactTimelineRulerWidget::mousePressEvent(QMouseEvent* ev)
 {
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  int usableWidth = width() - handleW;

  int x1 = handleHalfW + static_cast<int>(start * usableWidth);
  int x2 = handleHalfW + static_cast<int>(end * usableWidth);

  if (QRect(x1 - handleHalfW, 0, handleW, height()).contains(ev->pos()))
   impl_->draggingLeft = true;
  else if (QRect(x2 - handleHalfW, 0, handleW, height()).contains(ev->pos()))
   impl_->draggingRight = true;
 }

 void ArtifactTimelineRulerWidget::mouseMoveEvent(QMouseEvent* ev)
 {
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  int usableWidth = width() - handleW;

  if (impl_->draggingLeft) {
   float newStart = (float(ev->pos().x()) - handleHalfW) / float(usableWidth);
   setStart(qBound(0.0f, newStart, end - 0.01f));
  }
  else if (impl_->draggingRight) {
   float newEnd = (float(ev->pos().x()) - handleHalfW) / float(usableWidth);
   setEnd(qBound(start + 0.01f, newEnd, 1.0f));
  }
 }

 void ArtifactTimelineRulerWidget::mouseReleaseEvent(QMouseEvent* ev)
 {
  impl_->draggingLeft = impl_->draggingRight = false;
 }

};
