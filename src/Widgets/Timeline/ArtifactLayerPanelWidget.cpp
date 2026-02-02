module;
#include <wobjectimpl.h>
#include <QPainter>
#include <QWidget>
#include <QString>
#include <QVector>
#include <QScrollArea>
#include <QBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QContextMenuEvent>
#include <QMouseEvent>
module Artifact.Widgets.LayerPanelWidget;

import std;
import Utils.Path;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;


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
  
  QPushButton* parentButton = nullptr;
  QPushButton* fxButton = nullptr;
  QPushButton* shyButton = nullptr;
  QPushButton* adjButton = nullptr;
  QPushButton* blurButton = nullptr;
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
  visiblityButton->setFlat(true);
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
  layerNameButton->setText("Layer Name");
  layerNameButton->setCheckable(false);
 	
  auto parentButton = impl_->parentButton = new QPushButton();
  parentButton->setText("Parent");
 	
  auto layerBlendModeButton = impl_->layerBlendModeButton = new QPushButton();
  layerBlendModeButton->setText("Blend Mode");
  auto shyButton=impl_->shyButton = new QPushButton;
  shyButton->setFixedSize(QSize(28, 28));
  auto blurButton = impl_->blurButton = new QPushButton();
  blurButton->setFixedSize(QSize(28, 28));
 	
  auto adjButton = impl_->adjButton = new QPushButton();
  adjButton->setFixedSize(QSize(28, 28));
 	
  //blurButton->setText("Layer Name");
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
  qHBoxLayout->addWidget(blurButton);
  qHBoxLayout->addWidget(adjButton);
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
  CompositionID compositionId;
  QPixmap visibilityIcon;
  QPixmap lockIcon;
  QPixmap soloIcon;
  QPixmap normalLayerIcon;
  QPixmap adjLayerIcon;
  QPixmap nullLayerIcon;
 };

 ArtifactLayerPanelWidget::Impl::Impl()
 {
  visibilityIcon = QPixmap(getIconPath() + "/Png/visibility.png");
  visibilityIcon = visibilityIcon.scaled(28,28, Qt::KeepAspectRatio, Qt::SmoothTransformation);
 
 }

 ArtifactLayerPanelWidget::Impl::~Impl()
 {

 }

 ArtifactLayerPanelWidget::ArtifactLayerPanelWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl)
 {
  setWindowTitle("ArtifactLayerPanel");

    // Refresh when layers are removed elsewhere
    QObject::connect(ArtifactProjectService::instance(), &ArtifactProjectService::layerRemoved, this, [this](const LayerID&) {
        update();
    });
 }

void ArtifactLayerPanelWidget::setComposition(const CompositionID& id)
{
    impl_->compositionId = id;
    // trigger repaint
    update();
}

 ArtifactLayerPanelWidget::~ArtifactLayerPanelWidget()
 {
  delete impl_;
 }

 void ArtifactLayerPanelWidget::mousePressEvent(QMouseEvent* event)
 {
    // Right-click -> context menu for layer actions
    if (event->button() == Qt::RightButton) {
        const int rowH = 28;
        int idx = event->pos().y() / rowH;
        if (!impl_->compositionId.isNil()) {
            auto compResult = ArtifactProjectService::instance()->findComposition(impl_->compositionId);
            if (compResult.success) {
                auto comp = compResult.ptr.lock();
                if (comp) {
                    auto layers = comp->allLayer();
                    if (idx >= 0 && idx < layers.size()) {
                        auto layer = layers[idx];
                        if (layer) {
                            QMenu menu(this);
                            QAction* del = menu.addAction("Delete Layer");
                            QAction* act = menu.exec(event->globalPos());
                            if (act == del) {
                                ArtifactProjectService::instance()->removeLayerFromComposition(impl_->compositionId, layer->id());
                            }
                            event->accept();
                            return;
                        }
                    }
                }
            }
        }
    }
    QWidget::mousePressEvent(event);
 }

 void ArtifactLayerPanelWidget::paintEvent(QPaintEvent* event)
 {
  QPainter p(this);
  const int rowH = 28; // 行の高さ

  // 1. まず全体をベースの色で塗る（もしくはループ内で塗り分ける）
  // p.fillRect(rect(), QColor(40, 40, 40)); 

  // 2. 行ごとに色を変えて塗りつぶす
  for (int i = 0; i * rowH < height(); ++i) {
   int y = i * rowH;

   // 偶数行と奇数行で色を分ける
   if (i % 2 == 0) {
	p.fillRect(0, y, width(), rowH, QColor(42, 42, 42)); // 暗いグレー
   }
   else {
	p.fillRect(0, y, width(), rowH, QColor(45, 45, 45)); // わずかに明るいグレー
   }

   // ついでに横線も引く（塗りつぶし境界に線を引く場合）
   p.setPen(QColor(60, 60, 60));
   p.drawLine(0, y + rowH, width(), y + rowH);
  }

  // --- 以下、アイコンやテキストの描画 ---
  // テキストを中央寄せにするためのテクニック
  const int textOffsetX = 24 * 5;
  p.setPen(Qt::white);
  // If a composition is set, draw its layers; otherwise draw placeholder
  if (!impl_->compositionId.isNil()) {
    // Try to find composition by id via service
    auto compResult = ArtifactProjectService::instance()->findComposition(impl_->compositionId);
    if (compResult.success) {
      auto compShared = compResult.ptr.lock();
      if (compShared) {
        QVector<ArtifactAbstractLayerPtr> layers = compShared->allLayer();
        for (int idx = 0; idx < layers.size(); ++idx) {
          int y = idx * rowH;
          auto layer = layers[idx];
          QString name = layer ? layer->layerName() : QString("(empty)");
          p.drawText(QRect(textOffsetX, y, width(), rowH), Qt::AlignVCenter, name);
          if (!impl_->visibilityIcon.isNull()) {
            p.drawPixmap(4, y + (rowH - 16) / 2, 16, 16, impl_->visibilityIcon);
          }
        }
      } else {
        p.drawText(QRect(textOffsetX, 0, width(), rowH), Qt::AlignVCenter, QString("No composition"));
      }
    } else {
      p.drawText(QRect(textOffsetX, 0, width(), rowH), Qt::AlignVCenter, QString("No composition"));
    }
  } else {
    QString layerName = "Layer 1";
    p.drawText(QRect(textOffsetX, 0, width(), rowH), Qt::AlignVCenter, layerName);
    if (!impl_->visibilityIcon.isNull()) {
      p.drawPixmap(4, (rowH - 16) / 2, 16, 16, impl_->visibilityIcon);
    }
  }
 }

 class ArtifactLayerTimelinePanelWrapper::Impl
 {
 public:
  QScrollArea* scroll = nullptr;
  ArtifactLayerPanelHeaderWidget* header = nullptr;
  ArtifactLayerPanelWidget* panel = nullptr;
  CompositionID id;
 };

 ArtifactLayerTimelinePanelWrapper::ArtifactLayerTimelinePanelWrapper(QWidget* parent)
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

 ArtifactLayerTimelinePanelWrapper::ArtifactLayerTimelinePanelWrapper(const CompositionID& id, QWidget* parent /*= nullptr*/):QWidget(parent),impl_(new Impl())
 {
    // Initialize children and set the composition id
    impl_->header = new ArtifactLayerPanelHeaderWidget();
    impl_->panel = new ArtifactLayerPanelWidget;
    impl_->panel->setComposition(id);
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

    impl_->id = id;
 }

 ArtifactLayerTimelinePanelWrapper::~ArtifactLayerTimelinePanelWrapper()
 {
  delete impl_;
 }



}



