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
#include <QHash>
#include <QSet>
#include <QPolygon>
#include <QComboBox>
#include <QPointer>
#include <QLineEdit>
#include <QKeyEvent>
#include <QInputDialog>
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
module Artifact.Widgets.LayerPanelWidget;




import Utils.Path;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Layer.Blend;
import Artifact.Layer.InitParams;
import File.TypeDetector;

namespace Artifact
{
 using namespace ArtifactCore;
namespace {
  constexpr int kLayerRowHeight = 28;
  constexpr int kLayerHeaderHeight = 34;
  constexpr int kLayerColumnWidth = 28;
  constexpr int kLayerPropertyColumnCount = 5;
  constexpr int kInlineComboHeight = 24;
  constexpr int kInlineBlendWidth = 120;
  constexpr int kInlineParentWidth = 150;
  constexpr int kInlineComboGap = 6;
  constexpr int kInlineComboMarginY = 2;
  constexpr int kInlineComboReserve = kInlineParentWidth + kInlineBlendWidth + kInlineComboGap + 10;
  constexpr int kLayerNameMinWidth = 120;
 }

 namespace {
  std::shared_ptr<ArtifactAbstractComposition> safeCompositionLookup(const CompositionID& id)
  {
    auto* service = ArtifactProjectService::instance();
    if (!service) return nullptr;

    if (!id.isNil()) {
      auto result = service->findComposition(id);
      if (result.success) {
        if (auto comp = result.ptr.lock()) {
          return comp;
        }
      }
    }

    return service->currentComposition().lock();
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

  QString blendModeToText(const LAYER_BLEND_TYPE mode)
  {
    switch (mode) {
    case LAYER_BLEND_TYPE::BLEND_NORMAL: return QStringLiteral("Normal");
    case LAYER_BLEND_TYPE::BLEND_ADD: return QStringLiteral("Add");
    case LAYER_BLEND_TYPE::BLEND_MULTIPLY: return QStringLiteral("Multiply");
    case LAYER_BLEND_TYPE::BLEND_SCREEN: return QStringLiteral("Screen");
    case LAYER_BLEND_TYPE::BLEND_OVERLAY: return QStringLiteral("Overlay");
    case LAYER_BLEND_TYPE::BLEND_DARKEN: return QStringLiteral("Darken");
    case LAYER_BLEND_TYPE::BLEND_LIGHTEN: return QStringLiteral("Lighten");
    case LAYER_BLEND_TYPE::BLEND_COLOR_DODGE: return QStringLiteral("Color Dodge");
    case LAYER_BLEND_TYPE::BLEND_COLOR_BURN: return QStringLiteral("Color Burn");
    case LAYER_BLEND_TYPE::BLEND_HARD_LIGHT: return QStringLiteral("Hard Light");
    case LAYER_BLEND_TYPE::BLEND_SOFT_LIGHT: return QStringLiteral("Soft Light");
    case LAYER_BLEND_TYPE::BLEND_DIFFERENCE: return QStringLiteral("Difference");
    case LAYER_BLEND_TYPE::BLEND_EXCLUSION: return QStringLiteral("Exclusion");
    case LAYER_BLEND_TYPE::BLEND_HUE: return QStringLiteral("Hue");
    case LAYER_BLEND_TYPE::BLEND_SATURATION: return QStringLiteral("Saturation");
    case LAYER_BLEND_TYPE::BLEND_COLOR: return QStringLiteral("Color");
    case LAYER_BLEND_TYPE::BLEND_LUMINOSITY: return QStringLiteral("Luminosity");
    default: return QStringLiteral("Unknown");
    }
  }

  std::vector<std::pair<QString, LAYER_BLEND_TYPE>> blendModeItems()
  {
    return {
      {QStringLiteral("Normal"), LAYER_BLEND_TYPE::BLEND_NORMAL},
      {QStringLiteral("Add"), LAYER_BLEND_TYPE::BLEND_ADD},
      {QStringLiteral("Multiply"), LAYER_BLEND_TYPE::BLEND_MULTIPLY},
      {QStringLiteral("Screen"), LAYER_BLEND_TYPE::BLEND_SCREEN},
      {QStringLiteral("Overlay"), LAYER_BLEND_TYPE::BLEND_OVERLAY},
      {QStringLiteral("Darken"), LAYER_BLEND_TYPE::BLEND_DARKEN},
      {QStringLiteral("Lighten"), LAYER_BLEND_TYPE::BLEND_LIGHTEN},
      {QStringLiteral("Color Dodge"), LAYER_BLEND_TYPE::BLEND_COLOR_DODGE},
      {QStringLiteral("Color Burn"), LAYER_BLEND_TYPE::BLEND_COLOR_BURN},
      {QStringLiteral("Hard Light"), LAYER_BLEND_TYPE::BLEND_HARD_LIGHT},
      {QStringLiteral("Soft Light"), LAYER_BLEND_TYPE::BLEND_SOFT_LIGHT},
      {QStringLiteral("Difference"), LAYER_BLEND_TYPE::BLEND_DIFFERENCE},
      {QStringLiteral("Exclusion"), LAYER_BLEND_TYPE::BLEND_EXCLUSION},
      {QStringLiteral("Hue"), LAYER_BLEND_TYPE::BLEND_HUE},
      {QStringLiteral("Saturation"), LAYER_BLEND_TYPE::BLEND_SATURATION},
      {QStringLiteral("Color"), LAYER_BLEND_TYPE::BLEND_COLOR},
      {QStringLiteral("Luminosity"), LAYER_BLEND_TYPE::BLEND_LUMINOSITY}
    };
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
  QPushButton* parentHeaderButton = nullptr;
  QPushButton* blendHeaderButton = nullptr;
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
  auto parentHeader = impl_->parentHeaderButton = new QPushButton("Parent");
  parentHeader->setFixedWidth(kInlineParentWidth);
  parentHeader->setStyleSheet(btnStyle);
  auto blendHeader = impl_->blendHeaderButton = new QPushButton("Blend");
  blendHeader->setFixedWidth(kInlineBlendWidth);
  blendHeader->setStyleSheet(btnStyle);
  parentHeader->setEnabled(false);
  blendHeader->setEnabled(false);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(visButton);
  layout->addWidget(lockButton);
  layout->addWidget(soloButton);
  layout->addWidget(soundButton);
  layout->addWidget(shyButton);
  layout->addWidget(layerNameButton, 1);
  layout->addWidget(parentHeader);
  layout->addWidget(blendHeader);

  QObject::connect(shyButton, &QPushButton::toggled, this, [this](bool checked) {
    Q_EMIT shyToggled(checked);
  });

  setStyleSheet("background-color: #2D2D30; border-bottom: 1px solid #1a1a1a;");
  setFixedHeight(kLayerHeaderHeight);
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
  struct VisibleRow
  {
   ArtifactAbstractLayerPtr layer;
   int depth = 0;
   bool hasChildren = false;
   bool expanded = true;
  };

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
  QVector<VisibleRow> visibleRows;
  QHash<QString, bool> expandedByLayerId;
  QPointer<QComboBox> inlineParentEditor;
  QPointer<QComboBox> inlineBlendEditor;
  QPointer<QLineEdit> inlineNameEditor;
  LayerID editingLayerId;

  void clearInlineEditors()
  {
   if (inlineParentEditor) {
    inlineParentEditor->hide();
    inlineParentEditor->deleteLater();
    inlineParentEditor = nullptr;
   }
   if (inlineBlendEditor) {
    inlineBlendEditor->hide();
    inlineBlendEditor->deleteLater();
    inlineBlendEditor = nullptr;
   }
   if (inlineNameEditor) {
    inlineNameEditor->hide();
    inlineNameEditor->deleteLater();
    inlineNameEditor = nullptr;
   }
   editingLayerId = LayerID();
  }

  void rebuildVisibleRows()
  {
   visibleRows.clear();

   auto comp = safeCompositionLookup(compositionId);
   if (!comp) {
    return;
   }

   QVector<ArtifactAbstractLayerPtr> layers;
   for (auto& l : comp->allLayer()) {
    if (!l) continue;
    if (shyHidden && l->isShy()) continue;
    layers.push_back(l);
   }
   if (layers.isEmpty()) {
    return;
   }

   QHash<QString, ArtifactAbstractLayerPtr> byId;
   for (const auto& l : layers) {
    byId.insert(l->id().toString(), l);
   }

   QHash<QString, QVector<ArtifactAbstractLayerPtr>> children;
   QVector<ArtifactAbstractLayerPtr> roots;
   for (const auto& l : layers) {
    const QString parentId = l->parentLayerId().toString();
    if (parentId.isEmpty() || !byId.contains(parentId)) {
      roots.push_back(l);
    } else {
      children[parentId].push_back(l);
    }
   }

   QSet<QString> emitted;
   std::function<void(const ArtifactAbstractLayerPtr&, int, QSet<QString>&)> appendNode =
    [&](const ArtifactAbstractLayerPtr& node, int depth, QSet<QString>& stack) {
     if (!node) return;
     const QString nodeId = node->id().toString();
     if (stack.contains(nodeId)) return; // cycle guard
     if (emitted.contains(nodeId)) return;

     const auto nodeChildren = children.value(nodeId);
     const bool hasChildren = !nodeChildren.isEmpty();
     const bool expanded = expandedByLayerId.value(nodeId, true);
     visibleRows.push_back(VisibleRow{ node, depth, hasChildren, expanded });
     emitted.insert(nodeId);

     if (!hasChildren || !expanded) return;

     stack.insert(nodeId);
     for (const auto& child : nodeChildren) {
      appendNode(child, depth + 1, stack);
     }
     stack.remove(nodeId);
   };

   for (const auto& root : roots) {
    QSet<QString> stack;
    appendNode(root, 0, stack);
   }

   // fallback: if malformed hierarchy exists, ensure all nodes are still shown once.
   for (const auto& l : layers) {
    const QString id = l->id().toString();
    if (!emitted.contains(id)) {
      QSet<QString> stack;
      appendNode(l, 0, stack);
    }
   }
  }
 };

 W_OBJECT_IMPL(ArtifactLayerPanelWidget)

 ArtifactLayerPanelWidget::ArtifactLayerPanelWidget(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  setMouseTracking(true);
  setAcceptDrops(true);
  setFocusPolicy(Qt::StrongFocus);

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
    QObject::connect(service, &ArtifactProjectService::compositionCreated, this, [this](const CompositionID& compId) {
      if (impl_->compositionId.isNil()) {
        impl_->compositionId = compId;
      }
      updateLayout();
    });
    QObject::connect(service, &ArtifactProjectService::projectChanged, this, [this]() {
      updateLayout();
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
  impl_->clearInlineEditors();
  impl_->rebuildVisibleRows();
  const int count = impl_->visibleRows.size();
  const int contentHeight = std::max(kLayerRowHeight, count * kLayerRowHeight);
  setMinimumHeight(0);
  setFixedHeight(contentHeight);
  update();
 }

 void ArtifactLayerPanelWidget::mousePressEvent(QMouseEvent* event)
 {
  setFocus();
  const int rowH = kLayerRowHeight;
  const int colW = kLayerColumnWidth;
  int idx = event->pos().y() / rowH;
  int clickX = event->pos().x();

  if (idx < 0 || idx >= impl_->visibleRows.size()) return;
  const auto& row = impl_->visibleRows[idx];
  auto layer = row.layer;
  if (!layer) return;
  const int y = idx * rowH;
  const bool showInlineCombos = width() >= (kLayerColumnWidth * kLayerPropertyColumnCount + kInlineComboReserve + kLayerNameMinWidth);
  const int parentRectX = width() - kInlineComboReserve;
  const QRect parentRect(parentRectX, y + kInlineComboMarginY, kInlineParentWidth, kInlineComboHeight);
  const QRect blendRect(parentRect.right() + kInlineComboGap, y + kInlineComboMarginY, kInlineBlendWidth, kInlineComboHeight);
  const bool clickInInlineCombo = parentRect.contains(event->pos()) || blendRect.contains(event->pos());

  if (event->button() == Qt::LeftButton) {
    if (!clickInInlineCombo) {
      impl_->clearInlineEditors();
    }
    if (showInlineCombos && parentRect.contains(event->pos())) {
      impl_->clearInlineEditors();
      auto* combo = new QComboBox(this);
      combo->setGeometry(parentRect);
      combo->setStyleSheet(
        "QComboBox { background:#2d2d30; color:#ddd; border:1px solid #4a4a4f; padding:1px 6px; }"
        "QComboBox::drop-down { width:18px; border-left:1px solid #4a4a4f; }");
      combo->addItem(QStringLiteral("<None>"), QString());
      if (auto comp = safeCompositionLookup(impl_->compositionId)) {
        for (const auto& candidate : comp->allLayer()) {
          if (!candidate) continue;
          if (candidate->id() == layer->id()) continue;
          combo->addItem(candidate->layerName(), candidate->id().toString());
        }
      }
      const QString currentParentId = layer->parentLayerId().toString();
      for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).toString() == currentParentId) {
          combo->setCurrentIndex(i);
          break;
        }
      }
      QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this, layer, combo](int i) {
        const QString parentId = combo->itemData(i).toString();
        if (parentId.isEmpty()) layer->clearParent();
        else layer->setParentById(LayerID(parentId));
        combo->deleteLater();
        updateLayout();
      });
      impl_->inlineParentEditor = combo;
      combo->show();
      combo->setFocus();
      combo->showPopup();
      event->accept();
      return;
    }
    if (showInlineCombos && blendRect.contains(event->pos())) {
      impl_->clearInlineEditors();
      auto* combo = new QComboBox(this);
      combo->setGeometry(blendRect);
      combo->setStyleSheet(
        "QComboBox { background:#2d2d30; color:#ddd; border:1px solid #4a4a4f; padding:1px 6px; }"
        "QComboBox::drop-down { width:18px; border-left:1px solid #4a4a4f; }");
      const auto items = blendModeItems();
      for (const auto& [name, mode] : items) {
        combo->addItem(name, static_cast<int>(mode));
      }
      const int currentMode = static_cast<int>(layer->layerBlendType());
      for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).toInt() == currentMode) {
          combo->setCurrentIndex(i);
          break;
        }
      }
      QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this, layer, combo](int i) {
        const auto mode = static_cast<LAYER_BLEND_TYPE>(combo->itemData(i).toInt());
        layer->setBlendMode(mode);
        combo->deleteLater();
        update();
      });
      impl_->inlineBlendEditor = combo;
      combo->show();
      combo->setFocus();
      combo->showPopup();
      event->accept();
      return;
    }
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
    } else {
      const int nameStartX = colW * kLayerPropertyColumnCount;
      const int indent = 14;
      const int toggleSize = 10;
      const int toggleX = nameStartX + row.depth * indent + 2;
      const QRect toggleRect(toggleX, idx * rowH + (rowH - toggleSize) / 2, toggleSize, toggleSize);
      if (row.hasChildren && toggleRect.contains(event->pos())) {
        const QString idStr = layer->id().toString();
        impl_->expandedByLayerId[idStr] = !impl_->expandedByLayerId.value(idStr, true);
        updateLayout();
        event->accept();
        return;
      }
      if (auto* service = ArtifactProjectService::instance()) {
        service->selectLayer(layer->id());
      }
    }
    update();
  } else if (event->button() == Qt::RightButton) {
    if (auto* service = ArtifactProjectService::instance()) {
      service->selectLayer(layer->id());
    }

    QMenu menu(this);
    QAction* renameAct = menu.addAction("Rename Layer...");
    QAction* duplicateAct = menu.addAction("Duplicate Layer");
    QAction* deleteAct = menu.addAction("Delete Layer");
    QAction* expandAct = nullptr;
    QAction* collapseAct = nullptr;
    QAction* expandAllAct = nullptr;
    QAction* collapseAllAct = nullptr;

    if (row.hasChildren) {
      expandAct = menu.addAction("Expand Children");
      collapseAct = menu.addAction("Collapse Children");
      expandAct->setEnabled(!row.expanded);
      collapseAct->setEnabled(row.expanded);
    }
    expandAllAct = menu.addAction("Expand All");
    collapseAllAct = menu.addAction("Collapse All");

    menu.addSeparator();
    QAction* visAct = menu.addAction(layer->isVisible() ? "Hide Layer" : "Show Layer");
    QAction* lockAct = menu.addAction(layer->isLocked() ? "Unlock Layer" : "Lock Layer");
    QAction* soloAct = menu.addAction(layer->isSolo() ? "Disable Solo" : "Enable Solo");
    QAction* shyAct = menu.addAction(layer->isShy() ? "Disable Shy" : "Enable Shy");

    QMenu* parentMenu = menu.addMenu("Parent");
    QAction* selectParentAct = parentMenu->addAction("Select Parent");
    QAction* clearParentAct = parentMenu->addAction("Clear Parent");
    selectParentAct->setEnabled(layer->hasParent());
    clearParentAct->setEnabled(layer->hasParent());

    QMenu* createMenu = menu.addMenu("Create Layer");
    QAction* createSolidAct = createMenu->addAction("Solid Layer");
    QAction* createNullAct = createMenu->addAction("Null Layer");
    QAction* createAdjustAct = createMenu->addAction("Adjustment Layer");
    QAction* createTextAct = createMenu->addAction("Text Layer");

    QAction* chosen = menu.exec(event->globalPosition().toPoint());
    auto comp = safeCompositionLookup(impl_->compositionId);

    if (chosen == renameAct) {
      bool ok = false;
      const QString newName = QInputDialog::getText(
       this,
       "Rename Layer",
       "Layer name:",
       QLineEdit::Normal,
       layer->layerName(),
       &ok);
      if (ok) {
       const QString trimmed = newName.trimmed();
       if (!trimmed.isEmpty()) {
        layer->setLayerName(trimmed);
        update();
       }
      }
    } else if (chosen == duplicateAct) {
      if (comp) {
       auto result = ArtifactProjectManager::getInstance().duplicateLayerInComposition(comp->id(), layer->id());
       if (!result.success) {
        qWarning() << "Duplicate layer failed";
       }
      }
    } else if (chosen == deleteAct) {
      if (auto* service = ArtifactProjectService::instance()) {
       const CompositionID compId = comp ? comp->id() : impl_->compositionId;
       service->removeLayerFromComposition(compId, layer->id());
      }
    } else if (chosen == expandAct && row.hasChildren) {
      impl_->expandedByLayerId[layer->id().toString()] = true;
      updateLayout();
    } else if (chosen == collapseAct && row.hasChildren) {
      impl_->expandedByLayerId[layer->id().toString()] = false;
      updateLayout();
    } else if (chosen == expandAllAct) {
      for (const auto& vr : impl_->visibleRows) {
       if (vr.layer && vr.hasChildren) {
        impl_->expandedByLayerId[vr.layer->id().toString()] = true;
       }
      }
      updateLayout();
    } else if (chosen == collapseAllAct) {
      for (const auto& vr : impl_->visibleRows) {
       if (vr.layer && vr.hasChildren) {
        impl_->expandedByLayerId[vr.layer->id().toString()] = false;
       }
      }
      updateLayout();
    } else if (chosen == visAct) {
      layer->setVisible(!layer->isVisible());
      update();
    } else if (chosen == lockAct) {
      layer->setLocked(!layer->isLocked());
      update();
    } else if (chosen == soloAct) {
      layer->setSolo(!layer->isSolo());
      update();
    } else if (chosen == shyAct) {
      layer->setShy(!layer->isShy());
      updateLayout();
    } else if (chosen == selectParentAct) {
      if (layer->hasParent()) {
       if (auto* service = ArtifactProjectService::instance()) {
        service->selectLayer(layer->parentLayerId());
       }
      }
    } else if (chosen == clearParentAct) {
      layer->clearParent();
      updateLayout();
    } else if (chosen == createSolidAct) {
      ArtifactSolidLayerInitParams params(QStringLiteral("Solid"));
      if (comp) {
       auto sz = comp->settings().compositionSize();
       params.setWidth(sz.width());
       params.setHeight(sz.height());
      }
      if (auto* service = ArtifactProjectService::instance()) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createNullAct) {
      ArtifactNullLayerInitParams params(QStringLiteral("Null"));
      if (comp) {
       auto sz = comp->settings().compositionSize();
       params.setWidth(sz.width());
       params.setHeight(sz.height());
      }
      if (auto* service = ArtifactProjectService::instance()) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createAdjustAct) {
      ArtifactSolidLayerInitParams params(QStringLiteral("Adjustment Layer"));
      if (comp) {
       auto sz = comp->settings().compositionSize();
       params.setWidth(sz.width());
       params.setHeight(sz.height());
      }
      params.setColor(FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
      if (auto* service = ArtifactProjectService::instance()) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createTextAct) {
      ArtifactTextLayerInitParams params(QStringLiteral("Text"));
      if (auto* service = ArtifactProjectService::instance()) {
       service->addLayerToCurrentComposition(params);
      }
    }
  }
  event->accept();
 }

void ArtifactLayerPanelWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
  if (event->button() != Qt::LeftButton) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }
  const int rowH = kLayerRowHeight;
  const int colW = kLayerColumnWidth;
  const int idx = event->pos().y() / rowH;
  if (idx < 0 || idx >= impl_->visibleRows.size()) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }
  const auto& row = impl_->visibleRows[idx];
  auto layer = row.layer;
  if (!layer) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }

  if (row.hasChildren) {
   const int nameStartX = colW * kLayerPropertyColumnCount;
   const int nameX = nameStartX + row.depth * 14;
   const QRect treeHitRect(nameX, idx * rowH, std::max(40, width() - nameX), rowH);
   if (treeHitRect.contains(event->pos())) {
    const QString idStr = layer->id().toString();
    impl_->expandedByLayerId[idStr] = !impl_->expandedByLayerId.value(idStr, true);
    updateLayout();
    event->accept();
    return;
   }
  }

  const int nameStartX = colW * kLayerPropertyColumnCount;
  const bool showInlineCombos = width() >= (kLayerColumnWidth * kLayerPropertyColumnCount + kInlineComboReserve + kLayerNameMinWidth);
  const int parentRectX = width() - kInlineComboReserve;
  const int nameX = nameStartX + row.depth * 14 + (row.hasChildren ? 16 : 4);
  const int nameWidth = showInlineCombos ? std::max(20, parentRectX - nameX - 8) : std::max(20, width() - nameX - 8);
  const QRect editRect(nameX + 2, idx * rowH + 2, nameWidth, rowH - 4);

  if (!editRect.contains(event->pos())) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }

  impl_->clearInlineEditors();
  auto* editor = new QLineEdit(layer->layerName(), this);
  editor->setGeometry(editRect);
  editor->setStyleSheet(
   "QLineEdit { background:#2d2d30; color:#f0f0f0; border:1px solid #4a8bc2; padding:0 4px; }");
  impl_->inlineNameEditor = editor;
  impl_->editingLayerId = layer->id();
  editor->show();
  editor->setFocus();
  editor->selectAll();

  QObject::connect(editor, &QLineEdit::editingFinished, this, [this, editor]() {
   if (!editor || !editor->isVisible()) return;
   const QString newName = editor->text().trimmed();
   if (!newName.isEmpty()) {
    if (auto comp = safeCompositionLookup(impl_->compositionId)) {
     for (auto& l : comp->allLayer()) {
      if (l && l->id() == impl_->editingLayerId) {
       l->setLayerName(newName);
       break;
      }
     }
    }
   }
   impl_->clearInlineEditors();
   update();
  });
  QObject::connect(editor, &QLineEdit::returnPressed, this, [editor]() {
   if (editor) editor->clearFocus();
  });
  event->accept();
 }

 void ArtifactLayerPanelWidget::mouseMoveEvent(QMouseEvent* event)
 {
  int idx = event->pos().y() / kLayerRowHeight;
  if (idx != impl_->hoveredLayerIndex) {
    impl_->hoveredLayerIndex = idx;
    update();
  }
  bool pointer = event->pos().x() < kLayerColumnWidth * kLayerPropertyColumnCount;
  if (!pointer && idx >= 0 && idx < impl_->visibleRows.size()) {
    const auto& row = impl_->visibleRows[idx];
    if (row.hasChildren) {
      const int nameStartX = kLayerColumnWidth * kLayerPropertyColumnCount;
      const int indent = 14;
      const int toggleSize = 10;
      const int toggleX = nameStartX + row.depth * indent + 2;
      const QRect toggleRect(toggleX, idx * kLayerRowHeight + (kLayerRowHeight - toggleSize) / 2, toggleSize, toggleSize);
      pointer = toggleRect.contains(event->pos());
    }
  }
  setCursor(pointer ? Qt::PointingHandCursor : Qt::ArrowCursor);
 }

