module;
#include <wobjectimpl.h>
#include <QApplication>
#include <QCoreApplication>
#include <QAction>
#include <QActionGroup>
#include <QPainter>
#include <QFontMetrics>
#include <QPalette>
#include <QWidget>
#include <QString>
#include <QVector>
#include <QBoxLayout>
#include <QPushButton>
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
#include <QStringList>
#include <QPolygon>
#include <QIcon>
#include <QtSVG/QSvgRenderer>
#include <QComboBox>
#include <QAbstractItemView>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPointer>
#include <QLineEdit>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QWheelEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QDockWidget>
#include <QWidget>
#include <QMetaObject>
#include <QToolTip>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>
#include <QDrag>
#include <QMenu>
#include <QJsonDocument>
#include <QJsonObject>
#include <QClipboard>
module Artifact.Widgets.LayerPanelWidget;

import std;
import ArtifactCore.Utils.PerformanceProfiler;

import Utils.Path;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Widgets.ProjectManagerWidget;
import Artifact.Widgets.PrecomposeDialog;
import Artifact.Composition.Abstract;
import Translation.Manager;
import Artifact.Layer.Abstract;
import UI.ShortcutBindings;
import Artifact.Layer.Image;
import Artifact.Layer.Text;
import Artifact.Layer.Shape;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Layer.Svg;
import Artifact.Layer.Video;
import Artifact.Layer.Audio;
import Artifact.Layer.Camera;
import Artifact.Layer.Light;
import Artifact.Layer.Particle;
import Artifact.Layer.FormParticle;
import Artifact.Layer.Composition;
import Artifact.Layer.Solid2D;
import Artifact.Layer.Construction;
import Artifact.Layer.Clone;
import Artifact.Layer.Group;
import Layer.Matte;
import Layer.BlendModeInfo;
import Geometry.LayerAlignment;
import Artifact.Timeline.KeyframeModel;
import Undo.UndoManager;
import Artifact.Service.Playback;
import Layer.Blend;
import Artifact.Layer.InitParams;
import File.TypeDetector;
import Event.Bus;
import Artifact.Event.Types;
import Input.Operator;
import Widgets.Utils.CSS;

namespace Artifact
{
 using namespace ArtifactCore;

QMessageBox::StandardButton centeredQuestion(QWidget* parent,
                                             const QString& title,
                                             const QString& text);

namespace {
constexpr auto kLayerPanelContext = "Panel.LayerTree";

enum class MultiEditCycleMode {
  None,
  Align,
  Distribute,
  Shuffle
};

constexpr int kMultiEditCycleWindowMs = 4500;

struct LayerPlacementSnapshot {
  LayerID id;
  QRectF bounds;
  QPointF position;
};

template <typename Handler>
QAction* addIconAction(QMenu* menu, const QString& text, const QString& iconPath, Handler&& handler)
{
  QAction* action = menu->addAction(text, std::forward<Handler>(handler));
  action->setIcon(QIcon(resolveIconPath(iconPath)));
  return action;
}

inline QMenu* addIconMenu(QMenu* menu, const QString& text, const QString& iconPath)
{
  QMenu* subMenu = menu->addMenu(text);
  subMenu->setIcon(QIcon(resolveIconPath(iconPath)));
  return subMenu;
}

QDockWidget* findDockByTitle(QWidget* window, const QString& title)
{
  if (!window) {
    return nullptr;
  }
  const auto docks = window->findChildren<QDockWidget*>();
  for (QDockWidget* dock : docks) {
    if (dock && dock->windowTitle() == title) {
      return dock;
    }
  }
  return nullptr;
}

void setDockVisible(QWidget* window, const QString& title, bool visible)
{
  auto* dock = findDockByTitle(window, title);
  if (!dock) {
    return;
  }
  dock->setVisible(visible);
  if (visible) {
    dock->raise();
  }
}

void activateDock(QWidget* window, const QString& title)
{
  auto* dock = findDockByTitle(window, title);
  if (!dock) {
    return;
  }
  dock->setVisible(true);
  dock->raise();
  dock->activateWindow();
}

  QColor themeColor(const QString& value, const QColor& fallback)
  {
    const QColor color(value);
    return color.isValid() ? color : fallback;
  }

  QColor mixColor(const QColor& a, const QColor& b, qreal t)
  {
    const qreal clamped = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(a.redF() * (1.0 - clamped) + b.redF() * clamped,
                            a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
                            a.blueF() * (1.0 - clamped) + b.blueF() * clamped,
                            a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped);
  }

  QColor toneBadgeFill(LayerPresentationBadgeTone tone,
                       const QColor& background,
                       const QColor& surface,
                       const QColor& accent)
  {
    switch (tone) {
    case LayerPresentationBadgeTone::Container:
      return mixColor(background, accent, 0.22);
    case LayerPresentationBadgeTone::Media:
      return mixColor(background, surface, 0.34);
    case LayerPresentationBadgeTone::Motion:
      return mixColor(background, accent, 0.16);
    case LayerPresentationBadgeTone::Special:
      return mixColor(background, accent, 0.30);
    case LayerPresentationBadgeTone::Neutral:
    default:
      return mixColor(background, surface, 0.28);
    }
  }

  QColor toneBadgeText(LayerPresentationBadgeTone tone,
                       const QColor& text,
                       const QColor& accent)
  {
    switch (tone) {
    case LayerPresentationBadgeTone::Container:
      return mixColor(text, accent, 0.28);
    case LayerPresentationBadgeTone::Media:
      return text.darker(115);
    case LayerPresentationBadgeTone::Motion:
      return mixColor(text, accent, 0.34);
    case LayerPresentationBadgeTone::Special:
      return mixColor(text, accent, 0.22);
    case LayerPresentationBadgeTone::Neutral:
    default:
      return text.darker(120);
    }
  }

constexpr int kVariantChipWidth = 72;
constexpr int kVariantChipHeight = 16;
constexpr int kLayerTypeIconSize = 14;
constexpr int kLayerTypeIconGap = 5;

enum class TimelineLayerIconKind {
  Generic,
  Solid,
  Image,
  Svg,
  Video,
  Audio,
  Text,
  Shape,
  Precomp,
  Camera,
  Light,
  Group,
  Null,
  Adjustment,
  Particle,
  Clone,
  Model3D,
  Construction
};

TimelineLayerIconKind layerIconKindForLayer(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) return TimelineLayerIconKind::Generic;
  if (layer->isAdjustmentLayer()) return TimelineLayerIconKind::Adjustment;
  if (layer->isGroupLayer()) return TimelineLayerIconKind::Group;
  if (layer->isCloneLayer()) return TimelineLayerIconKind::Clone;
  if (layer->isConstructionLayer()) return TimelineLayerIconKind::Construction;
  if (layer->is3D()) return TimelineLayerIconKind::Model3D;
  if (dynamic_cast<ArtifactCompositionLayer*>(layer.get())) return TimelineLayerIconKind::Precomp;
  if (std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) return TimelineLayerIconKind::Text;
  if (std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) return TimelineLayerIconKind::Shape;
  if (std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) return TimelineLayerIconKind::Svg;
  if (std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) return TimelineLayerIconKind::Image;
  if (std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) return TimelineLayerIconKind::Audio;
  if (std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    if (layer->hasAudio() && !layer->hasVideo()) return TimelineLayerIconKind::Audio;
    return TimelineLayerIconKind::Video;
  }
  if (std::dynamic_pointer_cast<ArtifactCameraLayer>(layer)) return TimelineLayerIconKind::Camera;
  if (std::dynamic_pointer_cast<ArtifactLightLayer>(layer)) return TimelineLayerIconKind::Light;
  if (std::dynamic_pointer_cast<ArtifactParticleLayer>(layer)) return TimelineLayerIconKind::Particle;
  if (std::dynamic_pointer_cast<ArtifactFormParticleLayer>(layer)) return TimelineLayerIconKind::Particle;
  if (std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) return TimelineLayerIconKind::Solid;
  if (layer->isNullLayer()) return TimelineLayerIconKind::Null;
  if (layer->hasAudio() && !layer->hasVideo()) return TimelineLayerIconKind::Audio;
  return TimelineLayerIconKind::Solid;
}

  QString variantNameForIndex(int index)
  {
    if (index < 0) {
      return QStringLiteral("A");
    }

    QString name;
    int value = index + 1;
    while (value > 0) {
      --value;
      name.prepend(QChar(static_cast<ushort>('A' + (value % 26))));
      value /= 26;
    }
    return name;
  }

  QString variantDisplayName(const ArtifactAbstractLayerPtr& layer, int index)
  {
    const auto variants = layer ? layer->getVariants() : std::vector<LayerVariant*>{};
    if (index >= 0 && index < variants.size() && variants[index]) {
      const QString name = QString::fromStdString(variants[index]->GetName()).trimmed();
      if (!name.isEmpty()) {
        return name;
      }
    }
    return variantNameForIndex(index);
  }

  QString nextVariantName(const ArtifactAbstractLayerPtr& layer)
  {
    QSet<QString> used;
    if (layer) {
      const auto variants = layer->getVariants();
      for (int i = 0; i < variants.size(); ++i) {
        used.insert(variantDisplayName(layer, i));
      }
    }

    for (int i = 0; i < 1024; ++i) {
      const QString candidate = variantNameForIndex(i);
      if (!used.contains(candidate)) {
        return candidate;
      }
    }

    return QStringLiteral("Variant %1").arg(used.size() + 1);
  }

  QString variantChipText(const ArtifactAbstractLayerPtr& layer)
  {
    if (!layer) {
      return QStringLiteral("Variant");
    }

    const auto variants = layer->getVariants();
    const int activeIdx = static_cast<int>(layer->getActiveVariantIndex());
    if (activeIdx >= 0 && activeIdx < variants.size()) {
      return variantDisplayName(layer, activeIdx);
    }
    return QStringLiteral("Variant");
  }

  QRect variantChipRect(const QRect& rowRect,
                        const ArtifactAbstractLayerPtr& layer,
                        const bool showInlineCombos,
                        const int rowH)
  {
    if (!layer) {
      return {};
    }
    constexpr int kInlineComboReserveProxy = 286;
    const int chipX = rowRect.right() - (showInlineCombos ? kInlineComboReserveProxy : 0) -
                      kVariantChipWidth - 6;
    return QRect(chipX, rowRect.top() + (rowH - kVariantChipHeight) / 2,
                 kVariantChipWidth, kVariantChipHeight);
  }

  void showVariantPickerMenu(QWidget* parent,
                             const ArtifactAbstractLayerPtr& layer,
                             const QPoint& globalPos)
  {
    if (!parent || !layer) {
      return;
    }

    QMenu menu(parent);
    const auto variants = layer->getVariants();
    const int activeIdx = static_cast<int>(layer->getActiveVariantIndex());

    for (int i = 0; i < variants.size(); ++i) {
      const QString label = variantDisplayName(layer, i);
      QAction* action = menu.addAction(label, [parent, layer, i]() {
        auto* cmd = new ChangeActiveVariantCommand(layer,
                                                   layer->getActiveVariantIndex(),
                                                   static_cast<size_t>(i));
        UndoManager::instance()->push(std::unique_ptr<ChangeActiveVariantCommand>(cmd));
        parent->update();
      });
      action->setCheckable(true);
      action->setChecked(i == activeIdx);
    }

    if (!variants.empty()) {
      menu.addSeparator();
    }

    const QString newVariantName = nextVariantName(layer);
    menu.addAction(QStringLiteral("New Variant (%1)").arg(newVariantName), [parent, layer, newVariantName]() {
      auto* cmd = new CreateVariantCommand(layer, newVariantName.toStdString());
      UndoManager::instance()->push(std::unique_ptr<CreateVariantCommand>(cmd));
      parent->update();
    });

    menu.exec(globalPos);
  }

  void buildSelectedLayerMenu(
      QMenu* menu,
      const ArtifactAbstractLayerPtr& layer,
      const std::function<void()>& openInspector,
      const std::function<void()>& openProperties,
      const std::function<void(bool)>& toggleVisibility,
      const std::function<void(bool)>& toggleLock,
      const std::function<void(bool)>& toggleSolo,
      const std::function<void(bool)>& toggleShy,
      const std::function<void()>& selectParent,
      const std::function<void()>& clearParent,
      const std::function<void()>& renameLayer,
      const std::function<void()>& duplicateLayer,
      const std::function<void()>& deleteLayer,
      const std::function<void()>& precomposeSelectedLayers)
  {
    if (!menu || !layer) {
      return;
    }

    auto* inspectorAction = menu->addAction(QStringLiteral("インスペクターを開く"), [openInspector]() { openInspector(); });
    inspectorAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_settings.svg")));
    auto* propertiesAction = menu->addAction(QStringLiteral("プロパティを開く"), [openProperties]() { openProperties(); });
    propertiesAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_settings.svg")));
    menu->addSeparator();
    QMenu* switchMenu = addIconMenu(menu, QStringLiteral("切替"), QStringLiteral("Studio/layermenu_settings.svg"));
    auto* visibilityAction = addIconAction(switchMenu, QStringLiteral("表示/非表示を切替"), QStringLiteral("Studio/layermenu_visibility.svg"),
                                          [toggleVisibility, layer]() { toggleVisibility(!layer->isVisible()); });
    visibilityAction->setCheckable(true);
    visibilityAction->setChecked(layer->isVisible());
    auto* lockAction = addIconAction(switchMenu, QStringLiteral("ロックを切替"), QStringLiteral("Studio/layermenu_lock.svg"),
                                     [toggleLock, layer]() { toggleLock(!layer->isLocked()); });
    lockAction->setCheckable(true);
    lockAction->setChecked(layer->isLocked());
    auto* soloAction = addIconAction(switchMenu, QStringLiteral("ソロを切替"), QStringLiteral("Studio/layermenu_solo_only.svg"),
                                     [toggleSolo, layer]() { toggleSolo(!layer->isSolo()); });
    soloAction->setCheckable(true);
    soloAction->setChecked(layer->isSolo());
    auto* shyAction = addIconAction(switchMenu, QStringLiteral("シャイを切替"), QStringLiteral("Studio/layermenu_shy.svg"),
                                    [toggleShy, layer]() { toggleShy(!layer->isShy()); });
    shyAction->setCheckable(true);
    shyAction->setChecked(layer->isShy());
    menu->addSeparator();
    QMenu* organizeMenu = addIconMenu(menu, QStringLiteral("整理"), QStringLiteral("Studio/layermenu_group.svg"));
    addIconAction(organizeMenu, QStringLiteral("親を選択"), QStringLiteral("Studio/layermenu_parent_select.svg"),
                  [selectParent]() { selectParent(); });
    addIconAction(organizeMenu, QStringLiteral("親を解除"), QStringLiteral("Studio/layermenu_parent_clear.svg"),
                  [clearParent]() { clearParent(); });
    addIconAction(organizeMenu, QStringLiteral("レイヤー名を変更..."), QStringLiteral("Studio/layermenu_rename.svg"),
                  [renameLayer]() { renameLayer(); });
    addIconAction(organizeMenu, QStringLiteral("レイヤーを複製"), QStringLiteral("Studio/layermenu_content_copy.svg"),
                  [duplicateLayer]() { duplicateLayer(); });
    addIconAction(organizeMenu, QStringLiteral("レイヤーを削除"), QStringLiteral("Studio/layermenu_delete.svg"),
                  [deleteLayer]() { deleteLayer(); });
    addIconAction(organizeMenu, QStringLiteral("選択レイヤーをプリコンポーズ"), QStringLiteral("Studio/layermenu_group.svg"),
                  [precomposeSelectedLayers]() { precomposeSelectedLayers(); });
  }

  void applyLayerPanelButtonPalette(QPushButton* button, bool accent = false)
  {
    if (!button) {
      return;
    }
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor background = themeColor(theme.secondaryBackgroundColor, QColor(QStringLiteral("#2D2D30")));
    const QColor surface = themeColor(theme.backgroundColor, QColor(QStringLiteral("#24272D")));
    const QColor text = themeColor(theme.textColor, QColor(QStringLiteral("#CCC")));
    const QColor border = themeColor(theme.borderColor, QColor(QStringLiteral("#1A1A1A")));
    const QColor selection = themeColor(theme.selectionColor, QColor(QStringLiteral("#3B3BEF")));
    const QColor buttonFill = accent ? themeColor(theme.accentColor, QColor(QStringLiteral("#3B3BEF"))) : background;

    button->setAutoFillBackground(true);
    button->setAttribute(Qt::WA_StyledBackground, true);
    QPalette pal = button->palette();
    pal.setColor(QPalette::Window, buttonFill);
    pal.setColor(QPalette::Button, buttonFill);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, text);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, text);
    pal.setColor(QPalette::Disabled, QPalette::Text, text);
    pal.setColor(QPalette::Disabled, QPalette::Button, buttonFill);
    pal.setColor(QPalette::Disabled, QPalette::Window, buttonFill);
    pal.setColor(QPalette::Base, surface);
    pal.setColor(QPalette::Mid, border);
    pal.setColor(QPalette::Highlight, selection);
    pal.setColor(QPalette::HighlightedText, surface);
    button->setPalette(pal);
  }

  void applyLayerPanelComboPalette(QComboBox* combo)
  {
    if (!combo) {
      return;
    }
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor background = themeColor(theme.secondaryBackgroundColor, QColor(QStringLiteral("#2D2D30")));
    const QColor surface = themeColor(theme.backgroundColor, QColor(QStringLiteral("#24272D")));
    const QColor text = themeColor(theme.textColor, QColor(QStringLiteral("#DDD")));
    const QColor border = themeColor(theme.borderColor, QColor(QStringLiteral("#4A4A4F")));
    const QColor selection = themeColor(theme.selectionColor, QColor(QStringLiteral("#3B3BEF")));

    combo->setAutoFillBackground(true);
    QPalette pal = combo->palette();
    pal.setColor(QPalette::Window, background);
    pal.setColor(QPalette::Button, background);
    pal.setColor(QPalette::Base, surface);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Mid, border);
    pal.setColor(QPalette::Highlight, selection);
    pal.setColor(QPalette::HighlightedText, surface);
    combo->setPalette(pal);
    if (auto* view = combo->view()) {
      view->setPalette(pal);
      if (auto* viewport = view->viewport()) {
        viewport->setPalette(pal);
      }
    }
  }

  void applyLayerPanelLineEditPalette(QLineEdit* editor)
  {
    if (!editor) {
      return;
    }
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor background = themeColor(theme.secondaryBackgroundColor, QColor(QStringLiteral("#2D2D30")));
    const QColor surface = themeColor(theme.backgroundColor, QColor(QStringLiteral("#24272D")));
    const QColor text = themeColor(theme.textColor, QColor(QStringLiteral("#F0F0F0")));
    const QColor border = themeColor(theme.accentColor, QColor(QStringLiteral("#4A8BC2")));

    editor->setAutoFillBackground(true);
    QPalette pal = editor->palette();
    pal.setColor(QPalette::Window, background);
    pal.setColor(QPalette::Base, surface);
    pal.setColor(QPalette::Button, background);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::Highlight, border);
    pal.setColor(QPalette::HighlightedText, surface);
    editor->setPalette(pal);
  }

  constexpr int kLayerRowHeight = 28;
  constexpr int kLayerHeaderHeight = 26;
  constexpr int kLayerHeaderButtonSize = 24;
  constexpr int kLayerColumnWidth = 28;
  constexpr int kLayerPropertyColumnCount = 6;
  constexpr int kColumnDividerDragMargin = 4;
  constexpr int kInlineComboHeight = 24;
  constexpr int kInlineBlendWidth = 120;
  constexpr int kInlineParentWidth = 150;
  constexpr int kInlineComboGap = 6;
  constexpr int kInlineComboMarginY = 2;
  constexpr int kInlineComboReserve = kInlineParentWidth + kInlineBlendWidth + kInlineComboGap + 10;
  constexpr int kLayerNameMinWidth = 120;
  constexpr char kLayerReorderMimeType[] = "application/x-artifact-layer-reorder";
  constexpr char kLayerMatteLinkMimeType[] = "application/x-artifact-layer-matte-link";

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
  // Increase pixmap resolution for High DPI displays
  QPixmap pix = icon.pixmap(32, 32);
  if (pix.isNull()) {
   pix = icon.pixmap(48, 48);
  }
  return pix;
 }
 }

namespace {
  class LayerPanelWheelFilter final : public QObject
  {
  public:
   explicit LayerPanelWheelFilter(QWidget* target, QObject* parent = nullptr)
    : QObject(parent), target_(target)
   {
   }

  protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
    Q_UNUSED(watched);
    if (!target_ || event->type() != QEvent::Wheel) {
     return QObject::eventFilter(watched, event);
    }

    auto* wheelEvent = static_cast<QWheelEvent*>(event);
    auto* sourceWidget = qobject_cast<QWidget*>(watched);
    if (!sourceWidget) {
      return QObject::eventFilter(watched, event);
    }

    const QPoint targetPos = target_->mapFromGlobal(sourceWidget->mapToGlobal(wheelEvent->position().toPoint()));
    QWheelEvent forwardedEvent(
        QPointF(targetPos),
        QPointF(sourceWidget->mapToGlobal(wheelEvent->position().toPoint())),
        wheelEvent->pixelDelta(),
        wheelEvent->angleDelta(),
        wheelEvent->buttons(),
        wheelEvent->modifiers(),
        wheelEvent->phase(),
        wheelEvent->inverted(),
        wheelEvent->source());
    QCoreApplication::sendEvent(target_, &forwardedEvent);
    if (forwardedEvent.isAccepted()) {
      wheelEvent->accept();
      return true;
    }
    return QObject::eventFilter(watched, event);
   }

  private:
   QWidget* target_ = nullptr;
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
    if (filePath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
      return LayerType::Shape;
    }
    ArtifactCore::FileTypeDetector detector;
    const auto type = detector.detectByExtension(filePath);
    switch (type) {
    case ArtifactCore::FileType::Image:
      return LayerType::Image;
    case ArtifactCore::FileType::Video:
      return LayerType::Video;
    case ArtifactCore::FileType::Audio:
      return LayerType::Audio;
    default:
      return LayerType::Unknown;
    }
  }

  QStringList collectTimelineDroppedPaths(const QMimeData* mime)
  {
    QStringList validPaths;
    if (!mime) {
      return validPaths;
    }

    if (mime->hasUrls()) {
      for (const auto& url : mime->urls()) {
        if (!url.isLocalFile()) {
          continue;
        }
        const QString filePath = url.toLocalFile();
        const QFileInfo info(filePath);
        if (!info.exists() || info.isDir()) {
          continue;
        }
        const LayerType type = inferLayerTypeFromFile(filePath);
        if (type == LayerType::Image || type == LayerType::Video ||
            type == LayerType::Audio || type == LayerType::Shape) {
          validPaths.append(filePath);
        }
      }
    }

    if (!validPaths.isEmpty() || !mime->hasText()) {
      return validPaths;
    }

    const QStringList paths =
        mime->text().split(QStringLiteral("\n"), Qt::SkipEmptyParts);
    for (const QString& path : paths) {
      const QString trimmed = path.trimmed();
      if (trimmed.isEmpty()) {
        continue;
      }
      const QFileInfo info(trimmed);
      if (!info.exists() || info.isDir()) {
        continue;
      }
      const LayerType type = inferLayerTypeFromFile(trimmed);
      if (type == LayerType::Image || type == LayerType::Video ||
          type == LayerType::Audio || type == LayerType::Shape) {
        validPaths.append(trimmed);
      }
    }

    return validPaths;
  }

  void importTimelineDroppedPaths(const QStringList& validPaths)
  {
    auto* svc = ArtifactProjectService::instance();
    if (!svc || validPaths.isEmpty()) {
      return;
    }

    svc->importAssetsFromPathsAsync(validPaths, [svc](QStringList imported) {
      if (!svc || imported.isEmpty()) {
        return;
      }

      for (const auto& path : imported) {
        const LayerType type = inferLayerTypeFromFile(path);
        if (type == LayerType::Image) {
          ArtifactImageInitParams params(QFileInfo(path).baseName());
          params.setImagePath(path);
          svc->addLayerToCurrentComposition(params);
        } else if (type == LayerType::Shape) {
          ArtifactSvgInitParams params(QFileInfo(path).baseName());
          params.setSvgPath(path);
          svc->addLayerToCurrentComposition(params);
        } else if (type == LayerType::Audio) {
          ArtifactAudioInitParams params(QFileInfo(path).baseName());
          params.setAudioPath(path);
          svc->addLayerToCurrentComposition(params);
        } else if (type == LayerType::Video) {
          ArtifactVideoInitParams params(QFileInfo(path).baseName());
          params.setVideoPath(path);
          svc->addLayerToCurrentComposition(params);
        } else {
          ArtifactLayerInitParams params(QFileInfo(path).baseName(), type);
          svc->addLayerToCurrentComposition(params);
        }
      }
    });
  }

  QString blendModeToText(const LAYER_BLEND_TYPE mode)
  {
    return blendModeDisplayName(toBlendMode(mode));
  }

  std::vector<std::pair<QString, LAYER_BLEND_TYPE>> blendModeItems(const QSet<int>& favorites = {})
  {
    std::vector<std::pair<QString, LAYER_BLEND_TYPE>> items;
    items.reserve(blendModeCount);
    if (!favorites.isEmpty()) {
      for (const int fav : favorites) {
        if (fav >= 0 && fav < static_cast<int>(blendModeCount)) {
          const auto mode = static_cast<BlendMode>(fav);
          items.emplace_back(blendModeDisplayName(mode), toLegacyBlendType(mode));
        }
      }
      items.emplace_back(QString(), LAYER_BLEND_TYPE::BLEND_NORMAL);
    }
    for (std::size_t i = 0; i < blendModeCount; ++i) {
      const auto mode = static_cast<BlendMode>(i);
      const int idx = static_cast<int>(i);
      if (!favorites.contains(idx)) {
        items.emplace_back(blendModeDisplayName(mode), toLegacyBlendType(mode));
      }
    }
    return items;
  }

  QString humanizeTimelinePropertyLabel(QString name)
  {
   static const QHash<QString, QString> explicitLabels = {
    { QStringLiteral("transform.position.x"), QStringLiteral("Position X") },
    { QStringLiteral("transform.position.y"), QStringLiteral("Position Y") },
    { QStringLiteral("transform.scale.x"),    QStringLiteral("Scale X") },
    { QStringLiteral("transform.scale.y"),    QStringLiteral("Scale Y") },
    { QStringLiteral("transform.rotation"),   QStringLiteral("Rotation") },
    { QStringLiteral("transform.anchor.x"),   QStringLiteral("Anchor X") },
    { QStringLiteral("transform.anchor.y"),   QStringLiteral("Anchor Y") },
    { QStringLiteral("layer.opacity"),        QStringLiteral("Opacity") },
    { QStringLiteral("time.inPoint"),         QStringLiteral("In Point") },
    { QStringLiteral("time.outPoint"),        QStringLiteral("Out Point") },
    { QStringLiteral("time.startTime"),       QStringLiteral("Start Time") }
   };
   if (const auto it = explicitLabels.constFind(name); it != explicitLabels.constEnd()) {
    return it.value();
   }

   const int dot = name.lastIndexOf('.');
   if (dot >= 0 && dot + 1 < name.size()) {
    name = name.mid(dot + 1);
   }

   QString out;
   out.reserve(name.size() * 2);
   for (int i = 0; i < name.size(); ++i) {
    const QChar ch = name.at(i);
    if (ch == '_' || ch == '-') {
     out += ' ';
     continue;
    }
    if (i > 0 && ch.isUpper() && name.at(i - 1).isLetterOrNumber()) {
     out += ' ';
    }
    out += ch;
   }

   bool cap = true;
   for (int i = 0; i < out.size(); ++i) {
    if (out.at(i).isSpace()) {
     cap = true;
     continue;
    }
    if (cap) {
     out[i] = out.at(i).toUpper();
     cap = false;
    }
   }
   return out;
  }

  QVector<QString> layerPanelGroupLabels(const ArtifactAbstractLayerPtr& layer)
  {
   QVector<QString> labels;
   if (!layer) {
    return labels;
   }
   QSet<QString> seenPropertyNames;

   for (const auto& group : layer->getLayerPropertyGroups()) {
    if (ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(
            group.name())) {
     continue;
    }
    for (const auto& property : group.sortedProperties()) {
     if (!property || !property->isAnimatable()) {
      continue;
     }
     const QString propertyName = property->getName();
     if (propertyName.isEmpty()) {
      continue;
     }
     if (seenPropertyNames.contains(propertyName)) {
      continue;
     }
     seenPropertyNames.insert(propertyName);
     const QString label = humanizeTimelinePropertyLabel(propertyName);
     labels.push_back(label);
    }
   }

   return labels;
  }

  std::vector<ArtifactCore::PropertyGroup> layerPanelPropertyGroups(const ArtifactAbstractLayerPtr& layer)
  {
   if (!layer) {
    return {};
   }
   auto groups = layer->getLayerPropertyGroups();
   std::vector<ArtifactCore::PropertyGroup> result;
   result.reserve(groups.size());
   for (const auto& group : groups) {
    if (ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(
            group.name())) {
     continue;
    }
    if (group.propertyCount() == 0) {
     continue;
    }
    result.push_back(group);
   }
   return result;
  }

  bool timelineGroupExpandedByDefault(const QString& groupName)
  {
   return ArtifactTimelineKeyframeModel::isTimelinePropertyGroupExpandedByDefault(
       groupName);
  }

  QString compactPropertyRowLabel(const QString& propertyPath)
  {
   const QString fullLabel =
       ArtifactTimelineKeyframeModel::displayLabelForPropertyPath(propertyPath);
   const QString prefix = QStringLiteral("Transform / ");
   if (fullLabel.startsWith(prefix, Qt::CaseInsensitive)) {
    return fullLabel.mid(prefix.size());
   }
   return fullLabel;
  }

  QString maskSelectionPropertyPath(const int maskIndex)
  {
   if (maskIndex < 0) {
    return QString();
   }
   return QStringLiteral("mask.%1.enabled").arg(maskIndex);
  }

  QRect propertyKeyframeMarkerRect(const int widgetWidth, const int rowY, const int rowH)
  {
   constexpr int kMarkerSize = 14;
   constexpr int kMarkerMargin = 10;
   const int markerX = std::max(0, widgetWidth - kMarkerMargin - kMarkerSize);
   const int markerY = rowY + (rowH - kMarkerSize) / 2;
   return QRect(markerX, markerY, kMarkerSize, kMarkerSize);
  }

  bool togglePropertyKeyframeAtCurrentTime(const ArtifactCompositionPtr& composition,
                                           const ArtifactAbstractLayerPtr& layer,
                                           const QString& propertyPath,
                                           const RationalTime& currentTime)
  {
   const QString trimmedPropertyPath = propertyPath.trimmed();
   if (!composition || !layer || trimmedPropertyPath.isEmpty()) {
    return false;
   }

   auto property = layer->getProperty(trimmedPropertyPath);
   if (!property || !property->isAnimatable()) {
    return false;
   }

   if (property->hasKeyFrameAt(currentTime)) {
    property->removeKeyFrame(currentTime);
   } else {
    const QVariant value = property->interpolateValue(currentTime);
    property->addKeyFrame(currentTime, value.isValid() ? value : property->getValue());
   }

   layer->changed();
   ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
       LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                         LayerChangedEvent::ChangeType::Modified});
   return true;
  }
 }

}

namespace Artifact {

QString tt(const char* key, const char* fallback)
{
  return Artifact::TranslationManager::instance().tr(QString::fromUtf8(key), QString::fromUtf8(fallback));
}

 // ============================================================================
 // ArtifactLayerPanelHeaderWidget Implementation
 // ============================================================================

 class ArtifactLayerPanelHeaderWidget::Impl
 {
 public:
  Impl()
  {
    visibilityIcon = loadLayerPanelPixmap(QStringLiteral("Studio/layermenu_visibility.svg"), QStringLiteral("visibility.svg"));
    lockIcon = loadLayerPanelPixmap(QStringLiteral("Studio/layermenu_lock.svg"), QStringLiteral("lock.svg"));
    if (lockIcon.isNull()) lockIcon = loadLayerPanelPixmap(QStringLiteral("Studio/lock_open.svg"), QStringLiteral("unlock.png"));
    soloIcon = loadLayerPanelPixmap(QStringLiteral("Studio/layermenu_solo_only.svg"), QStringLiteral("solo_only.svg"));
    shyIcon = loadLayerPanelPixmap(QStringLiteral("Studio/layermenu_shy.svg"), QStringLiteral("shy.svg"));
  }
  ~Impl() = default;

