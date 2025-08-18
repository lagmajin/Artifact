module;
#include <QMenu>
#include <QWidget>
module Artifact.Menu.Layer;


//#include "../../../include/Widgets/Menu/ArtifactLayerMenu.hpp"






namespace Artifact {

 class ArtifactLayerMenu::Impl {
 private:

 public:
  Impl(QMenu* menu);
  QMenu* createLayerMenu = nullptr;
  QAction* createNullLayerAction_ = nullptr;
  QAction* createSolidLayerAction_ = nullptr;
  QAction* createAdjustableLayerAction_ = nullptr;
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

  layerTimeMenu_ = new QMenu();
  layerTimeMenu_->setTitle("Time(&T)");
  

  createLayerMenu = new QMenu("Create layer(&N)");
  //createCompositionAction->setText()
  //createLayerMenu->setDisabled(true);
  createLayerMenu->addAction(createSolidLayerAction_);
  createLayerMenu->addAction(createNullLayerAction_);
  

  menu->addMenu(createLayerMenu);
  menu->addMenu(layerTimeMenu_);




	
 }

 void ArtifactLayerMenu::Impl::handleCreateSolidLayer()
 {

 }

 void ArtifactLayerMenu::Impl::handleCreateNullLayer()
 {

 }

 void ArtifactLayerMenu::Impl::handleAdjustableLayer()
 {

 }

 ArtifactLayerMenu::ArtifactLayerMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle(tr("Layer(&L)"));

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