#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _SCL_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif
module;
#include<wobjectimpl.h>
#include <DockWidget.h>
#include <DockManager.h>
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>


#include <qcoro6/qcoro/qcorotask.h>
#include <qcoro6/qcoro/coroutine.h>
#include <qcoro6/qcoro/qcorotimer.h>
#include <objbase.h>

//#pragma comment(lib,"qtadvanceddockingd.lib")

module Artifact.MainWindow;

import std;
import Menu;

import Utils.Id;
import UI.SelectionManager;
import Input.Operator;
import Render.Queue.Manager;
import Artifact.Composition.Abstract;


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
import Artifact.Service.Playback;

import Artifact.Widgets.Timeline;
import Artifact.Widgets.ProjectManagerWidget;
import Artifact.Widgets.Render.Layer;
import Artifact.Widgets.LayerEditorPanel;

import Artifact.Widgets.Render.QueueManager;
import Artifact.Widgets.RenderQueueJobPanel;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Widgets.PlaybackControlWidget;
import Artifact.Widgets.RenderLayerWidgetv2;
import WinJumpList;
import AppProgress;
//import Platform.Mac.TouchBar;


namespace ArtifactWidgets {}//

namespace Artifact {

 using namespace ArtifactCore;
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
   ArtifactTimelineWidget* timelineWidget_ = nullptr;

   // Playback service
   ArtifactPlaybackService* playbackService_ = nullptr;

   // Status bar widgets
   QLabel* statusLabel_ = nullptr;
   QLabel* coordinatesLabel_ = nullptr;
   QLabel* zoomLabel_ = nullptr;
   QLabel* memoryLabel_ = nullptr;
   QLabel* fpsLabel_ = nullptr;

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

  if (!timelineWidget_) {
   // nullptr を親として渡して独立ウィンドウとして作成
   // window への参照が必要な場合は別途メソッドで設定
   timelineWidget_ = new ArtifactTimelineWidget(nullptr);

   // ウィンドウの属性を設定：独立ウィンドウとして表示
   timelineWidget_->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
   timelineWidget_->setAttribute(Qt::WA_DeleteOnClose, false);
  }
  timelineWidget_->setComposition(id);
  timelineWidget_->show();

  // Unified Progress API (Taskbar on Win, Dock on Mac)
  //AppProgress::setProgress(window, 50, 100);
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

  setStyleSheet("");

  auto menuBar = new ArtifactMenuBar(this);
  menuBar->setMainWindow(this);

  setDockNestingEnabled(true);
  
  
  // Setup status bar with multiple widgets
  auto statusbar = statusBar();
  statusbar->setStyleSheet(R"(
      QStatusBar {
          background: #181818;
          border-top: 1px solid #2d2d2d;
          min-height: 24px;
          color: #888;
      }
      QStatusBar::item { border: none; }
  )");
  
  // Create status bar labels
  auto statusContainer = new QWidget();
  auto statusLayout = new QHBoxLayout(statusContainer);
  statusLayout->setContentsMargins(8, 0, 8, 0);
  statusLayout->setSpacing(6);
  
  auto statusDot = new QLabel();
  statusDot->setFixedSize(6, 6);
  statusDot->setStyleSheet("background-color: #4CAF50; border-radius: 3px;");
  
  auto statusLabel = new QLabel("READY");
  statusLabel->setStyleSheet("color: #ccc; font-size: 10px; font-weight: bold; font-family: 'Segoe UI';");
  statusLabel->setObjectName("statusLabel");
  
  statusLayout->addWidget(statusDot);
  statusLayout->addWidget(statusLabel);
  statusLayout->addStretch();

  auto coordinatesLabel = new QLabel("X: 0 | Y: 0");
  coordinatesLabel->setMinimumWidth(100);
  coordinatesLabel->setAlignment(Qt::AlignCenter);
  coordinatesLabel->setObjectName("coordinatesLabel");
  coordinatesLabel->setStyleSheet("color: #888; border-left: 1px solid #2d2d2d; font-size: 10px; padding: 0 10px; font-family: 'Segoe UI';");
  