  QPixmap visibilityIcon;
  QPixmap lockIcon;
  QPixmap soloIcon;
  QPixmap audioIcon;
  QPixmap shyIcon;
  QPixmap parentWhipIcon;
  QIcon parentIcon;
  QIcon blendIcon;
  
  QPushButton* visibilityButton = nullptr;
  QPushButton* lockButton = nullptr;
  QPushButton* soloButton = nullptr;
  QPushButton* audioButton = nullptr;
  QPushButton* layerNameButton = nullptr;
  QPushButton* selectionMenuButton = nullptr;
  QPushButton* shyButton = nullptr;
  QPushButton* parentHeaderButton = nullptr;
  QPushButton* blendHeaderButton = nullptr;
  int columnWidths[kLayerPropertyColumnCount] = {};
  int dragColIndex = -1;
  int dragStartX = 0;
  QVector<int> dragStartWidths;
 };

 W_OBJECT_IMPL(ArtifactLayerPanelHeaderWidget)

ArtifactLayerPanelHeaderWidget::ArtifactLayerPanelHeaderWidget(QWidget* parent)
 : QWidget(parent), impl_(new Impl())
{
  setAcceptDrops(true);
  for (int i = 0; i < kLayerPropertyColumnCount; ++i) {
    impl_->columnWidths[i] = kLayerColumnWidth;
  }
  impl_->visibilityIcon = loadLayerPanelPixmap(QStringLiteral("Studio/layermenu_visibility.svg"), QStringLiteral("visibility.svg"));
  impl_->lockIcon = loadLayerPanelPixmap(QStringLiteral("Studio/layermenu_lock.svg"), QStringLiteral("lock.svg"));
  if (impl_->lockIcon.isNull()) impl_->lockIcon = loadLayerPanelPixmap(QStringLiteral("Studio/lock_open.svg"), QStringLiteral("unlock.png"));
  impl_->soloIcon = loadLayerPanelPixmap(QStringLiteral("Studio/layermenu_solo_only.svg"), QStringLiteral("solo_only.svg"));
  impl_->audioIcon = loadLayerPanelPixmap(QStringLiteral("Studio/layermenu_audiotrack.svg"), QStringLiteral("audiotrack.svg"));
  impl_->shyIcon = loadLayerPanelPixmap(QStringLiteral("Studio/layermenu_shy.svg"), QStringLiteral("shy.svg"));
  impl_->parentIcon = loadLayerPanelIcon(QStringLiteral("Studio/layermenu_parent_select.svg"));
  impl_->blendIcon = loadLayerPanelIcon(QStringLiteral("Studio/merge_type.svg"));
  impl_->parentWhipIcon = loadLayerPanelPixmap(QStringLiteral("Studio/pick_whip_parent.svg"), QStringLiteral("pick_whip_parent.svg"));

  auto visButton = impl_->visibilityButton = new QPushButton();
  visButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  visButton->setIcon(impl_->visibilityIcon);
  visButton->setIconSize(QSize(15, 15));
  visButton->setFocusPolicy(Qt::NoFocus);
  applyLayerPanelButtonPalette(visButton);

  auto lockButton = impl_->lockButton = new QPushButton();
  lockButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->lockIcon.isNull()) lockButton->setIcon(impl_->lockIcon);
  lockButton->setIconSize(QSize(15, 15));
  lockButton->setFocusPolicy(Qt::NoFocus);
  applyLayerPanelButtonPalette(lockButton);

  auto soloButton = impl_->soloButton = new QPushButton();
  soloButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->soloIcon.isNull()) soloButton->setIcon(impl_->soloIcon);
  soloButton->setIconSize(QSize(15, 15));
  soloButton->setFocusPolicy(Qt::NoFocus);
  applyLayerPanelButtonPalette(soloButton);

  auto audioButton = impl_->audioButton = new QPushButton();
  audioButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  if (!impl_->audioIcon.isNull()) audioButton->setIcon(impl_->audioIcon);
  audioButton->setIconSize(QSize(15, 15));
  audioButton->setFocusPolicy(Qt::NoFocus);
  applyLayerPanelButtonPalette(audioButton);

  auto shyButton = impl_->shyButton = new QPushButton;
  shyButton->setFixedSize(QSize(kLayerColumnWidth, kLayerHeaderButtonSize));
  shyButton->setCheckable(true);
  if (!impl_->shyIcon.isNull()) shyButton->setIcon(impl_->shyIcon);
  shyButton->setToolTip("Master Shy Switch");
  applyLayerPanelButtonPalette(shyButton, true);

  auto layerNameButton = impl_->layerNameButton = new QPushButton("Layer Name");
  layerNameButton->setFocusPolicy(Qt::NoFocus);
  layerNameButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  applyLayerPanelButtonPalette(layerNameButton);

  auto selectionMenuButton = impl_->selectionMenuButton = new QPushButton(QStringLiteral("選択レイヤー▼"));
  selectionMenuButton->setFocusPolicy(Qt::NoFocus);
  selectionMenuButton->setFlat(true);
  selectionMenuButton->setIcon(QIcon(resolveIconPath("Studio/layermenu_select_all.svg")));
  selectionMenuButton->setToolTip(QStringLiteral("選択中レイヤーの操作メニュー"));
  applyLayerPanelButtonPalette(selectionMenuButton);
  
  auto parentHeader = impl_->parentHeaderButton = new QPushButton("Parent");
  parentHeader->setFixedWidth(kInlineParentWidth);
  parentHeader->setToolTip(QString());
  parentHeader->setFocusPolicy(Qt::NoFocus);
  parentHeader->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  applyLayerPanelButtonPalette(parentHeader);
  
  auto blendHeader = impl_->blendHeaderButton = new QPushButton("Blend");
  blendHeader->setFixedWidth(kInlineBlendWidth);
  blendHeader->setToolTip(QString());
  blendHeader->setFocusPolicy(Qt::NoFocus);
  blendHeader->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  applyLayerPanelButtonPalette(blendHeader);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(visButton);
  layout->addWidget(lockButton);
  layout->addWidget(soloButton);
  layout->addWidget(audioButton);
  layout->addWidget(shyButton);
  layout->addWidget(layerNameButton, 1);
  layout->addWidget(selectionMenuButton);
  
  // These should match the spacing in paintEvent (kInlineComboGap = 6)
  layout->addWidget(parentHeader);
  layout->addSpacing(kInlineComboGap);
  layout->addWidget(blendHeader);
  layout->addSpacing(10); // Right margin in paintEvent logic

  QObject::connect(shyButton, &QPushButton::toggled, this, [this](bool checked) {
    Q_EMIT shyToggled(checked);
  });
  QObject::connect(visButton, &QPushButton::clicked, this, [this]() {
    auto* service = ArtifactProjectService::instance();
    auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
    if (!service || !comp) {
      return;
    }
    auto* panel = qobject_cast<ArtifactLayerPanelWidget*>(parentWidget());
    const LayerID selectedId = panel ? panel->selectedLayerId() : LayerID{};
    auto layer = selectedId.isNil() ? ArtifactAbstractLayerPtr{} : comp->layerById(selectedId);
    if (!layer) {
      return;
    }
    layer->setVisible(!layer->isVisible());
    if (panel) panel->updateLayout();
  });
  QObject::connect(lockButton, &QPushButton::clicked, this, [this]() {
    auto* service = ArtifactProjectService::instance();
    auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
    if (!service || !comp) {
      return;
    }
    auto* panel = qobject_cast<ArtifactLayerPanelWidget*>(parentWidget());
    const LayerID selectedId = panel ? panel->selectedLayerId() : LayerID{};
    auto layer = selectedId.isNil() ? ArtifactAbstractLayerPtr{} : comp->layerById(selectedId);
    if (!layer) {
      return;
    }
    layer->setLocked(!layer->isLocked());
    if (panel) panel->updateLayout();
  });
  QObject::connect(soloButton, &QPushButton::clicked, this, [this]() {
    auto* service = ArtifactProjectService::instance();
    auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
    if (!service || !comp) {
      return;
    }
    auto* panel = qobject_cast<ArtifactLayerPanelWidget*>(parentWidget());
    const LayerID selectedId = panel ? panel->selectedLayerId() : LayerID{};
    auto layer = selectedId.isNil() ? ArtifactAbstractLayerPtr{} : comp->layerById(selectedId);
    if (!layer) {
      return;
    }
    layer->setSolo(!layer->isSolo());
    if (panel) panel->updateLayout();
  });
  QObject::connect(audioButton, &QPushButton::clicked, this, [this]() {
    auto* service = ArtifactProjectService::instance();
    auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
    if (!service || !comp) {
      return;
    }
    auto* panel = qobject_cast<ArtifactLayerPanelWidget*>(parentWidget());
    const LayerID selectedId = panel ? panel->selectedLayerId() : LayerID{};
    auto layer = selectedId.isNil() ? ArtifactAbstractLayerPtr{} : comp->layerById(selectedId);
    if (!layer) {
      return;
    }
    if (auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
      videoLayer->setAudioMuted(!videoLayer->isAudioMuted());
      if (panel) panel->updateLayout();
      return;
    }
    if (auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
      audioLayer->mute();
      if (panel) panel->updateLayout();
    }
  });

  setAutoFillBackground(true);
  {
    const auto& theme = ArtifactCore::currentDCCTheme();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, themeColor(theme.secondaryBackgroundColor, QColor(QStringLiteral("#2D2D30"))));
    pal.setColor(QPalette::WindowText, themeColor(theme.textColor, QColor(QStringLiteral("#CCC"))));
    setPalette(pal);
  }
  setFixedHeight(kLayerHeaderHeight);
}

 ArtifactLayerPanelHeaderWidget::~ArtifactLayerPanelHeaderWidget()
 {
  delete impl_;
 }

int ArtifactLayerPanelHeaderWidget::buttonSize() const { return kLayerHeaderButtonSize; }
int ArtifactLayerPanelHeaderWidget::iconSize() const { return 14; }
QPushButton* ArtifactLayerPanelHeaderWidget::selectionMenuButton() const { return impl_ ? impl_->selectionMenuButton : nullptr; }
int ArtifactLayerPanelHeaderWidget::totalHeaderHeight() const
{
 return minimumHeight() > 0 ? minimumHeight() : sizeHint().height();
}

void ArtifactLayerPanelHeaderWidget::mousePressEvent(QMouseEvent* event)
{
 QWidget::mousePressEvent(event);
}

void ArtifactLayerPanelHeaderWidget::mouseMoveEvent(QMouseEvent* event)
{
 QWidget::mouseMoveEvent(event);
}

void ArtifactLayerPanelHeaderWidget::mouseReleaseEvent(QMouseEvent* event)
{
 QWidget::mouseReleaseEvent(event);
}

void ArtifactLayerPanelHeaderWidget::leaveEvent(QEvent* event)
{
 QWidget::leaveEvent(event);
}

