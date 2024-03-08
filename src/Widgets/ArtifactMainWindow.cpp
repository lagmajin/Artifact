#include "../../include/Widgets/menu/ArtifactMenuBar.hpp"
#include "../../include/Widgets/ArtifactMainWindow.hpp"




namespace Artifact {








 ArtifactMainWindow::ArtifactMainWindow(QWidget* parent /*= nullptr*/):QMainWindow(parent)
 {
  auto menuBar = new ArtifactMenuBar(this);

  this->setMenuBar(menuBar);

 }

 ArtifactMainWindow::~ArtifactMainWindow()
 {

 }

}