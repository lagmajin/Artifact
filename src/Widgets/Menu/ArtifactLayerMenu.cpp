#include "../../../include/Widgets/Menu/ArtifactLayerMenu.hpp"






namespace Artifact {

class ArtifactLayerMenuPrivate {

 };

 ArtifactLayerMenu::ArtifactLayerMenu(QWidget* parent/*=nullptr*/):QMenu(parent)
 {
  //setTitle(tr("Layer"));

  //setTitle(tr("V‹K..."));
 }

 ArtifactLayerMenu::~ArtifactLayerMenu()
 {

 }

QMenu* ArtifactLayerMenu::newLayerMenu() const
 {

   return nullptr;
 }

}