void ArtifactLayerPanelHeaderWidget::dragEnterEvent(QDragEnterEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (!validPaths.isEmpty()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void ArtifactLayerPanelHeaderWidget::dragMoveEvent(QDragMoveEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (!validPaths.isEmpty()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void ArtifactLayerPanelHeaderWidget::dropEvent(QDropEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (validPaths.isEmpty()) {
    event->ignore();
    return;
  }
  importTimelineDroppedPaths(validPaths);
  event->acceptProposedAction();
}

// ============================================================================
// ArtifactLayerPanelWidget Implementation
// ============================================================================

class ArtifactLayerPanelWidget::Impl;

using RowKind = TimelineRowKind;

struct VisibleRow {
 ArtifactAbstractLayerPtr layer;
 int depth = 0;
 bool hasChildren = false;
 bool expanded = true;
 RowKind kind = RowKind::Layer;
 QString label;
 QString propertyPath;
 QString groupKey;
 QString auxiliaryText;
 LayerPresentationBadgeTone auxiliaryTone = LayerPresentationBadgeTone::Neutral;
 QString stateText;
 LayerPresentationBadgeTone stateTone = LayerPresentationBadgeTone::Neutral;
};

QString groupLayerSummaryText(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer || !layer->isGroupLayer()) {
  return {};
 }
 auto* groupLayer = dynamic_cast<ArtifactGroupLayer*>(layer.get());
 if (!groupLayer) {
  return QStringLiteral("Group");
 }
 const int childCount = static_cast<int>(groupLayer->children().size());
 if (childCount <= 0) {
  return QStringLiteral("Empty Group");
 }
 return QStringLiteral("%1 items").arg(childCount);
}

QString summarizeLayerState(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer) {
  return {};
 }
 if (!layer->isVisible()) {
  return QStringLiteral("Hidden");
 }
 if (layer->isLocked()) {
  return QStringLiteral("Locked");
 }
 if (layer->isSolo()) {
  return QStringLiteral("Solo");
 }
 if (layer->isShy()) {
  return QStringLiteral("Shy");
 }
 const bool hasMasks = layer->hasMasks();
 const bool hasMattes = !layer->matteReferences().empty();
 if (hasMasks && hasMattes) {
  return QStringLiteral("Mask + Matte");
 }
 if (hasMasks) {
  return QStringLiteral("Masked");
 }
 if (hasMattes) {
  return QStringLiteral("Matted");
 }
 if (layer->hasParent()) {
  return QStringLiteral("Child");
 }
 return {};
}

LayerPresentationBadgeTone summarizeLayerStateTone(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer) {
  return LayerPresentationBadgeTone::Neutral;
 }
 if (!layer->isVisible()) {
  return LayerPresentationBadgeTone::Neutral;
 }
 if (layer->isLocked() || layer->isShy()) {
  return LayerPresentationBadgeTone::Special;
 }
 if (layer->isSolo()) {
  return LayerPresentationBadgeTone::Motion;
 }
 if (layer->hasMasks() || !layer->matteReferences().empty()) {
  return LayerPresentationBadgeTone::Special;
 }
 if (layer->hasParent()) {
  return LayerPresentationBadgeTone::Container;
 }
 return LayerPresentationBadgeTone::Neutral;
}

bool layerMatchesDisplayMode(const ArtifactAbstractLayerPtr& layer,
                             const TimelineLayerDisplayMode mode,
                             const LayerID& selectedLayerId)
{
 if (!layer) {
  return false;
 }
 switch (mode) {
 case TimelineLayerDisplayMode::AudioOnly:
  return layer->hasAudio();
 case TimelineLayerDisplayMode::VideoOnly:
  return layer->hasVideo();
 case TimelineLayerDisplayMode::SelectedOnly:
  return !selectedLayerId.isNil() && layer->id() == selectedLayerId;
 case TimelineLayerDisplayMode::AllLayers:
 case TimelineLayerDisplayMode::AnimatedOnly:
 case TimelineLayerDisplayMode::ImportantAndKeyframed:
 default:
  return true;
 }
}

bool propertyMatchesDisplayMode(const std::shared_ptr<ArtifactCore::AbstractProperty>& property,
                                const TimelineLayerDisplayMode mode)
{
 if (!property) {
  return false;
 }
 switch (mode) {
 case TimelineLayerDisplayMode::AnimatedOnly:
 case TimelineLayerDisplayMode::ImportantAndKeyframed:
  return property->isAnimatable() && !property->getKeyFrames().empty();
 case TimelineLayerDisplayMode::AudioOnly:
 case TimelineLayerDisplayMode::VideoOnly:
 case TimelineLayerDisplayMode::SelectedOnly:
 case TimelineLayerDisplayMode::AllLayers:
 default:
  return true;
 }
}

bool groupHasVisibleProperties(const ArtifactCore::PropertyGroup& group,
                               const TimelineLayerDisplayMode mode)
{
 for (const auto& property : group.sortedProperties()) {
  if (propertyMatchesDisplayMode(property, mode)) {
   return true;
  }
 }
 return false;
}

ArtifactLayerSelectionManager* currentLayerSelectionManager()
{
 auto* app = ArtifactApplicationManager::instance();
 return app ? app->layerSelectionManager() : nullptr;
}

QVector<LayerID> selectedLayerIdsSnapshot()
{
 QVector<LayerID> ids;
 auto* selectionManager = currentLayerSelectionManager();
 if (!selectionManager) {
  return ids;
 }

 const auto selected = selectionManager->selectedLayersInOrder();
 ids.reserve(static_cast<int>(selected.size()));
 for (const auto& layer : selected) {
  if (layer) {
   ids.push_back(layer->id());
  }
 }
 return ids;
}

bool isLayerSelectedInSelectionManager(const LayerID& id)
{
 if (id.isNil()) {
  return false;
 }
 auto* selectionManager = currentLayerSelectionManager();
 if (!selectionManager) {
  return false;
 }
 const auto selected = selectionManager->selectedLayers();
 for (const auto& layer : selected) {
  if (layer && layer->id() == id) {
   return true;
  }
 }
 return false;
}

QVector<LayerID> selectedLayerIdsInVisibleOrder(const QVector<VisibleRow>& rows)
{
  QVector<LayerID> ids;
  for (const auto& row : rows) {
  if (row.kind != RowKind::Layer || !row.layer) {
   continue;
  }
  if (isLayerSelectedInSelectionManager(row.layer->id())) {
   ids.push_back(row.layer->id());
  }
  }
  return ids;
}

QVector<LayerID> selectedLayerIdsInCompositionOrder(const ArtifactCompositionPtr& comp,
                                                   const QVector<LayerID>& selectedIds)
{
  QVector<LayerID> ordered;
  if (!comp || selectedIds.isEmpty()) {
    return ordered;
  }
  QSet<QString> wanted;
  for (const auto& id : selectedIds) {
    if (!id.isNil()) {
      wanted.insert(id.toString());
    }
  }
  for (const auto& layer : comp->allLayer()) {
    if (layer && wanted.contains(layer->id().toString())) {
      ordered.push_back(layer->id());
    }
  }
  return ordered;
}

QVector<LayerID> layerIdsWithStride(const QVector<LayerID>& orderedIds, int stride, int offset)
{
  QVector<LayerID> ids;
  if (stride <= 0) {
    return ids;
  }
  for (int i = offset; i < orderedIds.size(); i += stride) {
    ids.push_back(orderedIds[i]);
  }
  return ids;
}

QVector<LayerID> layerIdsWithSameName(const ArtifactCompositionPtr& comp, const QVector<LayerID>& selectedIds)
{
  QVector<LayerID> ids;
  if (!comp || selectedIds.isEmpty()) {
    return ids;
  }

  QSet<QString> names;
  for (const auto& id : selectedIds) {
    auto layer = comp->layerById(id);
    if (!layer) {
      continue;
    }
    const QString name = layer->layerName().trimmed();
    if (!name.isEmpty()) {
      names.insert(name);
    }
  }
  if (names.isEmpty()) {
    return ids;
  }

  for (const auto& layer : comp->allLayer()) {
    if (!layer) {
      continue;
    }
    if (names.contains(layer->layerName().trimmed())) {
      ids.push_back(layer->id());
    }
  }
  return ids;
}

void replaceSelectionWithIds(const ArtifactCompositionPtr& comp, const QVector<LayerID>& ids)
{
  auto* selectionManager = currentLayerSelectionManager();
  if (!selectionManager) {
    return;
  }

  selectionManager->setActiveComposition(comp);
  selectionManager->clearSelection();
  for (const auto& id : ids) {
    if (comp) {
      selectionManager->addToSelection(comp->layerById(id));
    }
  }
}

std::vector<LayerPlacementSnapshot> captureLayerPlacements(
    const ArtifactCompositionPtr& comp, const QVector<LayerID>& ids)
{
  std::vector<LayerPlacementSnapshot> snapshots;
  if (!comp || ids.isEmpty()) {
    return snapshots;
  }
  snapshots.reserve(static_cast<size_t>(ids.size()));
  for (const auto& id : ids) {
    auto layer = comp->layerById(id);
    if (!layer) {
      continue;
    }
    LayerPlacementSnapshot snapshot;
    snapshot.id = id;
    snapshot.bounds = layer->transformedBoundingBox();
    snapshot.position = QPointF(layer->transform3D().positionX(),
                                layer->transform3D().positionY());
    snapshots.push_back(snapshot);
  }
  return snapshots;
}

bool applyAlignPreset(const ArtifactCompositionPtr& comp,
                      const QVector<LayerID>& selectedIds, int presetIndex)
{
  if (!comp || selectedIds.size() < 2) {
    return false;
  }

  auto snapshots = captureLayerPlacements(comp, selectedIds);
  if (snapshots.size() < 2) {
    return false;
  }

  std::vector<ArtifactCore::AlignmentObject> objects;
  objects.reserve(snapshots.size());
  for (size_t i = 0; i < snapshots.size(); ++i) {
    const auto& snapshot = snapshots[i];
    ArtifactCore::AlignmentObject obj;
    obj.id = static_cast<int>(i);
    obj.bounds = snapshot.bounds;
    obj.currentPosition = snapshot.position;
    objects.push_back(obj);
  }

  const ArtifactCore::AlignType types[] = {
      ArtifactCore::AlignType::Left,
      ArtifactCore::AlignType::CenterHorizontal,
      ArtifactCore::AlignType::Right,
      ArtifactCore::AlignType::Top,
      ArtifactCore::AlignType::CenterVertical,
      ArtifactCore::AlignType::Bottom,
  };
  QRectF dummy;
  ArtifactCore::LayerAlignment::align(
      objects, types[presetIndex % (static_cast<int>(std::size(types)))],
      ArtifactCore::AlignmentTarget::Selection, dummy);

  const ArtifactCore::RationalTime time(0, 30000);
  for (size_t i = 0; i < snapshots.size() && i < objects.size(); ++i) {
    const size_t sourceIndex = static_cast<size_t>(objects[i].id);
    if (sourceIndex >= snapshots.size()) {
      continue;
    }
    auto layer = comp->layerById(snapshots[sourceIndex].id);
    if (!layer) {
      continue;
    }
    layer->transform3D().setPosition(time, objects[i].currentPosition.x(),
                                     objects[i].currentPosition.y());
    layer->changed();
  }
  return true;
}

bool applyDistributePreset(const ArtifactCompositionPtr& comp,
                           const QVector<LayerID>& selectedIds, int presetIndex)
{
  if (!comp || selectedIds.size() < 3) {
    return false;
  }

  auto snapshots = captureLayerPlacements(comp, selectedIds);
  if (snapshots.size() < 3) {
    return false;
  }

  std::vector<ArtifactCore::AlignmentObject> objects;
  objects.reserve(snapshots.size());
  for (size_t i = 0; i < snapshots.size(); ++i) {
    const auto& snapshot = snapshots[i];
    ArtifactCore::AlignmentObject obj;
    obj.id = static_cast<int>(i);
    obj.bounds = snapshot.bounds;
    obj.currentPosition = snapshot.position;
    objects.push_back(obj);
  }

  const ArtifactCore::DistributeType types[] = {
      ArtifactCore::DistributeType::Left,
      ArtifactCore::DistributeType::CenterHorizontal,
      ArtifactCore::DistributeType::Right,
      ArtifactCore::DistributeType::Top,
      ArtifactCore::DistributeType::CenterVertical,
      ArtifactCore::DistributeType::Bottom,
  };
  ArtifactCore::LayerAlignment::distribute(
      objects, types[presetIndex % (static_cast<int>(std::size(types)))]);

  const ArtifactCore::RationalTime time(0, 30000);
  for (size_t i = 0; i < snapshots.size() && i < objects.size(); ++i) {
    const size_t sourceIndex = static_cast<size_t>(objects[i].id);
    if (sourceIndex >= snapshots.size()) {
      continue;
    }
    auto layer = comp->layerById(snapshots[sourceIndex].id);
    if (!layer) {
      continue;
    }
    layer->transform3D().setPosition(time, objects[i].currentPosition.x(),
                                     objects[i].currentPosition.y());
    layer->changed();
  }
  return true;
}

bool shuffleSelectedLayers(const ArtifactCompositionPtr& comp,
                           const QVector<LayerID>& selectedIds, int presetIndex)
{
  if (!comp || selectedIds.size() < 2) {
    return false;
  }

  auto orderedIds = selectedLayerIdsInCompositionOrder(comp, selectedIds);
  if (orderedIds.size() < 2) {
    return false;
  }

  switch (presetIndex % 4) {
  case 0:
    std::reverse(orderedIds.begin(), orderedIds.end());
    break;
  case 1:
    if (orderedIds.size() > 2) {
      QVector<LayerID> mixed;
      mixed.reserve(orderedIds.size());
      int left = 0;
      int right = orderedIds.size() - 1;
      while (left <= right) {
        mixed.push_back(orderedIds[left++]);
        if (left <= right) {
          mixed.push_back(orderedIds[right--]);
        }
      }
      orderedIds = mixed;
    }
    break;
  case 2:
    if (orderedIds.size() > 2) {
      QVector<LayerID> shifted;
      shifted.reserve(orderedIds.size());
      for (int i = 1; i < orderedIds.size(); ++i) {
        shifted.push_back(orderedIds[i]);
      }
      shifted.push_back(orderedIds.front());
      orderedIds = shifted;
    }
    break;
  default:
    if (orderedIds.size() > 3) {
      QVector<LayerID> spaced;
      spaced.reserve(orderedIds.size());
      for (int i = 0; i < orderedIds.size(); i += 2) {
        spaced.push_back(orderedIds[i]);
      }
      for (int i = 1; i < orderedIds.size(); i += 2) {
        spaced.push_back(orderedIds[i]);
      }
      orderedIds = spaced;
    }
    break;
  }

  const auto layers = comp->allLayer();
  QVector<LayerID> untouched;
  untouched.reserve(layers.size() - orderedIds.size());
  QSet<QString> moved;
  for (const auto& id : orderedIds) {
    moved.insert(id.toString());
  }
  for (const auto& layer : layers) {
    if (!layer || moved.contains(layer->id().toString())) {
      continue;
    }
    untouched.push_back(layer->id());
  }

  int insertIndex = 0;
  for (const auto& layerId : untouched) {
    if (comp->layerById(layerId)) {
      comp->moveLayerToIndex(layerId, insertIndex++);
    }
  }
  for (const auto& layerId : orderedIds) {
    if (comp->layerById(layerId)) {
      comp->moveLayerToIndex(layerId, insertIndex++);
    }
  }
  return true;
}

void nudgeSelectedLayerSpacing(const ArtifactCompositionPtr& comp,
                               const QVector<LayerID>& selectedIds,
                               Qt::Key key)
{
  if (!comp || selectedIds.size() < 2) {
    return;
  }
  const int step = 8;
  const int delta = (key == Qt::Key_Left || key == Qt::Key_Up) ? -step : step;
  for (const auto& id : selectedIds) {
    auto layer = comp->layerById(id);
    if (!layer) {
      continue;
    }
    const ArtifactCore::RationalTime time(0, 30000);
    const float x = layer->transform3D().positionX();
    const float y = layer->transform3D().positionY();
    if (key == Qt::Key_Left || key == Qt::Key_Right) {
      layer->transform3D().setPosition(time, x + delta, y);
    } else {
      layer->transform3D().setPosition(time, x, y + delta);
    }
    layer->changed();
  }
}

QVector<LayerID> visibleLayerIdsInRange(const QVector<VisibleRow>& rows,
                                        const LayerID& from,
                                        const LayerID& to)
{
 if (from.isNil() || to.isNil()) {
  return {};
 }

 int fromIndex = -1;
 int toIndex = -1;
 for (int i = 0; i < rows.size(); ++i) {
  const auto& row = rows[i];
  if (row.kind != RowKind::Layer || !row.layer) {
   continue;
  }
  if (fromIndex < 0 && row.layer->id() == from) {
   fromIndex = i;
  }
  if (toIndex < 0 && row.layer->id() == to) {
   toIndex = i;
  }
  if (fromIndex >= 0 && toIndex >= 0) {
   break;
  }
 }

 if (fromIndex < 0 || toIndex < 0) {
  return {};
 }

 const int begin = std::min(fromIndex, toIndex);
 const int end = std::max(fromIndex, toIndex);
 QVector<LayerID> ids;
 for (int i = begin; i <= end; ++i) {
  const auto& row = rows[i];
  if (row.kind == RowKind::Layer && row.layer) {
   ids.push_back(row.layer->id());
  }
 }
 return ids;
}

QString matteTypeToText(MatteType type)
{
 switch (type) {
 case MatteType::Alpha:
  return QStringLiteral("Alpha");
 case MatteType::Luma:
  return QStringLiteral("Luma");
 case MatteType::InverseAlpha:
  return QStringLiteral("Inv Alpha");
 case MatteType::InverseLuma:
  return QStringLiteral("Inv Luma");
 }
 return QStringLiteral("Matte");
}

QString matteBlendModeToText(MatteBlendMode mode)
{
 switch (mode) {
 case MatteBlendMode::Add:
  return QStringLiteral("Add");
 case MatteBlendMode::Subtract:
  return QStringLiteral("Sub");
 case MatteBlendMode::Intersect:
  return QStringLiteral("Int");
 case MatteBlendMode::Difference:
  return QStringLiteral("Diff");
 }
 return QStringLiteral("Blend");
}

QString maskModeToText(MaskMode mode)
{
 switch (mode) {
 case MaskMode::Add:
  return QStringLiteral("Add");
 case MaskMode::Subtract:
  return QStringLiteral("Sub");
 case MaskMode::Intersect:
  return QStringLiteral("Int");
 case MaskMode::Difference:
  return QStringLiteral("Diff");
 }
 return QStringLiteral("Mask");
}

QString matteSummaryLabel(const LayerMatteReference& ref, int index)
{
 const QString base = QStringLiteral("Matte %1").arg(index + 1);
 QStringList tags;
 tags << matteTypeToText(ref.type);
 tags << QStringLiteral("Blend %1").arg(matteBlendModeToText(ref.blendMode));
  if (ref.invert) {
  tags << QStringLiteral("Inverted");
 }
 if (!ref.enabled) {
  tags << QStringLiteral("Off");
 }
 if (ref.opacity < 0.999f) {
  tags << QStringLiteral("Opacity %1").arg(QString::number(ref.opacity, 'f', 2));
 }
 return tags.isEmpty() ? base : base + QStringLiteral("  ") + tags.join(QStringLiteral(" / "));
}

QString matteSourceBadgeLabel(const ArtifactCompositionPtr& comp, const ArtifactAbstractLayerPtr& layer)
{
 if (!layer) {
  return QStringLiteral("Matte");
 }

 const auto matteRefs = layer->matteReferences();
 if (matteRefs.empty()) {
  return QStringLiteral("Matte");
 }

 const auto resolveSourceName = [&](const LayerMatteReference& ref) -> QString {
  if (!comp || ref.sourceLayerId.isNil()) {
   return QStringLiteral("<missing>");
  }
  auto source = comp->layerById(ref.sourceLayerId);
  if (!source) {
   return QStringLiteral("<missing>");
  }
  const QString name = source->layerName().trimmed();
  return name.isEmpty() ? ref.sourceLayerId.toString() : name;
 };

 const QString sourceName = resolveSourceName(matteRefs.front());
 const QString matteType = matteTypeToText(matteRefs.front().type);
 if (matteRefs.size() == 1) {
  return QStringLiteral("Matte: %1 (%2)").arg(sourceName, matteType);
 }
 return QStringLiteral("Matte: %1 (%2) +%3").arg(sourceName, matteType).arg(matteRefs.size() - 1);
}

LayerMatteReference makeDefaultMatteReference(const LayerID& sourceLayerId)
{
 LayerMatteReference ref;
 ref.sourceLayerId = sourceLayerId;
 ref.enabled = true;
 ref.type = MatteType::Alpha;
 ref.blendMode = MatteBlendMode::Add;
 ref.fitMode = MatteFitMode::Stretch;
 ref.opacity = 1.0f;
 ref.invert = false;
 return ref;
}

bool matteDropWouldCreateCycle(const ArtifactCompositionPtr& comp,
                               const ArtifactAbstractLayerPtr& targetLayer,
                               const LayerID& sourceLayerId)
{
 if (!comp || !targetLayer || sourceLayerId.isNil()) {
  return true;
 }

 const LayerID targetLayerId = targetLayer->id();
 if (targetLayerId == sourceLayerId) {
  return true;
 }

 QSet<QString> visited;
 QVector<LayerID> pending;
 pending.push_back(sourceLayerId);

 while (!pending.isEmpty()) {
  const LayerID currentId = pending.back();
  pending.pop_back();
  if (currentId.isNil()) {
   continue;
  }

  const QString key = currentId.toString();
  if (visited.contains(key)) {
   continue;
  }
  visited.insert(key);

  if (currentId == targetLayerId) {
   return true;
  }

  auto currentLayer = comp->layerById(currentId);
  if (!currentLayer) {
   continue;
  }

  for (const auto& ref : currentLayer->matteReferences()) {
   if (!ref.enabled || ref.sourceLayerId.isNil()) {
    continue;
   }
   pending.push_back(ref.sourceLayerId);
  }
 }

 return false;
}

bool applyMatteTypeToLayer(const ArtifactCompositionPtr& comp,
                           const ArtifactAbstractLayerPtr& layer,
                           int matteIndex,
                           MatteType matteType)
{
 if (!comp || !layer || matteIndex < 0) {
  return false;
 }
 auto beforeRefs = layer->matteReferences();
 if (matteIndex >= static_cast<int>(beforeRefs.size())) {
  return false;
 }

 auto afterRefs = beforeRefs;
 auto& ref = afterRefs[matteIndex];
 const MatteType previousType = ref.type;
 const bool previousInvert = ref.invert;
 ref.type = matteType;
 ref.invert = false;
 if (ref.type == previousType && ref.invert == previousInvert) {
  return false;
 }

 auto* cmd = new ChangeLayerMatteReferencesCommand(layer,
                                                   std::move(beforeRefs),
                                                   std::move(afterRefs));
 UndoManager::instance()->push(std::unique_ptr<ChangeLayerMatteReferencesCommand>(cmd));
 return true;
}

QString maskSummaryLabel(const LayerMask& mask, int index)
{
 const QString base = QStringLiteral("Mask %1").arg(index + 1);
 QStringList tags;
 const int pathCount = mask.maskPathCount();
 tags << QStringLiteral("%1 path%2").arg(pathCount).arg(pathCount == 1 ? QString() : QStringLiteral("s"));
 QStringList modeTags;
 const int limit = std::min(pathCount, 3);
 for (int i = 0; i < limit; ++i) {
  modeTags << maskModeToText(mask.maskPath(i).mode());
 }
 if (!modeTags.isEmpty()) {
  tags << QStringLiteral("Modes %1").arg(modeTags.join(QStringLiteral(",")));
 }
 if (pathCount > 3) {
  tags << QStringLiteral("+%1 more").arg(pathCount - 3);
 }
 if (!mask.isEnabled()) {
  tags << QStringLiteral("Off");
 }
 return base + QStringLiteral("  ") + tags.join(QStringLiteral(" / "));
}

class ArtifactLayerPanelWidget::Impl
{
public:
 Impl()
  {
  visibilityIcon    = loadLayerPanelPixmap(QStringLiteral("Studio/visibility.svg"),     QStringLiteral("eye.png"));
  lockIcon          = loadLayerPanelPixmap(QStringLiteral("Studio/lock.svg"));
  soloIcon          = loadLayerPanelPixmap(QStringLiteral("Studio/group.svg"),           QStringLiteral("solo.png"));
  audioIcon         = loadLayerPanelPixmap(QStringLiteral("Studio/volume.svg"),          QStringLiteral("volume.png"));
  shyIcon           = loadLayerPanelPixmap(QStringLiteral("Studio/visibility_off.svg"),  QStringLiteral("shy.png"));
  parentWhipIcon    = loadLayerPanelPixmap(QStringLiteral("Studio/pick_whip_parent.svg"), QStringLiteral("pick_whip_parent.svg"));
    // [Fix B] 右クリックメニュー用アイコンを構築時にキャッシュ（毎回 SVG パースを防ぐ）
    iconRename        = loadLayerPanelIcon(QStringLiteral("Studio/edit.svg"));
    iconCopy          = loadLayerPanelIcon(QStringLiteral("Studio/content_copy.svg"));
    iconDelete        = loadLayerPanelIcon(QStringLiteral("Studio/delete.svg"));
    iconFileOpen      = loadLayerPanelIcon(QStringLiteral("Studio/file_open.svg"));
    iconVisOn         = loadLayerPanelIcon(QStringLiteral("Studio/layermenu_visibility.svg"));
    iconVisOff        = loadLayerPanelIcon(QStringLiteral("Studio/layermenu_visibility_off.svg"), QStringLiteral("visibility_off.svg"));
    iconLock          = loadLayerPanelIcon(QStringLiteral("Studio/layermenu_lock.svg"));
    iconUnlock        = loadLayerPanelIcon(QStringLiteral("Studio/lock_open.svg"));
    iconSolo          = loadLayerPanelIcon(QStringLiteral("Studio/layermenu_solo_only.svg"));
    iconShy           = loadLayerPanelIcon(QStringLiteral("Studio/layermenu_shy.svg"));
    iconLink          = loadLayerPanelIcon(QStringLiteral("Studio/layermenu_parent_select.svg"));
    iconLinkOff       = loadLayerPanelIcon(QStringLiteral("Studio/layermenu_parent_clear.svg"));
    iconCreateSolid   = loadLayerPanelIcon(QStringLiteral("Studio/palette.svg"));
    iconCreateNull    = loadLayerPanelIcon(QStringLiteral("Studio/transform.svg"));
    iconCreateAdjust  = loadLayerPanelIcon(QStringLiteral("Studio/tune.svg"));
    iconCreateText    = loadLayerPanelIcon(QStringLiteral("Studio/title.svg"));
    iconCreateModel3D = loadLayerPanelIcon(QStringLiteral("Studio/model3d.svg"));
    iconLayerGeneric      = loadLayerPanelIcon(QStringLiteral("Studio/timeline_layer.svg"));
    iconLayerSolid        = loadLayerPanelIcon(QStringLiteral("Studio/layer_composite.svg"));
    iconLayerImage        = loadLayerPanelIcon(QStringLiteral("Studio/photo_filter.svg"));
    iconLayerSvg          = loadLayerPanelIcon(QStringLiteral("Studio/svg_layer.svg"));
    iconLayerVideo        = loadLayerPanelIcon(QStringLiteral("Studio/videocam.svg"));
    iconLayerAudio        = loadLayerPanelIcon(QStringLiteral("Studio/layermenu_audiotrack.svg"));
    iconLayerText         = loadLayerPanelIcon(QStringLiteral("Studio/text_fields.svg"));
    iconLayerShape        = loadLayerPanelIcon(QStringLiteral("Studio/shape_rect.svg"));
    iconLayerPrecomp      = loadLayerPanelIcon(QStringLiteral("Studio/composition.svg"));
    iconLayerCamera       = loadLayerPanelIcon(QStringLiteral("Studio/camera_alt.svg"));
    iconLayerLight        = loadLayerPanelIcon(QStringLiteral("Studio/wb_sunny.svg"));
    iconLayerGroup        = loadLayerPanelIcon(QStringLiteral("Studio/group.svg"));
    iconLayerNull         = loadLayerPanelIcon(QStringLiteral("Studio/transform.svg"));
    iconLayerAdjustment   = loadLayerPanelIcon(QStringLiteral("Studio/tune.svg"));
    iconLayerParticle     = loadLayerPanelIcon(QStringLiteral("Studio/particle.svg"));
    iconLayerClone        = loadLayerPanelIcon(QStringLiteral("Studio/content_copy.svg"));
    iconLayerModel3D      = loadLayerPanelIcon(QStringLiteral("Studio/model3d.svg"));
    iconLayerConstruction = loadLayerPanelIcon(QStringLiteral("Studio/draw.svg"));
  }
  ~Impl() = default;

  CompositionID compositionId;
  ArtifactTimelineKeyframeModel* keyframeModel = nullptr;
  RationalTime currentTime{};
  QString currentPropertyPath;

  QPixmap visibilityIcon;
  QPixmap lockIcon;
  QPixmap soloIcon;
  QPixmap audioIcon;
  QPixmap shyIcon;
  QPixmap parentWhipIcon;
  // [Fix B] 右クリックメニュー用アイコンキャッシュ
  QIcon iconRename, iconCopy, iconDelete, iconFileOpen;
  QIcon iconVisOn, iconVisOff, iconLock, iconUnlock, iconSolo, iconShy;
  QIcon iconLink, iconLinkOff;
  QIcon iconCreateSolid, iconCreateNull, iconCreateAdjust, iconCreateText, iconCreateModel3D;
  QIcon iconLayerGeneric, iconLayerSolid, iconLayerImage, iconLayerSvg, iconLayerVideo, iconLayerAudio;
  QIcon iconLayerText, iconLayerShape, iconLayerPrecomp, iconLayerCamera, iconLayerLight, iconLayerGroup;
  QIcon iconLayerNull, iconLayerAdjustment, iconLayerParticle, iconLayerClone, iconLayerModel3D;
  QIcon iconLayerConstruction;
  bool shyHidden = false;
  QString filterText;
  SearchMatchMode searchMatchMode = SearchMatchMode::AllVisible;
  TimelineLayerDisplayMode displayMode = TimelineLayerDisplayMode::AllLayers;
  int rowHeight = kLayerRowHeight;
  int propertyColumnWidth = kLayerColumnWidth * kLayerPropertyColumnCount;
  double verticalOffset = 0.0;
  int contentHeight = kLayerRowHeight;
  int hoveredLayerIndex = -1;
  LayerID selectedLayerId;
  LayerID selectionAnchorLayerId;
  LayerID selectedMaskLayerId;
  int selectedMaskIndex = -1;
  QVector<VisibleRow> visibleRows;
  QHash<QString, bool> expandedByLayerId;
  QHash<QString, bool> expandedByGroupKey;
  QPointer<QComboBox> inlineParentEditor;
  QPointer<QComboBox> inlineBlendEditor;
  QPointer<QLineEdit> inlineNameEditor;
  LayerID editingLayerId;
  bool layerNameEditable = true;
  QHash<int, LayerID> layerBookmarks;
  int columnWidths_[kLayerPropertyColumnCount];
  bool columnVisible_[kLayerPropertyColumnCount] = {true, true, true, true, true, true};
  int dragCol_ = -1;
  int dragStartX_ = 0;
  QVector<int> dragStartWidths_;
  QPoint dragStartPos;
  LayerID dragCandidateLayerId;
  LayerID draggedLayerId;
  int dragInsertVisibleRow = -1;
  int dragMatteHoverVisibleRow = -1;
  bool dragMatteLinkMode = false;
  LayerID pickWhipSourceLayerId_;
  bool pickWhipDragging_ = false;
  int pickWhipHoverRow_ = -1;
  MultiEditCycleMode multiEditMode = MultiEditCycleMode::None;
  int multiEditPresetIndex = 0;
  std::chrono::steady_clock::time_point multiEditStartedAt{};
  bool dragStarted_ = false;
  QSet<int> blendModeFavorites;
  bool updatingLayout = false;  // 再帰呼び出し防止フラグ
  QTimer* layoutDebounceTimer = nullptr;
  int lastContentHeight = -1;
  // E: Lock click flash
  mutable QElapsedTimer lockFlashTimer_;
  int lockFlashRowY_ = -1;
  // F: Incremental search
  QString incrementalSearchBuffer_;
  QTimer* incrementalSearchTimer_ = nullptr;
  // H: Mask filter toggle
  bool maskFilterEnabled_ = false;
  // Q: Undo/redo highlight flash
  mutable QElapsedTimer undoFlashTimer_;
  QVector<LayerID> undoFlashLayerIds_;
  // R: プロパティ値も含めた検索
  bool searchInProperties_ = false;
  // V: テンプレート保存
  QJsonObject layerTemplate_;
  // EventBus 購読リスト
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  
  void clearInlineEditors()
  {
   auto* parentEditor = inlineParentEditor.data();
   auto* blendEditor = inlineBlendEditor.data();
   auto* nameEditor = inlineNameEditor.data();

   inlineParentEditor = nullptr;
   inlineBlendEditor = nullptr;
   inlineNameEditor = nullptr;

   if (parentEditor) {
    parentEditor->hide();
    parentEditor->deleteLater();
   }

   if (blendEditor) {
    blendEditor->hide();
    blendEditor->deleteLater();
   }

   if (nameEditor) {
    nameEditor->hide();
    nameEditor->deleteLater();
   }

   editingLayerId = LayerID();
  }

  const QIcon& iconForLayerKind(TimelineLayerIconKind kind) const
  {
    switch (kind) {
    case TimelineLayerIconKind::Solid: return iconLayerSolid;
    case TimelineLayerIconKind::Image: return iconLayerImage;
    case TimelineLayerIconKind::Svg: return iconLayerSvg;
    case TimelineLayerIconKind::Video: return iconLayerVideo;
    case TimelineLayerIconKind::Audio: return iconLayerAudio;
    case TimelineLayerIconKind::Text: return iconLayerText;
    case TimelineLayerIconKind::Shape: return iconLayerShape;
    case TimelineLayerIconKind::Precomp: return iconLayerPrecomp;
    case TimelineLayerIconKind::Camera: return iconLayerCamera;
    case TimelineLayerIconKind::Light: return iconLayerLight;
    case TimelineLayerIconKind::Group: return iconLayerGroup;
    case TimelineLayerIconKind::Null: return iconLayerNull;
    case TimelineLayerIconKind::Adjustment: return iconLayerAdjustment;
    case TimelineLayerIconKind::Particle: return iconLayerParticle;
    case TimelineLayerIconKind::Clone: return iconLayerClone;
    case TimelineLayerIconKind::Model3D: return iconLayerModel3D;
    case TimelineLayerIconKind::Construction: return iconLayerConstruction;
    case TimelineLayerIconKind::Generic:
    default: return iconLayerGeneric;
    }
  }

  void clearMaskSelection()
  {
   selectedMaskLayerId = LayerID();
   selectedMaskIndex = -1;
  }

  void focusMaskSelection(const ArtifactAbstractLayerPtr& layer, const int maskIndex)
  {
   if (!layer) {
    clearMaskSelection();
    currentPropertyPath.clear();
    return;
   }

   if (maskIndex < 0 || maskIndex >= layer->maskCount()) {
    clearMaskSelection();
    currentPropertyPath.clear();
    return;
   }

   selectedLayerId = layer->id();
   selectedMaskLayerId = layer->id();
   selectedMaskIndex = maskIndex;
   currentPropertyPath = maskSelectionPropertyPath(maskIndex);
  }

  int maxVerticalOffset(const ArtifactLayerPanelWidget* owner) const
  {
   if (!owner) {
    return 0;
   }
   return std::max(0, contentHeight - owner->height());
  }

  int rowIndexFromViewportY(const int viewportY) const
  {
   if (rowHeight <= 0) {
    return -1;
   }
   return static_cast<int>(std::floor((static_cast<double>(viewportY) + verticalOffset) /
                                      static_cast<double>(rowHeight)));
  }

  int rowViewportY(const int rowIndex) const
  {
   return static_cast<int>(std::floor(static_cast<double>(rowIndex * rowHeight) - verticalOffset));
  }

  QRect rowViewportRect(const int rowIndex, const int height, const int width) const
  {
   return QRect(0, rowViewportY(rowIndex), width, height);
  }

  void setVerticalOffset(const double offset, ArtifactLayerPanelWidget* owner, const bool emitSignal = true)
  {
   const double clamped = std::max(0.0, std::min(offset, static_cast<double>(maxVerticalOffset(owner))));
   if (std::abs(verticalOffset - clamped) < 0.0001) {
    return;
   }
   verticalOffset = clamped;
   if (owner) {
    Q_EMIT owner->verticalOffsetChanged(verticalOffset);
    owner->update();
   }
   if (emitSignal) {
    ArtifactCore::globalEventBus().publish<TimelineVerticalScrollEvent>({verticalOffset, "ArtifactLayerPanelWidget"});
   }
  }

  void setLayerNameEditable(bool enabled, ArtifactLayerPanelWidget* owner)
  {
   if (layerNameEditable == enabled) {
    return;
   }
   layerNameEditable = enabled;
   if (!layerNameEditable) {
    clearInlineEditors();
    if (owner) {
     owner->update();
    }
   }
  }

  void clearDragState()
  {
   dragCandidateLayerId = LayerID();
   draggedLayerId = LayerID();
   dragInsertVisibleRow = -1;
   dragMatteHoverVisibleRow = -1;
   dragMatteLinkMode = false;
   dragStarted_ = false;
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
    const QString needle = filterText.trimmed();
   for (auto& l : comp->allLayer()) {
     if (!l) continue;
     if (shyHidden && l->isShy()) continue;
     if (maskFilterEnabled_ && !l->hasMasks()) continue;
     if (!needle.isEmpty()) {
       bool nameMatch = l->layerName().contains(needle, Qt::CaseInsensitive);
       bool propMatch = false;
       if (searchInProperties_ && !nameMatch) {
         const auto groups = l->getLayerPropertyGroups();
         for (const auto& group : groups) {
           if (ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(
                   group.name())) {
            continue;
           }
           if (group.name().contains(needle, Qt::CaseInsensitive)) {
             propMatch = true;
             break;
           }
         }
       }
       if (!nameMatch && !propMatch) continue;
     }
     layers.push_back(l);
    }
   std::reverse(layers.begin(), layers.end());
   if (layers.isEmpty()) {
    return;
   }

   QSet<QString> emitted;
   std::function<void(const ArtifactAbstractLayerPtr&, int, QSet<QString>&)> appendNode =
    [&](const ArtifactAbstractLayerPtr& node, int depth, QSet<QString>& stack) {
     if (!node) return;
     const QString nodeId = node->id().toString();
     if (stack.contains(nodeId)) return; // cycle guard
     if (emitted.contains(nodeId)) return;

   const auto panelGroups = layerPanelPropertyGroups(node);
   const auto matteRefs = node->matteReferences();
   const bool hasMaskStack = node->hasMasks();
   const bool hasMatteStack = !matteRefs.empty();
   const bool hasChildren = !panelGroups.empty() || hasMaskStack || hasMatteStack;
   const bool expanded = expandedByLayerId.value(nodeId, true);
   if (!layerMatchesDisplayMode(node, displayMode, selectedLayerId)) {
    return;
   }
   const auto presentation = describeLayerPresentation(node);
   visibleRows.push_back(VisibleRow{
    node,
    depth,
    hasChildren,
    expanded,
    RowKind::Layer,
    QString(),
    QString(),
    QString(),
    node->isGroupLayer() ? groupLayerSummaryText(node)
                         : presentation.timelineBadgeText,
    presentation.badgeTone,
    summarizeLayerState(node),
    summarizeLayerStateTone(node)
   });
   emitted.insert(nodeId);

   if (!hasChildren || !expanded) return;

     for (const auto& groupDef : panelGroups) {
     const QString groupName = groupDef.name().trimmed().isEmpty()
                                   ? QStringLiteral("Layer")
                                   : groupDef.name().trimmed();
     const QString groupKey = nodeId + QStringLiteral("::") + groupName.toLower();
     const bool groupExpanded =
         expandedByGroupKey.value(groupKey, timelineGroupExpandedByDefault(groupName));
      const bool hasVisibleProperties = groupHasVisibleProperties(groupDef, displayMode);
      if (!hasVisibleProperties) {
       continue;
      }
      visibleRows.push_back(VisibleRow{
       node,
       depth + 1,
       true,
       groupExpanded,
      RowKind::Group,
      groupName,
      QString(),
      groupKey,
       QStringLiteral("Grp"),
       LayerPresentationBadgeTone::Container
      });

      if (groupExpanded) {
       for (const auto& property : groupDef.sortedProperties()) {
        if (!property) {
         continue;
        }
        if (!propertyMatchesDisplayMode(property, displayMode)) {
         continue;
        }
        visibleRows.push_back(VisibleRow{
         node,
         depth + 2,
         false,
         false,
         RowKind::Property,
         compactPropertyRowLabel(property->getName()),
         property->getName(),
         QString(),
         QStringLiteral("Prp"),
         LayerPresentationBadgeTone::Neutral
        });
      }
      }
    }

    if (hasMaskStack) {
      const QString maskGroupKey = nodeId + QStringLiteral("::masks");
      const bool maskExpanded = expandedByGroupKey.value(maskGroupKey, false);
      visibleRows.push_back(VisibleRow{
       node,
       depth + 1,
       node->maskCount() > 0,
       maskExpanded,
       RowKind::MaskStack,
       QStringLiteral("Masks"),
       QString(),
       maskGroupKey,
       QStringLiteral("Msk"),
       LayerPresentationBadgeTone::Special
      });
      if (maskExpanded) {
       for (int maskIndex = 0; maskIndex < node->maskCount(); ++maskIndex) {
        const LayerMask mask = node->mask(maskIndex);
        visibleRows.push_back(VisibleRow{
         node,
         depth + 2,
         false,
         false,
         RowKind::Mask,
         maskSummaryLabel(mask, maskIndex),
         QString::number(maskIndex),
         QString(),
         QStringLiteral("Msk"),
         LayerPresentationBadgeTone::Special
        });
       }
      }
    }

    if (hasMatteStack) {
      const QString matteGroupKey = nodeId + QStringLiteral("::mattes");
      const bool matteExpanded = expandedByGroupKey.value(matteGroupKey, false);
      visibleRows.push_back(VisibleRow{
       node,
       depth + 1,
       !matteRefs.empty(),
       matteExpanded,
       RowKind::MatteStack,
       QStringLiteral("Track Mattes"),
       QString(),
       matteGroupKey,
       QStringLiteral("Mat"),
       LayerPresentationBadgeTone::Special
      });
      if (matteExpanded) {
       for (size_t matteIndex = 0; matteIndex < matteRefs.size(); ++matteIndex) {
        const LayerMatteReference& ref = matteRefs[matteIndex];
        visibleRows.push_back(VisibleRow{
         node,
         depth + 2,
         false,
         false,
         RowKind::Matte,
         matteSummaryLabel(ref, static_cast<int>(matteIndex)),
         QString::number(static_cast<int>(matteIndex)),
         QString(),
         QStringLiteral("Mat"),
         LayerPresentationBadgeTone::Special
        });
       }
      }
    }

   };

   // The composition vector renders bottom-to-top. `layers` is its reverse,
   // so preserve this exact top-to-bottom order in the timeline. Parenting is
   // transform metadata and must not regroup the compositing stack.
   for (const auto& layer : layers) {
    QSet<QString> stack;
    appendNode(layer, 0, stack);
   }
  }
 };

 W_OBJECT_IMPL(ArtifactLayerPanelWidget)

ArtifactLayerPanelWidget::ArtifactLayerPanelWidget(QWidget* parent)
 : QWidget(parent), impl_(new Impl())
{
  impl_->keyframeModel = new ArtifactTimelineKeyframeModel(this);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMouseTracking(true);
  setAcceptDrops(true);
  setFocusPolicy(Qt::StrongFocus);
  for (int i = 0; i < kLayerPropertyColumnCount; ++i) {
    impl_->columnWidths_[i] = kLayerColumnWidth;
  }

  impl_->incrementalSearchTimer_ = new QTimer(this);
  impl_->incrementalSearchTimer_->setSingleShot(true);
  impl_->incrementalSearchTimer_->setInterval(1500);
  QObject::connect(impl_->incrementalSearchTimer_, &QTimer::timeout, this, [this]() {
    impl_->incrementalSearchBuffer_.clear();
  });

 QObject::connect(UndoManager::instance(), &UndoManager::historyChanged, this, [this]() {
  impl_->undoFlashTimer_.start();
  impl_->undoFlashLayerIds_ = selectedLayerIdsSnapshot();
  updateLayout();
 });

 impl_->eventBusSubscriptions_.push_back(
  impl_->eventBus_.subscribe<FrameChangedEvent>([this](const FrameChangedEvent& event) {
    if (!impl_->compositionId.isNil() &&
        event.compositionId != impl_->compositionId.toString()) {
      return;
    }
    if (auto* playback = ArtifactPlaybackService::instance()) {
      const auto fps = playback->frameRate().framerate();
      impl_->currentTime = RationalTime(event.frame, fps);
      update();
    }
  }));

 impl_->eventBusSubscriptions_.push_back(
  impl_->eventBus_.subscribe<LayerChangedEvent>([this](const LayerChangedEvent& event) {
      const QString compositionId = event.compositionId;
      const LayerID layerId(event.layerId);
      const auto changeType = event.changeType;
      const bool allowInteractiveCreatedHandling =
          QCoreApplication::instance() &&
          QThread::currentThread() == QCoreApplication::instance()->thread();
      QMetaObject::invokeMethod(
          this,
          [this, compositionId, layerId, changeType,
           allowInteractiveCreatedHandling]() {
            if (!impl_) {
              return;
            }
            const bool targetsPanel =
                impl_->compositionId.isNil() ||
                compositionId == impl_->compositionId.toString();
            if (!targetsPanel) {
              return;
            }

            updateLayout();
            if (changeType != LayerChangedEvent::ChangeType::Created ||
                !allowInteractiveCreatedHandling) {
              return;
            }

            const auto widgets = QApplication::allWidgets();
            for (QWidget* w : widgets) {
              if (!w) continue;
              const QString className =
                  QString::fromLatin1(w->metaObject()->className());
              if (className.contains("ArtifactInspectorWidget",
                                     Qt::CaseInsensitive)) {
                w->show();
                w->raise();
                w->activateWindow();
                break;
              }
            }
            if (impl_->layerNameEditable) {
              editLayerName(layerId);
            }
            this->visibleRowsChanged();
          },
          Qt::QueuedConnection);
    }));
  impl_->eventBusSubscriptions_.push_back(
    impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
        [this](const LayerSelectionChangedEvent& event) {
          const LayerID layerId(event.layerId);
          // Guard: ignore spurious nil events when we already have a selection.
          // This prevents property-edit notifications from clearing the timeline
          // selection via the EventBus path.
          if (layerId.isNil() && !impl_->selectedLayerId.isNil()) {
            auto *app = ArtifactApplicationManager::instance();
            auto *sel = app ? app->layerSelectionManager() : nullptr;
            if (sel && sel->currentLayer()) {
              return;
            }
          }
          if (impl_->selectedLayerId != layerId) {
            const bool layerActuallyChanged = impl_->selectedLayerId != layerId;
            impl_->selectedLayerId = layerId;
            impl_->selectionAnchorLayerId = layerId.isNil() ? LayerID() : layerId;
            if (layerActuallyChanged) {
              impl_->currentPropertyPath.clear();
              this->propertyFocusChanged(impl_->selectedLayerId,
                                         impl_->currentPropertyPath);
            }
            update();
          }
        }));
  impl_->eventBusSubscriptions_.push_back(
    impl_->eventBus_.subscribe<CompositionCreatedEvent>(
        [this](const CompositionCreatedEvent& event) {
          if (impl_->compositionId.isNil()) {
            impl_->compositionId = CompositionID(event.compositionId);
          }
          updateLayout();
        }));
  impl_->eventBusSubscriptions_.push_back(
    impl_->eventBus_.subscribe<ProjectChangedEvent>(
        [this](const ProjectChangedEvent&) {
          updateLayout();
        }));
 }

ArtifactLayerPanelWidget::~ArtifactLayerPanelWidget()
{
  delete impl_;
}

void ArtifactLayerPanelWidget::setComposition(const CompositionID& id)
{
  impl_->compositionId = id;
  impl_->selectedLayerId = LayerID();
  impl_->selectionAnchorLayerId = LayerID();
  impl_->currentPropertyPath.clear();
  propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
  updateLayout();
}

void ArtifactLayerPanelWidget::setShyHidden(bool hidden)
{
  impl_->shyHidden = hidden;
  updateLayout();
}

void ArtifactLayerPanelWidget::setFilterText(const QString& text)
{
  if (impl_->filterText == text) {
    return;
  }
  impl_->filterText = text;
  updateLayout();
}

void ArtifactLayerPanelWidget::setSearchMatchMode(SearchMatchMode mode)
{
  if (impl_->searchMatchMode == mode) {
    return;
  }
  impl_->searchMatchMode = mode;
  updateLayout();
}

SearchMatchMode ArtifactLayerPanelWidget::searchMatchMode() const
{
  return impl_->searchMatchMode;
}

void ArtifactLayerPanelWidget::setDisplayMode(TimelineLayerDisplayMode mode)
{
  if (impl_->displayMode == mode) {
    return;
  }
  impl_->displayMode = mode;
  updateLayout();
}

TimelineLayerDisplayMode ArtifactLayerPanelWidget::displayMode() const
{
  return impl_->displayMode;
}

void ArtifactLayerPanelWidget::setRowHeight(int rowHeight)
{
  if (rowHeight <= 0 || impl_->rowHeight == rowHeight) {
    return;
  }
  impl_->rowHeight = rowHeight;
  updateLayout();
}

int ArtifactLayerPanelWidget::rowHeight() const
{
  return impl_->rowHeight;
}

void ArtifactLayerPanelWidget::setPropertyColumnWidth(int width)
{
  if (width <= 0 || impl_->propertyColumnWidth == width) {
    return;
  }
  impl_->propertyColumnWidth = width;
  updateLayout();
}

int ArtifactLayerPanelWidget::propertyColumnWidth() const
{
  return impl_->propertyColumnWidth;
}

void ArtifactLayerPanelWidget::setVerticalOffset(const double offset)
{
  impl_->setVerticalOffset(offset, this);
}

double ArtifactLayerPanelWidget::verticalOffset() const
{
  return impl_->verticalOffset;
}

void ArtifactLayerPanelWidget::updateLayout()
{
  impl_->clearDragState();
  unsetCursor();
  if (!impl_->layoutDebounceTimer) {
    performUpdateLayout();
    return;
  }
  impl_->layoutDebounceTimer->start();
}

void ArtifactLayerPanelWidget::performUpdateLayout()
{
  // 再帰呼び出しを防止
  if (impl_->updatingLayout) return;
  impl_->updatingLayout = true;

  const auto rowsEqual = [](const QVector<VisibleRow>& lhs,
                            const QVector<VisibleRow>& rhs) -> bool {
    if (lhs.size() != rhs.size()) {
      return false;
    }
    for (int i = 0; i < lhs.size(); ++i) {
      const auto& a = lhs[i];
      const auto& b = rhs[i];
      const QString aId = a.layer ? a.layer->id().toString() : QString();
      const QString bId = b.layer ? b.layer->id().toString() : QString();
      if (aId != bId ||
          a.depth != b.depth ||
          a.hasChildren != b.hasChildren ||
          a.expanded != b.expanded ||
          a.kind != b.kind ||
          a.label != b.label ||
          a.propertyPath != b.propertyPath ||
          a.groupKey != b.groupKey ||
          a.auxiliaryText != b.auxiliaryText ||
          a.auxiliaryTone != b.auxiliaryTone) {
        return false;
      }
    }
    return true;
  };

  const QVector<VisibleRow> oldRows = impl_->visibleRows;
  
  impl_->clearInlineEditors();
  impl_->rebuildVisibleRows();
  const bool structureChanged = !rowsEqual(oldRows, impl_->visibleRows);
  const int count = impl_->visibleRows.size();
  const int contentHeight = std::max(kLayerRowHeight, count * kLayerRowHeight);
  impl_->contentHeight = contentHeight;
  if (contentHeight != impl_->lastContentHeight) {
    updateGeometry();
    impl_->lastContentHeight = contentHeight;
  }

  if (!impl_->selectedMaskLayerId.isNil() &&
      impl_->selectedMaskLayerId == impl_->selectedLayerId) {
    auto currentComp = safeCompositionLookup(impl_->compositionId);
    auto layer = currentComp ? currentComp->layerById(impl_->selectedMaskLayerId)
                      : ArtifactAbstractLayerPtr{};
    if (!layer || layer->maskCount() <= 0) {
      if (impl_->selectedMaskIndex >= 0 ||
          !impl_->currentPropertyPath.isEmpty()) {
        impl_->clearMaskSelection();
        impl_->currentPropertyPath.clear();
        propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      }
    } else if (impl_->selectedMaskIndex >= layer->maskCount()) {
      const int clampedIndex = layer->maskCount() - 1;
      impl_->focusMaskSelection(layer, clampedIndex);
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
    }
  }

  impl_->setVerticalOffset(impl_->verticalOffset, this);
  update();
  if (structureChanged) {
    ArtifactCore::globalEventBus().publish<TimelineVisibleRowsChangedEvent>({});
    visibleRowsChanged();
  }
  
  impl_->updatingLayout = false;
 }

QVector<LayerID> ArtifactLayerPanelWidget::visibleTimelineRows() const
{
  QVector<LayerID> rows;
  const auto descriptors = visibleTimelineRowDescriptors();
  rows.reserve(descriptors.size());
  for (const auto& row : descriptors) {
   if (row.kind == TimelineRowKind::Layer && !row.layerId.isNil()) {
    rows.append(row.layerId);
   } else {
    rows.append(LayerID::Nil());
   }
  }
  return rows;
}

