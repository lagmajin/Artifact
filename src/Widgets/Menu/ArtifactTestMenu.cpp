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

 }

 ArtifactTestMenu::~ArtifactTestMenu()
 {

 }

 class ArtifactMediaTestMenuPrivate {
 private:

 public:

 };

 ArtifactMediaTestMenu::ArtifactMediaTestMenu(QWidget* parent /*= nullptr*/):QMenu(parent)
 {

 }

 ArtifactMediaTestMenu::~ArtifactMediaTestMenu()
 {

 }

};
