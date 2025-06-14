module;
#include <QWidget>
#include <wobjectimpl.h>
module Menu:ArtifactCompositionMenu;

//#include "../../../include/Widgets/menu/ArtifactCompositionMenu.hpp"






namespace Artifact {

 W_OBJECT_IMPL(ArtifactCompositionMenu)

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
