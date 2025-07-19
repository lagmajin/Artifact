module;
#include <QObject>
#include <QWidget>
#include <wobjectimpl.h>
#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QDialog>

module Menu.Composition;


import  Project.Manager;

import Dialog.Composition;



namespace Artifact {

 W_OBJECT_IMPL(ArtifactCompositionMenu)

 class ArtifactCompositionMenu::Impl {
 //W_OBJECT(ArtifactCompositionMenu::Impl)
 private:

 public:
  Impl(QMenu* menu);
  ~Impl();
  QAction* createCompositionAction=nullptr;
  QAction* createCompositionFromFootage = nullptr;
  QAction* changeCompositionSettingsAction = nullptr;

  QAction* saveAsFrameAction = nullptr;

  QAction* addToRenderQueue = nullptr;

  void showCreateCompositionSettingDialog();
 };

 //W_OBJECT_IMPL(ArtifactCompositionMenu::Impl)

 ArtifactCompositionMenu::Impl::Impl(QMenu* menu)
 {
  createCompositionAction = new QAction("Create composition");
  //createCompositionAction->setText()
  //createCompositionAction->setDisabled(true);

  menu->addAction(createCompositionAction);

  createCompositionFromFootage = new QAction("Create composition from footage");
  //createCompositionAction->setText()
  createCompositionFromFootage->setDisabled(true);

  changeCompositionSettingsAction = new QAction("Change composition settings");

  changeCompositionSettingsAction->setDisabled(true);
  
  saveAsFrameAction = new QAction("Save as image file");
  //createCompositionAction->setText()
  saveAsFrameAction->setDisabled(true);

  connect(createCompositionAction, &QAction::triggered, menu, [this]() {
   this->showCreateCompositionSettingDialog();
   }
   );


 }

 ArtifactCompositionMenu::Impl::~Impl()
 {

 }

 void ArtifactCompositionMenu::Impl::showCreateCompositionSettingDialog()
 {
  auto createCompositionSettingDialog = new CreateCompositionDialog();

  //createCompositionSettingDialog->show();

  if (createCompositionSettingDialog->exec())
  {

  }

 }

 ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setObjectName("CompositionMenu(&C)");

  setTitle("Composition");
  

  //addAction(impl_->createCompositionAction);

  addAction(impl_->createCompositionFromFootage);

  addAction(impl_->saveAsFrameAction);




  connect(this, &QMenu::aboutToShow, this, &ArtifactCompositionMenu::rebuildMenu);
 }

 ArtifactCompositionMenu::~ArtifactCompositionMenu()
 {

 }

 void ArtifactCompositionMenu::handleCreateCompositionRequested()
 {
  auto& instance=ArtifactProjectManager::getInstance();

  
 }

 void ArtifactCompositionMenu::rebuildMenu()
 {
  qDebug() << "Rebuild Menu";
 }

};
