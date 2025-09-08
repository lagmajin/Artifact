module;
#include <QMenu>
#include <QWidget>
module Artifact.Menu.Layer;


import Project.Manager;




namespace Artifact {

 class ArtifactLayerMenu::Impl {
 private:

 public:
  Impl(QMenu* menu);
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
 };

 ArtifactLayerMenu::Impl::Impl(QMenu* menu)
 {
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



	//connect(createSolidLayerAction_,&QAction::trigger,)


	
 }

 void ArtifactLayerMenu::Impl::handleCreateSolidLayer()
 {
  auto& manager = ArtifactProjectManager::getInstance();

 }

 void ArtifactLayerMenu::Impl::handleCreateNullLayer()
 {

 }

 void ArtifactLayerMenu::Impl::handleAdjustableLayer()
 {

 }

 void ArtifactLayerMenu::Impl::handleCameraLayer()
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