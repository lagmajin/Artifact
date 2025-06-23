

module Menu:MenuBar;

import Menu;





namespace Artifact {

 ArtifactMenuBar::ArtifactMenuBar(QWidget* parent) :QMenuBar(parent)
 {
  auto fileMenu = new ArtifactFileMenu(this);
  auto viewMenu = new ArtifactViewMenu(this);
  //auto compositionMenu = new ArtifactCompositionMenu(this);

  //auto testMenu = new ArtifactTestMenu(this);

  //auto compositionMenu=new artifact

  addMenu(fileMenu);
  addMenu(viewMenu);
  //addMenu(compositionMenu);
  //addMenu(testMenu);
 }

 ArtifactMenuBar::~ArtifactMenuBar()
 {

 }


};