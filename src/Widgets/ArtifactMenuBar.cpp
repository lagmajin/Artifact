#include "../../include/Widgets/menu/ArtifactMenuBar.hpp"
#include "../../include/Widgets/Menu/ArtifactFileMenu.hpp"



namespace Artifact {

 ArtifactMenuBar::ArtifactMenuBar(QWidget* parent) :QMenuBar(parent)
 {
  auto fileMenu = new ArtifactFileMenu(this);

  addMenu(fileMenu);


 }

 ArtifactMenuBar::~ArtifactMenuBar()
 {

 }


};