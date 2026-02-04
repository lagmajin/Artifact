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
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QMimeData>
#include <QUrl>
#include <QDebug>
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
  int hoveredLayerIndex = -1;  // マウスホバー中のレイヤーインデックス
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
  setAcceptDrops(true);

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
    // Left-click -> select layer
    else if (event->button() == Qt::LeftButton) {
        const int rowH = 28;
        int idx = event->pos().y() / rowH;
        impl_->hoveredLayerIndex = idx;
        update();  // Repaint to show selection
    }
    QWidget::mousePressEvent(event);
 }

 void ArtifactLayerPanelWidget::mouseMoveEvent(QMouseEvent* event)
 {
    const int rowH = 28;
    int idx = event->pos().y() / rowH;
    
    if (idx != impl_->hoveredLayerIndex) {
        impl_->hoveredLayerIndex = idx;
        update();  // Repaint to update hover highlight
    }
 }

 void ArtifactLayerPanelWidget::leaveEvent(QEvent* event)
 {
    if (impl_->hoveredLayerIndex >= 0) {
        impl_->hoveredLayerIndex = -1;
        update();
    }
    QWidget::leaveEvent(event);
 }

  void ArtifactLayerPanelWidget::paintEvent(QPaintEvent* event)
 {
  QPainter p(this);
  const int rowH = 28; // 行の高さ
  const int iconSize = 16;
  const int iconSpacing = 4;
  const int leftPadding = 4;
  
  // 背景を塗りつぶし
  for (int i = 0; i * rowH < height(); ++i) {
   int y = i * rowH;
   
   // ホバーハイライト
   if (i == impl_->hoveredLayerIndex) {
	p.fillRect(0, y, width(), rowH, QColor(55, 55, 80));  // 青系のハイライト
   }
   // 偶数行と奇数行で色を分ける
   else if (i % 2 == 0) {
	p.fillRect(0, y, width(), rowH, QColor(42, 42, 42));
   } else {
	p.fillRect(0, y, width(), rowH, QColor(45, 45, 45));
   }
   
   // 区切り線
   p.setPen(QColor(60, 60, 60));
   p.drawLine(0, y + rowH, width(), y + rowH);
  }

  // デバッグ出力
  qDebug() << "[paintEvent] compositionId.isNil()=" << impl_->compositionId.isNil();
  
  if (!impl_->compositionId.isNil()) {
    auto compResult = ArtifactProjectService::instance()->findComposition(impl_->compositionId);
    qDebug() << "[paintEvent] findComposition.success=" << compResult.success;
    
    if (compResult.success) {
      auto compShared = compResult.ptr.lock();
      qDebug() << "[paintEvent] compShared valid=" << (compShared != nullptr);
      
      if (compShared) {
        QVector<ArtifactAbstractLayerPtr> layers = compShared->allLayer();
        qDebug() << "[paintEvent] Rendering" << layers.size() << "layers";
        
        // After Effects風にレイヤーを逆順で描画（最後のレイヤーが最も上に表示）
        for (int idx = 0; idx < layers.size(); ++idx) {
          int y = idx * rowH;
          auto layer = layers[idx];
          
          if (layer) {
            QString layerName = layer->layerName();
            qDebug() << "[paintEvent] Drawing layer" << idx << ":" << layerName;
            
            int currentX = leftPadding;
            
            // 1. 可視性アイコン（目のアイコン）
            if (!impl_->visibilityIcon.isNull()) {
              p.drawPixmap(currentX, y + (rowH - iconSize) / 2, iconSize, iconSize, impl_->visibilityIcon);
            } else {
              // プレースホルダーボックス
              p.setPen(QPen(QColor(100, 100, 100), 1));
              p.drawRect(currentX, y + (rowH - iconSize) / 2, iconSize, iconSize);
            }
            currentX += iconSize + iconSpacing;
            
            // 2. ロック状態アイコン（鍵のアイコン）
            // TODO: レイヤーのロック状態を取得して描画
            if (!impl_->lockIcon.isNull()) {
              p.drawPixmap(currentX, y + (rowH - iconSize) / 2, iconSize, iconSize, impl_->lockIcon);
            }
            currentX += iconSize + iconSpacing;
            
            // 3. レイヤータイプアイコン（画像/動画/テキスト等）
            // TODO: レイヤータイプに応じたアイコンを描画
            
            // 4. レイヤー名テキスト
            p.setPen(Qt::white);
            QFont font = p.font();
            font.setPointSize(9);
            p.setFont(font);
            p.drawText(QRect(currentX, y, width() - currentX - leftPadding, rowH), 
                      Qt::AlignVCenter | Qt::AlignLeft, layerName);
            
            // 区切り線
            p.setPen(QColor(60, 60, 60));
            p.drawLine(0, y + rowH, width(), y + rowH);
          }
        }
      } else {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, "No composition loaded");
      }
    } else {
      p.setPen(Qt::gray);
      p.drawText(rect(), Qt::AlignCenter, "Composition not found");
    }
  } else {
    p.setPen(Qt::gray);
    p.drawText(rect(), Qt::AlignCenter, "No composition");
  }
 }

 void ArtifactLayerPanelWidget::dragEnterEvent(QDragEnterEvent* event)
 {
  // Accept layer ID, asset ID, and file URLs
  if (event->mimeData()->hasFormat("application/x-artifact-layerid") ||
      event->mimeData()->hasFormat("application/x-artifact-assetid") ||
      event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
 }

 void ArtifactLayerPanelWidget::dragMoveEvent(QDragMoveEvent* event)
 {
  // Keep accepting while dragging
  if (event->mimeData()->hasFormat("application/x-artifact-layerid") ||
      event->mimeData()->hasFormat("application/x-artifact-assetid") ||
      event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
 }

 void ArtifactLayerPanelWidget::dragLeaveEvent(QDragLeaveEvent* event)
 {
  // Optional: Could reset visual feedback here
  event->accept();
 }

 void ArtifactLayerPanelWidget::dropEvent(QDropEvent* event)
 {
  const QMimeData* mimeData = event->mimeData();

  // Handle LayerID drop
  if (mimeData->hasFormat("application/x-artifact-layerid")) {
   QByteArray data = mimeData->data("application/x-artifact-layerid");
   QString layerIdStr = QString::fromUtf8(data);
   LayerID layerId(layerIdStr);
   
   qDebug() << "[ArtifactLayerPanelWidget::dropEvent] Received LayerID:" << layerIdStr;
   
   // TODO: Handle layer drop - could be layer relocation, layer copy, etc.
   event->acceptProposedAction();
  }
  // Handle AssetID drop
  else if (mimeData->hasFormat("application/x-artifact-assetid")) {
   QByteArray data = mimeData->data("application/x-artifact-assetid");
   QString assetIdStr = QString::fromUtf8(data);
   
   qDebug() << "[ArtifactLayerPanelWidget::dropEvent] Received AssetID:" << assetIdStr;
   
   // TODO: Handle asset drop - could create new layer from asset
   event->acceptProposedAction();
  }
  // Handle file URL drop
  else if (mimeData->hasUrls()) {
   const QList<QUrl> urls = mimeData->urls();
   for (const QUrl& url : urls) {
    if (url.isLocalFile()) {
     QString filePath = url.toLocalFile();
     qDebug() << "[ArtifactLayerPanelWidget::dropEvent] Received file:" << filePath;
     
     // TODO: Convert to UniString and handle file import
     // Could create a new layer from the dropped file
    }
   }
   event->acceptProposedAction();
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



