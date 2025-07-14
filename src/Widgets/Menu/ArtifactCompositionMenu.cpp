module;
#include <QWidget>
#include <wobjectimpl.h>
#include <QDebug>
#include <QMenu>
#include <QAction>

module Menu.Composition;


import  Project.Manager;





namespace Artifact {

 W_OBJECT_IMPL(ArtifactCompositionMenu)

 class ArtifactCompositionMenu::Impl {
 private:

 public:
  Impl(QMenu* menu);
  ~Impl();
  QAction* createCompositionAction=nullptr;
  QAction* changeCompositionSettingsAction = nullptr;
 };

 ArtifactCompositionMenu::Impl::Impl(QMenu* menu)
 {
  createCompositionAction = new QAction("Create composition");
  //createCompositionAction->setText()
  createCompositionAction->setDisabled(true);



 }

 ArtifactCompositionMenu::Impl::~Impl()
 {

 }

 ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setObjectName("CompositionMenu(&C)");

  setTitle("Composition");
  

  addAction(impl_->createCompositionAction);

  

 }

 ArtifactCompositionMenu::~ArtifactCompositionMenu()
 {

 }

 void ArtifactCompositionMenu::handleCreateCompositionRequested()
 {
  auto& instance=ArtifactProjectManager::getInstance();

  
 }

};