QVector<TimelineRowDescriptor>
ArtifactLayerPanelWidget::visibleTimelineRowDescriptors() const
{
  QVector<TimelineRowDescriptor> rows;
  rows.reserve(impl_->visibleRows.size());
  for (const auto& row : impl_->visibleRows) {
   TimelineRowDescriptor descriptor;
   descriptor.layerId = row.layer ? row.layer->id() : LayerID::Nil();
   descriptor.kind = row.kind;
   descriptor.label = row.label;
   descriptor.propertyPath = row.propertyPath;
   descriptor.auxiliaryText = row.auxiliaryText;
   descriptor.auxiliaryTone = row.auxiliaryTone;
   descriptor.stateText = row.stateText;
   descriptor.stateTone = row.stateTone;
   if (descriptor.auxiliaryText.isEmpty() &&
       (row.kind == RowKind::Mask || row.kind == RowKind::Matte)) {
    descriptor.auxiliaryText = row.label;
   } else if (descriptor.auxiliaryText.isEmpty() &&
              (row.kind == RowKind::MaskStack || row.kind == RowKind::MatteStack)) {
    descriptor.auxiliaryText = row.label;
   }
   rows.push_back(std::move(descriptor));
  }
  return rows;
}

QVector<LayerID> ArtifactLayerPanelWidget::matchingTimelineRows() const
{
  QVector<LayerID> rows;
  rows.reserve(impl_->visibleRows.size());
  for (const auto& row : impl_->visibleRows) {
    if (row.kind == RowKind::Layer && row.layer) {
      rows.append(row.layer->id());
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
  if (!impl_->layerNameEditable) {
   return;
  }
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
    applyLayerPanelLineEditPalette(impl_->inlineNameEditor);

    // Position it
    const int rowIndent = impl_->visibleRows[idx].depth * 14;
    const int editNameStartX = [this]() {
      int x = 0;
      for (int i = 0; i < kLayerPropertyColumnCount; ++i) if (impl_->columnVisible_[i]) x += impl_->columnWidths_[i];
      return x;
    }();
    const int layerIconAdvance = kLayerTypeIconSize + kLayerTypeIconGap;
    const int textX = editNameStartX + rowIndent +
                      (impl_->visibleRows[idx].hasChildren ? 16 : 4) +
                      layerIconAdvance;
    const int editorWidth = std::max(60, width() - textX - kInlineParentWidth - kInlineBlendWidth - 8);
    impl_->inlineNameEditor->setGeometry(textX, impl_->rowViewportY(idx) + 2, editorWidth, kLayerRowHeight - 4);

    QObject::connect(impl_->inlineNameEditor, &QLineEdit::editingFinished, this, [this, l, id]() {
     if (!impl_->inlineNameEditor) return;
     QString newName = impl_->inlineNameEditor->text();
     impl_->inlineNameEditor->deleteLater();
     impl_->inlineNameEditor = nullptr;
     if (newName != l->layerName()) {
      const QString oldName = l->layerName();
      auto* cmd = new RenameLayerCommand(l, oldName, newName);
      UndoManager::instance()->push(std::unique_ptr<RenameLayerCommand>(cmd));
     }
     setFocus();
    });

    impl_->inlineNameEditor->show();
    impl_->inlineNameEditor->setFocus();
   }
  }
 }

void ArtifactLayerPanelWidget::scrollToLayer(const LayerID& id)
{
  const int idx = layerRowIndex(id);
  if (idx < 0) {
    return;
  }
  impl_->selectedLayerId = id;
  const int rowTop = idx * impl_->rowHeight;
  const int rowBottom = rowTop + impl_->rowHeight;
  const int visibleTop = static_cast<int>(std::floor(impl_->verticalOffset));
  const int visibleBottom = visibleTop + std::max(1, height());
  const int comfort = std::max(impl_->rowHeight, height() / 8);
  if (rowTop >= visibleTop + comfort &&
      rowBottom <= visibleBottom - comfort) {
    update();
    return;
  }
  const int desiredTop = std::max(0, rowTop - (height() / 3));
  impl_->setVerticalOffset(static_cast<double>(desiredTop), this);
  update();
}

LayerID ArtifactLayerPanelWidget::selectedLayerId() const {
  return impl_ ? impl_->selectedLayerId : LayerID{};
}

QString ArtifactLayerPanelWidget::currentPropertyPath() const {
  return impl_ ? impl_->currentPropertyPath : QString{};
}

 void ArtifactLayerPanelWidget::setLayerNameEditable(bool enabled)
 {
  impl_->setLayerNameEditable(enabled, this);
 }

 bool ArtifactLayerPanelWidget::isLayerNameEditable() const
 {
  return impl_->layerNameEditable;
 }


