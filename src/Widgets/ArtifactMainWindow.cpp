#include <DockWidget.h>
#include <DockManager.h>

#pragma comment(lib,"qtadvanceddockingd.lib")

#include "../../include/Widgets/menu/ArtifactMenuBar.hpp"
#include "../../include/Widgets/ArtifactMainWindow.hpp"
#include "../../include/Widgets/Render/ArtifactRenderManagerWidget.hpp"
#include "../../include/Widgets/Render/ArtifactOgreRenderWindow.hpp"
#include "../../include/Widgets/Render/ArtifactDiligentEngineRenderWindow.hpp"




namespace Artifact {

 using namespace ads;

 struct ArtifactMainWindowPrivate {


 };

 ArtifactMainWindow::ArtifactMainWindow(QWidget* parent /*= nullptr*/):QMainWindow(parent)
 {
  QPalette p = palette();
  p.setColor(QPalette::Window, QColor(30, 30, 30));

  setPalette(p);

  setAutoFillBackground(true);

  CDockManager::setConfigFlags(CDockManager::DefaultOpaqueConfig);
  CDockManager::setConfigFlag(CDockManager::RetainTabSizeWhenCloseButtonHidden, true);

  setStyleSheet("dark-style.css");

  auto menuBar = new ArtifactMenuBar(this);

  setDockNestingEnabled(true);

  setMenuBar(menuBar);

  resize(600, 640);

  auto DockManager = new CDockManager(this);

  QLabel* l = new QLabel();
  l->setWordWrap(true);
  l->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  l->setText("Lorem ipsum dolor sit amet, consectetuer adipiscing elit. ");

  // Create a dock widget with the title Label 1 and set the created label
  // as the dock widget content
  auto  DockWidget = new ads::CDockWidget("Label 1");
  DockWidget->setWidget(l);
  DockManager->addDockWidget(ads::TopDockWidgetArea, DockWidget);


  //auto render = new ArtifactRenderManagerWidget();
 
  //render->show();

  //auto qto=new QTOgreWindow();
  //qto->show();

  auto dWindow = new ArtifactDiligentEngineRenderWindow();
  dWindow->initialize();

  dWindow->show();

 }

 ArtifactMainWindow::~ArtifactMainWindow()
 {

 }

}