module;
#include <wobjectimpl.h>
#include <QPainter>
#include <QWidget>
#include <QString>
#include <QVector>
#include <QScrollArea>
#include <QBoxLayout>
#include <QPushButton>
module Artifact.Widgets.LayerPanelWidget;

import std;
import Utils.Path;


namespace Artifact
{
 using namespace ArtifactCore;

 class ArtifactLayerPanelHeaderWidget::Impl
 {
 public:
  Impl();
  ~Impl() = default;
  QPixmap visibilityIcon;
  QPixmap lockIcon;
  QPixmap soloIcon;
  QPushButton* visibilityButton = nullptr;
  QPushButton* lockButton = nullptr;
  QPushButton* soloButton = nullptr;
  QPushButton* soundButton = nullptr;
  QPushButton* layerNameButton = nullptr;
  QPushButton* layerBlendModeButton = nullptr;
  QPushButton* fxButton = nullptr;
  QPushButton* parentButton = nullptr;
 };

 ArtifactLayerPanelHeaderWidget::Impl::Impl()
 {
  visibilityIcon = QPixmap(getIconPath() + "/visibility.png");
 }
	
 W_OBJECT_IMPL(ArtifactLayerPanelHeaderWidget)
  ArtifactLayerPanelHeaderWidget::ArtifactLayerPanelHeaderWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  auto visiblityButton = impl_->visibilityButton = new QPushButton();
  visiblityButton->setFixedSize(QSize(28, 28));
  visiblityButton->setIcon(impl_->visibilityIcon);
  visiblityButton->setStyleSheet("background-color: gray; color: black;");
  auto lockButton = impl_->lockButton = new QPushButton();
  lockButton->setStyleSheet("background-color: gray; color: black;");
  lockButton->setFixedSize(QSize(28, 28));
  auto soloButton = impl_->soloButton = new QPushButton();
  soloButton->setFixedSize(QSize(28, 28));
  soloButton->setStyleSheet("background-color: gray; color: black;");
 	
  auto soundButton = impl_->soundButton = new QPushButton();
  soundButton->setFixedSize(QSize(28, 28));
  soundButton->setStyleSheet("background-color: gray; color: black;");
 	
  auto layerNameButton = impl_->layerNameButton = new QPushButton();
  layerNameButton->setText("Blend Mode");
  layerNameButton->setCheckable(false);
 	
  auto parentButton = impl_->parentButton = new QPushButton();
  parentButton->setText("Parent");
 	
  auto layerBlendModeButton = impl_->layerBlendModeButton = new QPushButton();
  layerBlendModeButton->setText("Layer Name");
  auto qHBoxLayout = new QHBoxLayout();
  qHBoxLayout->setContentsMargins(0, 0, 0, 0);
  qHBoxLayout->setSpacing(0);
  qHBoxLayout->addWidget(visiblityButton);
  qHBoxLayout->addWidget(lockButton);
  qHBoxLayout->addWidget(soloButton);
  qHBoxLayout->addWidget(soundButton);
  qHBoxLayout->addWidget(layerNameButton);
  qHBoxLayout->addWidget(layerBlendModeButton);
  qHBoxLayout->addWidget(parentButton);
  //qHBoxLayout->addWidget(soloButton);
  setLayout(qHBoxLayout);
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
  QPixmap visibilityIcon;
  QPixmap lockIcon;
  QPixmap soloIcon;
  QPixmap normalLayerIcon;
  QPixmap adjLayerIcon;
  QPixmap nullLayerIcon;
 };

 ArtifactLayerPanelWidget::Impl::Impl()
 {
  visibilityIcon = QPixmap(getIconPath() + "/visibility.png");
  visibilityIcon = visibilityIcon.scaled(28,28, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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

 void ArtifactLayerPanelWidget::mousePressEvent(QMouseEvent* event)
 {

 }

 void ArtifactLayerPanelWidget::paintEvent(QPaintEvent* event)
 {

  QPainter p(this);
  p.fillRect(rect(), QColor(40, 40, 40));
  const int rowH = 28;
  const int colW = 28;  // 行間隔
  const int numVLines = 4;

  p.setPen(QColor(70, 70, 70));   // ライン色

  for (int y = rowH; y < height(); y += rowH) {
   p.drawLine(0, y, width(), y);
  }



  for (int i = 0; i < numVLines; ++i) {
   int x = colW * (i + 1);  // 1本目は28, 2本目は56…
   p.drawLine(x, 0, x, height());
  }
  const int textOffsetX = 24 * 5;
  const int textOffsetY = 0;

  p.setPen(Qt::white);

  QString layerName = "Layer 1";
  p.drawText(textOffsetX, textOffsetY + p.fontMetrics().ascent(), layerName);

  p.drawPixmap(0, 0, impl_->visibilityIcon);


 }

 class ArtifactLayerPanelWrapper::Impl
 {
 public:
  QScrollArea* scroll = nullptr;
  ArtifactLayerPanelHeaderWidget* header = nullptr;
  ArtifactLayerPanelWidget* panel = nullptr;
 };

 ArtifactLayerPanelWrapper::ArtifactLayerPanelWrapper(QWidget* parent)
  : QWidget(parent),
  impl_(new Impl)
 {
  impl_->header = new ArtifactLayerPanelHeaderWidget();
  impl_->panel = new ArtifactLayerPanelWidget;
  impl_->scroll = new QScrollArea(this);

  impl_->scroll->setWidget(impl_->panel);
  impl_->scroll->setWidgetResizable(true);
  impl_->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(impl_->header);
  layout->addWidget(impl_->scroll);
  setLayout(layout);
 }

 ArtifactLayerPanelWrapper::~ArtifactLayerPanelWrapper()
 {
  delete impl_;
 }



}



