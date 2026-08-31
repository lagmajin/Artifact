module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <vector>
#include <QAction>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QSize>
#include <QSizePolicy>
#include <QMenuBar>
#include <QMenu>
#include <QSettings>
#include <QStringList>
#include <QWidget>
#include <QToolButton>
#include <wobjectimpl.h>
module Menu.MenuBar;


import Application.AppSettings;
import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Project;
import Artifact.Menu.File;
import Artifact.Menu.Edit;
import Menu.Composition;
import Artifact.Menu.Layer;
import Artifact.Menu.Effect;
import Menu.Animation;
import Artifact.Menu.Script;
import Menu.Render;
import Settings.Accessibility;
import Menu.Time;
import Artifact.Menu.View;
import Artifact.MainWindow;
import Menu.Option;
import Menu.Test;
import Menu.Help;
import Artifact.Widgets.Timeline;
import Artifact.Widgets.Timeline.GlobalSwitches;
import Math.Interpolate;
import Event.Bus;

namespace Artifact {

namespace {
constexpr qreal kMenuBarVisualScale = 1.2;

QFont scaledMenuFont(const QFont& baseFont)
{
 const auto* settings = ArtifactCore::ArtifactAppSettings::instance();
 const int scalePercent = settings ? settings->menuBarFontScalePercent() : 132;
 const qreal factor = qBound(0.5, static_cast<qreal>(scalePercent) / 100.0, 2.0) *
                      kMenuBarVisualScale;
 QFont font = baseFont;
 const qreal pointSize = font.pointSizeF() > 0 ? font.pointSizeF() : 10.0;
 font.setPointSizeF(pointSize * factor);
 return font;
}

int requiredMenuBarWidth(const QMenuBar* menuBar, const QFont& font)
{
 if (!menuBar) {
  return 0;
 }

 const QFontMetrics fm(font);
 const int visualIconSize = 19;
 int width = 0;
 for (const QAction* action : menuBar->actions()) {
  if (!action || !action->isVisible()) {
   continue;
  }
  const int iconWidth = action->icon().isNull() ? 0 : visualIconSize;
  const int spacing = action->icon().isNull() ? 0 : 5;
  width += fm.horizontalAdvance(action->text()) + iconWidth + spacing + 9;
 }
 return width + 8;
}

ArtifactTimelineWidget* activeTimelineWidget(QWidget* root)
{
 if (!root) {
  return nullptr;
 }

 const auto widgets = root->findChildren<ArtifactTimelineWidget*>();
 for (auto* widget : widgets) {
  if (widget && widget->hasFocus()) {
   return widget;
  }
 }
 for (auto* widget : widgets) {
  if (widget && widget->isVisible()) {
   return widget;
  }
 }
 return widgets.isEmpty() ? nullptr : widgets.front();
}

ArtifactTimelineGlobalSwitches* activeTimelineGlobalSwitches(QWidget* root)
{
 if (!root) {
  return nullptr;
 }

 const auto widgets = root->findChildren<ArtifactTimelineGlobalSwitches*>();
 for (auto* widget : widgets) {
  if (widget && widget->hasFocus()) {
   return widget;
  }
 }
 for (auto* widget : widgets) {
  if (widget && widget->isVisible()) {
   return widget;
  }
 }
 return widgets.isEmpty() ? nullptr : widgets.front();
}

ArtifactAbstractLayerPtr activeTimelineLayer()
{
 auto* selection = ArtifactLayerSelectionManager::instance();
 return selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
}

ArtifactCompositionPtr currentComposition()
{
 auto* service = ArtifactProjectService::instance();
 return service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
}
}

W_OBJECT_IMPL(ArtifactMenuBar)

class ArtifactMenuBar::Impl {
public:
 Impl(QWidget* mainWindow, ArtifactMenuBar* menuBar);
 ~Impl() = default;

 QWidget* mainWindow_ = nullptr;
 ArtifactMenuBar* menuBar_ = nullptr;
 QFont baseFont_;

