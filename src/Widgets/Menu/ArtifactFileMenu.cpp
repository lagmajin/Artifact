#include "../../../include/Widgets/menu/ArtifactFileMenu.hpp"






namespace Artifact {


 ArtifactFileMenu::ArtifactFileMenu(QWidget* parent /*= nullptr*/)
 {
  setObjectName("FileMenu");

  setTitle("File");

  QPalette p = palette();
  p.setColor(QPalette::Window, QColor(30, 30, 30));

  setPalette(p);

  setAutoFillBackground(true);
 }

 ArtifactFileMenu::~ArtifactFileMenu()
 {

 }












};