void ArtifactLayerPanelWidget::mousePressEvent(QMouseEvent* event)
{
  setFocus();
  const int rowH = kLayerRowHeight;
  int idx = impl_->rowIndexFromViewportY(event->pos().y());
  int clickX = event->pos().x();

  if (idx < 0 || idx >= impl_->visibleRows.size()) {
    impl_->clearDragState();
    return;
  }
  const auto row = impl_->visibleRows[idx];
  auto layer = row.layer;
  if (!layer) {
    impl_->clearDragState();
    return;
  }
  if (row.kind == RowKind::Group ||
      row.kind == RowKind::MaskStack ||
      row.kind == RowKind::MatteStack) {
   impl_->clearMaskSelection();
   if (event->button() == Qt::LeftButton) {
    if (!row.groupKey.trimmed().isEmpty()) {
      impl_->expandedByGroupKey[row.groupKey] = !impl_->expandedByGroupKey.value(row.groupKey, true);
      updateLayout();
    }
   }
   event->accept();
   return;
  }
  if (row.kind == RowKind::Property) {
   impl_->clearMaskSelection();
   if (event->button() == Qt::LeftButton) {
    RationalTime currentTime = impl_->currentTime;
    if (auto comp = safeCompositionLookup(impl_->compositionId)) {
     const auto fps = std::max(1.0, static_cast<double>(comp->frameRate().framerate()));
     currentTime = RationalTime(comp->framePosition().framePosition(), fps);
    }
    const QRect keyframeRect =
        propertyKeyframeMarkerRect(width(), impl_->rowViewportY(idx), rowH);
    if (keyframeRect.contains(event->pos())) {
     if (auto comp = safeCompositionLookup(impl_->compositionId)) {
      const QString propertyPath = row.propertyPath.trimmed();
      togglePropertyKeyframeAtCurrentTime(comp, layer, propertyPath, currentTime);
      auto* service = ArtifactProjectService::instance();
      if (service) {
       service->selectLayer(layer->id());
      }
      impl_->selectedLayerId = layer->id();
      impl_->currentPropertyPath = propertyPath;
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      update();
     }
     event->accept();
     return;
    }
    auto* service = ArtifactProjectService::instance();
    if (service) {
     service->selectLayer(layer->id());
    }
    impl_->selectedLayerId = layer->id();
    impl_->currentPropertyPath = row.propertyPath.trimmed();
    propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
    update();
   }
   event->accept();
   return;
  }
  if (row.kind == RowKind::Mask) {
    impl_->clearDragState();
    if (event->button() == Qt::LeftButton) {
      auto* service = ArtifactProjectService::instance();
      if (service) {
        service->selectLayer(layer->id());
      }
      impl_->selectedLayerId = layer->id();
      impl_->selectedMaskLayerId = layer->id();
      impl_->selectedMaskIndex = row.propertyPath.trimmed().toInt();
      impl_->currentPropertyPath = maskSelectionPropertyPath(impl_->selectedMaskIndex);
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      update();
    }
    event->accept();
    return;
  }
  if (row.kind == RowKind::Matte) {
    impl_->clearDragState();
    impl_->clearMaskSelection();
    if (event->button() == Qt::LeftButton) {
      auto* service = ArtifactProjectService::instance();
      if (service) {
        service->selectLayer(layer->id());
      }
      impl_->selectedLayerId = layer->id();
      impl_->currentPropertyPath.clear();
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      update();
    } else if (event->button() == Qt::RightButton) {
      auto comp = safeCompositionLookup(impl_->compositionId);
      if (comp && layer) {
        bool ok = false;
        const int matteIndex = row.propertyPath.trimmed().toInt(&ok);
        if (ok && matteIndex >= 0) {
          const auto matteRefs = layer->matteReferences();
          if (matteIndex < static_cast<int>(matteRefs.size())) {
            QMenu menu(this);
            QAction *focusAction = menu.addAction(tt("layer_panel.focus_source", "Focus source"));
            focusAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("matte_focus")},
                                             {QStringLiteral("index"), matteIndex}});

            QMenu *typeMenu = menu.addMenu(tt("layer_panel.set_matte_type", "Set matte type"));
            const QStringList typeLabels = {
                tt("layer_panel.alpha", "Alpha"),
                tt("layer_panel.luma", "Luma"),
                tt("layer_panel.inverted_alpha", "Inverted Alpha"),
                tt("layer_panel.inverted_luma", "Inverted Luma")};
            for (int typeIndex = 0; typeIndex < typeLabels.size(); ++typeIndex) {
              QAction *typeAction = typeMenu->addAction(typeLabels[typeIndex]);
              typeAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("matte_type")},
                                              {QStringLiteral("index"), matteIndex},
                                              {QStringLiteral("type"), typeIndex}});
            }

            if (QAction *chosenAction = menu.exec(event->globalPos())) {
              const QVariantMap data = chosenAction->data().toMap();
              const QString kind = data.value(QStringLiteral("kind")).toString();
              bool indexOk = false;
              const int index = data.value(QStringLiteral("index")).toInt(&indexOk);
              if (!indexOk) {
                return;
              }
              if (kind == QStringLiteral("matte_focus") && index >= 0 && index < static_cast<int>(matteRefs.size())) {
                const auto &chosenRef = matteRefs[index];
                if (!chosenRef.sourceLayerId.isNil()) {
                  if (auto *service = ArtifactProjectService::instance()) {
                    service->selectLayer(chosenRef.sourceLayerId);
                  }
                }
              } else if (kind == QStringLiteral("matte_type")) {
                bool typeOk = false;
                const int typeIndex = data.value(QStringLiteral("type")).toInt(&typeOk);
                if (!typeOk) {
                  return;
                }
                if (typeIndex >= 0 && typeIndex <= static_cast<int>(MatteType::InverseLuma)) {
                  if (applyMatteTypeToLayer(comp, layer, matteIndex,
                                            static_cast<MatteType>(typeIndex))) {
                    updateLayout();
                  }
                }
              }
            }
          }
        }
      }
    }
    event->accept();
    return;
  }
  auto* service = ArtifactProjectService::instance();
  const bool contextMenuRequest =
      (event->button() == Qt::RightButton) ||
      (event->button() == Qt::LeftButton &&
       (event->modifiers() & Qt::ControlModifier) &&
       !(event->modifiers() & (Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier)));
  if (row.kind != RowKind::Layer) {
    impl_->clearDragState();
    impl_->clearMaskSelection();
    if (event->button() == Qt::LeftButton) {
      if (service) {
       service->selectLayer(layer->id());
      }
      impl_->selectedLayerId = layer->id();
      impl_->currentPropertyPath.clear();
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      update();
    }
    event->accept();
    return;
  }
  
  //名前エリアまたはスイッチ列でドラッグを開始可能にするための準備
  if (event->button() == Qt::LeftButton && !contextMenuRequest) {
    impl_->dragStartPos = event->pos();
    impl_->dragCandidateLayerId = LayerID();
  } else {
    impl_->clearDragState();
  }

  const int y = impl_->rowViewportY(idx);
  const int mouseNameStartX = [this]() {
    int x = 0;
    for (int i = 0; i < kLayerPropertyColumnCount; ++i) if (impl_->columnVisible_[i]) x += impl_->columnWidths_[i];
    return x;
  }();
  const int nameX = mouseNameStartX + row.depth * 14;
  const bool showInlineCombos =
      row.kind == RowKind::Layer &&
      (width() - (nameX + 8)) >= (kInlineComboReserve + kLayerNameMinWidth);
  const int parentRectX = width() - kInlineComboReserve;
  const QRect parentRect(parentRectX, y + kInlineComboMarginY, kInlineParentWidth, kInlineComboHeight);
  const QRect blendRect(parentRect.right() + kInlineComboGap, y + kInlineComboMarginY, kInlineBlendWidth, kInlineComboHeight);
  const bool clickInInlineCombo = showInlineCombos &&
                                  (parentRect.contains(event->pos()) || blendRect.contains(event->pos()));

  if (event->button() == Qt::LeftButton) {
    auto* selectionManager = currentLayerSelectionManager();
    auto comp = safeCompositionLookup(impl_->compositionId);
    const bool rangeSelection = (event->modifiers() & Qt::ShiftModifier) &&
                                !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    const bool toggleSelection = (event->modifiers() & Qt::ControlModifier) &&
                                 !(event->modifiers() & (Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier));
    if (rangeSelection) {
      if (selectionManager) {
        selectionManager->setActiveComposition(comp);
        LayerID anchor = impl_->selectionAnchorLayerId;
        if (anchor.isNil()) {
          anchor = impl_->selectedLayerId.isNil() ? layer->id() : impl_->selectedLayerId;
        }
        const auto ids = visibleLayerIdsInRange(impl_->visibleRows, anchor, layer->id());
        if (!ids.isEmpty()) {
          selectionManager->clearSelection();
          for (const auto& id : ids) {
            if (comp) {
              selectionManager->addToSelection(comp->layerById(id));
            }
          }
          impl_->selectedLayerId = layer->id();
          impl_->selectionAnchorLayerId = anchor;
        } else {
          impl_->selectedLayerId = layer->id();
          impl_->selectionAnchorLayerId = layer->id();
          selectionManager->selectLayer(comp ? comp->layerById(layer->id()) : ArtifactAbstractLayerPtr{});
        }
      } else {
        impl_->selectedLayerId = layer->id();
        impl_->selectionAnchorLayerId = layer->id();
      }
      impl_->clearMaskSelection();
      impl_->currentPropertyPath.clear();
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      if (!clickInInlineCombo) {
        impl_->clearInlineEditors();
      }
      update();
      event->accept();
      return;
    }
    if (toggleSelection) {
      if (selectionManager) {
        selectionManager->setActiveComposition(comp);
        const auto selectedLayer = comp ? comp->layerById(layer->id()) : ArtifactAbstractLayerPtr{};
        if (selectionManager->isSelected(selectedLayer)) {
          selectionManager->removeFromSelection(selectedLayer);
        } else {
          selectionManager->addToSelection(selectedLayer);
        }
        const auto current = selectionManager->currentLayer();
        impl_->selectedLayerId = current ? current->id() : LayerID();
        impl_->selectionAnchorLayerId = current ? current->id() : LayerID();
      } else {
        impl_->selectedLayerId = layer->id();
        impl_->selectionAnchorLayerId = layer->id();
      }
      impl_->clearMaskSelection();
      impl_->currentPropertyPath.clear();
      propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
      if (!clickInInlineCombo) {
        impl_->clearInlineEditors();
      }
      update();
      event->accept();
      return;
    }

    impl_->selectedLayerId = layer->id();
    impl_->selectionAnchorLayerId = layer->id();
    impl_->clearMaskSelection();
    impl_->currentPropertyPath.clear();
    propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
    if (!clickInInlineCombo) {
      impl_->clearInlineEditors();
    }
    if (showInlineCombos && parentRect.contains(event->pos())) {
      impl_->clearInlineEditors();
      auto* combo = new QComboBox(this);
      combo->setGeometry(parentRect);
      applyLayerPanelComboPalette(combo);
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
      applyLayerPanelComboPalette(combo);
      const auto items = blendModeItems(impl_->blendModeFavorites);
      for (const auto& [name, mode] : items) {
        if (name.isEmpty()) {
          combo->insertSeparator(combo->count());
          continue;
        }
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
        UndoManager::instance()->push(std::make_unique<ChangeLayerBlendModeCommand>(layer, mode));
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
    // Pick Whip: 親レイヤードラッグ
    if (showInlineCombos && row.kind == RowKind::Layer && event->button() == Qt::LeftButton) {
      int whipCumX = 0;
      for (int ci = 0; ci < kLayerPropertyColumnCount; ++ci) {
        if (!impl_->columnVisible_[ci]) { whipCumX += impl_->columnWidths_[ci]; continue; }
        if (ci == 5) {
          const QRect whipColRect(whipCumX, impl_->rowViewportY(idx), impl_->columnWidths_[ci], kLayerRowHeight);
          if (whipColRect.contains(event->pos())) {
            impl_->pickWhipSourceLayerId_ = layer->id();
            impl_->pickWhipDragging_ = true;
            QDrag* drag = new QDrag(this);
            QMimeData* mime = new QMimeData();
            mime->setData(QStringLiteral("application/x-artifact-parent-link"), layer->id().toString().toUtf8());
            drag->setMimeData(mime);
            auto* pix = new QPixmap(impl_->parentWhipIcon);
            if (!pix->isNull()) drag->setPixmap(pix->scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            drag->exec(Qt::LinkAction);
            impl_->pickWhipDragging_ = false;
            event->accept();
            return;
          }
        }
        whipCumX += impl_->columnWidths_[ci];
      }
    }
    // M: 列の区切り線ドラッグ開始チェック（非表示列をスキップ）
    if (row.kind == RowKind::Layer && event->button() == Qt::LeftButton) {
      int cumX = 0;
      for (int ci = 0; ci < kLayerPropertyColumnCount - 1; ++ci) {
        cumX += impl_->columnWidths_[ci];
        if (!impl_->columnVisible_[ci]) continue;
        if (std::abs(clickX - cumX) <= kColumnDividerDragMargin) {
          impl_->dragCol_ = ci;
          impl_->dragStartX_ = clickX;
          impl_->dragStartWidths_.resize(kLayerPropertyColumnCount);
          for (int dci = 0; dci < kLayerPropertyColumnCount; ++dci) {
            impl_->dragStartWidths_[dci] = impl_->columnWidths_[dci];
          }
          event->accept();
          return;
        }
      }
    }
    bool handledLayerSwitch = false;
    {
      int cumX = 0;
      for (int ci = 0; ci < kLayerPropertyColumnCount; ++ci) {
        if (!impl_->columnVisible_[ci]) {
          cumX += impl_->columnWidths_[ci];
          continue;
        }
        const int nextCumX = cumX + impl_->columnWidths_[ci];
        if (clickX < nextCumX) {
          // E: Lock feedback - block toggles on locked layers (except lock column)
          if (layer && layer->isLocked() && ci != 1) {
            impl_->lockFlashTimer_.start();
            impl_->lockFlashRowY_ = impl_->rowViewportY(idx);
            update(0, impl_->lockFlashRowY_, width(), kLayerRowHeight);
            QToolTip::showText(event->globalPos(),
              QStringLiteral("このレイヤーはロックされています"), this);
            event->accept();
            return;
          }
          switch (ci) {
          case 0: if (layer) UndoManager::instance()->push(std::make_unique<SetLayerVisibilityCommand>(layer, !layer->isVisible())); handledLayerSwitch = true; break;
          case 1: if (layer) UndoManager::instance()->push(std::make_unique<SetLayerLockCommand>(layer, !layer->isLocked())); handledLayerSwitch = true; break;
          case 2: if (layer) UndoManager::instance()->push(std::make_unique<SetLayerSoloCommand>(layer, !layer->isSolo())); handledLayerSwitch = true; break;
          case 3: break; // Audio column - no toggle
          case 4: if (layer) UndoManager::instance()->push(std::make_unique<SetLayerShyCommand>(layer, !layer->isShy())); handledLayerSwitch = true; break;
          default: break;
          }
          break;
        }
        cumX = nextCumX;
      }
    }
    if (!handledLayerSwitch) {
      const int toggleSize = 10;
      const int toggleX = nameX + 2;
      const QRect toggleRect(toggleX, impl_->rowViewportY(idx) + (rowH - toggleSize) / 2, toggleSize, toggleSize);
      if (row.hasChildren && toggleRect.contains(event->pos())) {
        const QString idStr = layer->id().toString();
        impl_->expandedByLayerId[idStr] = !impl_->expandedByLayerId.value(idStr, true);
        updateLayout();
        event->accept();
        return;
      }
      
      const QRect chipRect = variantChipRect(QRect(0, impl_->rowViewportY(idx), width(), rowH),
                                             layer, showInlineCombos, rowH);
      if (chipRect.contains(event->pos())) {
          showVariantPickerMenu(this, layer, event->globalPos());
          event->accept();
          return;
      }
    }
    if (handledLayerSwitch) {
      if (service) {
        service->selectLayer(layer->id());
      }
      impl_->clearDragState();
      update();
      event->accept();
      return;
    }

    // K: Ctrl+Alt+Click → 選択レイヤーのみ表示（他を非表示）
    if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) == (Qt::ControlModifier | Qt::AltModifier) &&
        !(event->modifiers() & (Qt::ShiftModifier | Qt::MetaModifier)) && layer) {
      auto comp = safeCompositionLookup(impl_->compositionId);
      if (comp) {
        for (const auto& l : comp->allLayer()) {
          if (l) l->setVisible(l->id() == layer->id());
        }
        if (service) {
          service->selectLayer(layer->id());
        }
        impl_->selectedLayerId = layer->id();
        impl_->clearDragState();
        updateLayout();
        event->accept();
        return;
      }
    }

    // 名前エリアだけをドラッグ開始候補にする。スイッチ操作の微小移動で
    // レイヤー順序変更が走るのを避ける。
    if (service) {
      service->selectLayer(layer->id());
      impl_->dragStartPos = event->pos();
      impl_->dragCandidateLayerId = layer->id();
    }
    update();
  } else if (contextMenuRequest) {
    if (service && !isLayerSelectedInSelectionManager(layer->id())) {
      service->selectLayer(layer->id());
    }

    auto triggerDeleteLayer = [this, layer]() {
      if (!layer) {
        return;
      }
      const auto response = centeredQuestion(
          this, tt("layer_panel.delete_layer_title", "Delete Layer"),
          tt("layer_panel.delete_layer_message", "Delete layer \"%1\"?").arg(layer->layerName()));
      if (response != QMessageBox::Yes) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          svc->removeLayerFromComposition(compId, layer->id());
          impl_->selectedLayerId = LayerID();
          impl_->currentPropertyPath.clear();
          impl_->clearDragState();
          updateLayout();
        }
      }
    };

    const QVector<LayerID> selectedIds = selectedLayerIdsSnapshot();
    const QVector<LayerID> selectedVisibleIds = selectedLayerIdsInVisibleOrder(impl_->visibleRows);
    auto currentComp = safeCompositionLookup(impl_->compositionId);
    auto triggerDeleteSelectedLayers = [this, selectedIds]() {
      if (selectedIds.size() <= 1) {
        return;
      }
      const auto response = centeredQuestion(
          this, tt("layer_panel.delete_layers_title", "Delete Layers"),
          tt("layer_panel.delete_layers_message", "Delete %1 selected layers?").arg(selectedIds.size()));
      if (response != QMessageBox::Yes) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          if (auto* selectionManager = currentLayerSelectionManager()) {
            selectionManager->clearSelection();
          }
          impl_->clearDragState();
          for (const auto& layerId : selectedIds) {
            svc->removeLayerFromComposition(compId, layerId);
          }
  impl_->selectedLayerId = LayerID();
  impl_->selectionAnchorLayerId = LayerID();
  impl_->currentPropertyPath.clear();
  propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
  updateLayout();
        }
      }
    };

    auto triggerDuplicateLayer = [this, layer]() {
      if (!layer) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        if (svc->duplicateLayerInCurrentComposition(layer->id())) {
          updateLayout();
        }
      }
    };

    auto triggerDuplicateSelectedLayers = [this, selectedVisibleIds]() {
      if (selectedVisibleIds.size() <= 1) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          for (const auto& layerId : selectedVisibleIds) {
            svc->duplicateLayerInCurrentComposition(layerId);
          }
          updateLayout();
        }
      }
    };

    auto triggerGroupSelectedLayers = [this, selectedVisibleIds]() {
      if (selectedVisibleIds.size() <= 1) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        bool ok = false;
        const QString groupName = QInputDialog::getText(
            this,
            QStringLiteral("グループ化"),
            QStringLiteral("グループ名:"),
            QLineEdit::Normal,
            QStringLiteral("Group 1"),
            &ok);
        if (!ok) {
          return;
        }
        if (svc->groupSelectedLayersInCurrentComposition(UniString(groupName))) {
          updateLayout();
        }
      }
    };

    auto triggerPrecomposeSelectedLayers = [this, selectedVisibleIds]() {
      if (selectedVisibleIds.isEmpty()) {
        return;
      }
      auto* svc = ArtifactProjectService::instance();
      if (!svc) {
        return;
      }
      auto comp = svc->currentComposition().lock();
      if (!comp) {
        return;
      }

      // Resolve selection into ordered layer ids + names, mirroring the
      // Layer menu path so both entry points behave identically.
      QStringList selectedNames;
      QVector<LayerID> orderedIds;
      orderedIds.reserve(selectedVisibleIds.size());
      QSet<QString> wanted;
      for (const auto& id : selectedVisibleIds) {
        wanted.insert(id.toString());
      }
      for (const auto& layer : comp->allLayer()) {
        if (!layer) {
          continue;
        }
        if (!wanted.contains(layer->id().toString())) {
          continue;
        }
        orderedIds.push_back(layer->id());
        selectedNames.push_back(layer->layerName());
      }
      if (orderedIds.isEmpty()) {
        return;
      }

      PrecomposeDialog dialog(window());
      dialog.setSelectedLayerNames(selectedNames);
      dialog.setTotalLayerCount(comp->layerCount());
      if (dialog.exec() != QDialog::Accepted) {
        return;
      }

      const PrecomposeMode mode = dialog.moveSelectedOnly()
                                      ? PrecomposeMode::MoveSelected
                                      : PrecomposeMode::MoveAllAttributes;
      if (svc->precomposeLayersWithUndo(
              orderedIds, UniString(dialog.newCompositionName()),
              dialog.openNewComposition(), dialog.matchWorkspaceDuration(),
              mode)) {
        updateLayout();
      }
    };

    auto triggerSelectedVisibility = [this, currentComp, selectedVisibleIds](bool visible) {
      if (selectedVisibleIds.isEmpty() || !currentComp) {
        return;
      }
      auto macro = std::make_unique<MacroUndoCommand>(
          visible ? QStringLiteral("Show Layers") : QStringLiteral("Hide Layers"));
      for (const auto& layerId : selectedVisibleIds) {
        if (auto layer = currentComp->layerById(layerId)) {
          macro->addChild(std::make_unique<SetLayerVisibilityCommand>(layer, visible));
        }
      }
      UndoManager::instance()->push(std::move(macro));
      updateLayout();
    };

    auto triggerSelectedLock = [this, currentComp, selectedVisibleIds](bool locked) {
      if (selectedVisibleIds.isEmpty() || !currentComp) {
        return;
      }
      auto macro = std::make_unique<MacroUndoCommand>(
          locked ? QStringLiteral("Lock Layers") : QStringLiteral("Unlock Layers"));
      for (const auto& layerId : selectedVisibleIds) {
        if (auto layer = currentComp->layerById(layerId)) {
          macro->addChild(std::make_unique<SetLayerLockCommand>(layer, locked));
        }
      }
      UndoManager::instance()->push(std::move(macro));
      updateLayout();
    };

    auto triggerSelectedSolo = [this, currentComp, selectedVisibleIds](bool solo) {
      if (selectedVisibleIds.isEmpty() || !currentComp) {
        return;
      }
      auto macro = std::make_unique<MacroUndoCommand>(
          solo ? QStringLiteral("Solo Layers") : QStringLiteral("Unsolo Layers"));
      for (const auto& layerId : selectedVisibleIds) {
        if (auto layer = currentComp->layerById(layerId)) {
          macro->addChild(std::make_unique<SetLayerSoloCommand>(layer, solo));
        }
      }
      UndoManager::instance()->push(std::move(macro));
      updateLayout();
    };

    auto triggerSelectedShy = [this, currentComp, selectedVisibleIds](bool shy) {
      if (selectedVisibleIds.isEmpty() || !currentComp) {
        return;
      }
      auto macro = std::make_unique<MacroUndoCommand>(
          shy ? QStringLiteral("Shy Layers") : QStringLiteral("Unshy Layers"));
      for (const auto& layerId : selectedVisibleIds) {
        if (auto layer = currentComp->layerById(layerId)) {
          macro->addChild(std::make_unique<SetLayerShyCommand>(layer, shy));
        }
      }
      UndoManager::instance()->push(std::move(macro));
      updateLayout();
    };

    auto triggerReplaceLayerSource = [this, layer]() {
      if (!layer) {
        return;
      }
      const auto sourceFilter = QStringLiteral(
          "Image Files (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.exr);;"
          "All Files (*.*)");
      const QString currentPath = [&]() -> QString {
        if (auto imageLayer = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
          return imageLayer->sourcePath();
        }
        return {};
      }();
      const QString selectedPath = QFileDialog::getOpenFileName(
          this, QStringLiteral("Replace Image Source"),
          currentPath.isEmpty() ? QString() : QFileInfo(currentPath).absolutePath(),
          sourceFilter);
      if (selectedPath.isEmpty()) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        if (svc->replaceLayerSourceInCurrentComposition(layer->id(), selectedPath)) {
          updateLayout();
        }
      }
    };

    auto videoSourcePath = [layer]() -> QString {
      if (auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
        return videoLayer->sourcePath();
      }
      return {};
    };

    auto triggerReplaceVideoSource = [this, layer, videoSourcePath]() {
      if (!layer) {
        return;
      }
      const QString currentPath = videoSourcePath();
      const QString selectedPath = QFileDialog::getOpenFileName(
          this, QStringLiteral("Replace Video Source"),
          currentPath.isEmpty() ? QString() : QFileInfo(currentPath).absolutePath(),
          QStringLiteral("Video Files (*.mp4 *.mov *.mkv *.avi *.webm *.m4v *.mpg *.mpeg *.mxf *.gif);;All Files (*.*)"));
      if (selectedPath.isEmpty()) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        if (svc->replaceLayerSourceInCurrentComposition(layer->id(), selectedPath)) {
          updateLayout();
        }
      }
    };

    auto triggerReloadVideoSource = [this, layer, videoSourcePath]() {
      if (!layer) {
        return;
      }
      const QString currentPath = videoSourcePath();
      if (currentPath.trimmed().isEmpty()) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        if (svc->replaceLayerSourceInCurrentComposition(layer->id(), currentPath)) {
          updateLayout();
        }
      }
    };

    auto triggerRevealVideoSource = [layer, videoSourcePath]() {
      const QString currentPath = videoSourcePath();
      if (currentPath.trimmed().isEmpty()) {
        return;
      }
      const QFileInfo info(currentPath);
      const QString folder = info.absolutePath();
      if (!folder.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
      }
    };

    auto triggerOpenComposition = [this, layer]() {
      if (!layer) {
        return;
      }
      auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer.get());
      if (!compLayer) {
        return;
      }
      const CompositionID sourceCompId = compLayer->sourceCompositionId();
      if (sourceCompId.isNil()) {
        return;
      }
      if (auto *svc = ArtifactProjectService::instance()) {
        svc->changeCurrentComposition(sourceCompId);
      }
    };

    auto triggerRenameComposition = [this, layer]() {
      if (!layer) {
        return;
      }
      auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer.get());
      if (!compLayer) {
        return;
      }
      const CompositionID sourceCompId = compLayer->sourceCompositionId();
      if (sourceCompId.isNil()) {
        return;
      }
      auto *svc = ArtifactProjectService::instance();
      if (!svc) {
        return;
      }
      auto found = svc->findComposition(sourceCompId);
      auto comp = found.ptr.lock();
      const QString currentName = comp
                                      ? comp->settings().compositionName().toQString()
                                      : layer->layerName();
      bool ok = false;
      const QString newName = QInputDialog::getText(
          this, tt("layer_panel.rename_composition_title", "Rename Composition"),
          tt("layer_panel.new_composition_name", "New composition name:"),
          QLineEdit::Normal, currentName, &ok);
      if (!ok || newName.trimmed().isEmpty()) {
        return;
      }
      if (svc->renameComposition(sourceCompId, UniString(newName))) {
        updateLayout();
      }
    };

    auto triggerDuplicateComposition = [this, layer]() {
      if (!layer) {
        return;
      }
      auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer.get());
      if (!compLayer) {
        return;
      }
      const CompositionID sourceCompId = compLayer->sourceCompositionId();
      if (sourceCompId.isNil()) {
        return;
      }
      auto *svc = ArtifactProjectService::instance();
      if (!svc) {
        return;
      }
      if (svc->duplicateComposition(sourceCompId)) {
        updateLayout();
      }
    };

    auto triggerRenameLayer = [this, layer]() {
      if (!layer) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        bool ok = false;
        const QString newName = QInputDialog::getText(
            this, tt("layer_panel.rename_layer_title", "Rename Layer"),
            tt("layer_panel.new_layer_name", "New layer name:"),
            QLineEdit::Normal, layer->layerName(), &ok);
        if (!ok || newName.trimmed().isEmpty()) {
          return;
        }
        if (svc->renameLayerInCurrentComposition(layer->id(), newName)) {
          updateLayout();
        }
      }
    };

    auto triggerOpenInspector = [this]() {
      auto* mainWindow = qobject_cast<QWidget*>(this->window());
      if (!mainWindow) {
        return;
      }
      setDockVisible(mainWindow, QStringLiteral("Inspector"), true);
      activateDock(mainWindow, QStringLiteral("Inspector"));
    };

    auto triggerOpenProperties = [this]() {
      auto* mainWindow = qobject_cast<QWidget*>(this->window());
      if (!mainWindow) {
        return;
      }
      setDockVisible(mainWindow, QStringLiteral("Properties"), true);
      activateDock(mainWindow, QStringLiteral("Properties"));
    };

    auto triggerSelectParent = [this, layer]() {
      if (!layer) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        const LayerID parentId = svc->layerParentIdInCurrentComposition(layer->id());
        if (!parentId.isNil()) {
          svc->selectLayer(parentId);
        }
      }
    };

    auto triggerClearParent = [this, layer]() {
      if (!layer) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        if (svc->clearLayerParentInCurrentComposition(layer->id())) {
          updateLayout();
        }
      }
    };

    auto triggerSetProxyQuality = [this, layer](ProxyQuality quality) {
      if (!layer) {
        return;
      }
      auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
      if (!videoLayer || videoLayer->proxyQuality() == quality) {
        return;
      }
      videoLayer->setLayerPropertyValue(QStringLiteral("video.proxyQuality"),
                                        QVariant::fromValue(static_cast<int>(quality)));
      videoLayer->changed();
      if (auto comp = safeCompositionLookup(impl_->compositionId)) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), videoLayer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
    };

    auto triggerGenerateProxy = [this, layer]() {
      if (!layer) {
        return;
      }
      auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
      if (!videoLayer) {
        return;
      }
      const QString sourcePath = videoLayer->sourcePath().trimmed();
      if (sourcePath.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Proxy"),
                                 QStringLiteral("Source file が見つかりません。"));
        return;
      }

      auto* window = qobject_cast<QWidget*>(this->window());
      if (!window) {
        return;
      }
      setDockVisible(window, QStringLiteral("Project"), true);
      activateDock(window, QStringLiteral("Project"));

      auto* projectDock = window->findChild<ArtifactProjectManagerWidget*>(
          QStringLiteral("artifactProjectManagerWidget"));
      if (!projectDock) {
        const auto docks = window->findChildren<ArtifactProjectManagerWidget*>();
        if (!docks.isEmpty()) {
          projectDock = docks.first();
        }
      }
      if (!projectDock) {
        QMessageBox::information(this, QStringLiteral("Proxy"),
                                 QStringLiteral("Project dock が見つかりません。"));
        return;
      }

      if (!projectDock->selectItemsByFilePaths(QStringList{sourcePath})) {
        QMessageBox::information(this, QStringLiteral("Proxy"),
                                 QStringLiteral("Source file を Project で選択できませんでした。"));
        return;
      }
      projectDock->generateProxyForSelection();
    };

    auto triggerRevealProxy = [this, layer]() {
      if (!layer) {
        return;
      }
      auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
      if (!videoLayer) {
        return;
      }
      const QString proxyPath = videoLayer->proxyPath().trimmed();
      if (proxyPath.isEmpty() || !QFileInfo::exists(proxyPath)) {
        QMessageBox::information(this, QStringLiteral("Proxy"),
                                 QStringLiteral("表示できるプロキシがありません。"));
        return;
      }
      const QString folder = QFileInfo(proxyPath).absolutePath();
      if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder))) {
        QMessageBox::warning(this, QStringLiteral("Proxy"),
                             QStringLiteral("プロキシフォルダを開けませんでした。"));
      }
    };

    auto triggerClearProxy = [this, layer]() {
      if (!layer) {
        return;
      }
      auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
      if (!videoLayer) {
        return;
      }
      const QString proxyPath = videoLayer->proxyPath();
      if (proxyPath.isEmpty()) {
        return;
      }

      auto* window = qobject_cast<QWidget*>(this->window());
      auto* projectDock = window
                              ? window->findChild<ArtifactProjectManagerWidget*>(
                                    QStringLiteral("artifactProjectManagerWidget"))
                              : nullptr;
      if (!projectDock && window) {
        const auto docks = window->findChildren<ArtifactProjectManagerWidget*>();
        if (!docks.isEmpty()) {
          projectDock = docks.first();
        }
      }
      if (projectDock) {
        if (!projectDock->clearProxyForFilePath(videoLayer->sourcePath())) {
          QMessageBox::warning(this, QStringLiteral("Proxy"),
                               QStringLiteral("プロキシファイルを削除できませんでした。"));
        }
        return;
      }

      videoLayer->clearProxy();
      videoLayer->changed();
      if (auto comp = safeCompositionLookup(impl_->compositionId)) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), videoLayer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
    };

    const int ctxNameStartX = [this]() {
      int x = 0;
      for (int i = 0; i < kLayerPropertyColumnCount; ++i) if (impl_->columnVisible_[i]) x += impl_->columnWidths_[i];
      return x;
    }();
    const int nameX = ctxNameStartX + row.depth * 14;
    const bool showInlineCombos = width() - (nameX + 8) >= (kInlineComboReserve + kLayerNameMinWidth);
    const auto variants = layer->getVariants();
    const QRect chipRect = variantChipRect(QRect(0, impl_->rowViewportY(idx), width(), rowH),
                                           layer, showInlineCombos, rowH);
    if (chipRect.contains(event->pos())) {
      showVariantPickerMenu(this, layer, event->globalPos());
      event->accept();
      return;
    }

    auto comp = safeCompositionLookup(impl_->compositionId);
    QMenu menu(this);
    QMenu* frequentMenu = menu.addMenu(QStringLiteral("Frequent"));
    QMenu* allMenu = menu.addMenu(QStringLiteral("All"));
    if (auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer.get())) {
      QMenu *precompMenu = allMenu->addMenu(QStringLiteral("プリコンポーズ"));
      precompMenu->addAction(QStringLiteral("コンポジションを開く"),
                             [triggerOpenComposition]() {
                               triggerOpenComposition();
                             });
      precompMenu->addAction(QStringLiteral("コンポジション名を変更..."),
                             [triggerRenameComposition]() {
                               triggerRenameComposition();
                             });
      precompMenu->addAction(QStringLiteral("コンポジションを複製"),
                             [triggerDuplicateComposition]() {
                               triggerDuplicateComposition();
                             });
      if (compLayer->sourceCompositionId().isNil()) {
        precompMenu->setEnabled(false);
      }
    }
    if (auto *groupLayer = dynamic_cast<ArtifactGroupLayer *>(layer.get())) {
      QMenu *groupMenu = allMenu->addMenu(QStringLiteral("グループ"));
      groupMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_group.svg")));
      const bool collapsed = groupLayer->isCollapsed();
      QAction *toggleCollapseAct = groupMenu->addAction(
          collapsed ? QStringLiteral("展開する") : QStringLiteral("折りたたむ"));
      toggleCollapseAct->setIcon(QIcon(resolveIconPath(collapsed ? "Studio/arrow_drop_down.svg"
                                                               : "Studio/arrow_right.svg")));
      QAction *selectChildrenAct = groupMenu->addAction(QStringLiteral("子レイヤーを選択"));
      selectChildrenAct->setIcon(QIcon(resolveIconPath("Studio/select_all.svg")));
      QAction *showChildCountAct = groupMenu->addAction(
          QStringLiteral("子レイヤー数: %1").arg(static_cast<int>(groupLayer->children().size())));
      showChildCountAct->setEnabled(false);
      groupMenu->addSeparator();
      QMenu *outputModeMenu = groupMenu->addMenu(QStringLiteral("出力モード"));
      const auto addOutputModeAction = [this, layer, groupLayer, outputModeMenu](
                                           const QString& label, GroupOutputMode mode) {
        QAction *action = outputModeMenu->addAction(label, [this, layer, mode]() {
          auto *group = dynamic_cast<ArtifactGroupLayer *>(layer.get());
          if (!group) {
            return;
          }
          group->setOutputMode(mode);
          if (auto comp = safeCompositionLookup(impl_->compositionId)) {
            ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                LayerChangedEvent{comp->id().toString(), group->id().toString(),
                                  LayerChangedEvent::ChangeType::Modified});
          }
          updateLayout();
        });
        action->setCheckable(true);
        action->setChecked(groupLayer->outputMode() == mode);
      };
      addOutputModeAction(QStringLiteral("すべて表示 (各 100%)"), GroupOutputMode::All);
      addOutputModeAction(QStringLiteral("選択した子のみ (100%)"), GroupOutputMode::Single);
      addOutputModeAction(QStringLiteral("子で 100% を共有"), GroupOutputMode::Share);
      groupMenu->addSeparator();
      QMenu *activeChildMenu = groupMenu->addMenu(QStringLiteral("Single の出力先"));
      const auto children = groupLayer->children();
      if (children.empty()) {
        QAction *emptyAction = activeChildMenu->addAction(QStringLiteral("子レイヤーなし"));
        emptyAction->setEnabled(false);
      } else {
        for (const auto& child : children) {
          if (!child) {
            continue;
          }
          const LayerID childId = child->id();
          QAction *childAction = activeChildMenu->addAction(child->layerName(),
              [this, layer, childId]() {
                auto *group = dynamic_cast<ArtifactGroupLayer *>(layer.get());
                if (!group) {
                  return;
                }
                group->setActiveChildId(childId);
                if (auto comp = safeCompositionLookup(impl_->compositionId)) {
                  ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                      LayerChangedEvent{comp->id().toString(), group->id().toString(),
                                        LayerChangedEvent::ChangeType::Modified});
                }
                updateLayout();
              });
          childAction->setCheckable(true);
          childAction->setChecked(groupLayer->activeChildId() == childId);
          childAction->setEnabled(child->isVisible());
        }
      }
      groupMenu->addSeparator();
      groupMenu->addAction(QStringLiteral("グループ名を変更..."), [triggerRenameLayer]() {
        triggerRenameLayer();
      });

      QObject::connect(toggleCollapseAct, &QAction::triggered, groupMenu, [this, layer](bool) {
        auto *group = dynamic_cast<ArtifactGroupLayer *>(layer.get());
        if (!group) {
          return;
        }
        group->setCollapsed(!group->isCollapsed());
        if (auto comp = safeCompositionLookup(impl_->compositionId)) {
          ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
              LayerChangedEvent{comp->id().toString(), group->id().toString(),
                                LayerChangedEvent::ChangeType::Modified});
        }
        updateLayout();
      });
      QObject::connect(selectChildrenAct, &QAction::triggered, groupMenu, [this, layer](bool) {
        auto *group = dynamic_cast<ArtifactGroupLayer *>(layer.get());
        if (!group) {
          return;
        }
        auto *svc = ArtifactProjectService::instance();
        if (!svc) {
          return;
        }
        bool first = true;
        for (const auto& child : group->children()) {
          if (!child) {
            continue;
          }
          if (first) {
            svc->selectLayer(child->id());
            first = false;
          } else if (auto *selection = ArtifactLayerSelectionManager::instance()) {
            selection->addToSelection(child);
          }
        }
      });
    }
    const bool isImageLayer = std::dynamic_pointer_cast<ArtifactImageLayer>(layer) != nullptr;
    if (isImageLayer) {
      allMenu->addAction(tt("layer_panel.replace_image", "Replace Image..."), [triggerReplaceLayerSource]() {
        triggerReplaceLayerSource();
      });
    }
    const bool isVideoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) != nullptr;
    if (isVideoLayer) {
      QMenu* videoMenu = allMenu->addMenu(QStringLiteral("ビデオ"));
      videoMenu->setIcon(QIcon(resolveIconPath("Studio/videocam.svg")));
      QAction* replaceVideoAct = videoMenu->addAction(QIcon(resolveIconPath("Studio/file_open.svg")),
                                                      QStringLiteral("ソースを置換..."));
      QAction* reloadVideoAct = videoMenu->addAction(QIcon(resolveIconPath("Studio/replay.svg")),
                                                    QStringLiteral("ソースを再読み込み"));
      QAction* revealVideoAct = videoMenu->addAction(QIcon(resolveIconPath("Studio/folder_open.svg")),
                                                    QStringLiteral("ソースを表示"));
      videoMenu->addSeparator();
      QAction* muteAudioAct = videoMenu->addAction(QIcon(resolveIconPath("Studio/settings.svg")),
                                                   QStringLiteral("音声ミュートを切替"));
      QAction* toggleVideoAct = videoMenu->addAction(QIcon(resolveIconPath("Studio/visibility.svg")),
                                                     QStringLiteral("映像有効を切替"));

      const QString currentVideoPath = videoSourcePath();
      reloadVideoAct->setEnabled(!currentVideoPath.trimmed().isEmpty());
      revealVideoAct->setEnabled(!currentVideoPath.trimmed().isEmpty());
      muteAudioAct->setEnabled(true);
      toggleVideoAct->setEnabled(true);

      QObject::connect(replaceVideoAct, &QAction::triggered, videoMenu, [triggerReplaceVideoSource](bool) {
        triggerReplaceVideoSource();
      });
      QObject::connect(reloadVideoAct, &QAction::triggered, videoMenu, [triggerReloadVideoSource](bool) {
        triggerReloadVideoSource();
      });
      QObject::connect(revealVideoAct, &QAction::triggered, videoMenu, [triggerRevealVideoSource](bool) {
        triggerRevealVideoSource();
      });
      QObject::connect(muteAudioAct, &QAction::triggered, videoMenu, [this, layer](bool) {
        auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
        if (!videoLayer) {
          return;
        }
        const bool muted = !videoLayer->isAudioMuted();
        videoLayer->setAudioMuted(muted);
        if (layer->setLayerPropertyValue(QStringLiteral("video.audioMuted"), muted)) {
          updateLayout();
        }
      });
      QObject::connect(toggleVideoAct, &QAction::triggered, videoMenu, [this, layer](bool) {
        auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
        if (!videoLayer) {
          return;
        }
        const bool enabled = !videoLayer->hasVideo();
        videoLayer->setHasVideo(enabled);
        if (layer->setLayerPropertyValue(QStringLiteral("video.videoEnabled"), enabled)) {
          updateLayout();
        }
      });
      videoMenu->addSeparator();
      QMenu* proxyMenu = videoMenu->addMenu(QStringLiteral("プロキシ"));
      proxyMenu->setIcon(QIcon(resolveIconPath("Studio/resolution_half.svg")));
      QAction* proxyNoneAct = proxyMenu->addAction(QStringLiteral("無効"));
      proxyNoneAct->setIcon(QIcon(resolveIconPath("Studio/resolution_full.svg")));
      QAction* proxyQuarterAct = proxyMenu->addAction(QStringLiteral("1/4 画質"));
      proxyQuarterAct->setIcon(QIcon(resolveIconPath("Studio/resolution_quarter.svg")));
      QAction* proxyHalfAct = proxyMenu->addAction(QStringLiteral("1/2 画質"));
      proxyHalfAct->setIcon(QIcon(resolveIconPath("Studio/resolution_half.svg")));
      QAction* proxyFullAct = proxyMenu->addAction(QStringLiteral("フル画質"));
      proxyFullAct->setIcon(QIcon(resolveIconPath("Studio/resolution_full.svg")));
      QActionGroup* proxyQualityGroup = new QActionGroup(proxyMenu);
      proxyQualityGroup->setExclusive(true);
      proxyMenu->addSeparator();
      QAction* generateProxyAct = proxyMenu->addAction(QStringLiteral("プロキシを生成"));
      generateProxyAct->setIcon(QIcon(resolveIconPath("Studio/replay.svg")));
      QAction* revealProxyAct = proxyMenu->addAction(QStringLiteral("プロキシを表示"));
      revealProxyAct->setIcon(QIcon(resolveIconPath("Studio/folder_open.svg")));
      QAction* clearProxyAct = proxyMenu->addAction(QStringLiteral("プロキシを削除"));
      clearProxyAct->setIcon(QIcon(resolveIconPath("Studio/delete.svg")));
      QObject::connect(proxyNoneAct, &QAction::triggered, proxyMenu,
                       [triggerSetProxyQuality](bool) { triggerSetProxyQuality(ProxyQuality::None); });
      QObject::connect(proxyQuarterAct, &QAction::triggered, proxyMenu,
                       [triggerSetProxyQuality](bool) { triggerSetProxyQuality(ProxyQuality::Quarter); });
      QObject::connect(proxyHalfAct, &QAction::triggered, proxyMenu,
                       [triggerSetProxyQuality](bool) { triggerSetProxyQuality(ProxyQuality::Half); });
      QObject::connect(proxyFullAct, &QAction::triggered, proxyMenu,
                       [triggerSetProxyQuality](bool) { triggerSetProxyQuality(ProxyQuality::Full); });
      QObject::connect(generateProxyAct, &QAction::triggered, proxyMenu,
                       [triggerGenerateProxy](bool) { triggerGenerateProxy(); });
      QObject::connect(revealProxyAct, &QAction::triggered, proxyMenu,
                       [triggerRevealProxy](bool) { triggerRevealProxy(); });
      QObject::connect(clearProxyAct, &QAction::triggered, proxyMenu,
                       [triggerClearProxy](bool) { triggerClearProxy(); });
      const auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
      const bool hasProxy = videoLayer && !videoLayer->proxyPath().trimmed().isEmpty();
      const ProxyQuality currentProxyQuality = videoLayer ? videoLayer->proxyQuality() : ProxyQuality::None;
      proxyNoneAct->setCheckable(true);
      proxyQuarterAct->setCheckable(true);
      proxyHalfAct->setCheckable(true);
      proxyFullAct->setCheckable(true);
      proxyQualityGroup->addAction(proxyNoneAct);
      proxyQualityGroup->addAction(proxyQuarterAct);
      proxyQualityGroup->addAction(proxyHalfAct);
      proxyQualityGroup->addAction(proxyFullAct);
      proxyNoneAct->setChecked(currentProxyQuality == ProxyQuality::None);
      proxyQuarterAct->setChecked(currentProxyQuality == ProxyQuality::Quarter);
      proxyHalfAct->setChecked(currentProxyQuality == ProxyQuality::Half);
      proxyFullAct->setChecked(currentProxyQuality == ProxyQuality::Full);
      generateProxyAct->setEnabled(!videoSourcePath().trimmed().isEmpty());
      revealProxyAct->setEnabled(hasProxy);
      clearProxyAct->setEnabled(hasProxy);
    }
    if (!variants.empty()) {
      allMenu->addAction(QStringLiteral("バリアントを選択..."), [this, layer]() {
        showVariantPickerMenu(this, layer, QCursor::pos());
      });
    }
    const auto matteRefs = layer->matteReferences();
    const auto selectedLayer =
        ArtifactLayerSelectionManager::instance()
            ? ArtifactLayerSelectionManager::instance()->currentLayer()
            : ArtifactAbstractLayerPtr{};
    const bool canUseSelectedMatteSource =
        selectedLayer && selectedLayer->id() != layer->id() && comp &&
        comp->containsLayerById(selectedLayer->id());
    if (canUseSelectedMatteSource || !matteRefs.empty()) {
      QMenu *matteMenu = allMenu->addMenu(QStringLiteral("トラックマット"));
      if (canUseSelectedMatteSource) {
        QAction *addAction =
            matteMenu->addAction(QStringLiteral("選択レイヤーをソースにする"));
        addAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("matte_add_selected")},
                                       {QStringLiteral("selectedLayerId"), selectedLayer->id().toString()}});
      }
      if (!matteRefs.empty()) {
        const QStringList typeLabels = {
            QStringLiteral("アルファ"),
            QStringLiteral("ルーマ"),
            QStringLiteral("反転アルファ"),
            QStringLiteral("反転ルーマ")};
        for (int matteIndex = 0; matteIndex < static_cast<int>(matteRefs.size()); ++matteIndex) {
          const auto &ref = matteRefs[matteIndex];
          QString sourceName = QStringLiteral("<missing>");
          if (comp && !ref.sourceLayerId.isNil()) {
            if (auto source = comp->layerById(ref.sourceLayerId)) {
              const QString name = source->layerName().trimmed();
              sourceName = name.isEmpty() ? ref.sourceLayerId.toString() : name;
            }
          }
          QMenu *refMenu = matteMenu->addMenu(
              QStringLiteral("マット %1: %2").arg(matteIndex + 1).arg(sourceName));
          QAction *focusAction = refMenu->addAction(QStringLiteral("ソースにフォーカス"));
          focusAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("matte_focus")},
                                           {QStringLiteral("index"), matteIndex}});

          QMenu *typeMenu = refMenu->addMenu(QStringLiteral("マット種別を設定"));
          for (int typeIndex = 0; typeIndex < typeLabels.size(); ++typeIndex) {
            QAction *typeAction = typeMenu->addAction(typeLabels[typeIndex]);
            typeAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("matte_type")},
                                            {QStringLiteral("index"), matteIndex},
                                            {QStringLiteral("type"), typeIndex}});
          }
        }
      }
    }
    QMenu* stateMenu = frequentMenu->addMenu(QStringLiteral("状態"));
    stateMenu->addAction(QStringLiteral("表示を切替"), [this, layer]() {
      if (!layer) return;
      UndoManager::instance()->push(
          std::make_unique<SetLayerVisibilityCommand>(layer, !layer->isVisible()));
      updateLayout();
    });
    stateMenu->addAction(QStringLiteral("ロックを切替"), [this, layer]() {
      if (!layer) return;
      UndoManager::instance()->push(
          std::make_unique<SetLayerLockCommand>(layer, !layer->isLocked()));
      updateLayout();
    });
    stateMenu->addAction(QStringLiteral("ソロを切替"), [this, layer]() {
      if (!layer) return;
      UndoManager::instance()->push(
          std::make_unique<SetLayerSoloCommand>(layer, !layer->isSolo()));
      updateLayout();
    });
    stateMenu->addAction(QStringLiteral("シャイを切替"), [this, layer]() {
      if (!layer) return;
      UndoManager::instance()->push(
          std::make_unique<SetLayerShyCommand>(layer, !layer->isShy()));
      updateLayout();
    });
    stateMenu->addSeparator();
    stateMenu->addAction(QStringLiteral("スマートソロ"), [this, layer]() {
      if (!layer) {
        return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
        svc->smartSoloOnlyLayerInCurrentComposition(layer->id());
        updateLayout();
      }
    });

    // カラーラベル
    {
      QMenu* colorMenu = allMenu->addMenu(QStringLiteral("ラベル色"));
      static const std::pair<const char*, QColor> kLabelColors[] = {
        {"なし",    QColor()},
        {"赤",      QColor(231, 76, 60)},
        {"オレンジ",QColor(230, 126, 34)},
        {"黄",      QColor(241, 196, 15)},
        {"緑",      QColor(46, 204, 113)},
        {"シアン",  QColor(26, 188, 156)},
        {"青",      QColor(52, 152, 219)},
        {"マゼンタ",QColor(155, 89, 182)}
      };
      const int currentColor = layer->labelColorIndex();
      for (int ci = 0; ci < 8; ++ci) {
        QAction* ca = colorMenu->addAction(
            QString::fromLatin1(kLabelColors[ci].first), [this, layer, ci]() {
          if (!layer) return;
          layer->setLabelColorIndex(ci);
          update();
        });
        if (!kLabelColors[ci].second.isValid()) {
          ca->setIcon(QIcon());
        } else {
          QPixmap cp(12, 12);
          cp.fill(kLabelColors[ci].second);
          ca->setIcon(QIcon(cp));
        }
        if (ci == currentColor) {
          ca->setCheckable(true);
          ca->setChecked(true);
        }
      }
    }

    // 全レイヤー表示/ロック一括切替と一括グループ操作
    {
      QMenu* batchAllMenu = allMenu->addMenu(QStringLiteral("全レイヤー操作"));
      batchAllMenu->addAction(QStringLiteral("すべて表示"), [this]() {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (!comp) return;
        for (const auto& l : comp->allLayer()) {
          if (l) l->setVisible(true);
        }
        updateLayout();
      });
      batchAllMenu->addAction(QStringLiteral("すべて非表示"), [this]() {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (!comp) return;
        for (const auto& l : comp->allLayer()) {
          if (l) l->setVisible(false);
        }
        updateLayout();
      });
      batchAllMenu->addSeparator();
      batchAllMenu->addAction(QStringLiteral("すべてロック"), [this]() {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (!comp) return;
        for (const auto& l : comp->allLayer()) {
          if (l) l->setLocked(true);
        }
        updateLayout();
      });
      batchAllMenu->addAction(QStringLiteral("すべてロック解除"), [this]() {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (!comp) return;
        for (const auto& l : comp->allLayer()) {
          if (l) l->setLocked(false);
        }
        updateLayout();
      });
      batchAllMenu->addSeparator();
      batchAllMenu->addAction(QStringLiteral("すべてのグループを折りたたみ"), [this]() {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (!comp) return;
        for (const auto& l : comp->allLayer()) {
          if (l && l->isGroupLayer()) {
            const QString idStr = l->id().toString();
            impl_->expandedByLayerId[idStr] = false;
          }
        }
        updateLayout();
      });
      batchAllMenu->addAction(QStringLiteral("すべてのグループを展開"), [this]() {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (!comp) return;
        for (const auto& l : comp->allLayer()) {
          if (l && l->isGroupLayer()) {
            const QString idStr = l->id().toString();
            impl_->expandedByLayerId[idStr] = true;
          }
        }
        updateLayout();
      });
    }

    // M: カラム表示切替
    {
      QMenu* colMenu = allMenu->addMenu(QStringLiteral("カラム表示"));
      static const char* kColNames[] = {"表示", "ロック", "ソロ", "オーディオ", "シャイ"};
      for (int ci = 0; ci < kLayerPropertyColumnCount; ++ci) {
        QAction* ca = colMenu->addAction(QString::fromLatin1(kColNames[ci]), [this, ci]() {
          impl_->columnVisible_[ci] = !impl_->columnVisible_[ci];
          updateLayout();
        });
        ca->setCheckable(true);
        ca->setChecked(impl_->columnVisible_[ci]);
      }
    }

    // H: マスクありのみ表示トグル
    auto* maskFilterAction = allMenu->addAction(QStringLiteral("マスクありのみ表示"), [this]() {
      impl_->maskFilterEnabled_ = !impl_->maskFilterEnabled_;
      updateLayout();
    });
    maskFilterAction->setCheckable(true);
    maskFilterAction->setChecked(impl_->maskFilterEnabled_);

    // O: プロパティをクリップボードにコピー
    if (layer) {
      QMenu* copyMenu = allMenu->addMenu(QStringLiteral("プロパティをコピー"));
      copyMenu->addAction(QStringLiteral("JSON"), [this, layer]() {
        QJsonObject obj;
        obj[QStringLiteral("name")] = layer->layerName();
        obj[QStringLiteral("id")] = layer->id().toString();
        obj[QStringLiteral("type")] = describeLayerType(layer);
        obj[QStringLiteral("visible")] = layer->isVisible();
        obj[QStringLiteral("locked")] = layer->isLocked();
        obj[QStringLiteral("solo")] = layer->isSolo();
        obj[QStringLiteral("labelColor")] = layer->labelColorIndex();
        obj[QStringLiteral("opacity")] = layer->opacity();
        QApplication::clipboard()->setText(
          QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented)));
      });
      copyMenu->addAction(QStringLiteral("CSV"), [this, layer]() {
        QString csv = QStringLiteral("name,id,type,visible,locked,solo,labelColor,opacity\n");
        csv += QStringLiteral("\"%1\",\"%2\",\"%3\",%4,%5,%6,%7,%8\n")
                 .arg(layer->layerName(), layer->id().toString(),
                      describeLayerType(layer))
                 .arg(layer->isVisible())
                 .arg(layer->isLocked())
                 .arg(layer->isSolo())
                 .arg(layer->labelColorIndex())
                 .arg(layer->opacity());
        QApplication::clipboard()->setText(csv);
      });
    }

    // U: 新規レイヤー即作成
    {
      QMenu* newSubMenu = allMenu->addMenu(QStringLiteral("新規"));
      newSubMenu->addAction(QStringLiteral("ヌル"), [this]() {
        if (auto* svc = ArtifactProjectService::instance()) {
          ArtifactLayerInitParams params(QStringLiteral("Null Layer"), LayerType::Null);
          svc->addLayerToCurrentComposition(params, true, false);
          updateLayout();
        }
      });
      newSubMenu->addAction(QStringLiteral("単色"), [this]() {
        if (auto* svc = ArtifactProjectService::instance()) {
          ArtifactLayerInitParams params(QStringLiteral("Solid"), LayerType::Solid);
          svc->addLayerToCurrentComposition(params, true, false);
          updateLayout();
        }
      });
      newSubMenu->addAction(QStringLiteral("テキスト"), [this]() {
        if (auto* svc = ArtifactProjectService::instance()) {
          ArtifactLayerInitParams params(QStringLiteral("Text"), LayerType::Text);
          svc->addLayerToCurrentComposition(params, true, false);
          updateLayout();
        }
      });
      newSubMenu->addAction(QStringLiteral("空のグループ"), [this]() {
        if (auto* svc = ArtifactProjectService::instance()) {
          ArtifactLayerInitParams params(QStringLiteral("Group"), LayerType::Group);
          svc->addLayerToCurrentComposition(params, true, false);
          updateLayout();
        }
      });
      newSubMenu->addAction(QStringLiteral("マルチプレクサーグループ"), [this]() {
        if (auto* svc = ArtifactProjectService::instance()) {
          ArtifactLayerInitParams params(QStringLiteral("Multiplexer Group"), LayerType::Group);
          svc->addLayerToCurrentComposition(params, true, false);
          if (auto* selection = ArtifactLayerSelectionManager::instance()) {
            if (auto created = selection->currentLayer()) {
              if (auto* group = dynamic_cast<ArtifactGroupLayer*>(created.get())) {
                group->setOutputMode(GroupOutputMode::Single);
              }
            }
          }
          updateLayout();
        }
      });
    }

    // V: テンプレート保存
    if (layer) {
      allMenu->addAction(QStringLiteral("テンプレートに保存"), [this, layer]() {
        QJsonObject tmpl;
        tmpl[QStringLiteral("name")] = layer->layerName();
        tmpl[QStringLiteral("type")] = describeLayerType(layer);
        tmpl[QStringLiteral("visible")] = layer->isVisible();
        tmpl[QStringLiteral("locked")] = layer->isLocked();
        tmpl[QStringLiteral("solo")] = layer->isSolo();
        tmpl[QStringLiteral("labelColor")] = layer->labelColorIndex();
        tmpl[QStringLiteral("opacity")] = layer->opacity();
        impl_->layerTemplate_ = tmpl;
      });
      if (!impl_->layerTemplate_.isEmpty()) {
        allMenu->addAction(QStringLiteral("テンプレートから作成"), [this]() {
          const QString typeName = impl_->layerTemplate_.value(QStringLiteral("type")).toString();
          LayerType lt = LayerType::Null;
          if (typeName == "Null") lt = LayerType::Null;
          else if (typeName == "Solid") lt = LayerType::Solid;
          else if (typeName == "Text") lt = LayerType::Text;
          else if (typeName == "Group") lt = LayerType::Group;
          ArtifactLayerInitParams params(
            impl_->layerTemplate_.value(QStringLiteral("name")).toString(), lt);
          if (auto* svc = ArtifactProjectService::instance()) {
            svc->addLayerToCurrentComposition(params, true, false);
            updateLayout();
          }
        });
      }
    }

    // T: 整理メニュー
    {
      QMenu* cleanMenu = allMenu->addMenu(QStringLiteral("整理"));
      cleanMenu->addAction(QStringLiteral("未使用レイヤーを削除"), [this]() {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (!comp) return;
        auto* svc = ArtifactProjectService::instance();
        if (!svc) return;
        QVector<LayerID> toRemove;
        for (const auto& l : comp->allLayer()) {
          if (!l) continue;
          if (auto video = std::dynamic_pointer_cast<ArtifactVideoLayer>(l)) {
            if (video->sourcePath().trimmed().isEmpty()) toRemove.push_back(l->id());
          } else if (auto img = std::dynamic_pointer_cast<ArtifactImageLayer>(l)) {
            if (img->sourcePath().trimmed().isEmpty()) toRemove.push_back(l->id());
          }
        }
        for (const auto& id : toRemove) {
          svc->removeLayerFromComposition(comp->id(), id);
        }
        if (!toRemove.isEmpty()) updateLayout();
      });
      cleanMenu->addAction(QStringLiteral("空のマスクを削除"), [this]() {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (!comp) return;
        for (const auto& l : comp->allLayer()) {
          if (!l) continue;
          for (int mi = l->maskCount() - 1; mi >= 0; --mi) {
            if (l->mask(mi).maskPathCount() == 0) l->removeMask(mi);
          }
        }
        updateLayout();
      });
    }

    // ブレンドモードお気に入り管理
    {
      QMenu* bfMenu = allMenu->addMenu(QStringLiteral("ブレンドモードお気に入り"));
      for (std::size_t bi = 0; bi < blendModeCount; ++bi) {
        const auto mode = static_cast<BlendMode>(bi);
        QAction* bfa = bfMenu->addAction(blendModeDisplayName(mode), [this, bi]() {
          if (impl_->blendModeFavorites.contains(static_cast<int>(bi))) {
            impl_->blendModeFavorites.remove(static_cast<int>(bi));
          } else {
            impl_->blendModeFavorites.insert(static_cast<int>(bi));
          }
        });
        bfa->setCheckable(true);
        bfa->setChecked(impl_->blendModeFavorites.contains(static_cast<int>(bi)));
      }
    }

    QMenu* selectedLayerMenu = allMenu->addMenu(QStringLiteral("選択レイヤー"));
    selectedLayerMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_select_all.svg")));
    buildSelectedLayerMenu(selectedLayerMenu, layer, triggerOpenInspector, triggerOpenProperties,
                           triggerSelectedVisibility, triggerSelectedLock, triggerSelectedSolo,
                           triggerSelectedShy, triggerSelectParent, triggerClearParent,
                           triggerRenameLayer, triggerDuplicateLayer, triggerDeleteLayer,
                           triggerPrecomposeSelectedLayers);

    if (selectedIds.size() > 1) {
      QMenu* selectedBatchMenu = addIconMenu(selectedLayerMenu, QStringLiteral("一括操作"), QStringLiteral("Studio/layermenu_group.svg"));
      addIconAction(selectedBatchMenu, QStringLiteral("複製"), QStringLiteral("Studio/layermenu_content_copy.svg"), [triggerDuplicateSelectedLayers]() {
        triggerDuplicateSelectedLayers();
      });
      addIconAction(selectedBatchMenu, QStringLiteral("グループ化..."), QStringLiteral("Studio/layermenu_group.svg"), [triggerGroupSelectedLayers]() {
        triggerGroupSelectedLayers();
      });
      selectedBatchMenu->addSeparator();
      addIconAction(selectedBatchMenu, QStringLiteral("表示"), QStringLiteral("Studio/layermenu_visibility.svg"), [triggerSelectedVisibility]() {
        triggerSelectedVisibility(true);
      });
      addIconAction(selectedBatchMenu, QStringLiteral("非表示"), QStringLiteral("Studio/visibility_off.svg"), [triggerSelectedVisibility]() {
        triggerSelectedVisibility(false);
      });
      addIconAction(selectedBatchMenu, QStringLiteral("ロック"), QStringLiteral("Studio/layermenu_lock.svg"), [triggerSelectedLock]() {
        triggerSelectedLock(true);
      });
      addIconAction(selectedBatchMenu, QStringLiteral("ロック解除"), QStringLiteral("Studio/lock_open.svg"), [triggerSelectedLock]() {
        triggerSelectedLock(false);
      });
      addIconAction(selectedBatchMenu, QStringLiteral("ソロ"), QStringLiteral("Studio/layermenu_solo_only.svg"), [triggerSelectedSolo]() {
        triggerSelectedSolo(true);
      });
      addIconAction(selectedBatchMenu, QStringLiteral("ソロ解除"), QStringLiteral("Studio/layermenu_solo_only.svg"), [triggerSelectedSolo]() {
        triggerSelectedSolo(false);
      });
      addIconAction(selectedBatchMenu, QStringLiteral("シャイ"), QStringLiteral("Studio/layermenu_shy.svg"), [triggerSelectedShy]() {
        triggerSelectedShy(true);
      });
      addIconAction(selectedBatchMenu, QStringLiteral("シャイ解除"), QStringLiteral("Studio/timeline_switch_shy.svg"), [triggerSelectedShy]() {
        triggerSelectedShy(false);
      });
    }

    QMenu* selectionStateMenu = addIconMenu(selectedLayerMenu, QStringLiteral("選択状態"), QStringLiteral("Studio/editmenu_select_all.svg"));
    addIconAction(selectionStateMenu, QStringLiteral("全選択"), QStringLiteral("Studio/editmenu_select_all.svg"), [this]() {
      auto comp = safeCompositionLookup(impl_->compositionId);
      if (!comp) {
        return;
      }
      QVector<LayerID> allIds;
      allIds.reserve(comp->allLayer().size());
      for (const auto& l : comp->allLayer()) {
        if (l) {
          allIds.push_back(l->id());
        }
      }
      replaceSelectionWithIds(comp, allIds);
    });
    addIconAction(selectionStateMenu, QStringLiteral("選択反転"), QStringLiteral("Studio/editmenu_select_invert.svg"), [this, selectedIds]() {
      auto comp = safeCompositionLookup(impl_->compositionId);
      if (!comp) {
        return;
      }
      QSet<LayerID> selectedSet;
      for (const auto& id : selectedIds) {
        selectedSet.insert(id);
      }
      QVector<LayerID> inverted;
      for (const auto& l : comp->allLayer()) {
        if (l && !selectedSet.contains(l->id())) {
          inverted.push_back(l->id());
        }
      }
      replaceSelectionWithIds(comp, inverted);
    });
    addIconAction(selectionStateMenu, QStringLiteral("選択を解除"), QStringLiteral("Studio/editmenu_select_none.svg"), [this]() {
      replaceSelectionWithIds(safeCompositionLookup(impl_->compositionId), {});
    });
    selectedLayerMenu->addSeparator();
    QMenu* selectPatternMenu = addIconMenu(selectedLayerMenu, QStringLiteral("選択補助"), QStringLiteral("Studio/timemenu_step_forward.svg"));
    addIconAction(selectPatternMenu, QStringLiteral("2個おき"), QStringLiteral("Studio/timemenu_step_forward.svg"), [this, selectedVisibleIds]() {
      replaceSelectionWithIds(safeCompositionLookup(impl_->compositionId),
                              layerIdsWithStride(selectedVisibleIds, 2, 0));
    });
    addIconAction(selectPatternMenu, QStringLiteral("3個おき"), QStringLiteral("Studio/timemenu_step_backward.svg"), [this, selectedVisibleIds]() {
      replaceSelectionWithIds(safeCompositionLookup(impl_->compositionId),
                              layerIdsWithStride(selectedVisibleIds, 3, 0));
    });
    selectPatternMenu->addSeparator();
    addIconAction(selectPatternMenu, QStringLiteral("偶数"), QStringLiteral("Studio/viewmenu_grid_on.svg"), [this, selectedVisibleIds]() {
      replaceSelectionWithIds(safeCompositionLookup(impl_->compositionId),
                              layerIdsWithStride(selectedVisibleIds, 2, 1));
    });
    addIconAction(selectPatternMenu, QStringLiteral("奇数"), QStringLiteral("Studio/viewmenu_grid_view.svg"), [this, selectedVisibleIds]() {
      replaceSelectionWithIds(safeCompositionLookup(impl_->compositionId),
                              layerIdsWithStride(selectedVisibleIds, 2, 0));
    });
    selectPatternMenu->addSeparator();
    addIconAction(selectPatternMenu, QStringLiteral("同名だけ"), QStringLiteral("Studio/layermenu_content_copy.svg"), [this, selectedIds]() {
      auto comp = safeCompositionLookup(impl_->compositionId);
      replaceSelectionWithIds(comp, layerIdsWithSameName(comp, selectedIds));
    });
    addIconAction(selectPatternMenu, QStringLiteral("同種だけ"), QStringLiteral("Studio/select_same_type.svg"), [this, selectedVisibleIds]() {
      auto comp = safeCompositionLookup(impl_->compositionId);
      if (!comp || selectedVisibleIds.isEmpty()) return;
      const auto kind = layerIconKindForLayer(comp->layerById(selectedVisibleIds.first()));
      QVector<LayerID> matching;
      for (const auto& l : comp->allLayer()) {
        if (l && layerIconKindForLayer(l) == kind) {
          matching.push_back(l->id());
        }
      }
      replaceSelectionWithIds(comp, matching);
    });
    QAction *chosenAction = menu.exec(event->globalPos());
    if (chosenAction) {
      const QVariantMap data = chosenAction->data().toMap();
      const QString kind = data.value(QStringLiteral("kind")).toString();
      bool matteIndexOk = false;
      const int matteIndex = data.value(QStringLiteral("index")).toInt(&matteIndexOk);
      if (!matteIndexOk) {
        return;
      }
      if (kind == QStringLiteral("matte_add_selected")) {
        const auto selectedLayerId =
            LayerID(data.value(QStringLiteral("selectedLayerId")).toString());
        if (selectedLayerId.isNil()) {
          return;
        }
        auto beforeRefs = matteRefs;
        auto afterRefs = beforeRefs;
        afterRefs.push_back(makeDefaultMatteReference(selectedLayerId));
        auto* cmd = new ChangeLayerMatteReferencesCommand(layer,
                                                          std::move(beforeRefs),
                                                          std::move(afterRefs));
        UndoManager::instance()->push(std::unique_ptr<ChangeLayerMatteReferencesCommand>(cmd));
        updateLayout();
      } else if (kind == QStringLiteral("matte_focus")) {
        if (matteIndex >= 0 && matteIndex < static_cast<int>(matteRefs.size())) {
          const auto &ref = matteRefs[matteIndex];
          if (!ref.sourceLayerId.isNil()) {
            if (auto *svc = ArtifactProjectService::instance()) {
              svc->selectLayer(ref.sourceLayerId);
            }
          }
        }
      } else if (kind == QStringLiteral("matte_type")) {
        bool matteTypeOk = false;
        const int matteTypeIndex = data.value(QStringLiteral("type")).toInt(&matteTypeOk);
        if (!matteTypeOk) {
          return;
        }
        if (comp && matteIndex >= 0 && matteIndex < static_cast<int>(matteRefs.size()) &&
            matteTypeIndex >= 0 && matteTypeIndex <= static_cast<int>(MatteType::InverseLuma)) {
          if (applyMatteTypeToLayer(comp, layer, matteIndex,
                                    static_cast<MatteType>(matteTypeIndex))) {
            updateLayout();
          }
        }
      }
    }

    event->accept();
    return;
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
  const int nameStartX = [this]() {
    int x = 0;
    for (int i = 0; i < kLayerPropertyColumnCount; ++i) if (impl_->columnVisible_[i]) x += impl_->columnWidths_[i];
    return x;
  }();
  const int idx = impl_->rowIndexFromViewportY(event->pos().y());
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
   if (row.kind != RowKind::Layer) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }

  if (row.hasChildren) {
    const int nameX = nameStartX + row.depth * 14;
    const QRect treeHitRect(nameX, impl_->rowViewportY(idx), std::max(40, width() - nameX), rowH);
    if (treeHitRect.contains(event->pos())) {
    const QString idStr = layer->id().toString();
    impl_->expandedByLayerId[idStr] = !impl_->expandedByLayerId.value(idStr, true);
    updateLayout();
    event->accept();
    return;
    }
   }

  const bool showInlineCombos = width() >= (nameStartX + kInlineComboReserve + kLayerNameMinWidth);
  const int parentRectX = width() - kInlineComboReserve;
  const int nameX = nameStartX + row.depth * 14 + (row.hasChildren ? 16 : 4);
  const int nameWidth = showInlineCombos ? std::max(20, parentRectX - nameX - 8) : std::max(20, width() - nameX - 8);
  const QRect editRect(nameX + 2, impl_->rowViewportY(idx) + 2, nameWidth, rowH - 4);

  if (auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer.get())) {
    const CompositionID sourceCompId = compLayer->sourceCompositionId();
    if (!sourceCompId.isNil()) {
      if (auto *svc = ArtifactProjectService::instance()) {
        if (svc->changeCurrentComposition(sourceCompId).success) {
          event->accept();
          return;
        }
      }
    }
  }

   if (!impl_->layerNameEditable) {
    event->accept();
    return;
   }

  if (!editRect.contains(event->pos())) {
   QWidget::mouseDoubleClickEvent(event);
   return;
  }

  impl_->clearInlineEditors();
  auto* editor = new QLineEdit(layer->layerName(), this);
  editor->setGeometry(editRect);
  applyLayerPanelLineEditPalette(editor);
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
  // 列幅ドラッグ中
  if (impl_->dragCol_ >= 0 && (event->buttons() & Qt::LeftButton)) {
    const int delta = event->pos().x() - impl_->dragStartX_;
    const int newColW = std::max(16, impl_->dragStartWidths_[impl_->dragCol_] + delta);
    const int nextIdx = impl_->dragCol_ + 1;
    if (nextIdx < kLayerPropertyColumnCount) {
      const int nextNewW = std::max(16, impl_->dragStartWidths_[nextIdx] - delta);
      impl_->columnWidths_[impl_->dragCol_] = newColW;
      impl_->columnWidths_[nextIdx] = nextNewW;
    }
    update();
    event->accept();
    return;
  }

  if ((event->buttons() & Qt::LeftButton) && !impl_->dragCandidateLayerId.isNil()) {
    const int dragDistance = (event->pos() - impl_->dragStartPos).manhattanLength();
    if (!impl_->dragStarted_ && dragDistance >= QApplication::startDragDistance()) {
      impl_->dragStarted_ = true;
      impl_->draggedLayerId = impl_->dragCandidateLayerId;
      impl_->dragMatteLinkMode = (event->modifiers() & Qt::AltModifier) &&
                                 !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier));
    }
    if (impl_->dragStarted_) {
      if (impl_->dragMatteLinkMode) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const auto sourceLayer = comp ? comp->layerById(impl_->draggedLayerId) : ArtifactAbstractLayerPtr{};
        impl_->dragMatteHoverVisibleRow = impl_->rowIndexFromViewportY(event->pos().y());
        impl_->dragInsertVisibleRow = -1;
        const bool validDropTarget =
            impl_->dragMatteHoverVisibleRow >= 0 &&
            impl_->dragMatteHoverVisibleRow < impl_->visibleRows.size() &&
            impl_->visibleRows[impl_->dragMatteHoverVisibleRow].layer &&
            !matteDropWouldCreateCycle(comp,
                                       impl_->visibleRows[impl_->dragMatteHoverVisibleRow].layer,
                                       sourceLayer ? sourceLayer->id() : LayerID());
        setCursor(validDropTarget ? Qt::CrossCursor : Qt::ForbiddenCursor);
      } else {
        impl_->dragInsertVisibleRow = impl_->insertionVisibleRowForY(static_cast<int>(event->pos().y() + impl_->verticalOffset));
        impl_->dragMatteHoverVisibleRow = -1;
        setCursor(Qt::DragMoveCursor);
      }
      update();
      event->accept();
      return;
    }
  }

  int idx = impl_->rowIndexFromViewportY(event->pos().y());
  if (idx != impl_->hoveredLayerIndex) {
    const int previousHoveredIndex = impl_->hoveredLayerIndex;
    impl_->hoveredLayerIndex = idx;
    if (previousHoveredIndex >= 0 && previousHoveredIndex < impl_->visibleRows.size()) {
      update(0, impl_->rowViewportY(previousHoveredIndex), width(), kLayerRowHeight);
    }
    if (idx >= 0 && idx < impl_->visibleRows.size()) {
      update(0, impl_->rowViewportY(idx), width(), kLayerRowHeight);
    }
  }
  // M: 列区切り線ホバー（非表示列をスキップ）
  if (impl_->dragCol_ < 0 && idx >= 0 && idx < impl_->visibleRows.size()) {
    const auto& hoverRow = impl_->visibleRows[idx];
    if (hoverRow.kind == RowKind::Layer) {
      int cumCX = 0;
      for (int ci = 0; ci < kLayerPropertyColumnCount - 1; ++ci) {
        cumCX += impl_->columnWidths_[ci];
        if (!impl_->columnVisible_[ci]) continue;
        if (std::abs(event->pos().x() - cumCX) <= kColumnDividerDragMargin) {
          setCursor(Qt::SplitHCursor);
          event->accept();
          return;
        }
      }
    }
  }

  bool pointer = event->pos().x() < [this]() {
    int x = 0;
    for (int i = 0; i < kLayerPropertyColumnCount; ++i) if (impl_->columnVisible_[i]) x += impl_->columnWidths_[i];
    return x;
  }();
  const int colNameStartX = [this]() {
    int x = 0;
    for (int i = 0; i < kLayerPropertyColumnCount; ++i) if (impl_->columnVisible_[i]) x += impl_->columnWidths_[i];
    return x;
  }();
  if (!pointer && idx >= 0 && idx < impl_->visibleRows.size()) {
    const auto& row = impl_->visibleRows[idx];
    if (row.hasChildren) {
      const int nameStartX = colNameStartX;
      const int indent = 14;
      const int toggleSize = 10;
      const int toggleX = nameStartX + row.depth * indent + 2;
      const QRect toggleRect(toggleX, impl_->rowViewportY(idx) + (kLayerRowHeight - toggleSize) / 2, toggleSize, toggleSize);
      pointer = toggleRect.contains(event->pos());
    }
    if (!pointer && row.kind == RowKind::Property) {
      const QRect keyframeRect =
          propertyKeyframeMarkerRect(width(), impl_->rowViewportY(idx), kLayerRowHeight);
      pointer = keyframeRect.contains(event->pos());
    }
  }

  QString toolTipText;
  if (idx >= 0 && idx < impl_->visibleRows.size()) {
    const auto& row = impl_->visibleRows[idx];
    if (row.layer && row.kind == RowKind::Layer) {
      const auto mattes = row.layer->matteReferences();
      if (!mattes.empty()) {
        QStringList parts;
        parts.reserve(static_cast<int>(mattes.size()));
        for (size_t i = 0; i < mattes.size(); ++i) {
          parts << matteSummaryLabel(mattes[i], static_cast<int>(i));
        }
        toolTipText = QStringLiteral("Track Mattes: %1").arg(parts.join(QStringLiteral(" | ")));
      } else if (!impl_->dragStarted_) {
        toolTipText = QStringLiteral("Alt-drag a layer here to create a Track Matte link");
      }
    } else if (impl_->dragStarted_ && impl_->dragMatteLinkMode &&
               row.kind == RowKind::MatteStack) {
      toolTipText = QStringLiteral("Drop here to set the Track Matte source");
    }
    if (impl_->dragStarted_ && impl_->dragMatteLinkMode && row.layer) {
      auto comp = safeCompositionLookup(impl_->compositionId);
      const auto sourceLayer = comp ? comp->layerById(impl_->draggedLayerId) : ArtifactAbstractLayerPtr{};
      const QString sourceName = sourceLayer ? sourceLayer->layerName() : QStringLiteral("<unknown>");
      const QString targetName = row.layer ? row.layer->layerName() : QStringLiteral("<unknown>");
      if (matteDropWouldCreateCycle(comp, row.layer, sourceLayer ? sourceLayer->id() : LayerID())) {
        toolTipText = QStringLiteral("Track Matte drop blocked: cycle would be created\nSource: %1\nTarget: %2")
                          .arg(sourceName, targetName);
      } else {
        toolTipText = QStringLiteral("Track Matte link\nSource: %1\nTarget: %2")
                          .arg(sourceName, targetName);
      }
    }
  }
  setToolTip(toolTipText);
  setCursor(pointer ? Qt::PointingHandCursor : Qt::ArrowCursor);
 }

 void ArtifactLayerPanelWidget::mouseReleaseEvent(QMouseEvent* event)
 {
  if (impl_->dragCol_ >= 0) {
    impl_->dragCol_ = -1;
    impl_->dragStartWidths_.clear();
    unsetCursor();
    event->accept();
    return;
  }
  if (event->button() == Qt::LeftButton) {
    if (impl_->dragStarted_ && !impl_->draggedLayerId.isNil()) {
      auto* svc = ArtifactProjectService::instance();
      auto comp = safeCompositionLookup(impl_->compositionId);
      const LayerID dragLayerId = impl_->draggedLayerId;
      if (svc && comp && !dragLayerId.isNil()) {
        if (impl_->dragMatteLinkMode) {
          const int targetVisibleRow = impl_->dragMatteHoverVisibleRow;
          if (targetVisibleRow >= 0 && targetVisibleRow < impl_->visibleRows.size()) {
            const auto& targetRow = impl_->visibleRows[targetVisibleRow];
            auto targetLayer = targetRow.layer;
            auto sourceLayer = comp->layerById(dragLayerId);
            if (targetLayer && sourceLayer && !matteDropWouldCreateCycle(comp, targetLayer, dragLayerId)) {
              auto beforeRefs = targetLayer->matteReferences();
              auto afterRefs = beforeRefs;

              const auto replaceSource = [dragLayerId](LayerMatteReference ref) {
                ref.sourceLayerId = dragLayerId;
                ref.enabled = true;
                return ref;
              };

              bool changed = false;
              if (targetRow.kind == RowKind::Matte) {
                bool ok = false;
                const int matteIndex = targetRow.propertyPath.trimmed().toInt(&ok);
                if (ok && matteIndex >= 0 && matteIndex < static_cast<int>(afterRefs.size())) {
                  auto updated = afterRefs[matteIndex];
                  updated = replaceSource(updated);
                  if (updated.sourceLayerId != beforeRefs[matteIndex].sourceLayerId ||
                      updated.enabled != beforeRefs[matteIndex].enabled) {
                    afterRefs[matteIndex] = updated;
                    changed = true;
                  }
                }
              } else if (targetRow.kind == RowKind::Layer || targetRow.kind == RowKind::MatteStack) {
                if (afterRefs.empty()) {
                  afterRefs.push_back(makeDefaultMatteReference(dragLayerId));
                  changed = true;
                } else {
                  auto updated = replaceSource(afterRefs.front());
                  if (updated.sourceLayerId != beforeRefs.front().sourceLayerId ||
                      updated.enabled != beforeRefs.front().enabled) {
                    afterRefs.front() = updated;
                    changed = true;
                  }
                }
              }

              if (changed) {
                auto* cmd = new ChangeLayerMatteReferencesCommand(targetLayer, std::move(beforeRefs), std::move(afterRefs));
                UndoManager::instance()->push(std::unique_ptr<ChangeLayerMatteReferencesCommand>(cmd));
                updateLayout();
              }
            }
          }
        } else {
          QVector<LayerID> visibleLayerIds;
          visibleLayerIds.reserve(impl_->visibleRows.size());
          for (const auto& row : impl_->visibleRows) {
            if (row.kind == RowKind::Layer && row.layer) {
              visibleLayerIds.push_back(row.layer->id());
            }
          }
          const auto allLayers = comp->allLayer();
          int oldIndex = -1;
          for (int i = 0; i < allLayers.size(); ++i) {
            if (allLayers[i] && allLayers[i]->id() == dragLayerId) {
              oldIndex = i;
              break;
            }
          }
          if (oldIndex >= 0 && !visibleLayerIds.isEmpty()) {
            QVector<LayerID> remainingVisibleLayerIds;
            remainingVisibleLayerIds.reserve(visibleLayerIds.size());
            for (const auto& layerId : visibleLayerIds) {
              if (layerId != dragLayerId) {
                remainingVisibleLayerIds.push_back(layerId);
              }
            }
            const int targetVisibleIndex = std::clamp(
              impl_->layerCountBeforeVisibleRowExcluding(impl_->dragInsertVisibleRow, dragLayerId),
              0,
              static_cast<int>(remainingVisibleLayerIds.size()));
            int newIndex = static_cast<int>(remainingVisibleLayerIds.size()) -
                           targetVisibleIndex;
            newIndex = std::clamp(newIndex, 0, std::max(0, static_cast<int>(allLayers.size()) - 1));
            if (newIndex != oldIndex) {
              auto layer = comp->layerById(dragLayerId);
              auto* cmd = new MoveLayerIndexCommand(comp, layer, oldIndex, newIndex);
              UndoManager::instance()->push(std::unique_ptr<MoveLayerIndexCommand>(cmd));
              updateLayout();
            }
          }
        }
      }
      impl_->clearDragState();
      unsetCursor();
      update();
      event->accept();
      return;
    }
  }

  impl_->clearDragState();
  unsetCursor();
  setToolTip(QString());
  QWidget::mouseReleaseEvent(event);
 }

