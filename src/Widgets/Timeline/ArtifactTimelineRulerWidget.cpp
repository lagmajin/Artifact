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

  // ”wŒi
  p.fillRect(rect(), QColor(50, 50, 50));

  int x1 = impl_->start * width();
  int x2 = impl_->end * width();

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

 void ArtifactTimelineRulerWidget::mousePressEvent(QMouseEvent* ev)
 {
  int x1 = impl_->start * width();
  int x2 = impl_->end * width();

  if (QRect(x1 - 5, 0, 10, height()).contains(ev->pos()))
   impl_->draggingLeft = true;
  else if (QRect(x2 - 5, 0, 10, height()).contains(ev->pos()))
   impl_->draggingRight = true;
 }

 void ArtifactTimelineRulerWidget::mouseMoveEvent(QMouseEvent* ev)
 {
  if (impl_->draggingLeft) {
   impl_->start = qBound(0.0f, float(ev->pos().x()) / width(), impl_->end - 0.01f);
   update();
  }
  else if (impl_->draggingRight) {
   impl_->end = qBound(impl_->start + 0.01f, float(ev->pos().x()) / width(), 1.0f);
   update();
  }
 }

 void ArtifactTimelineRulerWidget::mouseReleaseEvent(QMouseEvent* ev)
 {
  impl_->draggingLeft = impl_->draggingRight = false;
 }

};