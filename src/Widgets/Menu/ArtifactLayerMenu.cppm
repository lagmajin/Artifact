module;

#include <wobjectimpl.h>
#include <QMenu>
#include <QWidget>
#include <QDebug>
module Artifact.Menu.Layer;

import std; 
import Artifact.Project.Manager;
import Artifact.Service.Project;
import Artifact.Layer.InitParams;
import Artifact.Layer.Factory;


import Artifact.Widgets.CreateLayerDialog;

namespace Artifact {

 W_OBJECT_IMPL(ArtifactLayerMenu)

 class ArtifactLayerMenu::Impl {
 private:
  ArtifactLayerMenu*	parentPtr_ = nullptr;
 public:
  Impl(ArtifactLayerMenu* menu);
  QMenu* createLayerMenu = nullptr;
  QAction* createNullLayerAction_ = nullptr;
  QAction* createSolidLayerAction_ = nullptr;
  QAction* createAdjustableLayerAction_ = nullptr;
  QAction* openLayerAction_ = nullptr;
  QMenu* layerTimeMenu_ = nullptr;

  void handleProjectOpend();
  void handleProjectClosed();
  void handleCreateNullLayer();
  void handleCreateSolidLayer();
  void handleAdjustableLayer();
  void handleTextLayer();
  void handleShapeLayer();
  void handleImageLayer();
  void handleCameraLayer();
  void handleLightLayer();
  void handleModel3DLayer();
  void handleOpenLayer();
 };

 ArtifactLayerMenu::Impl::Impl(ArtifactLayerMenu* menu)
 {
  parentPtr_ = static_cast<ArtifactLayerMenu*>(menu);

  createSolidLayerAction_ = new QAction("Create solid");
  createSolidLayerAction_->setText("CreateSolid");

  createAdjustableLayerAction_ = new QAction();


  createNullLayerAction_ = new QAction("Create null layer");

  //createNullLayerAction_->setDisabled(true);

  openLayerAction_ = new QAction("Open Layer");

  openLayerAction_->setDisabled(true);

  layerTimeMenu_ = new QMenu();
  layerTimeMenu_->setTitle("Time(&T)");
  

  createLayerMenu = new QMenu("Create layer(&N)");
  //createCompositionAction->setText()
  //createLayerMenu->setDisabled(true);
  createLayerMenu->addAction(createAdjustableLayerAction_);
  createLayerMenu->addAction(createNullLayerAction_);
  createLayerMenu->addAction(createSolidLayerAction_);
  createLayerMenu->addAction(createTextLayerAction_);
  createLayerMenu->addAction(createShapeLayerAction_);
  createLayerMenu->addAction(createImageLayerAction_);
  createLayerMenu->addAction(createCameraLayerAction_);
  createLayerMenu->addAction(createLightLayerAction_);
  createLayerMenu->addAction(createModel3DLayerAction_);
  createLayerMenu->addSeparator();
  //createLayerMenu->addAction(openLayerAction_);

  menu->addMenu(createLayerMenu);
  menu->addMenu(layerTimeMenu_);
  menu->addAction(openLayerAction_);





  QObject::connect(createNullLayerAction_, &QAction::triggered, [this]() {
   handleCreateNullLayer();
   });

  QObject::connect(createSolidLayerAction_, &QAction::triggered, [this]() {
   handleCreateSolidLayer();
   });
 }

  void ArtifactLayerMenu::Impl::handleCreateSolidLayer()
  {
   qDebug() << "[handleCreateSolidLayer] Called";
   auto& manager = ArtifactProjectManager::getInstance();
 	
  //ArtifactProjectService::instance()->
 	
 	
 	
  auto createSolidLayer = new CreateSolidLayerSettingDialog();
 	
  createSolidLayer->show();
  auto service = ArtifactProjectService::instance();
 	
   ArtifactSolidLayerInitParams params("Solid");
   qDebug() << "[handleCreateSolidLayer] Creating solid layer with name: Solid";

   service->addLayerToCurrentComposition(params);
   qDebug() << "[handleCreateSolidLayer] addLayerToCurrentComposition called";
 }

 void ArtifactLayerMenu::Impl::handleCreateNullLayer()
 {
   qDebug() << "[handleCreateNullLayer] Called";
  ArtifactNullLayerInitParams params("Null");
  auto service = ArtifactProjectService::instance();
 	
  service->addLayerToCurrentComposition(params);
  
  

 }

  void ArtifactLayerMenu::Impl::handleAdjustableLayer()
  {
   qDebug() << "[handleAdjustableLayer] Called";
   // TODO: 実装
  }

  void ArtifactLayerMenu::Impl::handleTextLayer()
  {
   qDebug() << "[handleTextLayer] Called";
   // TODO: 実装
  }

  void ArtifactLayerMenu::Impl::handleShapeLayer()
  {
   qDebug() << "[handleShapeLayer] Called";
   // TODO: 実装
  }

  void ArtifactLayerMenu::Impl::handleImageLayer()
  {
   qDebug() << "[handleImageLayer] Called";
   // TODO: 実装
  }

  void ArtifactLayerMenu::Impl::handleLightLayer()
  {
   qDebug() << "[handleLightLayer] Called";
   // TODO: 実装
  }

  void ArtifactLayerMenu::Impl::handleModel3DLayer()
  {
   qDebug() << "[handleModel3DLayer] Called";
   // TODO: 実装
  }

 void ArtifactLayerMenu::Impl::handleCameraLayer()
 {
  //ArtifactLayerInitParams param;
 }

 void ArtifactLayerMenu::Impl::handleOpenLayer()
 {

 }
	

 void ArtifactLayerMenu::Impl::handleProjectClosed()
 {

 }

 ArtifactLayerMenu::ArtifactLayerMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle(tr("Layer(&L)"));

  setTearOffEnabled(true);
  setSeparatorsCollapsible(true);
  setMinimumWidth(160);
  //setTitle(tr("新規..."));

  auto projectService = ArtifactProjectService::instance();
 	
 	//connect(projectService,)
 }

 ArtifactLayerMenu::~ArtifactLayerMenu()
 {

 }

QMenu* ArtifactLayerMenu::newLayerMenu() const
 {

   return nullptr;
 }



}