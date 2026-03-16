module;
#include <wobjectimpl.h>
#include <QApplication>
#include <QPainter>
#include <QWidget>
#include <QString>
#include <QVector>
#include <QScrollArea>
#include <QScrollBar>
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
#include <QIcon>
#include <QtSVG/QSvgRenderer>
#include <QComboBox>
#include <QPointer>
#include <QLineEdit>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QTimer>
module Artifact.Widgets.LayerPanelWidget;

import std;

import Utils.Path;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Video;
import Layer.Blend;
import Artifact.Layer.InitParams;
import File.TypeDetector;

namespace Artifact
{
 using namespace ArtifactCore;
namespace {
  constexpr int kLayerRowHeight = 28;
  constexpr int kLayerHeaderHeight = 26;
  constexpr int kLayerHeaderButtonSize = 24;
  constexpr int kLayerColumnWidth = 28;
  constexpr int kLayerPropertyColumnCount = 5;
  constexpr int kInlineComboHeight = 24;
  constexpr int kInlineBlendWidth = 120;
  constexpr int kInlineParentWidth = 150;
  constexpr int kInlineComboGap = 6;
  constexpr int kInlineComboMarginY = 2;
  constexpr int kInlineComboReserve = kInlineParentWidth + kInlineBlendWidth + kInlineComboGap + 10;
 constexpr int kLayerNameMinWidth = 120;

 QIcon loadSvgAsIcon(const QString& path, int size = 16)
 {
  if (path.isEmpty()) {
   return QIcon();
  }
  if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
   QSvgRenderer renderer(path);
   if (renderer.isValid()) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    painter.end();
    if (!pixmap.isNull()) {
     return QIcon(pixmap);
    }
   }
   return QIcon();
  }
  return QIcon(path);
 }

 QIcon loadLayerPanelIcon(const QString& resourceRelativePath, const QString& fallbackFileName = {})
 {
  QIcon icon = loadSvgAsIcon(resolveIconResourcePath(resourceRelativePath));
  if (!icon.isNull()) {
   return icon;
  }
  if (!fallbackFileName.isEmpty()) {
   icon = loadSvgAsIcon(resolveIconPath(fallbackFileName));
  }
  return icon;
 }

 QPixmap loadLayerPanelPixmap(const QString& resourceRelativePath, const QString& fallbackFileName = {})
 {
  QIcon icon = loadLayerPanelIcon(resourceRelativePath, fallbackFileName);
  if (icon.isNull()) {
   return QPixmap();
  }
  QPixmap pix = icon.pixmap(16, 16);
  if (pix.isNull()) {
   pix = icon.pixmap(20, 20);
  }
  return pix;
 }
 }

 namespace {
  class LayerPanelWheelFilter final : public QObject
  {
  public:
   explicit LayerPanelWheelFilter(QScrollArea* scrollArea, QObject* parent = nullptr)
    : QObject(parent), scrollArea_(scrollArea)
   {
   }

  protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
    Q_UNUSED(watched);
    if (!scrollArea_ || event->type() != QEvent::Wheel) {
     return QObject::eventFilter(watched, event);
    }

    auto* wheelEvent = static_cast<QWheelEvent*>(event);
    auto* bar = scrollArea_->verticalScrollBar();
    if (!bar || bar->maximum() <= 0) {
     return QObject::eventFilter(watched, event);
    }

    int delta = 0;
    if (!wheelEvent->pixelDelta().isNull()) {
     delta = wheelEvent->pixelDelta().y();
    } else {
     delta = bar->singleStep() * (wheelEvent->angleDelta().y() / 120);
     if (delta == 0) {
      delta = wheelEvent->angleDelta().y() / 6;
     }
    }

    if (delta == 0) {
     return QObject::eventFilter(watched, event);
    }

    bar->setValue(bar->value() - delta);
    wheelEvent->accept();
    return true;
   }

  private:
   QScrollArea* scrollArea_ = nullptr;
  };

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
      return LayerType::Video;
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

  QVector<QString> layerPanelGroupLabels(const ArtifactAbstractLayerPtr& layer)
  {
   QVector<QString> labels;
   if (!layer) {
    return labels;
   }

   auto hasLabel = [&labels](const QString& candidate) -> bool {
    return std::any_of(labels.cbegin(), labels.cend(), [&candidate](const QString& existing) {
     return existing.compare(candidate, Qt::CaseInsensitive) == 0;
    });
   };

   for (const auto& group : layer->getLayerPropertyGroups()) {
    const QString groupName = group.name().trimmed();
    if (groupName.isEmpty()) {
     continue;
    }
    if (groupName.compare(QStringLiteral("Layer"), Qt::CaseInsensitive) == 0) {
     continue;
    }
    if (!hasLabel(groupName)) {
     labels.push_back(groupName);
    }
   }

   // Fallback for visual/null-style layers that should expose timeline transform controls.
   const QString transformLabel = QStringLiteral("Transform");
   if ((layer->isNullLayer() || layer->isAdjustmentLayer() || layer->hasVideo()) && !hasLabel(transformLabel)) {
    labels.prepend(transformLabel);
   }

   return labels;
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
    visibilityIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/visibility.svg"), QStringLiteral("visibility.png"));
    lockIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock.svg"), QStringLiteral("lock.png"));
    if (lockIcon.isNull()) lockIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock_open.svg"), QStringLiteral("unlock.png"));
    soloIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/purple/group.svg"), QStringLiteral("solo.png"));
    soundIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/blue/volume_up.svg"));
    shyIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/orange/visibility_off.svg"));
  }
  ~Impl() = default;

  QPixmap visibilityIcon;
  QPixmap lockIcon;
  QPixmap soloIcon;
  QPixmap soundIcon;
  QPixmap shyIcon;
  
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
  visButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  visButton->setIcon(impl_->visibilityIcon);
  visButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");
  visButton->setFlat(true);

  auto lockButton = impl_->lockButton = new QPushButton();
  lockButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->lockIcon.isNull()) lockButton->setIcon(impl_->lockIcon);
  lockButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");

  auto soloButton = impl_->soloButton = new QPushButton();
  soloButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->soloIcon.isNull()) soloButton->setIcon(impl_->soloIcon);
  soloButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");

  auto soundButton = impl_->soundButton = new QPushButton();
  soundButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->soundIcon.isNull()) soundButton->setIcon(impl_->soundIcon);
  soundButton->setStyleSheet("background-color: #2D2D30; border: none; border-right: 1px solid #1a1a1a;");

  auto shyButton = impl_->shyButton = new QPushButton;
  shyButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  shyButton->setCheckable(true);
  if (!impl_->shyIcon.isNull()) shyButton->setIcon(impl_->shyIcon);
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

