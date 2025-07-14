module;
#include <QWidget>
module Menu.Layer;



//#include "../../../include/Widgets/Menu/ArtifactLayerMenu.hpp"






namespace Artifact {

 class ArtifactLayerMenu::Impl {
 private:

 public:

 };

 ArtifactLayerMenu::ArtifactLayerMenu(QWidget* parent/*=nullptr*/):QMenu(parent)
 {
  setTitle(tr("Layer"));

  //setTitle(tr("êVãK..."));
 }

 ArtifactLayerMenu::~ArtifactLayerMenu()
 {

 }

QMenu* ArtifactLayerMenu::newLayerMenu() const
 {

   return nullptr;
 }

}