  auto zoomLabel = new QLabel("ZOOM: 100%");
  zoomLabel->setMinimumWidth(80);
  zoomLabel->setAlignment(Qt::AlignCenter);
  zoomLabel->setObjectName("zoomLabel");
  zoomLabel->setStyleSheet("color: #888; border-left: 1px solid #2d2d2d; font-size: 10px; padding: 0 10px; font-family: 'Segoe UI';");
  
  auto memoryLabel = new QLabel("MEM: 0 MB");
  memoryLabel->setMinimumWidth(90);
  memoryLabel->setAlignment(Qt::AlignCenter);
  memoryLabel->setObjectName("memoryLabel");
  memoryLabel->setStyleSheet("color: #888; border-left: 1px solid #2d2d2d; font-size: 10px; padding: 0 10px; font-family: 'Segoe UI';");
  
  auto fpsLabel = new QLabel("FPS: 00.0");
  fpsLabel->setMinimumWidth(80);
  fpsLabel->setAlignment(Qt::AlignCenter);
  fpsLabel->setObjectName("fpsLabel");
  fpsLabel->setStyleSheet("color: #888; border-left: 1px solid #2d2d2d; font-size: 10px; padding: 0 10px; font-family: 'Segoe UI';");
  
  // Add widgets to status bar
  statusbar->addWidget(statusContainer, 1);
  statusbar->addPermanentWidget(fpsLabel);
  statusbar->addPermanentWidget(memoryLabel);
  statusbar->addPermanentWidget(zoomLabel);
  statusbar->addPermanentWidget(coordinatesLabel);

  
  // Store references in impl
  impl_->statusLabel_ = statusLabel;
  impl_->coordinatesLabel_ = coordinatesLabel;
  impl_->zoomLabel_ = zoomLabel;
  impl_->memoryLabel_ = memoryLabel;
  impl_->fpsLabel_ = fpsLabel;
  setMenuBar(menuBar);

  resize(600, 640);
  resize(1280, 900);

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

   // Use newer CDockWidget(title, widget) constructor to avoid deprecated overload
   auto centralDockWidget = new CDockWidget("centralWidget", nullptr);
  DockManager->setCentralWidget(centralDockWidget);

  DockManager->addDockWidget(ads::CenterDockWidgetArea, imageViewer, nullptr);


  auto projectManagerWidget = new ArtifactProjectManagerWidget();

  projectManagerWidget->show();
  auto  DockWidget2 = new Pane("Artifact.Project", projectManagerWidget);

  //DockWidget2->setWidget(projectManagerWidget);

  DockManager->addDockWidget(ads::LeftDockWidgetArea, DockWidget2);
 	
  
  auto inspectorWidget2 = new ArtifactInspectorWidget();
  inspectorWidget2->show();
  //auto  inspectorWidgetWrapper = new Pane("Inspector", inspectorWidget2);
  //DockWidget3->setWidget(inspectorWidget2);
  
 	
 	
  //DockManager->addDockWidget(ads::RightDockWidgetArea, inspectorWidgetWrapper);

  //auto mediaControlWidget = new ArtifactPlaybackControlWidget();
 	
  //mediaControlWidget->show();
 	

  auto& projectManager = ArtifactProjectManager::getInstance();


  //auto layerPreveiw =new ArtifactLayerEditorPanel();
  //layerPreveiw->show();

 	
 
    auto layerPreview2=new ArtifactLayerEditorWidgetV2();
   layerPreview2->show();

   auto assetBrowser = new ArtifactAssetBrowser();
   assetBrowser->show();
 	
  QObject::connect(&projectManager, &ArtifactProjectManager::projectCreated, this, [this]() {
   impl_->handleProjectCreated();
   });

  QObject::connect(&projectManager, &ArtifactProjectManager::compositionCreated, this, [this](const CompositionID& id) {
   impl_->handleCompositionCreated(id,this);
   
   
   });
   
   
   //ArtifactLayerEditor2DWidgetV2

  //auto compositionWidget2 = new ArtifactDiligentEngineComposition2DWidget();