int ArtifactLayerPanelHeaderWidget::buttonSize() const { return kLayerHeaderButtonSize; }
int ArtifactLayerPanelHeaderWidget::iconSize() const { return 14; }
int ArtifactLayerPanelHeaderWidget::totalHeaderHeight() const
{
 return minimumHeight() > 0 ? minimumHeight() : sizeHint().height();
}

 // ============================================================================
 // ArtifactLayerPanelWidget Implementation
 // ============================================================================

 class ArtifactLayerPanelWidget::Impl
 {
 public:
  enum class RowKind
  {
   Layer,
   Group
  };

  struct VisibleRow
  {
   ArtifactAbstractLayerPtr layer;
   int depth = 0;
   bool hasChildren = false;
   bool expanded = true;
   RowKind kind = RowKind::Layer;
   QString label;
  };

  Impl()
  {
    visibilityIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/neutral/visibility.svg"), QStringLiteral("visibility.png"));
    lockIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock.svg"), QStringLiteral("lock.png"));
    if (lockIcon.isNull()) lockIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/yellow/lock_open.svg"), QStringLiteral("unlock.png"));
    soloIcon = loadLayerPanelPixmap(QStringLiteral("MaterialVS/purple/group.svg"), QStringLiteral("solo.png"));
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
  QPoint dragStartPos;
  LayerID dragCandidateLayerId;
  LayerID draggedLayerId;
  int dragInsertVisibleRow = -1;

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

  void clearDragState()
  {
   dragCandidateLayerId = LayerID();
   draggedLayerId = LayerID();
   dragInsertVisibleRow = -1;
  }

  int insertionVisibleRowForY(const int y) const
  {
   if (visibleRows.isEmpty()) {
    return 0;
   }
   return std::clamp<int>(
    (y + (kLayerRowHeight / 2)) / kLayerRowHeight,
    0,
    static_cast<int>(visibleRows.size()));
  }

  int layerCountBeforeVisibleRow(const int visibleRowIndex) const
  {
   int count = 0;
   const int limit = std::clamp<int>(visibleRowIndex, 0, static_cast<int>(visibleRows.size()));
   for (int i = 0; i < limit; ++i) {
    const auto& row = visibleRows[i];
    if (row.kind == RowKind::Layer && row.layer) {
     ++count;
    }
   }
   return count;
  }

  int layerCountBeforeVisibleRowExcluding(const int visibleRowIndex, const LayerID& excludedLayerId) const
  {
   int count = 0;
   const int limit = std::clamp<int>(visibleRowIndex, 0, static_cast<int>(visibleRows.size()));
   for (int i = 0; i < limit; ++i) {
    const auto& row = visibleRows[i];
    if (row.kind != RowKind::Layer || !row.layer) {
     continue;
    }
    if (!excludedLayerId.isNil() && row.layer->id() == excludedLayerId) {
     continue;
    }
    ++count;
   }
   return count;
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
     const auto panelGroups = layerPanelGroupLabels(node);
     const bool hasChildren = !nodeChildren.isEmpty() || !panelGroups.isEmpty();
     const bool expanded = expandedByLayerId.value(nodeId, true);
     visibleRows.push_back(VisibleRow{ node, depth, hasChildren, expanded, RowKind::Layer, QString() });
     emitted.insert(nodeId);

     if (!hasChildren || !expanded) return;

     for (const auto& groupLabel : panelGroups) {
      visibleRows.push_back(VisibleRow{
       node,
       depth + 1,
       false,
       false,
       RowKind::Group,
       groupLabel
      });
     }

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
    QObject::connect(service, &ArtifactProjectService::layerCreated, this, [this](const CompositionID& compId, const LayerID& layerId) {
      if (impl_->compositionId == compId) {
        this->updateLayout();
        
        // Auto-focus Inspector
        const auto widgets = QApplication::allWidgets();
        for (QWidget* w : widgets) {
         if (!w) continue;
         const QString className = QString::fromLatin1(w->metaObject()->className());
         if (className.contains("ArtifactInspectorWidget", Qt::CaseInsensitive)) {
          w->show();
          w->raise();
          w->activateWindow();
          break;
         }
        }
        
        // Start inline editing of the new layer on the next event loop tick
        QTimer::singleShot(0, this, [this, layerId]() {
          this->editLayerName(layerId);
          // Ensure layer is visible in the scroll area wrapper if possible
          Q_EMIT visibleRowsChanged();
        });
      }
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
  updateGeometry();
  update();
  Q_EMIT visibleRowsChanged();
 }

 QVector<LayerID> ArtifactLayerPanelWidget::visibleTimelineRows() const
 {
  QVector<LayerID> rows;
  rows.reserve(impl_->visibleRows.size());
  for (const auto& row : impl_->visibleRows) {
   if (row.kind == Impl::RowKind::Layer && row.layer) {
    rows.append(row.layer->id());
   } else {
    rows.append(LayerID::Nil());
   }
  }
  return rows;
 }

 int ArtifactLayerPanelWidget::layerRowIndex(const LayerID& id) const
 {
  for (int i = 0; i < impl_->visibleRows.size(); ++i) {
   if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == id) {
    return i;
   }
  }
  return -1;
 }

 void ArtifactLayerPanelWidget::editLayerName(const LayerID& id)
 {
  int idx = layerRowIndex(id);
  if (idx >= 0) {
   impl_->selectedLayerId = id;
   update();

   // Fire a dummy F2 event or replicate F2 logic
   if (!impl_->inlineNameEditor) {
    auto comp = safeCompositionLookup(impl_->compositionId);
    if (!comp) return;
    auto l = comp->layerById(id);
    if (!l) return;

    impl_->inlineNameEditor = new QLineEdit(this);
    impl_->inlineNameEditor->setText(l->layerName());
    impl_->inlineNameEditor->selectAll();
    impl_->inlineNameEditor->setStyleSheet("background-color: #2D2D30; color: white; border: 1px solid #007ACC;");

    // Position it
    const int rowIndent = impl_->visibleRows[idx].depth * 14;
    const int nameStartX = kLayerColumnWidth * kLayerPropertyColumnCount;
    const int textX = nameStartX + rowIndent + (impl_->visibleRows[idx].hasChildren ? 16 : 4);
    const int editorWidth = std::max(60, width() - textX - kInlineParentWidth - kInlineBlendWidth - 8);
    impl_->inlineNameEditor->setGeometry(textX, idx * kLayerRowHeight + 2, editorWidth, kLayerRowHeight - 4);

    QObject::connect(impl_->inlineNameEditor, &QLineEdit::editingFinished, this, [this, id]() {
      if (!impl_->inlineNameEditor) return;
      QString newName = impl_->inlineNameEditor->text();
      impl_->inlineNameEditor->deleteLater();
      impl_->inlineNameEditor = nullptr;
      if (auto* svc = ArtifactProjectService::instance()) {
        svc->renameLayerInCurrentComposition(id, newName);
      }
      setFocus();
    });

    impl_->inlineNameEditor->show();
    impl_->inlineNameEditor->setFocus();
   }
  }
 }


 void ArtifactLayerPanelWidget::mousePressEvent(QMouseEvent* event)
 {
  setFocus();
  impl_->clearDragState();
  const int rowH = kLayerRowHeight;
  const int colW = kLayerColumnWidth;
  int idx = event->pos().y() / rowH;
  int clickX = event->pos().x();

  if (idx < 0 || idx >= impl_->visibleRows.size()) return;
  const auto& row = impl_->visibleRows[idx];
  auto layer = row.layer;
  if (!layer) return;
  auto* service = ArtifactProjectService::instance();
  if (row.kind != Impl::RowKind::Layer) {
   if (event->button() == Qt::LeftButton) {
    if (service) {
     service->selectLayer(layer->id());
    }
    update();
   }
   event->accept();
   return;
  }
  const int y = idx * rowH;
  const int nameStartX = colW * kLayerPropertyColumnCount;
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
      QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this, service, layer, combo](int i) {
        const QString parentId = combo->itemData(i).toString();
        if (service) {
          if (parentId.isEmpty()) {
            service->clearLayerParentInCurrentComposition(layer->id());
          } else {
            service->setLayerParentInCurrentComposition(layer->id(), LayerID(parentId));
          }
        }
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
      QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this, service, layer, combo](int i) {
        const auto mode = static_cast<LAYER_BLEND_TYPE>(combo->itemData(i).toInt());
        layer->setBlendMode(mode);
        if (service) {
          if (auto project = service->getCurrentProjectSharedPtr()) {
            project->projectChanged();
          }
        }
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
      if (service) service->setLayerVisibleInCurrentComposition(layer->id(), !layer->isVisible());
    } else if (clickX < colW * 2) {
      if (service) service->setLayerLockedInCurrentComposition(layer->id(), !layer->isLocked());
    } else if (clickX < colW * 3) {
      if (service) service->setLayerSoloInCurrentComposition(layer->id(), !layer->isSolo());
    } else if (clickX < colW * 4) {
      // Sound toggle
    } else if (clickX < colW * 5) {
      if (service) service->setLayerShyInCurrentComposition(layer->id(), !layer->isShy());
    } else {
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
      if (service) {
        service->selectLayer(layer->id());
      }
      impl_->dragStartPos = event->pos();
      impl_->dragCandidateLayerId = layer->id();
    }
    update();
  } else if (event->button() == Qt::RightButton) {
    if (service) {
      service->selectLayer(layer->id());
    }

    QMenu menu(this);
    QAction* renameAct = menu.addAction("Rename Layer...");
    QAction* replaceSourceAct = nullptr;
    QAction* duplicateAct = menu.addAction("Duplicate Layer");
    QAction* deleteAct = menu.addAction("Delete Layer");
    QAction* expandAct = nullptr;
    QAction* collapseAct = nullptr;
    QAction* expandAllAct = nullptr;
    QAction* collapseAllAct = nullptr;
    renameAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/blue/edit.svg")));
    duplicateAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/neutral/content_copy.svg")));
    deleteAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/red/delete.svg")));

    const bool supportsSourceReplacement =
      static_cast<bool>(std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) ||
      static_cast<bool>(std::dynamic_pointer_cast<ArtifactVideoLayer>(layer));
    if (supportsSourceReplacement) {
      replaceSourceAct = menu.addAction("Replace Source...");
      replaceSourceAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/blue/file_open.svg")));
    }

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
    visAct->setIcon(loadLayerPanelIcon(layer->isVisible()
      ? QStringLiteral("MaterialVS/neutral/visibility_off.svg")
      : QStringLiteral("MaterialVS/neutral/visibility.svg")));
    lockAct->setIcon(loadLayerPanelIcon(layer->isLocked()
      ? QStringLiteral("MaterialVS/yellow/lock_open.svg")
      : QStringLiteral("MaterialVS/yellow/lock.svg")));
    soloAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/purple/group.svg")));
    shyAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/orange/visibility_off.svg")));

    QMenu* parentMenu = menu.addMenu("Parent");
    QAction* selectParentAct = parentMenu->addAction("Select Parent");
    QAction* clearParentAct = parentMenu->addAction("Clear Parent");
    parentMenu->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/neutral/link.svg")));
    selectParentAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/neutral/link.svg")));
    clearParentAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/orange/link_off.svg")));
    selectParentAct->setEnabled(layer->hasParent());
    clearParentAct->setEnabled(layer->hasParent());

    QMenu* createMenu = menu.addMenu("Create Layer");
    QAction* createSolidAct = createMenu->addAction("Solid Layer");
    QAction* createNullAct = createMenu->addAction("Null Layer");
    QAction* createAdjustAct = createMenu->addAction("Adjustment Layer");
    QAction* createTextAct = createMenu->addAction("Text Layer");
    createMenu->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/green/format_shapes.svg")));
    createSolidAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/green/format_shapes.svg")));
    createNullAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/purple/group.svg")));
    createAdjustAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/orange/warning.svg")));
    createTextAct->setIcon(loadLayerPanelIcon(QStringLiteral("MaterialVS/purple/title.svg")));

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
        if (service) service->renameLayerInCurrentComposition(layer->id(), trimmed);
        update();
       }
      }
    } else if (chosen == replaceSourceAct) {
      QString filter;
      if (std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
       filter = QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;All Files (*.*)");
      } else {
       filter = QStringLiteral("Media Files (*.mp4 *.mov *.mkv *.avi *.webm *.mp3 *.wav *.flac *.aac *.m4a *.ogg);;All Files (*.*)");
      }

      const QString filePath = QFileDialog::getOpenFileName(
       this,
       QStringLiteral("Replace Layer Source"),
       QString(),
       filter);
      if (!filePath.isEmpty() && service) {
       if (!service->replaceLayerSourceInCurrentComposition(layer->id(), filePath)) {
        qWarning() << "Replace source failed for layer" << layer->id().toString() << filePath;
       }
      }
    } else if (chosen == duplicateAct) {
      if (service) {
       if (!service->duplicateLayerInCurrentComposition(layer->id())) {
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
      if (service) service->setLayerVisibleInCurrentComposition(layer->id(), !layer->isVisible());
      update();
    } else if (chosen == lockAct) {
      if (service) service->setLayerLockedInCurrentComposition(layer->id(), !layer->isLocked());
      update();
    } else if (chosen == soloAct) {
      if (service) service->setLayerSoloInCurrentComposition(layer->id(), !layer->isSolo());
      update();
    } else if (chosen == shyAct) {
      if (service) service->setLayerShyInCurrentComposition(layer->id(), !layer->isShy());
      updateLayout();
    } else if (chosen == selectParentAct) {
      if (layer->hasParent()) {
       if (service) {
        service->selectLayer(layer->parentLayerId());
       }
      }
    } else if (chosen == clearParentAct) {
      if (service) service->clearLayerParentInCurrentComposition(layer->id());
      updateLayout();
    } else if (chosen == createSolidAct) {
      ArtifactSolidLayerInitParams params(QStringLiteral("Solid"));
      if (comp) {
       auto sz = comp->settings().compositionSize();
       params.setWidth(sz.width());
       params.setHeight(sz.height());
      }
      if (service) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createNullAct) {
      ArtifactNullLayerInitParams params(QStringLiteral("Null"));
      if (comp) {
       auto sz = comp->settings().compositionSize();
       params.setWidth(sz.width());
       params.setHeight(sz.height());
      }
      if (service) {
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
      if (service) {
       service->addLayerToCurrentComposition(params);
      }
    } else if (chosen == createTextAct) {
      ArtifactTextLayerInitParams params(QStringLiteral("Text"));
      if (service) {
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
  if (row.kind != Impl::RowKind::Layer) {
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
    if (auto* service = ArtifactProjectService::instance()) {
     service->renameLayerInCurrentComposition(impl_->editingLayerId, newName);
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
  if ((event->buttons() & Qt::LeftButton) && !impl_->dragCandidateLayerId.isNil()) {
    const int dragDistance = (event->pos() - impl_->dragStartPos).manhattanLength();
    if (impl_->draggedLayerId.isNil() && dragDistance >= QApplication::startDragDistance()) {
      impl_->draggedLayerId = impl_->dragCandidateLayerId;
    }
    if (!impl_->draggedLayerId.isNil()) {
      const int nextInsertRow = impl_->insertionVisibleRowForY(event->pos().y());
      if (nextInsertRow != impl_->dragInsertVisibleRow) {
        impl_->dragInsertVisibleRow = nextInsertRow;
        update();
      }
      setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
    }
  }

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

 void ArtifactLayerPanelWidget::mouseReleaseEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton && !impl_->draggedLayerId.isNil()) {
   auto* service = ArtifactProjectService::instance();
   auto comp = safeCompositionLookup(impl_->compositionId);
   if (service && comp) {
    QVector<LayerID> visibleLayerIds;
    visibleLayerIds.reserve(impl_->visibleRows.size());
    for (const auto& row : impl_->visibleRows) {
     if (row.kind == Impl::RowKind::Layer && row.layer) {
      visibleLayerIds.push_back(row.layer->id());
     }
    }

    const auto allLayers = comp->allLayer();
    int oldIndex = -1;
    for (int i = 0; i < allLayers.size(); ++i) {
     if (allLayers[i] && allLayers[i]->id() == impl_->draggedLayerId) {
      oldIndex = i;
      break;
     }
    }

    if (oldIndex >= 0 && !visibleLayerIds.isEmpty()) {
     QVector<LayerID> remainingVisibleLayerIds;
     remainingVisibleLayerIds.reserve(visibleLayerIds.size());
     for (const auto& layerId : visibleLayerIds) {
      if (layerId != impl_->draggedLayerId) {
       remainingVisibleLayerIds.push_back(layerId);
      }
     }

     const int targetVisibleIndex = std::clamp(
      impl_->layerCountBeforeVisibleRowExcluding(impl_->dragInsertVisibleRow, impl_->draggedLayerId),
      0,
      static_cast<int>(remainingVisibleLayerIds.size()));

     int newIndex = oldIndex;
     if (targetVisibleIndex >= static_cast<int>(remainingVisibleLayerIds.size())) {
      newIndex = static_cast<int>(allLayers.size()) - 1;
     } else {
      const LayerID targetLayerId = remainingVisibleLayerIds[targetVisibleIndex];
      int targetIndex = -1;
      for (int i = 0; i < allLayers.size(); ++i) {
       if (allLayers[i] && allLayers[i]->id() == targetLayerId) {
        targetIndex = i;
        break;
       }
      }
      if (targetIndex >= 0) {
       newIndex = targetIndex;
      }
     }

     newIndex = std::clamp(
      newIndex,
      0,
      std::max(0, static_cast<int>(allLayers.size()) - 1));
     if (newIndex != oldIndex) {
      service->moveLayerInCurrentComposition(impl_->draggedLayerId, newIndex);
      updateLayout();
     }
    }
   }
   impl_->clearDragState();
   unsetCursor();
   update();
   event->accept();
   return;
  }

  impl_->clearDragState();
  unsetCursor();
  QWidget::mouseReleaseEvent(event);
 }

void ArtifactLayerPanelWidget::keyPressEvent(QKeyEvent* event)
{
  // Ctrl + [ / ] でレイヤー順序を移動
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_BracketRight) {
      if (!impl_->selectedLayerId.isNil()) {
        auto* service = ArtifactProjectService::instance();
        auto comp = service ? service->currentComposition().lock() : nullptr;
        if (comp) {
          // 現在のレイヤーインデックスを取得
          int currentLayerIndex = -1;
          auto layers = comp->allLayer();
          for (int i = 0; i < layers.size(); ++i) {
            if (layers[i]->id() == impl_->selectedLayerId) {
              currentLayerIndex = i;
              break;
            }
          }
          
          if (currentLayerIndex >= 0) {
            int newIndex = currentLayerIndex;
            if (event->key() == Qt::Key_BracketLeft) {
              // 背面へ (インデックスを減らす)
              newIndex = std::max(0, currentLayerIndex - 1);
            } else if (event->key() == Qt::Key_BracketRight) {
              // 前面へ (インデックスを増やす)
              newIndex = std::min(static_cast<int>(layers.size()) - 1, currentLayerIndex + 1);
            }
            
            if (newIndex != currentLayerIndex) {
              service->moveLayerInCurrentComposition(impl_->selectedLayerId, newIndex);
              event->accept();
              return;
            }
          }
        }
      }
    }
  }

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

  void ArtifactLayerPanelWidget::wheelEvent(QWheelEvent* event)
  {
   const int delta = event->angleDelta().y();
   if (delta == 0 || impl_->visibleRows.isEmpty()) {
    QWidget::wheelEvent(event);
    return;
   }
   
   // マウスの X 位置をチェック（ブレンドモードエリアか？）
   const int mouseX = event->position().x();
   const int blendModeStartX = kLayerColumnWidth * kLayerPropertyColumnCount;
   const bool isBlendModeArea = (mouseX >= blendModeStartX);
   
   if (isBlendModeArea) {
    // ブレンドモードエリア：ホイールでブレンドモードを変更
    if (!impl_->selectedLayerId.isNil()) {
      auto* service = ArtifactProjectService::instance();
      auto comp = service ? service->currentComposition().lock() : nullptr;
      if (comp) {
        auto layer = comp->layerById(impl_->selectedLayerId);
        if (layer) {
          const auto items = blendModeItems();
          const int currentMode = static_cast<int>(layer->layerBlendType());
          int currentIndex = 0;
          for (int i = 0; i < items.size(); ++i) {
            if (static_cast<int>(items[i].second) == currentMode) {
              currentIndex = i;
              break;
            }
          }
          const int dir = (delta > 0) ? -1 : 1;
          int newIndex = (currentIndex + dir + items.size()) % items.size();
          const auto newMode = items[newIndex].second;
          layer->setBlendMode(newMode);
          if (service) {
            if (auto project = service->getCurrentProjectSharedPtr()) {
              project->projectChanged();
            }
          }
          update();
          event->accept();
          return;
        }
      }
    }
   }
   
   // それ以外：選択レイヤーを変更
   // 現在の選択インデックスを探す
   int selectedIdx = -1;
   for (int i = 0; i < impl_->visibleRows.size(); ++i) {
    if (impl_->visibleRows[i].kind == Impl::RowKind::Layer &&
        impl_->visibleRows[i].layer &&
        impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
     selectedIdx = i;
     break;
    }
   }
   // ホイール上 → 前(index小)、下 → 次(index大)
   const int dir = (delta > 0) ? -1 : 1;
   int newIdx = (selectedIdx < 0) ? (dir > 0 ? 0 : impl_->visibleRows.size() - 1)
                                  : (selectedIdx + dir);
   // RowKind::Layerの行を探す
   while (newIdx >= 0 && newIdx < impl_->visibleRows.size()) {
    if (impl_->visibleRows[newIdx].kind == Impl::RowKind::Layer &&
        impl_->visibleRows[newIdx].layer)
     break;
    newIdx += dir;
   }
   if (newIdx < 0 || newIdx >= impl_->visibleRows.size()) {
    event->accept();
    return;
   }
   const auto& row = impl_->visibleRows[newIdx];
   if (row.layer) {
    impl_->selectedLayerId = row.layer->id();
    if (auto* svc = ArtifactProjectService::instance()) {
     svc->selectLayer(row.layer->id());
    }
    update();
    event->accept();
   }
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
    const bool isGroupRow = (row.kind == Impl::RowKind::Group);
    bool sel = (l->id() == impl_->selectedLayerId);

    if (sel && !isGroupRow) p.fillRect(0, y, width(), rowH, QColor(70, 100, 150));
    else if (i == impl_->hoveredLayerIndex) p.fillRect(0, y, width(), rowH, QColor(55, 55, 80));
    else p.fillRect(0, y, width(), rowH, (i % 2 == 0) ? QColor(42, 42, 42) : QColor(45, 45, 45));

    p.setPen(QColor(60, 60, 60));
    p.drawLine(0, y + rowH, width(), y + rowH);

    if (isGroupRow) {
      const int textX = nameStartX + row.depth * indent + 4;
      p.setPen(QColor(196, 196, 196));
      p.drawText(textX, y, std::max(20, width() - textX - 8), rowH, Qt::AlignVCenter | Qt::AlignLeft, row.label);
      continue;
    }

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

  if (!impl_->draggedLayerId.isNil() && impl_->dragInsertVisibleRow >= 0) {
    const int lineY = std::clamp(impl_->dragInsertVisibleRow * rowH, 1, std::max(1, height() - 2));
    const QColor accent(0, 153, 255);
    QPen pen(accent, 2);
    p.setPen(pen);
    p.drawLine(0, lineY, width(), lineY);

    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    const int markerSize = 6;
    p.drawEllipse(QPoint(markerSize, lineY), markerSize / 2, markerSize / 2);
    p.drawEllipse(QPoint(std::max(markerSize, width() - markerSize), lineY), markerSize / 2, markerSize / 2);
  }
}

 void ArtifactLayerPanelWidget::dragEnterEvent(QDragEnterEvent* e)
 {
   const QMimeData* mime = e->mimeData();
   if (mime->hasUrls()) {
     for (const auto& url : mime->urls()) {
       if (url.isLocalFile()) {
         const QString filePath = url.toLocalFile();
         const LayerType type = inferLayerTypeFromFile(filePath);
         if (type == LayerType::Image || type == LayerType::Video || type == LayerType::Audio) {
           e->acceptProposedAction();
           update();
           return;
         }
       }
     }
   }
   if (mime->hasText()) {
     const QStringList paths = mime->text().split(QStringLiteral("\n"), Qt::SkipEmptyParts);
     for (const QString& path : paths) {
       const QString trimmed = path.trimmed();
       if (!trimmed.isEmpty()) {
         const LayerType type = inferLayerTypeFromFile(trimmed);
         if (type == LayerType::Image || type == LayerType::Video || type == LayerType::Audio) {
           e->acceptProposedAction();
           update();
           return;
         }
       }
     }
   }
   e->ignore();
 }

 void ArtifactLayerPanelWidget::dragMoveEvent(QDragMoveEvent* e)
 {
   const QMimeData* mime = e->mimeData();
   if (mime->hasUrls()) {
     e->acceptProposedAction();
   } else if (mime->hasText()) {
     e->acceptProposedAction();
   } else {
     e->ignore();
   }
 }
 void ArtifactLayerPanelWidget::dragLeaveEvent(QDragLeaveEvent* e)
 {
   e->accept();
   update();  // ビジュアルフィードバック解除用に再描画
 }
 void ArtifactLayerPanelWidget::dropEvent(QDropEvent* event)
 {
  const QMimeData* mime = event->mimeData();
  if (!mime) {
    event->ignore();
    return;
  }

  QStringList validPaths;

  if (mime->hasUrls()) {
    for (const auto& url : mime->urls()) {
      if (url.isLocalFile()) {
        const QString filePath = url.toLocalFile();
        const LayerType type = inferLayerTypeFromFile(filePath);
        if (type == LayerType::Image || type == LayerType::Video || type == LayerType::Audio) {
          validPaths.append(filePath);
        }
      }
    }
  }

  if (validPaths.isEmpty() && mime->hasText()) {
    const QStringList paths = mime->text().split(QStringLiteral("\n"), Qt::SkipEmptyParts);
    for (const QString& path : paths) {
      const QString trimmed = path.trimmed();
      if (trimmed.isEmpty()) continue;
      const LayerType type = inferLayerTypeFromFile(trimmed);
      if (type == LayerType::Image || type == LayerType::Video || type == LayerType::Audio) {
        validPaths.append(trimmed);
      }
    }
  }

  if (validPaths.isEmpty()) {
    event->ignore();
    return;
  }

  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
    event->ignore();
    return;
  }

  auto imported = svc->importAssetsFromPaths(validPaths);

  for (const auto& path : imported) {
    LayerType type = inferLayerTypeFromFile(path);
    
    // 画像レイヤーの場合は ArtifactImageInitParams を使用してパスを設定
    if (type == LayerType::Image) {
      ArtifactImageInitParams params(QFileInfo(path).baseName());
      params.setImagePath(path);
      svc->addLayerToCurrentComposition(params);
    } else {
      ArtifactLayerInitParams params(QFileInfo(path).baseName(), type);
      svc->addLayerToCurrentComposition(params);
    }
  }

  event->acceptProposedAction();
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

 W_OBJECT_IMPL(ArtifactLayerTimelinePanelWrapper)

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
  impl_->scroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  impl_->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  impl_->scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  impl_->scroll->setFrameShape(QFrame::NoFrame);
  impl_->panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto* wheelFilter = new LayerPanelWheelFilter(impl_->scroll, this);
  this->installEventFilter(wheelFilter);
  impl_->header->installEventFilter(wheelFilter);
  impl_->panel->installEventFilter(wheelFilter);
  impl_->scroll->viewport()->installEventFilter(wheelFilter);

  layout->addWidget(impl_->header);
  layout->addWidget(impl_->scroll, 1);

  QObject::connect(impl_->header, &ArtifactLayerPanelHeaderWidget::shyToggled,
                   impl_->panel, &ArtifactLayerPanelWidget::setShyHidden);
  QObject::connect(impl_->panel, &ArtifactLayerPanelWidget::visibleRowsChanged,
                   this, [this]() {
                    const bool isEmpty = impl_->panel->visibleTimelineRows().isEmpty();
                    impl_->scroll->setAlignment(isEmpty ? (Qt::AlignHCenter | Qt::AlignVCenter)
                                                        : (Qt::AlignLeft | Qt::AlignTop));
                    Q_EMIT visibleRowsChanged();
                   });
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

 QVector<LayerID> ArtifactLayerTimelinePanelWrapper::visibleTimelineRows() const
 {
  if (!impl_ || !impl_->panel) {
   return {};
  }
  return impl_->panel->visibleTimelineRows();
 }

} // namespace Artifact
