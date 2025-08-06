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
  QAction* createLayerMenu = nullptr;
  QAction* createNullLayerMenu=nullptr;
 };

 ArtifactLayerMenu::Impl::Impl(QMenu* menu)
 {
  createLayerMenu = new QAction("Create layer");
  //createCompositionAction->setText()
  createLayerMenu->setDisabled(true);

  menu->addAction(createLayerMenu);

  createNullLayerMenu = new QAction("Create composition from footage");
  //createCompositionAction->setText()
  createNullLayerMenu->setDisabled(true);
 }

 ArtifactLayerMenu::ArtifactLayerMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle(tr("Layer"));

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