void ArtifactLayerPanelWidget::focusInEvent(QFocusEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    input->setActiveContext(QString::fromLatin1(kLayerPanelContext));
  }
  QWidget::focusInEvent(event);
}

void ArtifactLayerPanelWidget::focusOutEvent(QFocusEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    if (input->activeContext() == QString::fromLatin1(kLayerPanelContext)) {
      input->setActiveContext(QStringLiteral("Global"));
    }
  }
  QWidget::focusOutEvent(event);
}

void ArtifactLayerPanelWidget::keyPressEvent(QKeyEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    input->setActiveContext(QString::fromLatin1(kLayerPanelContext));
    if (event && input->processKeyPress(this, event->key(), event->modifiers())) {
      event->accept();
      return;
    }
  }

  if ((event->modifiers() & Qt::ControlModifier) &&
      !(event->modifiers() & (Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier))) {
    if (event->key() == Qt::Key_Z) {
      if (auto *mgr = UndoManager::instance()) {
        mgr->undo();
        event->accept();
        return;
      }
    } else if (event->key() == Qt::Key_Y) {
      if (auto *mgr = UndoManager::instance()) {
        mgr->redo();
        event->accept();
        return;
      }
    } else if (event->key() == Qt::Key_D) {
      if (auto* svc = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          const QVector<LayerID> selectedIds =
              selectedLayerIdsInVisibleOrder(impl_->visibleRows);
          if (!selectedIds.isEmpty()) {
            for (const auto& layerId : selectedIds) {
              svc->duplicateLayerInCurrentComposition(layerId);
            }
            updateLayout();
            event->accept();
            return;
          }
        }
      }
    } else if (event->key() == Qt::Key_G) {
      if (auto* svc = ArtifactProjectService::instance()) {
        if (svc->groupSelectedLayersWithUndo()) {
          updateLayout();
          event->accept();
          return;
        }
      }
    } else if (event->key() == Qt::Key_I) {
      auto comp = safeCompositionLookup(impl_->compositionId);
      auto* sel = currentLayerSelectionManager();
      if (comp && sel) {
        sel->clearSelection();
        for (const auto& row : impl_->visibleRows) {
          if (row.kind == RowKind::Layer && row.layer) {
            const bool currentlySelected = isLayerSelectedInSelectionManager(row.layer->id());
            if (!currentlySelected) {
              sel->addToSelection(comp->layerById(row.layer->id()));
            }
          }
        }
        update();
        event->accept();
        return;
      }
    } else if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
      const int bookmark = event->key() - Qt::Key_1 + 1;
      if (!impl_->selectedLayerId.isNil()) {
        impl_->layerBookmarks[bookmark] = impl_->selectedLayerId;
      }
      event->accept();
      return;
    } else if (event->key() == Qt::Key_F) {
      bool ok = false;
      const QString term = QInputDialog::getText(this,
          QStringLiteral("レイヤー検索"),
          QStringLiteral("名前またはプロパティ値を入力:"),
          QLineEdit::Normal, impl_->filterText, &ok);
      if (ok && !term.trimmed().isEmpty()) {
        impl_->searchInProperties_ = true;
        setFilterText(term.trimmed());
      } else if (ok) {
        impl_->searchInProperties_ = false;
        setFilterText(QString());
      }
      event->accept();
      return;
    }
  }

  auto moveSelectedLayerBy = [this](int delta) -> bool {
    if (impl_->selectedLayerId.isNil()) {
      return false;
    }

    auto* service = ArtifactProjectService::instance();
    auto comp = service ? service->currentComposition().lock() : nullptr;
    if (!service || !comp) {
      return false;
    }

    const auto layers = comp->allLayer();
    int currentLayerIndex = -1;
    for (int i = 0; i < layers.size(); ++i) {
      if (layers[i] && layers[i]->id() == impl_->selectedLayerId) {
        currentLayerIndex = i;
        break;
      }
    }
    if (currentLayerIndex < 0) {
      return false;
    }

    const int newIndex = std::clamp(currentLayerIndex + delta, 0, static_cast<int>(layers.size()) - 1);
    if (newIndex == currentLayerIndex) {
      return true;
    }

    if (!service->moveLayerInCurrentComposition(impl_->selectedLayerId, newIndex)) {
      return false;
    }

    updateLayout();
    if (auto* svc = ArtifactProjectService::instance()) {
      svc->selectLayer(impl_->selectedLayerId);
    }
    return true;
  };

  // W: Ctrl+Shift+↑↓ 複製＋移動
  if ((event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) == (Qt::ControlModifier | Qt::ShiftModifier) &&
      !(event->modifiers() & (Qt::AltModifier | Qt::MetaModifier))) {
    if ((event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) && !impl_->selectedLayerId.isNil()) {
      if (auto* svc = ArtifactProjectService::instance()) {
        if (svc->duplicateLayerInCurrentComposition(impl_->selectedLayerId)) {
          // Composition indices grow toward the front, while timeline rows
          // are displayed top-to-bottom in reverse composition order.
          const int dir = (event->key() == Qt::Key_Up) ? +1 : -1;
          moveSelectedLayerBy(dir);
          updateLayout();
          event->accept();
          return;
        }
      }
    }
  }

  if (event->matches(QKeySequence::SelectAll)) {
    auto* selectionManager = currentLayerSelectionManager();
    auto comp = safeCompositionLookup(impl_->compositionId);
    if (selectionManager && comp) {
      QVector<LayerID> visibleLayerIds;
      visibleLayerIds.reserve(impl_->visibleRows.size());
      for (const auto& row : impl_->visibleRows) {
        if (row.kind == RowKind::Layer && row.layer) {
          visibleLayerIds.push_back(row.layer->id());
        }
      }

      if (!visibleLayerIds.isEmpty()) {
        selectionManager->clearSelection();
        for (const auto& layerId : visibleLayerIds) {
          selectionManager->addToSelection(comp->layerById(layerId));
        }
        impl_->selectedMaskLayerId = LayerID();
        impl_->selectedMaskIndex = -1;
        impl_->selectedLayerId = visibleLayerIds.last();
        impl_->selectionAnchorLayerId = visibleLayerIds.first();
        impl_->currentPropertyPath.clear();
        propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
        update();
        event->accept();
        return;
      }
    }
  }

  auto toggleSelectedSolo = [this]() {
    if (impl_->selectedLayerId.isNil()) return;
    auto comp = safeCompositionLookup(impl_->compositionId);
    if (!comp) return;
    auto layer = comp->layerById(impl_->selectedLayerId);
    if (!layer) return;
    UndoManager::instance()->push(std::make_unique<SetLayerSoloCommand>(layer, !layer->isSolo()));
    updateLayout();
  };
  auto toggleSelectedLock = [this]() {
    if (impl_->selectedLayerId.isNil()) return;
    auto comp = safeCompositionLookup(impl_->compositionId);
    if (!comp) return;
    auto layer = comp->layerById(impl_->selectedLayerId);
    if (!layer) return;
    UndoManager::instance()->push(std::make_unique<SetLayerLockCommand>(layer, !layer->isLocked()));
    updateLayout();
  };
  auto toggleSelectedShy = [this]() {
    if (impl_->selectedLayerId.isNil()) return;
    auto comp = safeCompositionLookup(impl_->compositionId);
    if (!comp) return;
    auto layer = comp->layerById(impl_->selectedLayerId);
    if (!layer) return;
    UndoManager::instance()->push(std::make_unique<SetLayerShyCommand>(layer, !layer->isShy()));
    updateLayout();
  };

  if (!(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier))) {
    if (event->key() == Qt::Key_S) {
      toggleSelectedSolo();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_L) {
      toggleSelectedLock();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_P) {
      toggleSelectedShy();
      event->accept();
      return;
    }
  }

  if (event->key() == Qt::Key_U &&
      !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier))) {
    const auto nextMode =
        impl_->displayMode == TimelineLayerDisplayMode::ImportantAndKeyframed
            ? TimelineLayerDisplayMode::AllLayers
            : TimelineLayerDisplayMode::ImportantAndKeyframed;
    setDisplayMode(nextMode);
    event->accept();
    return;
  }

  if (ArtifactCore::ShortcutBindings::instance().matches(
          event, ArtifactCore::ShortcutId::LayerDeleteSelected) ||
      event->key() == Qt::Key_Backspace) {
    if (!impl_->selectedMaskLayerId.isNil() &&
        impl_->selectedMaskIndex >= 0 &&
        impl_->selectedMaskLayerId == impl_->selectedLayerId) {
      auto comp = safeCompositionLookup(impl_->compositionId);
      const CompositionID compId = comp ? comp->id() : impl_->compositionId;
      auto layer = comp ? comp->layerById(impl_->selectedLayerId) : ArtifactAbstractLayerPtr{};
      if (!compId.isNil() && layer &&
          impl_->selectedMaskIndex < layer->maskCount()) {
        std::vector<LayerMask> beforeMasks;
        beforeMasks.reserve(static_cast<size_t>(layer->maskCount()));
        for (int i = 0; i < layer->maskCount(); ++i) {
          beforeMasks.push_back(layer->mask(i));
        }

        layer->removeMask(impl_->selectedMaskIndex);

        std::vector<LayerMask> afterMasks;
        afterMasks.reserve(static_cast<size_t>(layer->maskCount()));
        for (int i = 0; i < layer->maskCount(); ++i) {
          afterMasks.push_back(layer->mask(i));
        }

        if (auto *undo = UndoManager::instance()) {
          undo->push(std::make_unique<MaskEditCommand>(layer, std::move(beforeMasks),
                                                       std::move(afterMasks)));
        }
        const int nextMaskCount = layer->maskCount();
        if (nextMaskCount > 0) {
          const int nextMaskIndex =
              std::min(impl_->selectedMaskIndex, nextMaskCount - 1);
          impl_->focusMaskSelection(layer, nextMaskIndex);
          propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
        } else {
          impl_->clearMaskSelection();
          impl_->currentPropertyPath.clear();
          propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
        }
        updateLayout();
        event->accept();
        return;
      }
    }
    QVector<LayerID> selectedIds = selectedLayerIdsSnapshot();
    if (selectedIds.isEmpty() && !impl_->selectedLayerId.isNil()) {
      selectedIds.push_back(impl_->selectedLayerId);
    }
    if (!selectedIds.isEmpty()) {
      if (auto* service = ArtifactProjectService::instance()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        const CompositionID compId = comp ? comp->id() : impl_->compositionId;
        if (!compId.isNil()) {
          if (auto* selectionManager = currentLayerSelectionManager()) {
            selectionManager->clearSelection();
          }
          impl_->clearMaskSelection();
          impl_->selectedLayerId = LayerID();
          impl_->currentPropertyPath.clear();
          propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
          impl_->clearDragState();
          for (const auto& layerId : selectedIds) {
            service->removeLayerFromComposition(compId, layerId);
          }
          updateLayout();
          event->accept();
          return;
        }
      }
    }
  }

  const QVector<LayerID> selectedIds = selectedLayerIdsSnapshot();
  auto* service = ArtifactProjectService::instance();
  auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};

  auto armMultiEditCycle = [this](MultiEditCycleMode mode, int presetCount) -> int {
    const auto now = std::chrono::steady_clock::now();
    const bool sameMode = impl_->multiEditMode == mode &&
        impl_->multiEditStartedAt.time_since_epoch().count() > 0 &&
        now - impl_->multiEditStartedAt <= std::chrono::milliseconds(kMultiEditCycleWindowMs);
    if (!sameMode) {
      impl_->multiEditMode = mode;
      impl_->multiEditPresetIndex = 0;
    } else {
      impl_->multiEditPresetIndex = (impl_->multiEditPresetIndex + 1) % presetCount;
    }
    impl_->multiEditStartedAt = now;
    return impl_->multiEditPresetIndex;
  };

  auto applyMultiEditPreset = [this, &comp, &service, &selectedIds](MultiEditCycleMode mode,
                                                                   int presetIndex) -> bool {
    bool changed = false;
    switch (mode) {
    case MultiEditCycleMode::Align:
      changed = applyAlignPreset(comp, selectedIds, presetIndex);
      break;
    case MultiEditCycleMode::Distribute:
      changed = applyDistributePreset(comp, selectedIds, presetIndex);
      break;
    case MultiEditCycleMode::Shuffle:
      changed = shuffleSelectedLayers(comp, selectedIds, presetIndex);
      break;
    case MultiEditCycleMode::None:
      break;
    }
    if (changed) {
      if (service) {
        service->selectLayer(selectedIds.isEmpty() ? LayerID() : selectedIds.last());
      }
      updateLayout();
    }
    return changed;
  };

  if ((event->modifiers() & Qt::AltModifier) &&
      !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier))) {
    if (event->key() == Qt::Key_Up) {
      if (moveSelectedLayerBy(+1)) {
        event->accept();
        return;
      }
    } else if (event->key() == Qt::Key_Down) {
      if (moveSelectedLayerBy(-1)) {
        event->accept();
        return;
      }
    }
  }

  // J: Shift+↑↓ 範囲選択拡張
  if ((event->modifiers() & Qt::ShiftModifier) &&
      !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
      const bool forward = (event->key() == Qt::Key_Down);
      int selIdx = -1;
      for (int i = 0; i < impl_->visibleRows.size(); ++i) {
        if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
          selIdx = i;
          break;
        }
      }
      if (selIdx < 0) { event->accept(); return; }
      int targetIdx = -1;
      const int start = forward ? selIdx + 1 : 0;
      const int end = forward ? impl_->visibleRows.size() : selIdx;
      for (int i = start; i < end; ++i) {
        const int actualIdx = forward ? i : (end - 1 - (i - start));
        if (impl_->visibleRows[actualIdx].layer && impl_->visibleRows[actualIdx].kind == RowKind::Layer) {
          targetIdx = actualIdx;
          break;
        }
      }
      if (targetIdx < 0) { event->accept(); return; }
      auto comp = safeCompositionLookup(impl_->compositionId);
      auto* sel = currentLayerSelectionManager();
      if (comp && sel) {
        LayerID anchor = impl_->selectionAnchorLayerId;
        if (anchor.isNil()) anchor = impl_->selectedLayerId;
        const auto ids = visibleLayerIdsInRange(impl_->visibleRows, anchor,
                          impl_->visibleRows[targetIdx].layer->id());
        if (!ids.isEmpty()) {
          sel->clearSelection();
          for (const auto& id : ids) {
            sel->addToSelection(comp->layerById(id));
          }
          impl_->selectedLayerId = impl_->visibleRows[targetIdx].layer->id();
          update();
          event->accept();
          return;
        }
      }
    }
  }

  if (!(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier))) {
    if (event->key() == Qt::Key_Up) {
      int selIdx = -1;
      for (int i = 0; i < impl_->visibleRows.size(); ++i) {
        if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
          selIdx = i;
          break;
        }
      }
      if (selIdx > 0) {
        for (int i = selIdx - 1; i >= 0; --i) {
          if (impl_->visibleRows[i].layer && impl_->visibleRows[i].kind == RowKind::Layer) {
            if (auto* svc = ArtifactProjectService::instance()) {
              svc->selectLayer(impl_->visibleRows[i].layer->id());
              impl_->selectedLayerId = impl_->visibleRows[i].layer->id();
              update();
              event->accept();
              return;
            }
          }
        }
      }
    } else if (event->key() == Qt::Key_Down) {
      int selIdx = -1;
      for (int i = 0; i < impl_->visibleRows.size(); ++i) {
        if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
          selIdx = i;
          break;
        }
      }
      if (selIdx >= 0 && selIdx + 1 < impl_->visibleRows.size()) {
        for (int i = selIdx + 1; i < impl_->visibleRows.size(); ++i) {
          if (impl_->visibleRows[i].layer && impl_->visibleRows[i].kind == RowKind::Layer) {
            if (auto* svc = ArtifactProjectService::instance()) {
              svc->selectLayer(impl_->visibleRows[i].layer->id());
              impl_->selectedLayerId = impl_->visibleRows[i].layer->id();
              update();
              event->accept();
              return;
            }
          }
        }
      }
    } else if (event->key() == Qt::Key_I) {
      if (!impl_->selectedLayerId.isNil()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (comp) {
          auto layer = comp->layerById(impl_->selectedLayerId);
          if (layer) {
            if (auto* playback = ArtifactPlaybackService::instance()) {
              playback->goToFrame(layer->inPoint());
              event->accept();
              return;
            }
          }
        }
      }
    } else if (event->key() == Qt::Key_O) {
      if (!impl_->selectedLayerId.isNil()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (comp) {
          auto layer = comp->layerById(impl_->selectedLayerId);
          if (layer) {
            if (auto* playback = ArtifactPlaybackService::instance()) {
              playback->goToFrame(layer->outPoint());
              event->accept();
              return;
            }
          }
        }
      }
    } else if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
      const int bookmark = event->key() - Qt::Key_1 + 1;
      auto it = impl_->layerBookmarks.constFind(bookmark);
      if (it != impl_->layerBookmarks.constEnd() && !it->isNil()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (comp && comp->layerById(*it)) {
          if (auto* svc = ArtifactProjectService::instance()) {
            svc->selectLayer(*it);
            impl_->selectedLayerId = *it;
            update();
            event->accept();
            return;
          }
        }
      }
    } else if (event->key() == Qt::Key_A && selectedIds.size() >= 2) {
      const int presetIndex = armMultiEditCycle(MultiEditCycleMode::Align, 6);
      if (applyMultiEditPreset(MultiEditCycleMode::Align, presetIndex)) {
        event->accept();
        return;
      }
    }
    if (event->key() == Qt::Key_D && selectedIds.size() >= 3) {
      const int presetIndex = armMultiEditCycle(MultiEditCycleMode::Distribute, 6);
      if (applyMultiEditPreset(MultiEditCycleMode::Distribute, presetIndex)) {
        event->accept();
        return;
      }
    }
    if (event->key() == Qt::Key_R && selectedIds.size() >= 2) {
      const int presetIndex = armMultiEditCycle(MultiEditCycleMode::Shuffle, 4);
      if (applyMultiEditPreset(MultiEditCycleMode::Shuffle, presetIndex)) {
        event->accept();
        return;
      }
    }
  }

  // Ctrl + [ / ] でレイヤー順序を移動
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_BracketRight) {
      if (moveSelectedLayerBy(event->key() == Qt::Key_BracketLeft ? -1 : +1)) {
        event->accept();
        return;
      }
    }
  }

  // Shift + P で親レイヤーを選択
  if ((event->modifiers() & Qt::ShiftModifier) &&
      !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
    if (event->key() == Qt::Key_P) {
      if (!impl_->selectedLayerId.isNil()) {
        auto comp = safeCompositionLookup(impl_->compositionId);
        if (comp) {
          auto layer = comp->layerById(impl_->selectedLayerId);
          if (layer) {
            const QString parentId = layer->parentLayerId().toString();
            if (!parentId.isEmpty()) {
              auto parentLayer = comp->layerById(LayerID(parentId));
              if (parentLayer) {
                if (auto* svc = ArtifactProjectService::instance()) {
                  svc->selectLayer(parentLayer->id());
                  impl_->selectedLayerId = parentLayer->id();
                  update();
                  event->accept();
                  return;
                }
              }
            }
          }
        }
      }
    }
  }

  if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
   if ((impl_->multiEditMode == MultiEditCycleMode::Align ||
        impl_->multiEditMode == MultiEditCycleMode::Distribute) &&
       impl_->multiEditStartedAt.time_since_epoch().count() > 0 &&
       std::chrono::steady_clock::now() - impl_->multiEditStartedAt <=
           std::chrono::milliseconds(kMultiEditCycleWindowMs) &&
       selectedIds.size() >= 2) {
    nudgeSelectedLayerSpacing(comp, selectedIds, static_cast<Qt::Key>(event->key()));
    updateLayout();
    event->accept();
    return;
   }
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

  if (impl_->layerNameEditable && event->key() == Qt::Key_F2 && !impl_->inlineNameEditor) {
   int selectedIdx = -1;
   for (int i = 0; i < impl_->visibleRows.size(); ++i) {
    if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
     selectedIdx = i;
     break;
    }
   }
   if (selectedIdx >= 0) {
      const int y = impl_->rowViewportY(selectedIdx) + kLayerRowHeight / 2;
      const int x = [this]() {
        int x = 0;
        for (int i = 0; i < kLayerPropertyColumnCount; ++i) x += impl_->columnWidths_[i];
        return x;
      }() + 20;
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
  } else if (event->key() == Qt::Key_Escape) {
   if (auto* selectionManager = currentLayerSelectionManager()) {
    selectionManager->clearSelection();
   }
   impl_->selectedMaskLayerId = LayerID();
   impl_->selectedMaskIndex = -1;
   impl_->selectedLayerId = LayerID();
   impl_->currentPropertyPath.clear();
   propertyFocusChanged(impl_->selectedLayerId, impl_->currentPropertyPath);
   update();
   event->accept();
   return;
  }

  // Tab/Shift+Tab selection jump (outside modifier guard)
  if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
    const bool forward = (event->key() == Qt::Key_Tab);
    if (impl_->selectedLayerId.isNil()) {
      if (!impl_->visibleRows.isEmpty()) {
        for (const auto& r : impl_->visibleRows) {
          if (r.kind == RowKind::Layer && r.layer) {
            if (auto* svc = ArtifactProjectService::instance()) {
              svc->selectLayer(r.layer->id());
              impl_->selectedLayerId = r.layer->id();
              update();
              event->accept();
              return;
            }
          }
        }
      }
    } else {
      int curIdx = -1;
      for (int i = 0; i < impl_->visibleRows.size(); ++i) {
        if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
          curIdx = i;
          break;
        }
      }
      if (curIdx >= 0) {
        const int start = forward ? curIdx + 1 : 0;
        const int end = forward ? impl_->visibleRows.size() : curIdx;
        const int step = forward ? 1 : 1;
        for (int i = start; i < end; ++i) {
          if (impl_->visibleRows[i].kind == RowKind::Layer && impl_->visibleRows[i].layer) {
            if (auto* svc = ArtifactProjectService::instance()) {
              svc->selectLayer(impl_->visibleRows[i].layer->id());
              impl_->selectedLayerId = impl_->visibleRows[i].layer->id();
              update();
              event->accept();
              return;
            }
          }
        }
      }
    }
  }

  // S: / キーでフィルタ欄呼出
  if (!event->modifiers() && event->key() == Qt::Key_Slash) {
    bool ok = false;
    const QString term = QInputDialog::getText(this,
        QStringLiteral("レイヤーフィルタ"),
        QStringLiteral("フィルタ文字列:"),
        QLineEdit::Normal, impl_->filterText, &ok);
    if (ok) {
      impl_->searchInProperties_ = false;
      setFilterText(term.trimmed());
    }
    event->accept();
    return;
  }

  // F: Incremental search on printable chars
  if (!event->modifiers() && event->text().length() == 1 && event->text().at(0).isPrint()) {
    const QChar ch = event->text().at(0);
    impl_->incrementalSearchBuffer_.append(ch);
    impl_->incrementalSearchTimer_->start();
    setFilterText(impl_->incrementalSearchBuffer_);
    event->accept();
    return;
  }

  QWidget::keyPressEvent(event);
  }

