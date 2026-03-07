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
#include <QFileInfo>
module Artifact.Widgets.LayerPanelWidget;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>



import Utils.Path;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import File.TypeDetector;

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

  LayerType inferLayerTypeFromFile(const QString& filePath)
  {
    ArtifactCore::FileTypeDetector detector;
    const auto type = detector.detect(filePath);
    switch (type) {
    case ArtifactCore::FileType::Image:
      return LayerType::Image;
    case ArtifactCore::FileType::Video:
      return LayerType::Video;
    case ArtifactCore::FileType::Audio:
      return LayerType::Audio;
    default:
      return LayerType::Media;
    }
  }
 }

 // ============================================================================
 // ArtifactLayerPanelHeaderWidget Implementation
 // ============================================================================

 class ArtifactLayerPanelHeaderWidget::Impl
 {
 public:
  Impl()
  {
    visibilityIcon = QPixmap(resolveIconPath("visibility.png"));
    lockIcon = QPixmap(resolveIconPath("lock.png"));
    if (lockIcon.isNull()) lockIcon = QPixmap(resolveIconPath("unlock.png"));
    soloIcon = QPixmap(resolveIconPath("solo.png"));
  }
  ~Impl() = default;

  QPixmap visibilityIcon;
  QPixmap lockIcon;
  QPixmap soloIcon;
  
  QPushButton* visibilityButton = nullptr;
  QPushButton* lockButton = nullptr;
  QPushButton* soloButton = nullptr;
  QPushButton* soundButton = nullptr;
  QPushButton* layerNameButton = nullptr;
  QPushButton* shyButton = nullptr;
 };

 W_OBJECT_IMPL(ArtifactLayerPanelHeaderWidget)

 ArtifactLayerPanelHeaderWidget::ArtifactLayerPanelHeaderWidget(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  auto visButton = impl_->visibilityButton = new QPushButton();
  visButton->setFixedSize(QSize(28, 28));
  visButton->setIcon(impl_->visibilityIcon);
  visButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");
  visButton->setFlat(true);
  
  auto lockButton = impl_->lockButton = new QPushButton();
  lockButton->setFixedSize(QSize(28, 28));
  if (!impl_->lockIcon.isNull()) lockButton->setIcon(impl_->lockIcon);
  lockButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");

  auto soloButton = impl_->soloButton = new QPushButton();
  soloButton->setFixedSize(QSize(28, 28));
  if (!impl_->soloIcon.isNull()) soloButton->setIcon(impl_->soloIcon);
  soloButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");

  auto soundButton = impl_->soundButton = new QPushButton();
  soundButton->setFixedSize(QSize(28, 28));
  soundButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");

  auto shyButton = impl_->shyButton = new QPushButton;
  shyButton->setFixedSize(QSize(28, 28));
  shyButton->setCheckable(true);
  shyButton->setToolTip("Master Shy Switch");
  shyButton->setStyleSheet("QPushButton { background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a; } QPushButton:checked { background-color: #3b3bef; }");

  auto layerNameButton = impl_->layerNameButton = new QPushButton("Layer Name");
  QString btnStyle = "QPushButton { background-color: #2D2D30; color: #CCC; border: none; border-right: 1px solid #1a1a1a; font-size: 11px; text-align: left; padding-left: 5px; }";
  layerNameButton->setStyleSheet(btnStyle);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(visButton);
  layout->addWidget(lockButton);
  layout->addWidget(soloButton);
  layout->addWidget(soundButton);
  layout->addWidget(shyButton);
  layout->addWidget(layerNameButton, 1);

  QObject::connect(shyButton, &QPushButton::toggled, this, [this](bool checked) {
    Q_EMIT shyToggled(checked);
  });

  setStyleSheet("background-color: #2D2D30; border-bottom: 1px solid #1a1a1a;");
  setFixedHeight(28);
 }

 ArtifactLayerPanelHeaderWidget::~ArtifactLayerPanelHeaderWidget()
 {
  delete impl_;
 }

 int ArtifactLayerPanelHeaderWidget::buttonSize() const { return 28; }
 int ArtifactLayerPanelHeaderWidget::iconSize() const { return 16; }
 int ArtifactLayerPanelHeaderWidget::totalHeaderHeight() const { return height(); }

 // ============================================================================
 // ArtifactLayerPanelWidget Implementation
 // ============================================================================

 class ArtifactLayerPanelWidget::Impl
 {
 public:
  Impl()
  {
    visibilityIcon = QPixmap(resolveIconPath("visibility.png"));
    lockIcon = QPixmap(resolveIconPath("lock.png"));
    if (lockIcon.isNull()) lockIcon = QPixmap(resolveIconPath("unlock.png"));
    soloIcon = QPixmap(resolveIconPath("solo.png"));
  }
  ~Impl() = default;

  CompositionID compositionId;
  QPixmap visibilityIcon;
  QPixmap lockIcon;
  QPixmap soloIcon;
  bool shyHidden = false;
  int hoveredLayerIndex = -1;
  LayerID selectedLayerId;
 };

 W_OBJECT_IMPL(ArtifactLayerPanelWidget)

 ArtifactLayerPanelWidget::ArtifactLayerPanelWidget(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  setMouseTracking(true);
  setAcceptDrops(true);

  if (auto* service = ArtifactProjectService::instance()) {
    QObject::connect(service, &ArtifactProjectService::layerCreated, this, [this](const CompositionID& compId, const LayerID&) {
      if (impl_->compositionId == compId) this->updateLayout();
    });
    QObject::connect(service, &ArtifactProjectService::layerRemoved, this, [this](const CompositionID& compId, const LayerID&) {
      if (impl_->compositionId == compId) this->updateLayout();
    });
    QObject::connect(service, &ArtifactProjectService::layerSelected, this, [this](const LayerID& layerId) {
      if (impl_->selectedLayerId != layerId) {
        impl_->selectedLayerId = layerId;
        update();
      }
    });
  }
 }

 ArtifactLayerPanelWidget::~ArtifactLayerPanelWidget()
 {
  delete impl_;
 }

 void ArtifactLayerPanelWidget::setComposition(const CompositionID& id)
 {
  impl_->compositionId = id;
  impl_->selectedLayerId = LayerID();
  updateLayout();
 }

 void ArtifactLayerPanelWidget::setShyHidden(bool hidden)
 {
  impl_->shyHidden = hidden;
  updateLayout();
 }

 void ArtifactLayerPanelWidget::updateLayout()
 {
  auto comp = safeCompositionLookup(impl_->compositionId);
  if (!comp) return;

  auto all = comp->allLayer();
  int count = 0;
  if (impl_->shyHidden) {
    for (auto& l : all) if (l && !l->isShy()) count++;
  } else {
    count = all.size();
  }
  setMinimumHeight(std::max(100, count * 28));
  update();
 }

 void ArtifactLayerPanelWidget::mousePressEvent(QMouseEvent* event)
 {
  const int rowH = 28;
  const int colW = 28;
  int idx = event->pos().y() / rowH;
  int clickX = event->pos().x();

  auto comp = safeCompositionLookup(impl_->compositionId);
  if (!comp) return;

  auto all = comp->allLayer();
  QVector<ArtifactAbstractLayerPtr> layers;
  if (impl_->shyHidden) {
    for (auto& l : all) if (l && !l->isShy()) layers.push_back(l);
  } else {
    layers = all;
  }

  if (idx < 0 || idx >= layers.size()) return;
  auto layer = layers[idx];
  if (!layer) return;

  if (event->button() == Qt::LeftButton) {
    if (clickX < colW) {
      layer->setVisible(!layer->isVisible());
    } else if (clickX < colW * 2) {
      layer->setLocked(!layer->isLocked());
    } else if (clickX < colW * 3) {
      layer->setSolo(!layer->isSolo());
    } else if (clickX < colW * 4) {
      // Sound toggle
    } else if (clickX < colW * 5) {
      layer->setShy(!layer->isShy());
    } else if (clickX < colW * 6) {
      layer->setGuide(!layer->isGuide());
    } else {
      if (auto* service = ArtifactProjectService::instance()) {
        service->selectLayer(layer->id());
      }
    }
    update();
  } else if (event->button() == Qt::RightButton) {
    QMenu menu(this);
    QAction* del = menu.addAction("Delete Layer");
    if (menu.exec(event->globalPosition().toPoint()) == del) {
      if (auto* service = ArtifactProjectService::instance()) {
        service->removeLayerFromComposition(impl_->compositionId, layer->id());
      }
    }
  }
  event->accept();
 }

 void ArtifactLayerPanelWidget::mouseMoveEvent(QMouseEvent* event)
 {
  int idx = event->pos().y() / 28;
  if (idx != impl_->hoveredLayerIndex) {
    impl_->hoveredLayerIndex = idx;
    update();
  }
  setCursor(event->pos().x() < 28 * 6 ? Qt::PointingHandCursor : Qt::ArrowCursor);
 }

 void ArtifactLayerPanelWidget::leaveEvent(QEvent*)
 {
  impl_->hoveredLayerIndex = -1;
  update();
 }

 void ArtifactLayerPanelWidget::paintEvent(QPaintEvent*)
 {
  QPainter p(this);
  const int rowH = 28;
  const int colW = 28;
  const int iconSize = 16;
  const int offset = (colW - iconSize) / 2;

  auto comp = safeCompositionLookup(impl_->compositionId);
  if (!comp) return;

  auto all = comp->allLayer();
  QVector<ArtifactAbstractLayerPtr> layers;
  if (impl_->shyHidden) {
    for (auto& l : all) if (l && !l->isShy()) layers.push_back(l);
  } else {
    layers = all;
  }

  for (int i = 0; i < layers.size(); ++i) {
    int y = i * rowH;
    auto l = layers[i];
    bool sel = (l->id() == impl_->selectedLayerId);

    if (sel) p.fillRect(0, y, width(), rowH, QColor(70, 100, 150));
    else if (i == impl_->hoveredLayerIndex) p.fillRect(0, y, width(), rowH, QColor(55, 55, 80));
    else p.fillRect(0, y, width(), rowH, (i % 2 == 0) ? QColor(42, 42, 42) : QColor(45, 45, 45));

    p.setPen(QColor(60, 60, 60));
    p.drawLine(0, y + rowH, width(), y + rowH);

    int curX = 0;
    // Visibility
    p.setOpacity(l->isVisible() ? 1.0 : 0.3);
    if (!impl_->visibilityIcon.isNull()) p.drawPixmap(curX + offset, y + offset, iconSize, iconSize, impl_->visibilityIcon);
    else p.fillRect(curX + offset, y + offset, iconSize, iconSize, Qt::green);
    curX += colW;
    p.drawLine(curX - 1, y, curX - 1, y + rowH);

    // Lock
    bool locked = l->isLocked();
    p.setOpacity(locked ? 1.0 : 0.15);
    if (!impl_->lockIcon.isNull()) p.drawPixmap(curX + offset, y + offset, iconSize, iconSize, impl_->lockIcon);
    else if (locked) p.fillRect(curX + offset + 4, y + offset + 4, 8, 8, Qt::red);
    curX += colW;
    p.drawLine(curX - 1, y, curX - 1, y + rowH);

    // Solo
    bool solo = l->isSolo();
    p.setOpacity(solo ? 1.0 : 0.15);
    if (!impl_->soloIcon.isNull()) p.drawPixmap(curX + offset, y + offset, iconSize, iconSize, impl_->soloIcon);
    else if (solo) p.fillRect(curX + offset + 4, y + offset + 4, 8, 8, Qt::yellow);
    curX += colW;
    p.drawLine(curX - 1, y, curX - 1, y + rowH);

    // Sound
    curX += colW;
    p.drawLine(curX - 1, y, curX - 1, y + rowH);

    // Shy
    bool shy = l->isShy();
    p.setOpacity(1.0);
    if (shy) p.fillRect(curX + offset + 4, y + offset + 4, 8, 8, QColor(100, 100, 255));
    else { p.setPen(QColor(80, 80, 80)); p.drawRect(curX + offset + 4, y + offset + 4, 8, 8); }
    curX += colW;
    p.drawLine(curX - 1, y, curX - 1, y + rowH);

    // Guide
    bool guide = l->isGuide();
    if (guide) p.fillRect(curX + offset + 4, y + offset + 4, 8, 8, QColor(50, 200, 50));
    else { p.setPen(QColor(80, 80, 80)); p.drawRect(curX + offset + 4, y + offset + 4, 8, 8); }
    curX += colW;
    p.drawLine(curX - 1, y, curX - 1, y + rowH);

    // Name
    p.setPen(Qt::white);
    p.drawText(curX + 8, y, width() - curX - 8, rowH, Qt::AlignVCenter | Qt::AlignLeft, l->layerName());
  }
 }

 void ArtifactLayerPanelWidget::dragEnterEvent(QDragEnterEvent* e) { e->acceptProposedAction(); }
 void ArtifactLayerPanelWidget::dragMoveEvent(QDragMoveEvent* e) { e->acceptProposedAction(); }
 void ArtifactLayerPanelWidget::dragLeaveEvent(QDragLeaveEvent* e) { e->accept(); }
 void ArtifactLayerPanelWidget::dropEvent(QDropEvent* event)
 {
  const QMimeData* mime = event->mimeData();
  if (mime->hasUrls()) {
    QStringList paths;
    for (auto& url : mime->urls()) if (url.isLocalFile()) paths.append(url.toLocalFile());
    if (auto* svc = ArtifactProjectService::instance()) {
      auto imported = svc->importAssetsFromPaths(paths);
      for (auto& path : imported) {
        LayerType type = inferLayerTypeFromFile(path);
        ArtifactLayerInitParams p(QFileInfo(path).baseName(), type);
        svc->addLayerToCurrentComposition(p);
      }
    }
    event->acceptProposedAction();
  }
 }

 // ============================================================================
 // ArtifactLayerTimelinePanelWrapper Implementation
 // ============================================================================

 class ArtifactLayerTimelinePanelWrapper::Impl
 {
 public:
  QScrollArea* scroll = nullptr;
  ArtifactLayerPanelHeaderWidget* header = nullptr;
  ArtifactLayerPanelWidget* panel = nullptr;
  CompositionID id;
 };

 ArtifactLayerTimelinePanelWrapper::ArtifactLayerTimelinePanelWrapper(QWidget* parent)
  : QWidget(parent), impl_(new Impl)
 {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);

  impl_->header = new ArtifactLayerPanelHeaderWidget();
  impl_->panel = new ArtifactLayerPanelWidget();
  impl_->scroll = new QScrollArea();
  impl_->scroll->setWidget(impl_->panel);
  impl_->scroll->setWidgetResizable(true);
  impl_->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  impl_->scroll->setFrameShape(QFrame::NoFrame);

  layout->addWidget(impl_->header);
  layout->addWidget(impl_->scroll, 1);

  QObject::connect(impl_->header, &ArtifactLayerPanelHeaderWidget::shyToggled,
                   impl_->panel, &ArtifactLayerPanelWidget::setShyHidden);
 }

 ArtifactLayerTimelinePanelWrapper::ArtifactLayerTimelinePanelWrapper(const CompositionID& id, QWidget* parent)
  : ArtifactLayerTimelinePanelWrapper(parent)
 {
  setComposition(id);
 }

 ArtifactLayerTimelinePanelWrapper::~ArtifactLayerTimelinePanelWrapper()
 {
  delete impl_;
 }

 void ArtifactLayerTimelinePanelWrapper::setComposition(const CompositionID& id)
 {
  impl_->id = id;
  impl_->panel->setComposition(id);
 }

 QScrollBar* ArtifactLayerTimelinePanelWrapper::verticalScrollBar() const
 {
  return impl_->scroll->verticalScrollBar();
 }

} // namespace Artifact
