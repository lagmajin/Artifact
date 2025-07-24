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
import Widgets.Render.Queue;
import Widgets.Render.Composition;
import Project.Manager;

namespace ArtifactWidgets {}//

namespace Artifact {

 using namespace ArtifactWidgets;

 using namespace ads;
 W_OBJECT_IMPL(ArtifactMainWindow)

  class ArtifactMainWindow::Impl {
  private:

  public:
   Impl(ArtifactMainWindow* mainWindow);
   ~Impl();
   void projectCreated();
   void compositionCreated();
   ArtifactMainWindow* mainWindow_=nullptr;
 };

 void ArtifactMainWindow::Impl::projectCreated()
 {
  qDebug() << "project created";
 }

 void ArtifactMainWindow::Impl::compositionCreated()
 {

 }

 ArtifactMainWindow::Impl::Impl(ArtifactMainWindow* mainWindow):mainWindow_(mainWindow)
 {

 }

 ArtifactMainWindow::Impl::~Impl()
 {

 }

 ArtifactMainWindow::ArtifactMainWindow(QWidget* parent /*= nullptr*/):QMainWindow(parent),impl_(new Impl(this))
 {
  QPalette p = palette();
  p.setColor(QPalette::Window, QColor(30, 30, 30));

  setPalette(p);

  setAutoFillBackground(true);

  CDockManager::setConfigFlags(CDockManager::DefaultOpaqueConfig);
  CDockManager::setConfigFlag(CDockManager::RetainTabSizeWhenCloseButtonHidden, true);

  setStyleSheet("dark-style.css");

  auto menuBar = new ArtifactMenuBar(this);
  menuBar->setMainWindow(this);

  setDockNestingEnabled(true);

  setMenuBar(menuBar);

  resize(600, 640);

  auto DockManager = new CDockManager(this);

  //QLabel* l = new QLabel();
  //l->setWordWrap(true);
  //l->setAlignment(Qt::AlignTop | Qt::AlignLeft);

  //auto  DockWidget = new Pane("Label 1",l);
  //DockWidget->setWidget(l);
  //DockManager->addDockWidget(ads::TopDockWidgetArea, DockWidget);


  //auto render = new ArtifactRenderManagerWidget();
 
  //render->show();

  //auto qto=new QTOgreWindow();
  //qto->show();

  //auto dWindow = new ArtifactDiligentEngineRenderWindow();
  //dWindow->initialize();

  //dWindow->show();

  auto projectManagerWidget = new ArtifactProjectManagerWidget();

  projectManagerWidget->show();
  auto  DockWidget2 = new Pane("Project", projectManagerWidget);

  //DockWidget2->setWidget(projectManagerWidget);

  DockManager->addDockWidget(ads::LeftDockWidgetArea, DockWidget2);

  auto imageView = new BasicImageViewWidget();
 // imageView->show();

  auto dockwidget5 = new Pane("Image Viewer", imageView);
  //dockwidget5->setWidget(imageView);
  DockManager->addDockWidget(ads::CenterDockWidgetArea, dockwidget5);


  

  
  
  auto inspectorWidget2 = new ArtifactInspectorWidget();
  auto  DockWidget3 = new Pane("Inspector", inspectorWidget2);
  //DockWidget3->setWidget(inspectorWidget2);

  DockManager->addDockWidget(ads::RightDockWidgetArea, DockWidget3);

  
  auto renderManagerWidget = new RenderQueueManagerWidget();

  auto  DockWidget4 = new Pane("RenderQueueManager", renderManagerWidget);
  DockWidget4->setWindowIcon(QIcon::fromTheme("folder"));

  //DockWidget4->setWidget(renderManagerWidget);

  DockManager->addDockWidget(ads::BottomDockWidgetArea, DockWidget4);
 
  
  auto compositionWidget = new ArtifactDiligentEngineComposition2DWindow();

  auto  DockWidget5 = new Pane("CompositionWidget", compositionWidget);
  //DockWidget5->setWidget(compositionWidget);

  DockManager->addDockWidget(ads::CenterDockWidgetArea, DockWidget5);

  auto& projectManager = ArtifactProjectManager::getInstance();

 
  QObject::connect(&projectManager, &ArtifactProjectManager::newProjectCreated, [this]() {
   impl_->projectCreated();
   });

  QObject::connect(&projectManager, &ArtifactProjectManager::newCompositionCreated, [this]() {
   //impl_->compositionCreated();

   //impl_->compositionCreated(this);
   });

  //auto compositionWidget2 = new ArtifactDiligentEngineComposition2DWidget();

  //compositionWidget2->show();

  //auto compositionWidget3 = new ArtifactDiligentEngineComposition2DWindow();

  //compositionWidget3->show();
 }

 ArtifactMainWindow::~ArtifactMainWindow()
 {
  delete impl_;
 }

 void ArtifactMainWindow::addWidget()
 {

 }

 void ArtifactMainWindow::addDockedWidget(const QString& title, ads::DockWidgetArea area, QWidget* widget)
 {

 }


}