#include "../../../include/Widgets/Menu/ArtifactViewMenu.hpp"





namespace Artifact {


 ArtifactViewMenu::ArtifactViewMenu(QWidget* parent/*=nullptr*/):QMenu(parent)
 {
  setObjectName("ViewMenu");

  setTitle("View");
 }

 ArtifactViewMenu::~ArtifactViewMenu()
 {

 }

};