module;
#include <wobjectimpl.h>
#include <QPainter>
#include <QWidget>
#include <QString>
#include <QVector>
#include <QScrollArea>
#include <QBoxLayout>
module Artifact.Widgets.LayerPanelWidget;



namespace Artifact
{
 W_OBJECT_IMPL(ArtifactLayerPanelHeaderWidget)
  ArtifactLayerPanelHeaderWidget::ArtifactLayerPanelHeaderWidget(const QWidget* parent /*= nullptr*/)
 {

 }

 ArtifactLayerPanelHeaderWidget::~ArtifactLayerPanelHeaderWidget()
 {

 }
 W_OBJECT_IMPL(ArtifactLayerPanelWidget)

  class ArtifactLayerPanelWidget::Impl
 {
 private:


 public:
  Impl();
  ~Impl();
 };

 ArtifactLayerPanelWidget::Impl::Impl()
 {

 }

 ArtifactLayerPanelWidget::Impl::~Impl()
 {

 }

 ArtifactLayerPanelWidget::ArtifactLayerPanelWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl)
 {
  setWindowTitle("ArtifactLayerPanel");
 }

 ArtifactLayerPanelWidget::~ArtifactLayerPanelWidget()
 {
  delete impl_;
 }

 void ArtifactLayerPanelWidget::paintEvent(QPaintEvent* event)
 {

  QPainter p(this);
  p.fillRect(rect(), QColor(40, 40, 40));

  p.setPen(QColor(70, 70, 70));   // ÉâÉCÉìêF
  const int rowH = 28;            // çsä‘äu

  for (int y = rowH; y < height(); y += rowH) {
   p.drawLine(0, y, width(), y);
  }

 }

 class ArtifactLayerPanelWrapper::Impl
 {
 public:
  QScrollArea* scroll = nullptr;
  ArtifactLayerPanelWidget* panel = nullptr;
 };

 ArtifactLayerPanelWrapper::ArtifactLayerPanelWrapper(QWidget* parent)
  : QWidget(parent),
  impl_(new Impl)
 {
  impl_->panel = new ArtifactLayerPanelWidget;
  impl_->scroll = new QScrollArea(this);

  impl_->scroll->setWidget(impl_->panel);
  impl_->scroll->setWidgetResizable(true);
  impl_->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(impl_->scroll);
  setLayout(layout);
 }

 ArtifactLayerPanelWrapper::~ArtifactLayerPanelWrapper()
 {
  delete impl_;
 }



}