 ArtifactFileMenu* fileMenu = nullptr;
 ArtifactEditMenu* editMenu = nullptr;
 ArtifactCompositionMenu* compMenu = nullptr;
 ArtifactLayerMenu* layerMenu = nullptr;
 ArtifactEffectMenu* effectMenu = nullptr;
 ArtifactAnimationMenu* animationMenu = nullptr;
 ArtifactScriptMenu* scriptMenu = nullptr;
 ArtifactRenderMenu* renderMenu = nullptr;
 ArtifactTimeMenu* timeMenu = nullptr;
 ArtifactViewMenu* viewMenu = nullptr;
 ArtifactOptionMenu* optionMenu = nullptr;
 ArtifactTestMenu* testMenu = nullptr;
 ArtifactHelpMenu* helpMenu = nullptr;
 std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
};

ArtifactMenuBar::Impl::Impl(QWidget* mainWindow, ArtifactMenuBar* menuBar)
 : mainWindow_(mainWindow), menuBar_(menuBar)
{
 fileMenu = new ArtifactFileMenu(menuBar);
 editMenu = new ArtifactEditMenu(mainWindow, menuBar);
 compMenu = new ArtifactCompositionMenu(mainWindow, menuBar);
 layerMenu = new ArtifactLayerMenu(mainWindow, menuBar);
 effectMenu = new ArtifactEffectMenu(menuBar);
 animationMenu = new ArtifactAnimationMenu(menuBar);
 scriptMenu = new ArtifactScriptMenu(menuBar);
 renderMenu = new ArtifactRenderMenu(mainWindow, menuBar);
 timeMenu = new ArtifactTimeMenu(menuBar);
 viewMenu = new ArtifactViewMenu(static_cast<QWidget*>(menuBar));
 optionMenu = new ArtifactOptionMenu(menuBar);
#if defined(_DEBUG) || !defined(NDEBUG)
 testMenu = new ArtifactTestMenu(menuBar);
#endif
 helpMenu = new ArtifactHelpMenu(menuBar);

 menuBar->addMenu(static_cast<QMenu*>(fileMenu));
 menuBar->addMenu(static_cast<QMenu*>(editMenu));
 menuBar->addMenu(static_cast<QMenu*>(compMenu));
 menuBar->addMenu(static_cast<QMenu*>(layerMenu));
 menuBar->addMenu(static_cast<QMenu*>(effectMenu));
 menuBar->addMenu(static_cast<QMenu*>(animationMenu));
 menuBar->addMenu(static_cast<QMenu*>(scriptMenu));
 menuBar->addMenu(static_cast<QMenu*>(renderMenu));
 menuBar->addMenu(static_cast<QMenu*>(timeMenu));
 menuBar->addMenu(static_cast<QMenu*>(viewMenu));
 menuBar->addMenu(static_cast<QMenu*>(optionMenu));
#if defined(_DEBUG) || !defined(NDEBUG)
 if (testMenu) {
  menuBar->addMenu(static_cast<QMenu*>(testMenu));
 }
#endif
 menuBar->addMenu(static_cast<QMenu*>(helpMenu));

 auto *addPanelButton = new QToolButton(menuBar);
 addPanelButton->setText(QStringLiteral("+"));
 addPanelButton->setToolTip(QStringLiteral("Add or restore a dock panel"));
 addPanelButton->setAccessibleName(QStringLiteral("Add or restore dock panel"));
 addPanelButton->setAccessibleDescription(
     QStringLiteral("Show and activate a registered dock panel"));
 addPanelButton->setAutoRaise(true);
 addPanelButton->setPopupMode(QToolButton::InstantPopup);
 auto *addPanelMenu = new QMenu(addPanelButton);
 addPanelButton->setMenu(addPanelMenu);
 QObject::connect(addPanelMenu, &QMenu::aboutToShow, menuBar,
                  [mainWindow, addPanelMenu]() {
                    addPanelMenu->clear();
                    auto *window = dynamic_cast<ArtifactMainWindow*>(mainWindow);
                    if (!window) return;
                    const auto dockIds = window->dockIds();
                    QSettings settings;
                    const auto normalizeDockIds = [window, &dockIds](
                                                     const QStringList& storedIds) {
                      QStringList normalized;
                      for (const QString& storedId : storedIds) {
                        const QString dockId = window->resolveDockId(storedId);
                        if (!dockId.isEmpty() && dockIds.contains(dockId) &&
                            !normalized.contains(dockId)) {
                          normalized.append(dockId);
                        }
                      }
                      return normalized;
                    };
                    const QString favoriteKey = QStringLiteral("Workspace/FavoriteDockIds");
                    const QString recentKey = QStringLiteral("Workspace/RecentDockIds");
                    const QStringList storedFavorites = settings.value(favoriteKey).toStringList();
                    const QStringList favorites = normalizeDockIds(storedFavorites);
                    if (favorites != storedFavorites) {
                      settings.setValue(favoriteKey, favorites);
                    }
                    const QStringList storedRecent = settings.value(recentKey).toStringList();
                    const QStringList recent = normalizeDockIds(storedRecent);
                    if (recent != storedRecent) {
                      settings.setValue(recentKey, recent);
                    }
                    auto addActivate = [window](QMenu* menu, const QString& dockId) {
                      const QString title = window->dockDisplayTitle(dockId);
                      QAction* action = menu->addAction(title);
                      action->setToolTip(QStringLiteral("Show and activate %1").arg(title));
                      QObject::connect(action, &QAction::triggered, window,
                                       [window, dockId]() {
                                         window->setDockVisible(dockId, true);
                                         window->activateDock(dockId);
                                         QSettings settings;
                                         QStringList ids = settings.value(
                                             QStringLiteral("Workspace/RecentDockIds")).toStringList();
                                         ids.removeAll(dockId);
                                         ids.prepend(dockId);
                                         while (ids.size() > 8) ids.removeLast();
                                         settings.setValue(QStringLiteral("Workspace/RecentDockIds"), ids);
                                       });
                    };
                    auto *recentMenu = addPanelMenu->addMenu(QStringLiteral("最近使ったパネル"));
                    for (const auto& dockId : recent) {
                      if (dockIds.contains(dockId)) addActivate(recentMenu, dockId);
                    }
                    if (recentMenu->actions().isEmpty()) {
                      recentMenu->addAction(QStringLiteral("(なし)"))->setEnabled(false);
                    }
                    auto *favoriteMenu = addPanelMenu->addMenu(QStringLiteral("お気に入り"));
                    for (const auto& dockId : dockIds) {
                      if (favorites.contains(dockId)) addActivate(favoriteMenu, dockId);
                    }
                    if (favoriteMenu->actions().isEmpty()) {
                      favoriteMenu->addAction(QStringLiteral("(なし)"))->setEnabled(false);
                    }
                    addPanelMenu->addSeparator();
                    for (const auto& dockId : dockIds) {
                      addActivate(addPanelMenu, dockId);
                    }
                    if (dockIds.isEmpty()) {
                      addPanelMenu->addAction(QStringLiteral("(no panels)"))->setEnabled(false);
                    }
                  });
 menuBar->setCornerWidget(addPanelButton, Qt::TopRightCorner);

 // Top-level menu titles stay text-only. Icons are reserved for commands
 // inside each menu, where they improve scanning without crowding the bar.
 for (QAction* action : menuBar->actions()) {
  if (action && action->menu()) {
   action->setIcon(QIcon());
  }
 }

 // left-handed: align menus to the right side
 if (Accessibility::isLeftHanded()) {
  menuBar->setLayoutDirection(Qt::RightToLeft);
 }

 // font scale
 const float fs = Accessibility::fontScale();
 if (qAbs(fs - 1.0f) > 0.01f) {
  QFont menuFont = menuBar->font();
  menuFont.setPointSizeF(menuFont.pointSizeF() * fs);
  menuBar->setFont(menuFont);
 }

 auto& eventBus = ArtifactCore::globalEventBus();
 eventBusSubscriptions_.push_back(
     eventBus.subscribe<TimelineInterpolationCommandRequestedEvent>(
         [this](const TimelineInterpolationCommandRequestedEvent& event) {
          if (auto* timeline = activeTimelineWidget(mainWindow_)) {
           timeline->applyInterpolationToSelectedKeyframes(event.type);
          }
         }));
 eventBusSubscriptions_.push_back(
     eventBus.subscribe<TimelineTimeRemapCommandRequestedEvent>(
         [this](const TimelineTimeRemapCommandRequestedEvent& event) {
          auto layer = activeTimelineLayer();
          if (!layer) {
           return;
          }
          switch (event.kind) {
          case TimelineTimeRemapCommandKind::Enable:
           if (!layer->isTimeRemapEnabled()) {
            layer->setTimeRemapEnabled(true);
           }
           break;
          case TimelineTimeRemapCommandKind::Freeze: {
           auto comp = currentComposition();
           if (!comp) {
            return;
           }
           const int64_t compFrame = comp->framePosition().framePosition();
           const int64_t sourceFrame = static_cast<int64_t>(
               std::llround(layer->getSourceFrameAtCompFrame(compFrame)));
           layer->clearTimeRemap();
           layer->setTimeRemapEnabled(true);
           layer->setTimeRemapKey(compFrame, static_cast<double>(sourceFrame));
           break;
          }
          case TimelineTimeRemapCommandKind::Reverse: {
           auto comp = currentComposition();
           if (!comp) {
            return;
           }
           const int64_t compFrame = comp->framePosition().framePosition();
           const int64_t clipStartSourceFrame = layer->startTime().framePosition();
           const int64_t clipFrameCount = std::max<int64_t>(
               1, layer->outPoint().framePosition() - layer->inPoint().framePosition());
           const int64_t clipEndSourceFrame = clipStartSourceFrame + clipFrameCount - 1;
           layer->clearTimeRemap();
           layer->setTimeRemapEnabled(true);
           if (clipFrameCount <= 1) {
            layer->setTimeRemapKey(compFrame, static_cast<double>(clipStartSourceFrame));
            return;
           }
           layer->setTimeRemapKey(layer->inPoint().framePosition(),
                                  static_cast<double>(clipEndSourceFrame));
           layer->setTimeRemapKey(layer->outPoint().framePosition() - 1,
                                  static_cast<double>(clipStartSourceFrame));
           break;
          }
          }
         }));
 eventBusSubscriptions_.push_back(
     eventBus.subscribe<TimelineGraphCommandRequestedEvent>(
         [this](const TimelineGraphCommandRequestedEvent& event) {
          switch (event.kind) {
          case TimelineGraphCommandKind::ShowEditor:
           if (auto* switches = activeTimelineGlobalSwitches(mainWindow_)) {
            switches->setGraphEditorActive(true);
           }
           break;
          case TimelineGraphCommandKind::ShowValue:
           if (auto* timeline = activeTimelineWidget(mainWindow_)) {
            timeline->showValueGraph();
           }
           break;
          case TimelineGraphCommandKind::ShowSpeed:
           if (auto* timeline = activeTimelineWidget(mainWindow_)) {
            timeline->showSpeedGraph();
           }
           break;
          }
         }));
 eventBusSubscriptions_.push_back(
     eventBus.subscribe<TimelineKeyframeEditCommandRequestedEvent>(
         [this](const TimelineKeyframeEditCommandRequestedEvent& event) {
          if (auto* timeline = activeTimelineWidget(mainWindow_)) {
           switch (event.kind) {
           case TimelineKeyframeEditCommandKind::Add:
            timeline->addKeyframeAtPlayhead();
            break;
           case TimelineKeyframeEditCommandKind::Remove:
            timeline->removeKeyframeAtPlayhead();
            break;
           case TimelineKeyframeEditCommandKind::SelectAll:
            timeline->selectAllKeyframes();
            break;
           case TimelineKeyframeEditCommandKind::Copy:
            timeline->copySelectedKeyframes();
            break;
           case TimelineKeyframeEditCommandKind::Paste:
            timeline->pasteKeyframesAtPlayhead();
            break;
           case TimelineKeyframeEditCommandKind::ReverseSelected:
            timeline->reverseSelectedKeyframes();
            break;
           case TimelineKeyframeEditCommandKind::ReverseCurrentLayer:
            timeline->reverseAllKeyframesInCurrentLayer();
            break;
           case TimelineKeyframeEditCommandKind::ReverseSelectedLayers:
            timeline->reverseAllKeyframesInSelectedLayers();
            break;
           case TimelineKeyframeEditCommandKind::ReverseComposition:
            timeline->reverseAllKeyframesInComposition();
            break;
           }
          }
         }));
 eventBusSubscriptions_.push_back(
     eventBus.subscribe<TimelineKeyframeNavigationRequestedEvent>(
         [this](const TimelineKeyframeNavigationRequestedEvent& event) {
          if (auto* timeline = activeTimelineWidget(mainWindow_)) {
           switch (event.kind) {
           case TimelineKeyframeNavigationKind::Next:
            timeline->jumpToKeyframeHit(+1);
            break;
           case TimelineKeyframeNavigationKind::Previous:
            timeline->jumpToKeyframeHit(-1);
            break;
           case TimelineKeyframeNavigationKind::First:
            timeline->jumpToFirstKeyframe();
            break;
           case TimelineKeyframeNavigationKind::Last:
            timeline->jumpToLastKeyframe();
            break;
           }
          }
         }));
}

ArtifactMenuBar::ArtifactMenuBar(QWidget* mainWindow, QWidget* parent)
 : QMenuBar(parent), impl_(new Impl(mainWindow, this))
{
 setAutoFillBackground(true);
 setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
 impl_->baseFont_ = font();
 refreshFontFromSettings();
}

ArtifactMenuBar::~ArtifactMenuBar()
{
 delete impl_;
}

void ArtifactMenuBar::setMainWindow(QWidget* window)
{
 if (!impl_) return;
 impl_->mainWindow_ = window;
}

void ArtifactMenuBar::refreshFontFromSettings()
{
 if (!impl_) return;
 const QFont f = scaledMenuFont(impl_->baseFont_);
 setFont(f);
 setMinimumWidth(requiredMenuBarWidth(this, f));
 for (auto* menu : findChildren<QMenu*>()) {
  if (menu) {
   menu->setFont(f);
  }
 }
 updateGeometry();
}

}
