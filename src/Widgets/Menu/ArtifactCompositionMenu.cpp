#include "../../../include/Widgets/menu/ArtifactCompositionMenu.hpp"






namespace Artifact {

 class ArtifactCompositionMenuPrivate {
 private:

 public:

 };

 ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* parent/*=nullptr*/):QMenu(parent)
 {
  setObjectName("CompositionMenu(&C)");

  setTitle("Composition");

 }

 ArtifactCompositionMenu::~ArtifactCompositionMenu()
 {

 }

};
