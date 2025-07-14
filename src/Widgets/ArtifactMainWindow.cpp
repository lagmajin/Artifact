module;


#include<wobjectimpl.h>


#include <DockWidget.h>
#include <DockManager.h>

#include <QLabel>


#pragma comment(lib,"qtadvanceddockingd.lib")

//#include "../../include/Widgets/menu/ArtifactMenuBar.hpp"
//#include "../../include/Widgets/Render/ArtifactRenderManagerWidget.hpp"
//#include "../../include/Widgets/Render/ArtifactOgreRenderWindow.hpp"
//#include "../../include/Widgets/Render/ArtifactDiligentEngineRenderWindow.hpp"

module ArtifactMainWindow;

import Menu;
import ArtifactProjectManagerWidget;
import DockWidget;
import BasicImageViewWidget;
import Widgets.Inspector;

namespace ArtifactWidgets {}//

namespace Artifact {

 using namespace ArtifactWidgets;

 using namespace ads;
 W_OBJECT_IMPL(ArtifactMainWindow)
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

  auto  DockWidget = new Pane("Label 1",l);
  //DockWidget->setWidget(l);
  DockManager->addDockWidget(ads::TopDockWidgetArea, DockWidget);


  //auto render = new ArtifactRenderManagerWidget();
 
  //render->show();

  //auto qto=new QTOgreWindow();
  //qto->show();

  //auto dWindow = new ArtifactDiligentEngineRenderWindow();
  //dWindow->initialize();

  //dWindow->show();

  auto projectManagerWidget = new ArtifactProjectManagerWidget();

  projectManagerWidget->show();
  auto  DockWidget2 = new Pane("Project",nullptr);

  DockWidget2->setWidget(projectManagerWidget);

  DockManager->addDockWidget(ads::LeftDockWidgetArea, DockWidget2);

  auto imageView = new BasicImageViewWidget();
  imageView->show();
  auto  DockWidget3 = new Pane("Inspector", nullptr);
  
  auto inspectorWidget2 = new ArtifactInspectorWidget();

  DockWidget3->setWidget(inspectorWidget2);

  DockManager->addDockWidget(ads::RightDockWidgetArea, DockWidget3);
 }

 ArtifactMainWindow::~ArtifactMainWindow()
 {

 }

 void ArtifactMainWindow::addWidget()
 {

 }

 void ArtifactMainWindow::addDockedWidget(QWidget* widget, const QString& title, ads::DockWidgetArea area)
 {
  //auto dock = new ads::CDockWidget(title);
  //dock->setWidget(widget);
  //m_dockManager->addDockWidget(area, dock);
 }

}