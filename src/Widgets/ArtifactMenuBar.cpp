#include "../../include/Widgets/Menu/ArtifactFileMenu.hpp"
#include "../../include/Widgets/Menu/ArtifactCompositionMenu.hpp"

#include "../../include/Widgets/menu/ArtifactMenuBar.hpp"





namespace Artifact {

 ArtifactMenuBar::ArtifactMenuBar(QWidget* parent) :QMenuBar(parent)
 {
  auto fileMenu = new ArtifactFileMenu(this);

  auto compositionMenu = new ArtifactCompositionMenu(this);

  //auto compositionMenu=new artifact

  addMenu(fileMenu);
  addMenu(compositionMenu);

 }

 ArtifactMenuBar::~ArtifactMenuBar()
 {

 }


};