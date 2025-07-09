module;
#include <QWidget>
#include <wobjectimpl.h>
#include <QAction>
module Menu.Composition;








namespace Artifact {

 W_OBJECT_IMPL(ArtifactCompositionMenu)

 class ArtifactCompositionMenu::Impl {
 private:

 public:
  Impl(QMenu* menu);
  QAction* createCompositionAction;

 };

 ArtifactCompositionMenu::Impl::Impl(QMenu* menu)
 {

 }

 ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setObjectName("CompositionMenu(&C)");

  setTitle("Composition");
  
  
  

 }

 ArtifactCompositionMenu::~ArtifactCompositionMenu()
 {

 }

 void ArtifactCompositionMenu::handleCreateCompositionRequested()
 {

 }

};
