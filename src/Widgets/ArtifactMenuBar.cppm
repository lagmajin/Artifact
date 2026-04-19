module;
#include <utility>
#include <QFont>
#include <QSize>
#include <QMenuBar>
#include <QMenu>
#include <QWidget>
#include <wobjectimpl.h>
module Menu.MenuBar;


import Application.AppSettings;
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

namespace Artifact {

namespace {
QFont scaledMenuFont(const QFont& baseFont)
{
 const auto* settings = ArtifactCore::ArtifactAppSettings::instance();
 const int scalePercent = settings ? settings->menuBarFontScalePercent() : 132;
 const qreal factor = qBound(0.5, static_cast<qreal>(scalePercent) / 100.0, 2.0);
 QFont font = baseFont;
 const qreal pointSize = font.pointSizeF() > 0 ? font.pointSizeF() : 10.0;
 font.setPointSizeF(pointSize * factor);
 return font;
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
 for (auto* menu : findChildren<QMenu*>()) {
  if (menu) {
   menu->setFont(f);
  }
 }
}

}
