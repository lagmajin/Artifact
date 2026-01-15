module;
#include<wobjectimpl.h>
#include <DockWidget.h>
#include <DockManager.h>
#include <QLabel>
#include <QMessageBox>


#include <qcoro6/qcoro/qcorotask.h>
#include <qcoro6/qcoro/coroutine.h>
#include <qcoro6/qcoro/qcorotimer.h>

//#pragma comment(lib,"qtadvanceddockingd.lib")

module Artifact.MainWindow;

import std;
import Menu;

import Utils.Id;


import Widgets.NativeHelper;
import DockWidget;
import BasicImageViewWidget;
import Widgets.ToolBar;
import Widgets.Inspector;
//import Widgets.Render.Queue;
import Widgets.Render.Composition;
import Widgets.AssetBrowser;
import Widgets.KeyboardOverlayDialog;

import Artifact.Project.Manager;
import Artifact.Service.Project;

import Artifact.Widgets.Timeline;
import Artifact.Widgets.ProjectManagerWidget;
import Artifact.Widgets.Render.Layer;
import Artifact.Widgets.LayerEditorPanel;

import Artifact.Widgets.Render.QueueManager;
import Artifact.Widgets.RenderQueueJobPanel;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Widgets.PlaybackControlWidget;
import Artifact.Widgets.RenderLayerWidgetv2;


namespace ArtifactWidgets {}//

namespace Artifact {

 using namespace ArtifactWidgets;
 using namespace std::chrono_literals;
 using namespace ads;


 // ReSharper disable CppInspection
 W_OBJECT_IMPL(ArtifactMainWindow)
  // ReSharper restore CppInspection
	
  class ArtifactMainWindow::Impl {
  private:

  public:
   Impl(ArtifactMainWindow* mainWindow);
   ~Impl();
   void setupUi();

   void handleProjectCreated();
   void handleCompositionCreated();
   void handleCompositionCreated(const CompositionID id, ArtifactMainWindow* window);
   void handleLayerCreated(ArtifactMainWindow* window);
 	ArtifactMainWindow* mainWindow_=nullptr;

   void handleDefaultKeyPressEvent(QKeyEvent* event);
   void handleDefaultKeyReleaseEvent(QKeyEvent* event);
    QCoro::Task<> setupUiAsync();
 };

 void ArtifactMainWindow::Impl::handleProjectCreated()
 {
  qDebug() << "project created";
 	
  mainWindow_->setWindowTitle("Artifact - New Project");
 	
 }

 void ArtifactMainWindow::Impl::handleCompositionCreated(const CompositionID id, ArtifactMainWindow* window)
 {
  qDebug() << "composition created" <<id.toString();

  auto timeTest = new ArtifactLayerTimelinePanelWrapper(id);
  timeTest->show();

  //auto timelineWidget = new ArtifactTimelineWidget();

  //timelineWidget->show();
 }

 ArtifactMainWindow::Impl::Impl(ArtifactMainWindow* mainWindow):mainWindow_(mainWindow)
 {

 }

 ArtifactMainWindow::Impl::~Impl()
 {

 }

 void ArtifactMainWindow::Impl::handleDefaultKeyPressEvent(QKeyEvent* event)
 {

 }

 void ArtifactMainWindow::Impl::handleDefaultKeyReleaseEvent(QKeyEvent* event)
 {

 }

 void ArtifactMainWindow::Impl::setupUi()
 {
  auto pal = mainWindow_->palette();
  pal.setColor(QPalette::Window, QColor(30, 30, 30));

  mainWindow_->setPalette(pal);


  mainWindow_->setAutoFillBackground(true);
 }

 QCoro::Task<> ArtifactMainWindow::Impl::setupUiAsync()
 {
  co_await QCoro::sleepFor(0ms);
 }

