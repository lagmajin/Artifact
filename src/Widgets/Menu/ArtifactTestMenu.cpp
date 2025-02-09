#include "../../../include/Widgets/Menu/Test/ArtifactImageProcessingTestMenu.hpp"

#include "../../../include/Widgets/Menu/ArtifactTestMenu.hpp"





namespace Artifact {

 ArtifactRenderTestMenu::ArtifactRenderTestMenu(QWidget* parent /*= nullptr*/):QMenu(parent)
 {
  setTitle("RenderTest");
 }

 ArtifactRenderTestMenu::~ArtifactRenderTestMenu()
 {

 }

 ArtifactTestMenu::ArtifactTestMenu(QWidget* parent /*= nullptr*/) :QMenu(parent)
 {
  setTitle("Test");

  auto imageProcessingTestMenu = new ArtifactImageProcessingTestMenu(this);

  addMenu(imageProcessingTestMenu);

 }

 ArtifactTestMenu::~ArtifactTestMenu()
 {

 }

 class ArtifactMediaTestMenuPrivate {
 private:

 public:

 };




 ArtifactMediaTestMenu::ArtifactMediaTestMenu(QWidget* parent /*= nullptr*/) :QMenu(parent)
 {

 }

 ArtifactMediaTestMenu::~ArtifactMediaTestMenu()
 {

 }



};
