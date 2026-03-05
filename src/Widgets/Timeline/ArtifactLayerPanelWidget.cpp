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

 namespace {
  std::shared_ptr<ArtifactAbstractComposition> safeCompositionLookup(const CompositionID& id)
  {
    if (id.isNil()) return nullptr;
    auto* service = ArtifactProjectService::instance();
    if (!service) return nullptr;
    auto result = service->findComposition(id);
    if (!result.success) return nullptr;
    return result.ptr.lock();
  }
 }

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

 int ArtifactLayerPanelHeaderWidget::buttonSize() const
 {
  return 28;  // Fixed button size defined in constructor
 }

 int ArtifactLayerPanelHeaderWidget::iconSize() const
 {
  return 16;  // Icon size used within buttons
 }

 int ArtifactLayerPanelHeaderWidget::totalHeaderHeight() const
 {
  return height();  // Returns the actual widget height
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
  LayerID selectedLayerId;     // 選択されたレイヤーID
 };

 ArtifactLayerPanelWidget::Impl::Impl()
 {
  visibilityIcon = QPixmap(getIconPath() + "/Png/visibility.png");
  visibilityIcon = visibilityIcon.scaled(28,28, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  lockIcon = QPixmap(getIconPath() + "/Png/lock.png");
  if (!lockIcon.isNull()) {
    lockIcon = lockIcon.scaled(28,28, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }

  soloIcon = QPixmap(getIconPath() + "/Png/solo.png");
  if (!soloIcon.isNull()) {
    soloIcon = soloIcon.scaled(28,28, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }
 }

 ArtifactLayerPanelWidget::Impl::~Impl()
 {

 }

ArtifactLayerPanelWidget::ArtifactLayerPanelWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl)
{
 setWindowTitle("ArtifactLayerPanel");
 setAcceptDrops(true);

 if (auto* service = ArtifactProjectService::instance()) {
   QObject::connect(service, &ArtifactProjectService::layerCreated, this, [this](const LayerID&) {
     update();
   });
   QObject::connect(service, &ArtifactProjectService::layerRemoved, this, [this](const LayerID&) {
     update();
   });
   QObject::connect(service, &ArtifactProjectService::layerSelected, this, [this](const LayerID& layerId) {
     if (impl_->selectedLayerId != layerId) {
         impl_->selectedLayerId = layerId;
         update();
     }
   });
 }
}

void ArtifactLayerPanelWidget::setComposition(const CompositionID& id)
{
    impl_->compositionId = id;
    impl_->selectedLayerId = LayerID();  // Reset selection on comp change
    // trigger repaint
    update();
}

 ArtifactLayerPanelWidget::~ArtifactLayerPanelWidget()
 {
  delete impl_;
 }

 void ArtifactLayerPanelWidget::mousePressEvent(QMouseEvent* event)
 {
    const int rowH = 28;
    const int colW = 28;

    int idx = event->pos().y() / rowH;
    int clickX = event->pos().x();

    auto comp = safeCompositionLookup(impl_->compositionId);
    if (!comp) {
        QWidget::mousePressEvent(event);
        return;
    }

    const auto layers = comp->allLayer();
    if (idx < 0 || idx >= layers.size()) {
        QWidget::mousePressEvent(event);
        return;
    }

    auto layer = layers[idx];
    if (!layer) {
        QWidget::mousePressEvent(event);
        return;
    }

    // Column 0: Visibility
    if (event->button() == Qt::LeftButton && clickX >= 0 && clickX < colW) {
        bool currentVisibility = layer->isVisible();
        layer->setVisible(!currentVisibility);
        update();
        event->accept();
        return;
    }

    // Column 1: Lock
    if (event->button() == Qt::LeftButton && clickX >= colW && clickX < colW * 2) {
        // TODO: Toggle lock state
        event->accept();
        return;
    }

    // Column 2: Solo
    if (event->button() == Qt::LeftButton && clickX >= colW * 2 && clickX < colW * 3) {
        // TODO: Toggle solo state
        event->accept();
        return;
    }

    // Column 3: Sound
    if (event->button() == Qt::LeftButton && clickX >= colW * 3 && clickX < colW * 4) {
        // TODO: Toggle sound state
        event->accept();
        return;
    }

    // Right-click -> context menu
    if (event->button() == Qt::RightButton) {
        QMenu menu(this);
        QAction* del = menu.addAction("Delete Layer");
        QAction* rename = menu.addAction("Rename Layer");
        QAction* duplicate = menu.addAction("Duplicate Layer");

        QAction* act = menu.exec(event->globalPosition().toPoint());
        if (act == del) {
            if (auto* service = ArtifactProjectService::instance()) {
                service->removeLayerFromComposition(impl_->compositionId, layer->id());
            }
        } else if (act == rename) {
            // TODO: Implement rename
        } else if (act == duplicate) {
            // TODO: Implement duplicate
        }
        event->accept();
        return;
    }

    // Left-click on layer name or other columns -> select layer
    if (event->button() == Qt::LeftButton) {
        impl_->hoveredLayerIndex = idx; // Used temporarily as hover highlight for now, keep it visible
        if (auto* service = ArtifactProjectService::instance()) {
            service->selectLayer(layer->id());
        }
        update();
    }

    QWidget::mousePressEvent(event);
 }

 void ArtifactLayerPanelWidget::mouseMoveEvent(QMouseEvent* event)
 {
    const int rowH = 28;
    const int colW = 28;

    int idx = event->pos().y() / rowH;
    int mouseX = event->pos().x();

    if (idx != impl_->hoveredLayerIndex) {
        impl_->hoveredLayerIndex = idx;
        update();
    }

    // Pointer cursor on icon columns
    if (mouseX >= 0 && mouseX < colW * 4) {
        setCursor(Qt::PointingHandCursor);
    } else {
        setCursor(Qt::ArrowCursor);
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
  const int rowH = 28; 
  const int colW = 28;
  const int iconSize = 16;
  
  auto compShared = safeCompositionLookup(impl_->compositionId);
  QVector<ArtifactAbstractLayerPtr> layers;
  if (compShared) {
      layers = compShared->allLayer();
  }

  // 背景描画
  for (int i = 0; i * rowH < height(); ++i) {
   int y = i * rowH;
   
   bool isSelected = false;
   if (i < layers.size() && layers[i]) {
       isSelected = (layers[i]->id() == impl_->selectedLayerId);
   }

   if (isSelected) {
       p.fillRect(0, y, width(), rowH, QColor(70, 100, 150)); // Selected color
   } else if (i == impl_->hoveredLayerIndex) {
       p.fillRect(0, y, width(), rowH, QColor(55, 55, 80));  // Hover color
   } else if (i % 2 == 0) {
       p.fillRect(0, y, width(), rowH, QColor(42, 42, 42));  // Even row
   } else {
       p.fillRect(0, y, width(), rowH, QColor(45, 45, 45));  // Odd row
   }
   
   p.setPen(QColor(60, 60, 60));
   p.drawLine(0, y + rowH, width(), y + rowH);
  }

  if (compShared) {
    for (int idx = 0; idx < layers.size(); ++idx) {
      int y = idx * rowH;
      auto layer = layers[idx];
      if (!layer) continue;

      QString layerName = layer->layerName();
      if (layerName.isEmpty()) layerName = QString("Layer %1").arg(idx + 1);

      int currentX = 0;
      int iconOffset = (colW - iconSize) / 2;

      // Visibility Icon (Col 0)
      bool isVisible = layer->isVisible();
      if (!impl_->visibilityIcon.isNull()) {
        p.setOpacity(isVisible ? 1.0 : 0.3);
        p.drawPixmap(currentX + iconOffset, y + (rowH - iconSize) / 2, iconSize, iconSize, impl_->visibilityIcon);
        p.setOpacity(1.0);
      } else {
        QColor visColor = isVisible ? QColor(100, 200, 100) : QColor(80, 80, 80);
        p.fillRect(currentX + iconOffset, y + (rowH - iconSize) / 2, iconSize, iconSize, visColor);
      }
      
      // Draw vertical separator
      p.setPen(QPen(QColor(60, 60, 60), 1));
      p.drawLine(currentX + colW - 1, y, currentX + colW - 1, y + rowH);
      currentX += colW;

      // Lock Icon (Col 1)
      if (!impl_->lockIcon.isNull()) {
        p.drawPixmap(currentX + iconOffset, y + (rowH - iconSize) / 2, iconSize, iconSize, impl_->lockIcon);
      }
      p.drawLine(currentX + colW - 1, y, currentX + colW - 1, y + rowH);
      currentX += colW;

      // Solo Icon (Col 2)
      if (!impl_->soloIcon.isNull()) {
        p.drawPixmap(currentX + iconOffset, y + (rowH - iconSize) / 2, iconSize, iconSize, impl_->soloIcon);
      }
      p.drawLine(currentX + colW - 1, y, currentX + colW - 1, y + rowH);
      currentX += colW;

      // Sound Icon (Col 3) - Empty placeholder for now
      p.drawLine(currentX + colW - 1, y, currentX + colW - 1, y + rowH);
      currentX += colW;

      // Layer Name (Col 4 onwards)
      p.setPen(Qt::white);
      QFont font = p.font();
      font.setPointSize(10);
      font.setBold(false);
      p.setFont(font);

      int textX = currentX + 8; // Small padding
      int textWidth = width() - textX;
      QRect textRect(textX, y, textWidth, rowH);

      p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, layerName);
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

  // Set panel minimum width/height
  impl_->panel->setMinimumHeight(100);
  impl_->scroll->setWidget(impl_->panel);
  impl_->scroll->setWidgetResizable(true);
  impl_->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(impl_->header);
  layout->addWidget(impl_->scroll, 1);  // Add stretch factor to fill remaining space
  setLayout(layout);

  // Set header height based on button size
  int headerHeight = impl_->header->buttonSize();
  impl_->header->setFixedHeight(headerHeight);
  qDebug() << "[ArtifactLayerTimelinePanelWrapper] Header height set to:" << headerHeight;
 }

 ArtifactLayerTimelinePanelWrapper::ArtifactLayerTimelinePanelWrapper(const CompositionID& id, QWidget* parent /*= nullptr*/):QWidget(parent),impl_(new Impl())
 {
    // Initialize children and set the composition id
    impl_->header = new ArtifactLayerPanelHeaderWidget();
    impl_->panel = new ArtifactLayerPanelWidget;
    impl_->scroll = new QScrollArea(this);

    // Set panel minimum width/height
    impl_->panel->setMinimumHeight(100);
    impl_->scroll->setWidget(impl_->panel);
    impl_->scroll->setWidgetResizable(true);
    impl_->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(impl_->header);
    layout->addWidget(impl_->scroll, 1);  // Add stretch factor to fill remaining space
    setLayout(layout);

    // Set header height based on button size
    int headerHeight = impl_->header->buttonSize();
    impl_->header->setFixedHeight(headerHeight);
    qDebug() << "[ArtifactLayerTimelinePanelWrapper] Header height set to:" << headerHeight;

    if (!id.isNil() && ArtifactProjectService::instance()) {
        impl_->panel->setComposition(id);
        impl_->id = id;
    }
 }

 ArtifactLayerTimelinePanelWrapper::~ArtifactLayerTimelinePanelWrapper()
 {
  delete impl_;
 }

 void ArtifactLayerTimelinePanelWrapper::setComposition(const CompositionID& id)
 {
  impl_->id = id;
  if (impl_->panel) {
   impl_->panel->setComposition(id);
  }
 }

 QScrollBar* ArtifactLayerTimelinePanelWrapper::verticalScrollBar() const
 {
  if (impl_->scroll) {
   return impl_->scroll->verticalScrollBar();
  }
  return nullptr;
 }



}



