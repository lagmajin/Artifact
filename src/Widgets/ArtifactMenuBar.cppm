module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QAction>
#include <QFont>
#include <QFontMetrics>
#include <QSize>
#include <QSizePolicy>
#include <QMenuBar>
#include <QMenu>
#include <QWidget>
#include <wobjectimpl.h>
module Menu.MenuBar;


import Application.AppSettings;
import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
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
import Menu.Time;
import Artifact.Menu.View;
import Menu.Option;
import Menu.Test;
import Menu.Help;
import Artifact.Widgets.Timeline;
import Artifact.Widgets.Timeline.GlobalSwitches;
import Math.Interpolate;

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

 connect(animationMenu, &ArtifactAnimationMenu::addKeyframeRequested, menuBar, [this]() {
  if (auto* timeline = activeTimelineWidget(mainWindow_)) {
   timeline->addKeyframeAtPlayhead();
  }
 });
connect(animationMenu, &ArtifactAnimationMenu::applyInterpolationRequested, menuBar,
         [this](const ArtifactCore::InterpolationType type) {
          if (auto* timeline = activeTimelineWidget(mainWindow_)) {
           timeline->applyInterpolationToSelectedKeyframes(type);
          }
         });
 connect(animationMenu, &ArtifactAnimationMenu::showGraphEditorRequested, menuBar, [this]() {
  if (auto* switches = activeTimelineGlobalSwitches(mainWindow_)) {
   switches->setGraphEditorActive(true);
  }
 });
 connect(animationMenu, &ArtifactAnimationMenu::toggleValueGraphRequested, menuBar,
         [this]() {
          if (auto* timeline = activeTimelineWidget(mainWindow_)) {
           timeline->showValueGraph();
          }
         });
 connect(animationMenu, &ArtifactAnimationMenu::toggleVelocityGraphRequested, menuBar,
         [this]() {
          if (auto* timeline = activeTimelineWidget(mainWindow_)) {
           timeline->showSpeedGraph();
          }
         });
 connect(animationMenu, &ArtifactAnimationMenu::enableTimeRemapRequested, menuBar, [this]() {
  auto layer = activeTimelineLayer();
  if (!layer) {
   return;
  }
  if (!layer->isTimeRemapEnabled()) {
   layer->setTimeRemapEnabled(true);
  }
 });
connect(animationMenu, &ArtifactAnimationMenu::freezeFrameRequested, menuBar, [this]() {
  auto layer = activeTimelineLayer();
  auto comp = currentComposition();
  if (!layer || !comp) {
   return;
  }
  const int64_t compFrame = comp->framePosition().framePosition();
  const int64_t sourceFrame = static_cast<int64_t>(
      std::llround(layer->getSourceFrameAtCompFrame(compFrame)));
  layer->clearTimeRemap();
  layer->setTimeRemapEnabled(true);
  layer->setTimeRemapKey(compFrame, static_cast<double>(sourceFrame));
 });
 connect(animationMenu, &ArtifactAnimationMenu::timeReverseRequested, menuBar, [this]() {
  auto layer = activeTimelineLayer();
  auto comp = currentComposition();
  if (!layer || !comp) {
   return;
  }
  const int64_t compFrame = comp->framePosition().framePosition();
  const int64_t clipStartSourceFrame = layer->startTime().framePosition();
  const int64_t clipFrameCount =
      std::max<int64_t>(1, layer->outPoint().framePosition() - layer->inPoint().framePosition());
  const int64_t clipEndSourceFrame = clipStartSourceFrame + clipFrameCount - 1;
  layer->clearTimeRemap();
  layer->setTimeRemapEnabled(true);
  if (clipFrameCount <= 1) {
   layer->setTimeRemapKey(compFrame, static_cast<double>(clipStartSourceFrame));
   return;
  }
  layer->setTimeRemapKey(layer->inPoint().framePosition(), static_cast<double>(clipEndSourceFrame));
  layer->setTimeRemapKey(layer->outPoint().framePosition() - 1, static_cast<double>(clipStartSourceFrame));
 });
 connect(animationMenu, &ArtifactAnimationMenu::removeKeyframeRequested, menuBar, [this]() {
  if (auto* timeline = activeTimelineWidget(mainWindow_)) {
   timeline->removeKeyframeAtPlayhead();
  }
 });
 connect(animationMenu, &ArtifactAnimationMenu::selectAllKeyframesRequested, menuBar, [this]() {
  if (auto* timeline = activeTimelineWidget(mainWindow_)) {
   timeline->selectAllKeyframes();
  }
 });
 connect(animationMenu, &ArtifactAnimationMenu::copyKeyframesRequested, menuBar, [this]() {
  if (auto* timeline = activeTimelineWidget(mainWindow_)) {
   timeline->copySelectedKeyframes();
  }
 });
 connect(animationMenu, &ArtifactAnimationMenu::pasteKeyframesRequested, menuBar, [this]() {
  if (auto* timeline = activeTimelineWidget(mainWindow_)) {
   timeline->pasteKeyframesAtPlayhead();
  }
 });
 connect(animationMenu, &ArtifactAnimationMenu::goToNextKeyframeRequested, menuBar, [this]() {
  if (auto* timeline = activeTimelineWidget(mainWindow_)) {
   timeline->jumpToKeyframeHit(+1);
  }
 });
 connect(animationMenu, &ArtifactAnimationMenu::goToPreviousKeyframeRequested, menuBar, [this]() {
  if (auto* timeline = activeTimelineWidget(mainWindow_)) {
   timeline->jumpToKeyframeHit(-1);
  }
 });
 connect(animationMenu, &ArtifactAnimationMenu::goToFirstKeyframeRequested, menuBar, [this]() {
  if (auto* timeline = activeTimelineWidget(mainWindow_)) {
   timeline->jumpToFirstKeyframe();
  }
 });
 connect(animationMenu, &ArtifactAnimationMenu::goToLastKeyframeRequested, menuBar, [this]() {
  if (auto* timeline = activeTimelineWidget(mainWindow_)) {
   timeline->jumpToLastKeyframe();
  }
 });
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