void ArtifactLayerPanelWidget::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
   int selectedIdx = -1;
   for (int i = 0; i < impl_->visibleRows.size(); ++i) {
    if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
     selectedIdx = i;
     break;
    }
   }
   if (selectedIdx >= 0) {
    const auto& row = impl_->visibleRows[selectedIdx];
    if (row.layer && row.hasChildren) {
     const QString idStr = row.layer->id().toString();
     const bool current = impl_->expandedByLayerId.value(idStr, true);
     const bool next = (event->key() == Qt::Key_Right) ? true : false;
     if (current != next) {
      impl_->expandedByLayerId[idStr] = next;
      updateLayout();
     }
     event->accept();
     return;
    }
   }
  }

  if (event->key() == Qt::Key_F2 && !impl_->inlineNameEditor) {
   int selectedIdx = -1;
   for (int i = 0; i < impl_->visibleRows.size(); ++i) {
    if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
     selectedIdx = i;
     break;
    }
   }
   if (selectedIdx >= 0) {
    const int y = selectedIdx * kLayerRowHeight + kLayerRowHeight / 2;
    const int x = kLayerColumnWidth * kLayerPropertyColumnCount + 20;
    QMouseEvent fakeEvent(QEvent::MouseButtonDblClick, QPointF(x, y), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    mouseDoubleClickEvent(&fakeEvent);
    event->accept();
    return;
   }
  } else if (event->key() == Qt::Key_Escape && impl_->inlineNameEditor) {
   impl_->clearInlineEditors();
   update();
   event->accept();
   return;
  }
  QWidget::keyPressEvent(event);
 }

 void ArtifactLayerPanelWidget::leaveEvent(QEvent*)
 {
  impl_->hoveredLayerIndex = -1;
  update();
 }

 void ArtifactLayerPanelWidget::paintEvent(QPaintEvent*)
 {
  QPainter p(this);
  const int rowH = kLayerRowHeight;
  const int colW = kLayerColumnWidth;
  const int iconSize = 16;
  const int offset = (colW - iconSize) / 2;
  const int nameStartX = colW * kLayerPropertyColumnCount;
  const int indent = 14;
  const int toggleSize = 10;
  p.fillRect(rect(), QColor(42, 42, 42));

  if (impl_->visibleRows.isEmpty()) {
    auto comp = safeCompositionLookup(impl_->compositionId);
    if (!comp) {
      p.setPen(QColor(150, 150, 150));
      p.drawText(rect(), Qt::AlignCenter, "No composition selected");
      return;
    }
    p.setPen(QColor(150, 150, 150));
    p.drawText(rect(), Qt::AlignCenter, "No layers");
    return;
  }

  for (int i = 0; i < impl_->visibleRows.size(); ++i) {
    int y = i * rowH;
    const auto& row = impl_->visibleRows[i];
    auto l = row.layer;
    if (!l) continue;
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

    // Name
    const int nameX = nameStartX + row.depth * indent;
    if (row.hasChildren) {
      const int tx = nameX + 2;
      const int ty = y + (rowH - toggleSize) / 2;
      QPolygon tri;
      if (row.expanded) {
        tri << QPoint(tx, ty + 2) << QPoint(tx + toggleSize, ty + 2) << QPoint(tx + toggleSize / 2, ty + toggleSize - 1);
      } else {
        tri << QPoint(tx + 2, ty) << QPoint(tx + 2, ty + toggleSize) << QPoint(tx + toggleSize - 1, ty + toggleSize / 2);
      }
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(180, 180, 180));
      p.drawPolygon(tri);
    }

    p.setPen(Qt::white);
    const int textX = nameX + (row.hasChildren ? 16 : 4);
    const bool showInlineCombos = width() >= (kLayerColumnWidth * kLayerPropertyColumnCount + kInlineComboReserve + kLayerNameMinWidth);
    const int parentRectX = width() - kInlineComboReserve;
    const QRect parentRect(parentRectX, y + kInlineComboMarginY, kInlineParentWidth, kInlineComboHeight);
    const QRect blendRect(parentRect.right() + kInlineComboGap, y + kInlineComboMarginY, kInlineBlendWidth, kInlineComboHeight);

    auto drawInlineCombo = [&](const QRect& r, const QString& label) {
      p.setPen(QColor(80, 80, 86));
      p.setBrush(QColor(38, 38, 42));
      p.drawRoundedRect(r, 3, 3);
      p.setPen(QColor(210, 210, 210));
      p.drawText(r.adjusted(6, 0, -16, 0), Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(label, Qt::ElideRight, r.width() - 20));
      QPolygon arrow;
      const int ax = r.right() - 10;
      const int ay = r.center().y();
      arrow << QPoint(ax - 4, ay - 2) << QPoint(ax + 4, ay - 2) << QPoint(ax, ay + 3);
      p.setBrush(QColor(170, 170, 170));
      p.setPen(Qt::NoPen);
      p.drawPolygon(arrow);
    };

    const QString parentId = l->parentLayerId().toString();
    QString parentName = QStringLiteral("<None>");
    if (!parentId.isEmpty()) {
      if (auto comp = safeCompositionLookup(impl_->compositionId)) {
        for (const auto& candidate : comp->allLayer()) {
          if (candidate && candidate->id().toString() == parentId) {
            parentName = candidate->layerName();
            break;
          }
        }
      }
    }

    if (showInlineCombos) {
      drawInlineCombo(parentRect, QStringLiteral("Parent: %1").arg(parentName));
      drawInlineCombo(blendRect, QStringLiteral("Blend: %1").arg(blendModeToText(l->layerBlendType())));
    }
    p.setPen(Qt::white);
    const int textWidth = showInlineCombos ? std::max(20, parentRect.left() - textX - 8) : std::max(20, width() - textX - 8);
    p.drawText(textX + 4, y, textWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, l->layerName());
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
