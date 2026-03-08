module;
#include <QMenuBar>
#include <QMenu>
#include <wobjectimpl.h>

module Menu.MenuBar;

import Artifact.MainWindow;
import Artifact.Menu.File;
import Menu.Composition;
import Artifact.Menu.Layer;
import Artifact.Menu.Help;

namespace Artifact {

class ArtifactMenuBar::Impl {
public:
    Impl(ArtifactMainWindow* mainWindow, ArtifactMenuBar* menuBar);
    ~Impl() = default;

    ArtifactMainWindow* mainWindow_ = nullptr;
    ArtifactMenuBar* menuBar_ = nullptr;
    
    ArtifactFileMenu* fileMenu = nullptr;
    ArtifactCompositionMenu* compMenu = nullptr;
    ArtifactLayerMenu* layerMenu = nullptr;
    ArtifactHelpMenu* helpMenu = nullptr;
};

ArtifactMenuBar::Impl::Impl(ArtifactMainWindow* mainWindow, ArtifactMenuBar* menuBar)
    : mainWindow_(mainWindow), menuBar_(menuBar)
{
    fileMenu = new ArtifactFileMenu(menuBar);
    compMenu = new ArtifactCompositionMenu(mainWindow, menuBar);
    layerMenu = new ArtifactLayerMenu(menuBar);
    helpMenu = new ArtifactHelpMenu(menuBar);

    menuBar->addMenu(fileMenu);
    menuBar->addMenu(compMenu);
    menuBar->addMenu(layerMenu);
    menuBar->addMenu(helpMenu);
}

W_OBJECT_IMPL(ArtifactMenuBar)

ArtifactMenuBar::ArtifactMenuBar(ArtifactMainWindow* mainWindow, QWidget* parent)
    : QMenuBar(parent), impl_(new Impl(mainWindow, this))
{
    setStyleSheet("QMenuBar { background-color: #1E1E1E; color: #E0E0E0; }");
}

ArtifactMenuBar::~ArtifactMenuBar()
{
    delete impl_;
}

void ArtifactMenuBar::setMainWindow(ArtifactMainWindow* window)
{
    // Re-initialize if needed
}

} // namespace Artifact