  //compositionWidget2->show();

  //auto compositionWidget3 = new ArtifactDiligentEngineComposition2DWindow();

  //compositionWidget3->show();
 	
  //NativeGUIHelper::applyMicaEffect(this);
  
  // Mica背景のためにウィンドウ表示前に設定（showEvent後の設定はネイティブウィンドウ再生成→qregionクラッシュの原因になる）
  setAttribute(Qt::WA_TranslucentBackground);
  setStyleSheet("QMainWindow { background: transparent; }");
 
  // Register action handlers for UI/System integration
  auto* am = ActionManager::instance();
  am->getAction("artifact.layer.create_from_asset")->setExecuteCallback([this](const QVariantMap& params) {
      QString assetIdStr = params["assetId"].toString();
      QPointF dropPos = params["dropPos"].toPointF();
      
      auto& projectManager = ArtifactProjectManager::getInstance();
      Id compId = SelectionManager::instance().activeComposition();
      
      // If no active composition in SelectionManager, try the one from ProjectManager
      if (compId.isNull()) {
          auto currentComp = projectManager.currentComposition();
          if (currentComp) compId = currentComp->id();
      }
      
      if (!compId.isNull()) {
          qDebug() << "Handling Drag & Drop: Creating layer from asset" << assetIdStr << "at" << dropPos;
          
          // Create a named Image/Footage layer init params
          // Note: In a real app, we would look up the asset name from the ID
          ArtifactLayerInitParams layerParams("New Asset Layer", LayerType::Image);
          
          auto result = projectManager.addLayerToComposition(compId, layerParams);
          if (result.success) {
              statusBar()->showMessage(tr("Created layer from asset"), 2000);
          }
      }
  });

  am->getAction("artifact.comp.add_to_render_queue")->setExecuteCallback([this](const QVariantMap& params) {
      Id compId = SelectionManager::instance().activeComposition();
      if (!compId.isNull()) {
          // In real app, we'd get the comp name from projectManager
          RendererQueueManager::instance().addJob(compId, "Rendered Composition");
          statusBar()->showMessage(tr("Added to Render Queue (Ctrl+M)"), 2000);
      }
  });

  // macOS Touch Bar setup
  //MacTouchBar::install(this);
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
  statusBar()->showMessage(message, timeoutMs);
  if (impl_->statusLabel_) {
   impl_->statusLabel_->setText(message);
  }
 }

 void ArtifactMainWindow::closeAllDocks()
 {

 }

 void ArtifactMainWindow::setStatusZoomLevel(float zoomPercent) { if (impl_->zoomLabel_) impl_->zoomLabel_->setText(QString("ZOOM: %1%").arg(static_cast<int>(zoomPercent))); }

 void ArtifactMainWindow::setStatusCoordinates(int x, int y)
 {
  if (impl_->coordinatesLabel_) {
   impl_->coordinatesLabel_->setText(QString("X: %1 | Y: %2").arg(x).arg(y));
  }
 }

 void ArtifactMainWindow::setStatusMemoryUsage(uint64_t memoryMB) { if (impl_->memoryLabel_) impl_->memoryLabel_->setText(QString("MEM: %1 MB").arg(memoryMB)); }

 void ArtifactMainWindow::setStatusFPS(double fps) { if (impl_->fpsLabel_) impl_->fpsLabel_->setText(QString("FPS: %1").arg(QString::number(fps, 'f', 1))); }

 void ArtifactMainWindow::setStatusReady()
 {
  if (impl_->statusLabel_) {
   impl_->statusLabel_->setText("Ready");
  }
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
   // tr() はメインスレッドで取得してからスレッドへ渡す
   const QString title = tr("New Window");
   const QString desc  = tr("Open a new Artifact window");
   // WinJumpList の COM 操作を独立スレッドで実行 (Qt メッセージループと完全分離)
   std::thread([title, desc]() {
    ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WinJumpList jumpList;
    jumpList.addNewWindowTask(title, desc);
    jumpList.apply();
    ::CoUninitialize();
   }).detach();
  });
 }

}