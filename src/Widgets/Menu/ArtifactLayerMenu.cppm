module;

#include <wobjectimpl.h>
#include <QMenu>
#include <QWidget>
module Artifact.Menu.Layer;

import std; 
import Project.Manager;




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

  void handleCreateNullLayer();
  void handleCreateSolidLayer();
  void handleAdjustableLayer();
  void handleCameraLayer();

  void handleOpenLayer();
 };

 ArtifactLayerMenu::Impl::Impl(ArtifactLayerMenu* menu)
 {
  parentPtr_ = static_cast<ArtifactLayerMenu*>(menu);

  createSolidLayerAction_ = new QAction("Create solid");
  createSolidLayerAction_->setText("CreateSolid");

  createAdjustableLayerAction_ = new QAction();


  createNullLayerAction_ = new QAction("Create null layer");

  createNullLayerAction_->setDisabled(true);

  openLayerAction_ = new QAction("Open Layer");

  openLayerAction_->setDisabled(true);

  layerTimeMenu_ = new QMenu();
  layerTimeMenu_->setTitle("Time(&T)");
  

  createLayerMenu = new QMenu("Create layer(&N)");
  //createCompositionAction->setText()
  //createLayerMenu->setDisabled(true);
  createLayerMenu->addAction(createSolidLayerAction_);
  createLayerMenu->addAction(createNullLayerAction_);
  createLayerMenu->addAction(createAdjustableLayerAction_);
  createLayerMenu->addSeparator();
  //createLayerMenu->addAction(openLayerAction_);

  menu->addMenu(createLayerMenu);
  menu->addMenu(layerTimeMenu_);
  menu->addAction(openLayerAction_);





  QObject::connect(createNullLayerAction_, &QAction::triggered, [this]() {
   handleCreateNullLayer();
   });

	
 }

 void ArtifactLayerMenu::Impl::handleCreateSolidLayer()
 {
  auto& manager = ArtifactProjectManager::getInstance();


  
 }

 void ArtifactLayerMenu::Impl::handleCreateNullLayer()
 {
  auto& manager = ArtifactProjectManager::getInstance();

  

 }

 void ArtifactLayerMenu::Impl::handleAdjustableLayer()
 {

 }

 void ArtifactLayerMenu::Impl::handleCameraLayer()
 {

 }

 void ArtifactLayerMenu::Impl::handleOpenLayer()
 {

 }

 ArtifactLayerMenu::ArtifactLayerMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle(tr("Layer(&L)"));

  setTearOffEnabled(true);

  //setTitle(tr("新規..."));


 }

 ArtifactLayerMenu::~ArtifactLayerMenu()
 {

 }

QMenu* ArtifactLayerMenu::newLayerMenu() const
 {

   return nullptr;
 }



}