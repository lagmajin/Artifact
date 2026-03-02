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

 void ArtifactTimelineRulerWidget::paintEvent(QPaintEvent*)
 {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Background
  p.fillRect(rect(), QColor(50, 50, 50));

  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  int usableWidth = width() - handleW;

  int x1 = handleHalfW + static_cast<int>(impl_->start * usableWidth);
  int x2 = handleHalfW + static_cast<int>(impl_->end * usableWidth);

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

 void ArtifactTimelineRulerWidget::mousePressEvent(QMouseEvent* ev)
 {
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  int usableWidth = width() - handleW;

  int x1 = handleHalfW + static_cast<int>(impl_->start * usableWidth);
  int x2 = handleHalfW + static_cast<int>(impl_->end * usableWidth);

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
   impl_->start = qBound(0.0f, newStart, impl_->end - 0.01f);
   update();
  }
  else if (impl_->draggingRight) {
   float newEnd = (float(ev->pos().x()) - handleHalfW) / float(usableWidth);
   impl_->end = qBound(impl_->start + 0.01f, newEnd, 1.0f);
   update();
  }
 }

 void ArtifactTimelineRulerWidget::mouseReleaseEvent(QMouseEvent* ev)
 {
  impl_->draggingLeft = impl_->draggingRight = false;
 }

};