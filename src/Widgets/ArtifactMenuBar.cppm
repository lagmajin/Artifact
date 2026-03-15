module;
#include <QMenuBar>
#include <QMenu>
#include <wobjectimpl.h>

module Menu.MenuBar;

import Artifact.Menu.File;
import Artifact.Menu.Edit;
import Menu.Composition;
import Artifact.Menu.Layer;
import Artifact.Menu.Effect;
import Menu.Animation;
import Menu.Render;
import Menu.Time;
import Artifact.Menu.View;
import Menu.Option;
import Menu.Test;
import Menu.Help;

namespace Artifact {

W_OBJECT_IMPL(ArtifactMenuBar)

class ArtifactMenuBar::Impl {
public:
 Impl(QWidget* mainWindow, ArtifactMenuBar* menuBar);
 ~Impl() = default;

 QWidget* mainWindow_ = nullptr;
 ArtifactMenuBar* menuBar_ = nullptr;

 ArtifactFileMenu* fileMenu = nullptr;
 ArtifactEditMenu* editMenu = nullptr;
 ArtifactCompositionMenu* compMenu = nullptr;
 ArtifactLayerMenu* layerMenu = nullptr;
 ArtifactEffectMenu* effectMenu = nullptr;
 ArtifactAnimationMenu* animationMenu = nullptr;
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
 editMenu = new ArtifactEditMenu(menuBar);
 compMenu = new ArtifactCompositionMenu(mainWindow, menuBar);
 layerMenu = new ArtifactLayerMenu(menuBar);
 effectMenu = new ArtifactEffectMenu(menuBar);
 animationMenu = new ArtifactAnimationMenu(menuBar);
 renderMenu = new ArtifactRenderMenu(mainWindow, menuBar);
 timeMenu = new ArtifactTimeMenu(menuBar);
 viewMenu = new ArtifactViewMenu(menuBar);
 optionMenu = new ArtifactOptionMenu(menuBar);
#if defined(_DEBUG) || !defined(NDEBUG)
 testMenu = new ArtifactTestMenu(menuBar);
#endif
 helpMenu = new ArtifactHelpMenu(menuBar);

 menuBar->addMenu(fileMenu);
 menuBar->addMenu(editMenu);
 menuBar->addMenu(compMenu);
 menuBar->addMenu(layerMenu);
 menuBar->addMenu(effectMenu);
 menuBar->addMenu(animationMenu);
 menuBar->addMenu(renderMenu);
 menuBar->addMenu(timeMenu);
 menuBar->addMenu(viewMenu);
 menuBar->addMenu(optionMenu);
#if defined(_DEBUG) || !defined(NDEBUG)
 if (testMenu) {
  menuBar->addMenu(testMenu);
 }
#endif
 menuBar->addMenu(helpMenu);
}

ArtifactMenuBar::ArtifactMenuBar(QWidget* mainWindow, QWidget* parent)
 : QMenuBar(parent), impl_(new Impl(mainWindow, this))
{
 setStyleSheet(R"(
 QMenuBar { background-color: #1E1E1E; color: #E0E0E0; font-size: 13px; }
 QMenuBar::item { padding: 4px 10px; }
 QMenu { background-color: #2D2D2D; color: #E0E0E0; font-size: 13px; icon-size: 16px; border: 1px solid #444444; }
 QMenu::item { padding: 6px 28px 6px 28px; }
 QMenu::item:selected { background-color: #414141; }
)");
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

}