void ArtifactLayerPanelWidget::wheelEvent(QWheelEvent* event)
{
   if (impl_->visibleRows.isEmpty()) {
    QWidget::wheelEvent(event);
    return;
   }

   int delta = 0;
   if (!event->pixelDelta().isNull()) {
    delta = event->pixelDelta().y();
   } else {
    delta = (event->angleDelta().y() / 120) * impl_->rowHeight;
    if (delta == 0) {
     delta = event->angleDelta().y() / 6;
    }
   }
   if (delta == 0) {
    QWidget::wheelEvent(event);
    return;
   }

   impl_->setVerticalOffset(impl_->verticalOffset - static_cast<double>(delta), this);
   event->accept();
}

void ArtifactLayerPanelWidget::leaveEvent(QEvent*)
{
  const int previousHoveredIndex = impl_->hoveredLayerIndex;
  impl_->hoveredLayerIndex = -1;
  if (previousHoveredIndex >= 0 && previousHoveredIndex < impl_->visibleRows.size()) {
    update(0, impl_->rowViewportY(previousHoveredIndex), width(), kLayerRowHeight);
  }
}

void ArtifactLayerPanelWidget::paintEvent(QPaintEvent* event)
{
  ArtifactCore::ProfileTimer _profTimer("LayerPanelPaint",
                                        ArtifactCore::ProfileCategory::UI);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  const int rowH = kLayerRowHeight;
  const int iconSize = 16;
  const int offset = (kLayerColumnWidth - iconSize) / 2;
  const int indent = 14;
  const int toggleSize = 10;
  
  const QRect dirtyRect = event->rect();
  const auto& theme = ArtifactCore::currentDCCTheme();
  const QColor background = themeColor(theme.secondaryBackgroundColor, QColor(QStringLiteral("#2A2A2A")));
  const QColor surface = themeColor(theme.backgroundColor, QColor(QStringLiteral("#25272D")));
  const QColor text = themeColor(theme.textColor, QColor(QStringLiteral("#DADADA")));
  const QColor accent = themeColor(theme.accentColor, QColor(QStringLiteral("#E4B76C")));
  const QColor selection = themeColor(theme.selectionColor, QColor(QStringLiteral("#4A515C")));
  const QColor border = themeColor(theme.borderColor, QColor(QStringLiteral("#1A1A1A")));

  p.fillRect(dirtyRect, background);

  RationalTime currentTime = impl_->currentTime;
  if (auto comp = safeCompositionLookup(impl_->compositionId)) {
    const auto fps = std::max(1.0, static_cast<double>(comp->frameRate().framerate()));
    currentTime = RationalTime(comp->framePosition().framePosition(), fps);
  }

  if (impl_->visibleRows.isEmpty()) {
    auto comp = safeCompositionLookup(impl_->compositionId);
    if (!comp) {
      p.setPen(text.darker(120));
      p.drawText(rect(), Qt::AlignCenter, "Open a composition to view layers");
      return;
    }
    p.setPen(text.darker(120));
    p.drawText(rect(), Qt::AlignCenter, "No layers yet");
    return;
  }

  const int nameStartX = [&]() {
    int x = 0;
    for (int i = 0; i < kLayerPropertyColumnCount; ++i) if (impl_->columnVisible_[i]) x += impl_->columnWidths_[i];
    return x;
  }();

  p.save();
  p.translate(0.0, -impl_->verticalOffset);

  // 可視範囲のみループを回す（仮想化）
  const int startRow = std::max(0, static_cast<int>(std::floor((dirtyRect.top() + impl_->verticalOffset) / rowH)));
  const int endRow = std::min(static_cast<int>(impl_->visibleRows.size() - 1),
                              static_cast<int>(std::floor((dirtyRect.bottom() + impl_->verticalOffset) / rowH)));

  for (int i = startRow; i <= endRow; ++i) {
    int y = i * rowH;
    const auto& row = impl_->visibleRows[i];
    auto l = row.layer;
    if (!l) continue;
    const bool isGroupRow = (row.kind == RowKind::Group ||
                            row.kind == RowKind::MaskStack ||
                            row.kind == RowKind::MatteStack);
    const bool isPropertyRow = (row.kind == RowKind::Property);
    const bool isDisplayLeafRow = (row.kind == RowKind::Mask || row.kind == RowKind::Matte);
    const bool sel = isLayerSelectedInSelectionManager(l->id());
    const bool layerSelected = sel && row.kind == RowKind::Layer;
    const bool selectionAnchor =
        layerSelected && !impl_->selectionAnchorLayerId.isNil() &&
        l->id() == impl_->selectionAnchorLayerId;
    const bool maskSelected = sel && row.kind == RowKind::Mask &&
                              impl_->selectedMaskLayerId == l->id() &&
                              impl_->selectedMaskIndex == row.propertyPath.trimmed().toInt();
    const QString propertyPath =
        (isPropertyRow && !row.propertyPath.trimmed().isEmpty())
            ? row.propertyPath
            : impl_->currentPropertyPath;
    const bool propertyFocused =
        isPropertyRow && propertyPath.compare(impl_->currentPropertyPath, Qt::CaseInsensitive) == 0;
    const auto property = isPropertyRow ? l->getProperty(row.propertyPath) : nullptr;
    const bool propertyAnimatable = property && property->isAnimatable();
    const bool propertyKeyframed = propertyAnimatable && property->hasKeyFrameAt(currentTime);

    const QColor rowBase = (i % 2 == 0) ? background : mixColor(background, surface, 0.25);
    const QColor rowHover = mixColor(background, text, 0.08);
    const QColor rowSelected = mixColor(background, accent, 0.48);
    if (propertyFocused) {
      p.fillRect(0, y, width(), rowH, mixColor(background, selection, 0.32));
    } else if (maskSelected) {
      p.fillRect(0, y, width(), rowH, mixColor(background, accent, 0.30));
    } else if (layerSelected) {
      QColor selectedEdge = mixColor(accent, text, 0.15);
      selectedEdge.setAlpha(220);
      p.fillRect(0, y, width(), rowH, rowSelected); // Stronger amber selection
      p.fillRect(0, y, 4, rowH, selectedEdge);
      p.fillRect(0, y, width(), 1, selectedEdge);
      p.fillRect(0, y + rowH - 1, width(), 1, selectedEdge.darker(110));
    }
    else if (i == impl_->hoveredLayerIndex) p.fillRect(0, y, width(), rowH, rowHover); // Subtle grey hover
    else p.fillRect(0, y, width(), rowH, rowBase);

    // N: ラベル色を行背景に薄く反映
    if (!isPropertyRow && !isDisplayLeafRow) {
      const int colorIdx = l->labelColorIndex();
      if (colorIdx > 0 && colorIdx < 8) {
        static const QColor kRowTintColors[] = {
          QColor(), QColor(231, 76, 60), QColor(230, 126, 34),
          QColor(241, 196, 15), QColor(46, 204, 113),
          QColor(26, 188, 156), QColor(52, 152, 219), QColor(155, 89, 182)
        };
        QColor tint = kRowTintColors[colorIdx];
        tint.setAlpha(layerSelected ? 22 : 36);
        p.fillRect(0, y, width(), rowH, tint);
      }
    }

    // E: Lock flash overlay (red tint, 300ms)
    if (impl_->lockFlashTimer_.isValid() && !impl_->lockFlashTimer_.hasExpired(300) &&
        impl_->rowViewportY(i) == impl_->lockFlashRowY_) {
      QColor flashColor(220, 60, 60, 80);
      p.fillRect(0, y, width(), rowH, flashColor);
    }

    if (impl_->dragMatteLinkMode && i == impl_->dragMatteHoverVisibleRow) {
      const bool validMatteTarget =
          (row.kind == RowKind::Layer || row.kind == RowKind::MatteStack || row.kind == RowKind::Matte);
      if (validMatteTarget) {
        p.fillRect(0, y, width(), rowH, mixColor(background, accent, 0.22));
        p.fillRect(0, y, 4, rowH, accent);
        auto comp = safeCompositionLookup(impl_->compositionId);
        const auto sourceLayer = comp ? comp->layerById(impl_->draggedLayerId) : ArtifactAbstractLayerPtr{};
        const bool blocked = !row.layer || matteDropWouldCreateCycle(comp, row.layer,
                                                                      sourceLayer ? sourceLayer->id()
                                                                                  : LayerID());
        const QString badgeText = blocked ? QStringLiteral("MATTE BLOCKED")
                                          : (row.kind == RowKind::MatteStack
                                                 ? QStringLiteral("ADD MATTE")
                                                 : QStringLiteral("MATTE LINK"));
        QFont badgeFont = p.font();
        badgeFont.setBold(true);
        badgeFont.setPointSizeF(std::max<qreal>(7.0, badgeFont.pointSizeF() - 1.0));
        p.setFont(badgeFont);
        const QFontMetrics fm(badgeFont);
        const int badgeW = fm.horizontalAdvance(badgeText) + 18;
        const QRect badgeRect(width() - badgeW - 10, y + 5, badgeW, rowH - 10);
        QColor badgeFill = blocked ? QColor(160, 76, 76) : accent;
        badgeFill.setAlpha(220);
        p.setPen(blocked ? QColor(95, 38, 38) : accent.darker(180));
        p.setBrush(badgeFill);
        p.drawRoundedRect(badgeRect, 4, 4);
        p.setPen(blocked ? QColor(255, 236, 236) : mixColor(background, text, 0.95));
        p.drawText(badgeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, badgeText);
      }
    }
    if (impl_->dragMatteLinkMode) {
      auto comp = safeCompositionLookup(impl_->compositionId);
      const auto sourceLayer =
          comp ? comp->layerById(impl_->draggedLayerId) : ArtifactAbstractLayerPtr{};
      if (sourceLayer && row.layer && row.layer->id() == sourceLayer->id()) {
        QColor sourceTint = mixColor(background, accent, 0.16);
        sourceTint.setAlpha(96);
        p.fillRect(0, y, width(), rowH, sourceTint);
        p.fillRect(0, y, 4, rowH, accent.lighter(120));

        QFont badgeFont = p.font();
        badgeFont.setBold(true);
        badgeFont.setPointSizeF(std::max<qreal>(7.0, badgeFont.pointSizeF() - 1.0));
        p.setFont(badgeFont);
        const QFontMetrics fm(badgeFont);
        const QString badgeText = QStringLiteral("MATTE SOURCE");
        const int badgeW = fm.horizontalAdvance(badgeText) + 18;
        const QRect badgeRect(width() - badgeW - 10, y + 5, badgeW, rowH - 10);
        QColor badgeFill = accent.darker(150);
        badgeFill.setAlpha(210);
        p.setPen(accent.darker(180));
        p.setBrush(badgeFill);
        p.drawRoundedRect(badgeRect, 4, 4);
        p.setPen(mixColor(background, text, 0.96));
        p.drawText(badgeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   badgeText);
      }
    }

    if (isPropertyRow) {
      if (propertyFocused) {
        QColor focusStrip = accent;
        focusStrip.setAlpha(propertyKeyframed ? 180 : 110);
        p.fillRect(0, y, 3, rowH, focusStrip);
      } else if (propertyKeyframed) {
        QColor keyStrip = accent.lighter(120);
        keyStrip.setAlpha(96);
        p.fillRect(0, y, 2, rowH, keyStrip);
      }
    }

    if (maskSelected) {
      p.fillRect(0, y, 4, rowH, accent);
    } else if (selectionAnchor) {
      QColor anchorColor = mixColor(background, accent, 0.90);
      anchorColor.setAlpha(230);
      p.fillRect(0, y, 3, rowH, anchorColor);
    }

    // Q: Undo/redo flash overlay
    if (impl_->undoFlashTimer_.isValid() && !impl_->undoFlashTimer_.hasExpired(400)) {
      if (impl_->undoFlashLayerIds_.contains(l->id())) {
        QColor flashColor = accent;
        flashColor.setAlpha(60);
        p.fillRect(0, y, 4, rowH, flashColor);
      }
    }

    p.setPen(border.darker(120));
    p.drawLine(0, y + rowH, width(), y + rowH);

    if (isGroupRow) {
      const int toggleX = nameStartX + row.depth * indent + 2;
      const int textX = row.hasChildren ? (toggleX + toggleSize + 6) : (nameStartX + row.depth * indent + 4);
      if (row.layer && row.layer->isGroupLayer()) {
        p.fillRect(0, y, 4, rowH, mixColor(background, accent, 0.85));
        p.fillRect(4, y + 1, 2, rowH - 2, mixColor(background, accent, 0.45));
      }
      if (row.hasChildren) {
        const int toggleY = y + (rowH - toggleSize) / 2;
        QPolygonF tri;
        if (row.expanded) {
          tri << QPointF(toggleX + 0.5, toggleY + 2.0)
              << QPointF(toggleX + toggleSize - 0.5, toggleY + 2.0)
              << QPointF(toggleX + toggleSize / 2.0, toggleY + toggleSize - 1.0);
        } else {
          tri << QPointF(toggleX + 2.0, toggleY + 0.5)
              << QPointF(toggleX + 2.0, toggleY + toggleSize - 0.5)
              << QPointF(toggleX + toggleSize - 1.0, toggleY + toggleSize / 2.0);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(text.darker(25));
        p.drawPolygon(tri);
      }
      const QString groupAux = row.auxiliaryText.trimmed();
      if (!groupAux.isEmpty()) {
        const QFontMetrics fm(p.font());
        const int badgeW = std::min(96, std::max(54, fm.horizontalAdvance(groupAux) + 16));
        const QRect badgeRect(width() - badgeW - 8, y + 5, badgeW, rowH - 10);
        p.setPen(layerSelected ? accent.darker(180) : border);
        p.setBrush(toneBadgeFill(row.auxiliaryTone, background, surface, accent));
        p.drawRoundedRect(badgeRect, 4, 4);
        p.setPen(toneBadgeText(row.auxiliaryTone, text, accent));
        p.drawText(badgeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   fm.elidedText(groupAux, Qt::ElideRight, badgeRect.width() - 16));
      }
      const QColor groupText = maskSelected ? accent.lighter(135)
                                            : (row.layer && row.layer->isGroupLayer()
                                                   ? mixColor(text, accent, 0.28)
                                                   : (row.auxiliaryTone == LayerPresentationBadgeTone::Neutral
                                                   ? text
                                                   : mixColor(text, accent, 0.18)));
      p.setPen(groupText);
      const int groupTextWidth = std::max(20, width() - textX - 8 - (groupAux.isEmpty() ? 0 : 100));
      p.drawText(textX, y, groupTextWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, row.label);
      continue;
    }

    int curX = 0;
    if (!isPropertyRow && !isDisplayLeafRow) {
      const int colCount = kLayerPropertyColumnCount;
      const QPixmap* colIcons[colCount] = {&impl_->visibilityIcon, &impl_->lockIcon, &impl_->soloIcon, &impl_->audioIcon, &impl_->shyIcon, &impl_->parentWhipIcon};
      const auto colOpacity = [&](int ci) -> double {
        if (ci == 0) return l->isVisible() ? 1.0 : 0.3;
        if (ci == 1) return l->isLocked() ? 1.0 : 0.15;
        if (ci == 2) return l->isSolo() ? 1.0 : 0.15;
        if (ci == 3) return 0.15;
        if (ci == 4) return l->isShy() ? 1.0 : 0.15;
        if (ci == 5) return l->hasParent() ? 1.0 : 0.35;
        return 1.0;
      };
      for (int ci = 0; ci < colCount; ++ci) {
        if (!impl_->columnVisible_[ci]) continue;
        const int cw = impl_->columnWidths_[ci];
        p.setOpacity(colOpacity(ci));
        if (!colIcons[ci]->isNull()) {
          p.drawPixmap(QRect(curX + offset, y + offset, iconSize, iconSize), *colIcons[ci]);
        }
        // Pick Whip hover highlight
        if (ci == 5 && i == impl_->pickWhipHoverRow_) {
          auto comp = safeCompositionLookup(impl_->compositionId);
          const auto sourceLayer =
              comp ? comp->layerById(impl_->pickWhipSourceLayerId_) : ArtifactAbstractLayerPtr{};
          const bool blocked =
              !row.layer || !sourceLayer ||
              wouldCreateCycle(sourceLayer->id(), row.layer->id());
          const QString badgeText = blocked ? QStringLiteral("PARENT BLOCKED")
                                            : QStringLiteral("PARENT LINK");
          p.fillRect(curX, y, cw, rowH, QColor(255, 255, 255, 25));
          p.setPen(QPen(QColor(255, 255, 255, 80), 1));
          p.drawRect(curX + 2, y + 2, cw - 4, rowH - 4);

          QFont badgeFont = p.font();
          badgeFont.setBold(true);
          badgeFont.setPointSizeF(std::max<qreal>(7.0, badgeFont.pointSizeF() - 1.0));
          p.setFont(badgeFont);
          const QFontMetrics fm(badgeFont);
          const int badgeW = fm.horizontalAdvance(badgeText) + 18;
          const QRect badgeRect(width() - badgeW - 10, y + 5, badgeW, rowH - 10);
          QColor badgeFill = blocked ? QColor(160, 76, 76) : accent;
          badgeFill.setAlpha(220);
          p.setPen(blocked ? QColor(95, 38, 38) : accent.darker(180));
          p.setBrush(badgeFill);
          p.drawRoundedRect(badgeRect, 4, 4);
          p.setPen(blocked ? QColor(255, 236, 236) : mixColor(background, text, 0.95));
          p.drawText(badgeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, badgeText);
          p.setFont(font());
        }
        curX += cw;
        p.setOpacity(1.0);
        p.drawLine(curX - 1, y, curX - 1, y + rowH);
      }
    } else {
      curX = nameStartX;
    }

    // カラーラベルインジケータ
    if (!isPropertyRow && !isDisplayLeafRow) {
      const int colorIdx = l->labelColorIndex();
      if (colorIdx > 0 && colorIdx < 8) {
        static const QColor kLabelDotColors[] = {
          QColor(),
          QColor(231, 76, 60),
          QColor(230, 126, 34),
          QColor(241, 196, 15),
          QColor(46, 204, 113),
          QColor(26, 188, 156),
          QColor(52, 152, 219),
          QColor(155, 89, 182)
        };
        p.setPen(Qt::NoPen);
        p.setBrush(kLabelDotColors[colorIdx]);
        const int dotSize = 6;
        const int dotX = curX + 3;
        const int dotY = y + (rowH - dotSize) / 2;
        p.drawEllipse(dotX, dotY, dotSize, dotSize);
      }
    }

    // ブックマークバッジ
    if (!isPropertyRow && !isDisplayLeafRow) {
      for (auto it = impl_->layerBookmarks.constBegin(); it != impl_->layerBookmarks.constEnd(); ++it) {
        if (it.value() == l->id()) {
          const int badgeX = curX + 2;
          const int badgeSize = 14;
          const QRect badgeRect(badgeX, y + (rowH - badgeSize) / 2, badgeSize, badgeSize);
          p.setPen(Qt::NoPen);
          p.setBrush(mixColor(background, accent, 0.55));
          p.drawRoundedRect(badgeRect, 3, 3);
          p.setPen(mixColor(background, text, 0.90));
          QFont bf = p.font();
          bf.setPointSizeF(std::max<qreal>(7.0, bf.pointSizeF() - 2.0));
          bf.setBold(true);
          p.setFont(bf);
          p.drawText(badgeRect, Qt::AlignCenter, QString::number(it.key()));
          break;
        }
      }
    }

    // Name
    const int nameX = nameStartX + row.depth * indent;
    if ((row.kind == RowKind::Layer || isGroupRow) && row.hasChildren) {
      const int tx = nameX + 2;
      const int ty = y + (rowH - toggleSize) / 2;
      QPolygonF tri;
      if (row.expanded) {
        tri << QPointF(tx + 0.5, ty + 2.0)
            << QPointF(tx + toggleSize - 0.5, ty + 2.0)
            << QPointF(tx + toggleSize / 2.0, ty + toggleSize - 1.0);
      } else {
        tri << QPointF(tx + 2.0, ty + 0.5)
            << QPointF(tx + 2.0, ty + toggleSize - 0.5)
            << QPointF(tx + toggleSize - 1.0, ty + toggleSize / 2.0);
      }
      p.setPen(Qt::NoPen);
      p.setBrush(text.darker(28));
      p.drawPolygon(tri);
    }

    p.setPen(text);
    int textX = nameX + ((row.hasChildren && (row.kind == RowKind::Layer || isGroupRow)) ? 16 : 4);
    if (row.kind == RowKind::Layer) {
      const QRect layerTypeIconRect(textX, y + (rowH - kLayerTypeIconSize) / 2,
                                    kLayerTypeIconSize, kLayerTypeIconSize);
      const QIcon& layerTypeIcon = impl_->iconForLayerKind(layerIconKindForLayer(l));
      if (!layerTypeIcon.isNull()) {
        p.setOpacity(layerSelected ? 1.0 : 0.90);
        p.drawPixmap(layerTypeIconRect, layerTypeIcon.pixmap(layerTypeIconRect.size()));
        p.setOpacity(1.0);
      } else {
        p.setPen(QPen(layerSelected ? accent.darker(180) : border, 1.0));
        p.setBrush(mixColor(background, accent, 0.24));
        p.drawRoundedRect(layerTypeIconRect.adjusted(1, 1, -1, -1), 2, 2);
      }
      textX += kLayerTypeIconSize + kLayerTypeIconGap;
      p.setPen(text);
    }
    const bool showInlineCombos = row.kind == RowKind::Layer &&
                                  (width() - (nameX + 8)) >= (kInlineComboReserve + kLayerNameMinWidth);
    const int parentRectX = width() - kInlineComboReserve;
    const QRect parentRect(parentRectX, y + kInlineComboMarginY, kInlineParentWidth, kInlineComboHeight);
    const QRect blendRect(parentRect.right() + kInlineComboGap, y + kInlineComboMarginY, kInlineBlendWidth, kInlineComboHeight);

    auto drawInlineCombo = [&](const QRect& r, const QString& label) {
     p.setPen(layerSelected ? accent.darker(180) : border);
     p.setBrush(layerSelected ? mixColor(background, accent, 0.22) : mixColor(background, surface, 0.28));
     p.drawRoundedRect(r, 3, 3);
     p.setPen(layerSelected ? mixColor(text, accent, 0.48) : mixColor(text, accent, 0.16));
     p.drawText(r.adjusted(6, 0, -16, 0), Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(label, Qt::ElideRight, r.width() - 20));
    };

    if (showInlineCombos) {
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
     drawInlineCombo(parentRect, QStringLiteral("Parent: %1").arg(parentName));
     drawInlineCombo(blendRect, QStringLiteral("Blend: %1").arg(blendModeToText(l->layerBlendType())));
    }
    if (isPropertyRow) {
     const QRect keyframeRect = propertyKeyframeMarkerRect(width(), y, rowH);
     const int textWidth = std::max(20, keyframeRect.left() - textX - 10);
     p.setPen(propertyFocused ? accent.lighter(130)
                              : (propertyKeyframed ? mixColor(text, accent, 0.26)
                                                   : (layerSelected ? text.lighter(112) : text)));
     p.drawText(textX + 4, y, textWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, row.label);
     if (propertyAnimatable) {
      const QRectF marker = QRectF(keyframeRect).adjusted(2.0, 2.0, -2.0, -2.0);
      QPolygonF diamond;
      diamond << QPointF(marker.center().x(), marker.top())
              << QPointF(marker.right(), marker.center().y())
              << QPointF(marker.center().x(), marker.bottom())
              << QPointF(marker.left(), marker.center().y());
      p.setPen(QPen(propertyKeyframed ? accent.lighter(105) : text.darker(145), 1.4));
      p.setBrush(propertyKeyframed ? accent.lighter(105) : Qt::NoBrush);
      p.drawPolygon(diamond);
     }
    } else if (isDisplayLeafRow) {
     const int leafBadgeW = 84;
     const QRect badgeRect(width() - leafBadgeW - 8, y + 5, leafBadgeW, rowH - 10);
     p.setPen(layerSelected ? accent.darker(180) : border);
     p.setBrush(mixColor(background, surface, 0.30));
     p.drawRoundedRect(badgeRect, 4, 4);
     p.setPen(mixColor(text.darker(120), accent, 0.22));
     p.drawText(badgeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                row.auxiliaryText.isEmpty()
                    ? (row.kind == RowKind::Mask ? QStringLiteral("Mask")
                                                 : QStringLiteral("Matte"))
                    : row.auxiliaryText);
      const int labelWidth = std::max(20, badgeRect.left() - textX - 10);
      p.setPen(mixColor(text, accent, 0.10));
      p.drawText(textX + 4, y, labelWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, row.label);
    } else {
     const auto matteRefs = l->matteReferences();
     const bool hasMatteRefs = !matteRefs.empty();
     bool matteBroken = false;
     if (hasMatteRefs) {
      auto comp = safeCompositionLookup(impl_->compositionId);
      for (const auto& ref : matteRefs) {
       if (ref.sourceLayerId.isNil() || ref.sourceLayerId == l->id()) {
        matteBroken = true;
        break;
       }
       auto sourceLayer = comp ? comp->layerById(ref.sourceLayerId) : ArtifactAbstractLayerPtr{};
       if (!sourceLayer || matteDropWouldCreateCycle(comp, sourceLayer, l->id())) {
        matteBroken = true;
        break;
       }
      }
     }
     const auto variants = l->getVariants();
     const int variantChipW = kVariantChipWidth;
     const QFontMetrics fm(p.font());
     const int iconGap = hasMatteRefs ? 18 : 0;
     const QString layerName = l->layerName();
     const QString layerAux = row.auxiliaryText.trimmed();
     const QString layerState = row.stateText.trimmed();
     const QString matteBadgeText = hasMatteRefs ? matteSourceBadgeLabel(safeCompositionLookup(impl_->compositionId), l)
                                                 : QString();
     const int matteBadgeW = hasMatteRefs
                                 ? std::min(160, std::max(66, fm.horizontalAdvance(matteBadgeText) + 16))
                                 : 0;
     const int matteBadgeX = textX + 4 + 18;
     const QRect matteBadgeRect(matteBadgeX, y + 5, matteBadgeW, rowH - 10);
     const int layerTextX = hasMatteRefs ? (matteBadgeRect.right() + 8) : (textX + 4 + iconGap);
     const int layerStateW = layerState.isEmpty()
                                 ? 0
                                 : std::min(128, std::max(52, fm.horizontalAdvance(layerState) + 16));
     if (hasMatteRefs) {
      const QRect iconRect(textX + 4, y + 6, 14, 14);
      const QIcon& matteIcon = matteBroken ? impl_->iconLinkOff : impl_->iconLink;
      if (!matteIcon.isNull()) {
        p.setOpacity(matteBroken ? 0.95 : 1.0);
        p.drawPixmap(iconRect, matteIcon.pixmap(iconRect.size()));
        p.setOpacity(1.0);
      } else {
        p.setPen(QPen(matteBroken ? accent.darker(120) : border, 1.2));
        p.setBrush(mixColor(background, accent, matteBroken ? 0.18 : 0.10));
        p.drawEllipse(iconRect);
      }
      p.setPen(matteBroken ? accent.darker(130) : text.darker(118));
      p.setBrush(matteBroken ? mixColor(background, accent, 0.20) : mixColor(background, surface, 0.26));
      p.drawRoundedRect(matteBadgeRect, 4, 4);
      p.setPen(matteBroken ? accent.lighter(120) : text);
      p.drawText(matteBadgeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                 fm.elidedText(matteBadgeText, Qt::ElideRight, matteBadgeRect.width() - 16));
     }
     if (!layerAux.isEmpty()) {
      const int badgeTextWidth = fm.horizontalAdvance(layerAux) + 16;
      const int badgeWidth = std::min(120, std::max(52, badgeTextWidth));
      const int badgeGap = layerStateW > 0 ? 6 : 0;
      const int badgeBundleWidth = badgeWidth + badgeGap + layerStateW;
      const int badgeX = std::max(layerTextX, width() - (showInlineCombos ? kInlineComboReserve : 0) - variantChipW - badgeBundleWidth - 10);
      const QRect badgeRect(badgeX, y + 5, badgeWidth, rowH - 10);
      const QRect stateRect(badgeRect.right() + badgeGap,
                            y + 5, layerStateW, rowH - 10);
      const int nameWidth = std::max(20, badgeRect.left() - (layerTextX + 4));
      const QString elidedName = fm.elidedText(layerName, Qt::ElideRight, nameWidth);
      const QColor layerTextColor = maskSelected ? accent.lighter(135)
                                                 : (layerSelected ? mixColor(text, accent, 0.24)
                                                                  : mixColor(text, accent, 0.08));
      p.setPen(layerTextColor);
      p.drawText(layerTextX, y, nameWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, elidedName);
      p.setPen(layerSelected ? accent.darker(180) : border);
      p.setBrush(toneBadgeFill(row.auxiliaryTone, background, surface, accent));
      p.drawRoundedRect(badgeRect, 4, 4);
      p.setPen(toneBadgeText(row.auxiliaryTone, text, accent));
      p.drawText(badgeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                 fm.elidedText(layerAux, Qt::ElideRight, badgeRect.width() - 16));
      if (layerStateW > 0) {
       p.setPen(layerSelected ? accent.darker(180) : border);
       p.setBrush(toneBadgeFill(row.stateTone, background, surface, accent));
       p.drawRoundedRect(stateRect, 4, 4);
       p.setPen(toneBadgeText(row.stateTone, text, accent));
       p.drawText(stateRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                  fm.elidedText(layerState, Qt::ElideRight, stateRect.width() - 16));
      }
     } else {
      const QColor layerTextColor = maskSelected ? accent.lighter(135)
                                                 : (layerSelected ? mixColor(text, accent, 0.24)
                                                                  : mixColor(text, accent, 0.08));
      p.setPen(layerTextColor);
      const int nameWidth = std::max(20, width() - layerTextX - variantChipW - 10 - layerStateW);
      p.drawText(layerTextX, y, nameWidth, rowH, Qt::AlignVCenter | Qt::AlignLeft, layerName);
      if (layerStateW > 0) {
       const int stateX = std::max(layerTextX + 80, width() - variantChipW - layerStateW - 10);
       const QRect stateRect(stateX, y + 5, layerStateW, rowH - 10);
       p.setPen(layerSelected ? accent.darker(180) : border);
       p.setBrush(toneBadgeFill(row.stateTone, background, surface, accent));
       p.drawRoundedRect(stateRect, 4, 4);
       p.setPen(toneBadgeText(row.stateTone, text, accent));
       p.drawText(stateRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                  fm.elidedText(layerState, Qt::ElideRight, stateRect.width() - 16));
      }
     }

     const QRect chipRect = variantChipRect(QRect(0, y, width(), rowH), l,
                                            showInlineCombos, rowH);
     const QString chipText = QStringLiteral("%1 ▾").arg(variantChipText(l));
     p.save();
     p.setFont(QFont("Inter", 8, QFont::Bold));
     p.setBrush(surface);
     p.setPen(layerSelected ? accent.darker(180) : border);
     p.drawRoundedRect(chipRect, 4, 4);
     p.setPen(text);
     const QFontMetrics chipFm(p.font());
     const QString elidedChip = chipFm.elidedText(chipText, Qt::ElideRight, chipRect.width() - 8);
     p.drawText(chipRect.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, elidedChip);
     p.restore();
    }
   }

   if (!impl_->draggedLayerId.isNil() && impl_->dragInsertVisibleRow >= 0) {
     const int lineY = std::clamp(static_cast<int>(std::floor(impl_->dragInsertVisibleRow * rowH - impl_->verticalOffset)), 1, std::max(1, height() - 2));
    QPen pen(accent, 2);
    p.setPen(pen);
    p.drawLine(0, lineY, width(), lineY);

    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    const int markerSize = 6;
    p.drawEllipse(QPoint(markerSize, lineY), markerSize / 2, markerSize / 2);
    p.drawEllipse(QPoint(std::max(markerSize, width() - markerSize), lineY), markerSize / 2, markerSize / 2);
  }

  p.restore();

   // P: ステータスバー（下部に常時表示）
   {
    constexpr int kStatusH = 22;
   auto comp = safeCompositionLookup(impl_->compositionId);
   const int totalLayers = comp ? static_cast<int>(comp->allLayer().size()) : 0;
    const int selectedCount = currentLayerSelectionManager()
                                  ? static_cast<int>(currentLayerSelectionManager()->selectedLayers().size())
                                  : 0;
    int maskCount = 0;
    if (comp) {
      for (const auto& l : comp->allLayer()) {
        if (l) maskCount += l->maskCount();
      }
    }
    const QString statusText = QStringLiteral("レイヤー: %1  選択: %2  マスク: %3")
                                   .arg(totalLayers)
                                   .arg(selectedCount)
                                   .arg(maskCount);
    const QRect sbRect(0, height() - kStatusH, width(), kStatusH);
    QColor sbBg = mixColor(background, surface, 0.60);
    sbBg.setAlpha(230);
    p.fillRect(sbRect, sbBg);
    p.setPen(text.darker(110));
    QFont sf = p.font();
    sf.setPointSizeF(qMax(7.0, sf.pointSizeF() - 2.0));
    p.setFont(sf);
    p.drawText(sbRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, statusText);
    p.drawLine(0, sbRect.top(), width(), sbRect.top());
   }

 }

void ArtifactLayerPanelWidget::dragEnterEvent(QDragEnterEvent* e)
  {
  const QMimeData* mime = e->mimeData();
  if (mime && mime->hasFormat(kLayerReorderMimeType)) {
    e->acceptProposedAction();
    update();
    return;
  }
  if (mime && mime->hasFormat(QStringLiteral("application/x-artifact-parent-link"))) {
    e->acceptProposedAction();
    update();
    return;
  }
  if (mime->hasUrls()) {
    for (const auto& url : mime->urls()) {
        if (url.isLocalFile()) {
         const QString filePath = url.toLocalFile();
         const LayerType type = inferLayerTypeFromFile(filePath);
         if (type == LayerType::Image || type == LayerType::Video || type == LayerType::Audio || type == LayerType::Shape) {
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
         if (type == LayerType::Image || type == LayerType::Video || type == LayerType::Audio || type == LayerType::Shape) {
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
  if (mime && mime->hasFormat(kLayerReorderMimeType)) {
    impl_->dragInsertVisibleRow = impl_->insertionVisibleRowForY(static_cast<int>(e->position().y() + impl_->verticalOffset));
    e->acceptProposedAction();
    update();
    return;
  }
  if (mime && mime->hasFormat(QStringLiteral("application/x-artifact-parent-link"))) {
    const int idx = impl_->rowIndexFromViewportY(static_cast<int>(e->position().y()));
    impl_->pickWhipHoverRow_ = idx;
    if (idx >= 0 && idx < impl_->visibleRows.size()) {
      const LayerID sourceId(QString::fromUtf8(mime->data(QStringLiteral("application/x-artifact-parent-link"))));
      auto target = impl_->visibleRows[idx].layer;
      if (target && target->id() != sourceId && !wouldCreateCycle(sourceId, target->id())) {
        e->acceptProposedAction();
        update();
        return;
      }
    }
    e->ignore();
    update();
    return;
  }
  if (mime && (mime->hasUrls() || mime->hasText())) {
    e->acceptProposedAction();
  } else {
     e->ignore();
   }
 }

 void ArtifactLayerPanelWidget::dragLeaveEvent(QDragLeaveEvent* e)
 {
  e->accept();
  impl_->dragInsertVisibleRow = -1;
  impl_->dragMatteHoverVisibleRow = -1;
  impl_->pickWhipHoverRow_ = -1;
  setToolTip(QString());
  update();  // ビジュアルフィードバック解除
 }

 void ArtifactLayerPanelWidget::dropEvent(QDropEvent* event)
 {
  const QMimeData* mime = event->mimeData();
  if (!mime) {
    event->ignore();
    return;
  }

  if (mime->hasFormat(kLayerReorderMimeType)) {
    auto* svc = ArtifactProjectService::instance();
    auto comp = safeCompositionLookup(impl_->compositionId);

    // MIME からドラッグ中のレイヤーIDを取得（impl_->draggedLayerId は別インスタンスでは無効）
    const LayerID dragLayerId = LayerID(QString::fromUtf8(mime->data(kLayerReorderMimeType)));
    if (svc && comp && !dragLayerId.isNil()) {
      QVector<LayerID> visibleLayerIds;
      visibleLayerIds.reserve(impl_->visibleRows.size());
      for (const auto& row : impl_->visibleRows) {
        if (row.kind == RowKind::Layer && row.layer) {
          visibleLayerIds.push_back(row.layer->id());
        }
      }

      const auto allLayers = comp->allLayer();
      int oldIndex = -1;
      for (int i = 0; i < allLayers.size(); ++i) {
        if (allLayers[i] && allLayers[i]->id() == dragLayerId) {
          oldIndex = i;
          break;
        }
      }

      if (oldIndex >= 0 && !visibleLayerIds.isEmpty()) {
        // ドラッグ中のレイヤーを除いた可視レイヤーリスト
        QVector<LayerID> remainingVisibleLayerIds;
        remainingVisibleLayerIds.reserve(visibleLayerIds.size());
        for (const auto& layerId : visibleLayerIds) {
          if (layerId != dragLayerId) {
            remainingVisibleLayerIds.push_back(layerId);
          }
        }

        // ドラッグ中のレイヤーを除いた中での挿入位置
        const int targetVisibleIndex = std::clamp(
          impl_->layerCountBeforeVisibleRowExcluding(impl_->dragInsertVisibleRow, dragLayerId),
          0,
          static_cast<int>(remainingVisibleLayerIds.size()));

        int newIndex = static_cast<int>(remainingVisibleLayerIds.size()) -
                       targetVisibleIndex;

        newIndex = std::clamp(newIndex, 0, std::max(0, static_cast<int>(allLayers.size()) - 1));
         if (newIndex != oldIndex) {
          auto layer = comp->layerById(dragLayerId);
          auto* cmd = new MoveLayerIndexCommand(comp, layer, oldIndex, newIndex);
          UndoManager::instance()->push(std::unique_ptr<MoveLayerIndexCommand>(cmd));
          updateLayout();
         }
      }
    }
    impl_->clearDragState();
    event->acceptProposedAction();
    update();
    return;
  }

  // Pick Whip parent-link drop
  if (mime->hasFormat("application/x-artifact-parent-link")) {
    const int dropIdx = impl_->rowIndexFromViewportY(static_cast<int>(event->position().y()));
    if (dropIdx >= 0 && dropIdx < impl_->visibleRows.size()) {
      LayerID sourceId(QString::fromUtf8(mime->data(QStringLiteral("application/x-artifact-parent-link"))));
      auto target = impl_->visibleRows[dropIdx].layer;
      if (target && target->id() != sourceId && !wouldCreateCycle(sourceId, target->id())) {
        auto* svc = ArtifactProjectService::instance();
        if (svc) {
          svc->clearLayerParentInCurrentComposition(sourceId);
          svc->setLayerParentInCurrentComposition(sourceId, target->id());
        }
      }
    }
    impl_->pickWhipHoverRow_ = -1;
    event->acceptProposedAction();
    update();
    return;
  }

  QStringList validPaths = collectTimelineDroppedPaths(mime);

  if (validPaths.isEmpty()) {
    event->ignore();
    return;
  }

  importTimelineDroppedPaths(validPaths);

  event->acceptProposedAction();
 }

bool ArtifactLayerPanelWidget::wouldCreateCycle(const LayerID& childId, const LayerID& parentId) const {
  auto comp = safeCompositionLookup(impl_->compositionId);
  if (!comp) return true;
  LayerID cursor = parentId;
  int guard = 0;
  while (!cursor.isNil() && guard++ < 1024) {
    if (cursor == childId) return true;
    auto layer = comp->layerById(cursor);
    cursor = layer ? layer->parentLayerId() : LayerID();
  }
  return false;
}

 // ============================================================================
 // ArtifactLayerTimelinePanelWrapper Implementation
 // ============================================================================

class ArtifactLayerTimelinePanelWrapper::Impl
{
public:
  ArtifactLayerPanelHeaderWidget* header = nullptr;
  ArtifactLayerPanelWidget* panel = nullptr;
  CompositionID id;
};

 W_OBJECT_IMPL(ArtifactLayerTimelinePanelWrapper)

 ArtifactLayerTimelinePanelWrapper::ArtifactLayerTimelinePanelWrapper(QWidget* parent)
  : QWidget(parent), impl_(new Impl)
 {
  setAcceptDrops(true);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);

 impl_->header = new ArtifactLayerPanelHeaderWidget();
 impl_->panel = new ArtifactLayerPanelWidget();
  impl_->panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto* wheelFilter = new LayerPanelWheelFilter(impl_->panel, this);
  this->installEventFilter(wheelFilter);
  impl_->header->installEventFilter(wheelFilter);

  layout->addWidget(impl_->header);
  layout->addWidget(impl_->panel, 1);

  QObject::connect(impl_->header, &ArtifactLayerPanelHeaderWidget::shyToggled,
                   impl_->panel, &ArtifactLayerPanelWidget::setShyHidden);
  QObject::connect(impl_->panel, &ArtifactLayerPanelWidget::visibleRowsChanged,
                   this, [this]() {
                     this->visibleRowsChanged();
                   });
  QObject::connect(impl_->panel, &ArtifactLayerPanelWidget::verticalOffsetChanged,
                   this, [this](double offset) {
                       this->verticalOffsetChanged(offset);
                     });
  QObject::connect(
      impl_->panel, &ArtifactLayerPanelWidget::propertyFocusChanged, this,
      [this](const LayerID& layerId, const QString& propertyPath) {
        this->propertyFocusChanged(layerId, propertyPath);
      });

  if (auto* selectionButton = impl_->header->selectionMenuButton()) {
    QObject::connect(selectionButton, &QPushButton::clicked, this, [this, selectionButton]() {
      auto* svc = ArtifactProjectService::instance();
      auto comp = svc ? svc->currentComposition().lock() : ArtifactCompositionPtr{};
      if (!svc || !comp) {
        return;
      }

      const LayerID selectedId = impl_->panel->selectedLayerId();
      auto layer = selectedId.isNil() ? ArtifactAbstractLayerPtr{} : comp->layerById(selectedId);
      if (!layer) {
        return;
      }

      QMenu menu(impl_->panel);
      buildSelectedLayerMenu(
          &menu, layer,
          [this]() { impl_->panel->setFocus(Qt::OtherFocusReason); },
          [this]() { impl_->panel->setFocus(Qt::OtherFocusReason); },
          [this](bool visible) {
            if (auto* service = ArtifactProjectService::instance()) {
              auto comp = service->currentComposition().lock();
              if (comp) {
                if (auto l = comp->layerById(impl_->panel->selectedLayerId())) {
                  l->setVisible(visible);
                  impl_->panel->updateLayout();
                }
              }
            }
          },
          [this](bool locked) {
            if (auto* service = ArtifactProjectService::instance()) {
              auto comp = service->currentComposition().lock();
              if (comp) {
                if (auto l = comp->layerById(impl_->panel->selectedLayerId())) {
                  l->setLocked(locked);
                  impl_->panel->updateLayout();
                }
              }
            }
          },
          [this](bool solo) {
            if (auto* service = ArtifactProjectService::instance()) {
              auto comp = service->currentComposition().lock();
              if (comp) {
                if (auto l = comp->layerById(impl_->panel->selectedLayerId())) {
                  l->setSolo(solo);
                  impl_->panel->updateLayout();
                }
              }
            }
          },
          [this](bool shy) {
            if (auto* service = ArtifactProjectService::instance()) {
              auto comp = service->currentComposition().lock();
              if (comp) {
                if (auto l = comp->layerById(impl_->panel->selectedLayerId())) {
                  l->setShy(shy);
                  impl_->panel->updateLayout();
                }
              }
            }
          },
          [this]() { /* parent selection handled from layer menu */ },
          [this]() { /* parent clear handled from layer menu */ },
          [this, selectionButton]() {
            if (auto* service = ArtifactProjectService::instance()) {
              if (auto comp = service->currentComposition().lock()) {
                if (auto l = comp->layerById(impl_->panel->selectedLayerId())) {
                  impl_->panel->editLayerName(l->id());
                }
              }
            }
          },
          [this]() {
            if (auto* service = ArtifactProjectService::instance()) {
              if (auto comp = service->currentComposition().lock()) {
                if (auto l = comp->layerById(impl_->panel->selectedLayerId())) {
                  service->duplicateLayerInCurrentComposition(l->id());
                }
              }
            }
          },
          [this]() {
            if (auto* service = ArtifactProjectService::instance()) {
              if (auto comp = service->currentComposition().lock()) {
                if (auto l = comp->layerById(impl_->panel->selectedLayerId())) {
                  service->removeLayerFromComposition(comp->id(), l->id());
                }
              }
            }
          },
          [this]() {
            if (auto* service = ArtifactProjectService::instance()) {
              if (auto comp = service->currentComposition().lock()) {
                if (auto l = comp->layerById(impl_->panel->selectedLayerId())) {
                  impl_->panel->setComposition(comp->id());
                  impl_->panel->scrollToLayer(l->id());
                }
              }
            }
          });
      menu.exec(selectionButton->mapToGlobal(QPoint(0, selectionButton->height())));
    });
  }
}

void ArtifactLayerTimelinePanelWrapper::dragEnterEvent(QDragEnterEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (!validPaths.isEmpty()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void ArtifactLayerTimelinePanelWrapper::dragMoveEvent(QDragMoveEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (!validPaths.isEmpty()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void ArtifactLayerTimelinePanelWrapper::dropEvent(QDropEvent* event)
{
  const QStringList validPaths = collectTimelineDroppedPaths(event->mimeData());
  if (validPaths.isEmpty()) {
    event->ignore();
    return;
  }
  importTimelineDroppedPaths(validPaths);
  event->acceptProposedAction();
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
   impl_->panel->updateLayout();
  }

  void ArtifactLayerTimelinePanelWrapper::setFilterText(const QString& text)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setFilterText(text);
   }
  }

  void ArtifactLayerTimelinePanelWrapper::setSearchMatchMode(SearchMatchMode mode)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setSearchMatchMode(mode);
   }
  }

  SearchMatchMode ArtifactLayerTimelinePanelWrapper::searchMatchMode() const
  {
   return impl_ && impl_->panel ? impl_->panel->searchMatchMode() : SearchMatchMode::AllVisible;
  }

  void ArtifactLayerTimelinePanelWrapper::setDisplayMode(TimelineLayerDisplayMode mode)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setDisplayMode(mode);
   }
  }

  TimelineLayerDisplayMode ArtifactLayerTimelinePanelWrapper::displayMode() const
  {
   return impl_ && impl_->panel ? impl_->panel->displayMode() : TimelineLayerDisplayMode::AllLayers;
  }

  void ArtifactLayerTimelinePanelWrapper::setRowHeight(int rowHeight)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setRowHeight(rowHeight);
   }
  }

  int ArtifactLayerTimelinePanelWrapper::rowHeight() const
  {
   return impl_ && impl_->panel ? impl_->panel->rowHeight() : 0;
  }

  void ArtifactLayerTimelinePanelWrapper::setPropertyColumnWidth(int width)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setPropertyColumnWidth(width);
   }
  }

  int ArtifactLayerTimelinePanelWrapper::propertyColumnWidth() const
  {
   return impl_ && impl_->panel ? impl_->panel->propertyColumnWidth() : 0;
  }

  void ArtifactLayerTimelinePanelWrapper::setVerticalOffset(double offset)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->setVerticalOffset(offset);
   }
  }

  double ArtifactLayerTimelinePanelWrapper::verticalOffset() const
  {
   return impl_ && impl_->panel ? impl_->panel->verticalOffset() : 0.0;
  }

  void ArtifactLayerTimelinePanelWrapper::setLayerNameEditable(bool enabled)
  {
    if (impl_ && impl_->panel) {
     impl_->panel->setLayerNameEditable(enabled);
   }
  }

  bool ArtifactLayerTimelinePanelWrapper::isLayerNameEditable() const
  {
   return impl_ && impl_->panel ? impl_->panel->isLayerNameEditable() : false;
  }

 QVector<LayerID> ArtifactLayerTimelinePanelWrapper::visibleTimelineRows() const
 {
  if (!impl_ || !impl_->panel) {
   return {};
  }
  return impl_->panel->visibleTimelineRows();
 }

 QVector<TimelineRowDescriptor>
 ArtifactLayerTimelinePanelWrapper::visibleTimelineRowDescriptors() const
 {
  if (!impl_ || !impl_->panel) {
   return {};
  }
  return impl_->panel->visibleTimelineRowDescriptors();
 }

 QVector<LayerID> ArtifactLayerTimelinePanelWrapper::matchingTimelineRows() const
 {
  if (!impl_ || !impl_->panel) {
   return {};
  }
  return impl_->panel->matchingTimelineRows();
 }

  void ArtifactLayerTimelinePanelWrapper::scrollToLayer(const LayerID& id)
  {
   if (impl_ && impl_->panel) {
    impl_->panel->scrollToLayer(id);
   }
  }

  LayerID ArtifactLayerTimelinePanelWrapper::selectedLayerId() const
  {
   return impl_ && impl_->panel ? impl_->panel->selectedLayerId() : LayerID{};
  }

  QString ArtifactLayerTimelinePanelWrapper::currentPropertyPath() const
  {
   return impl_ && impl_->panel ? impl_->panel->currentPropertyPath() : QString{};
  }

} // namespace Artifact