 ArtifactMainWindow::ArtifactMainWindow(QWidget* parent /*= nullptr*/):QMainWindow(parent),impl_(new Impl(this))
 {
  impl_->setupUi();

 

  CDockManager::setConfigFlags(CDockManager::DefaultOpaqueConfig);
  CDockManager::setConfigFlag(CDockManager::RetainTabSizeWhenCloseButtonHidden, true);

  setStyleSheet("dark-style.css");

  auto menuBar = new ArtifactMenuBar(this);
  menuBar->setMainWindow(this);

  setDockNestingEnabled(true);
  statusBar();
  setMenuBar(menuBar);

  resize(600, 640);

  auto toolBar = new ArtifactToolBar(this);
  addToolBar(toolBar);


  auto DockManager = new CDockManager(this);
  DockManager->setStyleSheet(R"(
    QDockWidget::title {
        background: rgb(50,50,50);
        padding-left: 4px;
    }
)");
  auto imageView = new BasicImageViewWidget();
  auto imageViewer = new Pane("Image Viewer", imageView);

  auto centralDockWidget = new CDockWidget("centralWidget");
  DockManager->setCentralWidget(centralDockWidget);

  DockManager->addDockWidget(ads::CenterDockWidgetArea, imageViewer, nullptr);


  auto projectManagerWidget = new ArtifactProjectManagerWidget();

  projectManagerWidget->show();
  auto  DockWidget2 = new Pane("Artifact.Project", projectManagerWidget);

  //DockWidget2->setWidget(projectManagerWidget);

  DockManager->addDockWidget(ads::LeftDockWidgetArea, DockWidget2);
 	
  
  auto inspectorWidget2 = new ArtifactInspectorWidget();
  auto  inspectorWidgetWrapper = new Pane("Inspector", inspectorWidget2);
  //DockWidget3->setWidget(inspectorWidget2);

 	
 	
  DockManager->addDockWidget(ads::RightDockWidgetArea, inspectorWidgetWrapper);

  auto mediaControlWidget = new ArtifactPlaybackControlWidget();
 	
  mediaControlWidget->show();
 	

  auto& projectManager = ArtifactProjectManager::getInstance();


  auto layerPreveiw =new ArtifactLayerEditorPanel();
  layerPreveiw->show();

	auto timelineWidget = new ArtifactTimelineWidget();
	timelineWidget->show();
 
    auto layerPreview2=new ArtifactLayerEditorWidgetV2();
   layerPreview2->show();
 	
  QObject::connect(&projectManager, &ArtifactProjectManager::projectCreated, [this]() {
   impl_->handleProjectCreated();
   });

  QObject::connect(&projectManager, &ArtifactProjectManager::compositionCreated, [this](const CompositionID& id) {
   impl_->handleCompositionCreated(id,this);
   
   
   });
   
   
   //ArtifactLayerEditor2DWidgetV2

  //auto compositionWidget2 = new ArtifactDiligentEngineComposition2DWidget();

  //compositionWidget2->show();

  //auto compositionWidget3 = new ArtifactDiligentEngineComposition2DWindow();

  //compositionWidget3->show();
 	
  //NativeGUIHelper::applyMicaEffect(this);
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

 void ArtifactMainWindow::showStatusMessage(const QString& message, int timeoutMs /*= 2000*/)
 {

 }

 void ArtifactMainWindow::closeAllDocks()
 {

 }

 void ArtifactMainWindow::keyPressEvent(QKeyEvent* event)
 {
  QMainWindow::keyPressEvent(event);

 }

 void ArtifactMainWindow::keyReleaseEvent(QKeyEvent* event)
 {
  QMainWindow::keyReleaseEvent(event);
 }

 void ArtifactMainWindow::closeEvent(QCloseEvent* event)
 {
  auto ret = QMessageBox::question(
   this,
   tr("確認"),
   tr("終了しますか？"),
   QMessageBox::Yes | QMessageBox::No
  );

  if (ret == QMessageBox::Yes) {
   event->accept();   // 閉じる
  }
  else {
   event->ignore();   // 閉じない
  }
 }

 void ArtifactMainWindow::showEvent(QShowEvent* event)
 {
  QMainWindow::showEvent(event); // 親の処理を呼ぶ
  QTimer::singleShot(0, this, [this]() {
   NativeGUIHelper::applyMicaEffect(this);
   NativeGUIHelper::applyWindowRound(this);
  });
 }

}