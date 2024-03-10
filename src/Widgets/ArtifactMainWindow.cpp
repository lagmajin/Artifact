#include <DockWidget.h>
#include <DockManager.h>

#pragma comment(lib,"qtadvanceddockingd.lib")

#include "../../include/Widgets/menu/ArtifactMenuBar.hpp"
#include "../../include/Widgets/ArtifactMainWindow.hpp"




namespace Artifact {

 using namespace ads;

 struct ArtifactMainWindowPrivate {


 };






 ArtifactMainWindow::ArtifactMainWindow(QWidget* parent /*= nullptr*/):QMainWindow(parent)
 {
  CDockManager::setConfigFlags(CDockManager::DefaultOpaqueConfig);
  CDockManager::setConfigFlag(CDockManager::RetainTabSizeWhenCloseButtonHidden, true);


  auto menuBar = new ArtifactMenuBar(this);

  setDockNestingEnabled(true);

  setMenuBar(menuBar);

  resize(600, 640);

  auto DockManager = new CDockManager(this);
  //CDockWidget* TableDockWidget = new CDockWidget("Table 1");
  //DockManager->addAutoHideDockWidget(SideBarLeft, TableDockWidget);

  //setCentralWidget(DockManager);
 }

 ArtifactMainWindow::~ArtifactMainWindow()
 {

 }

}