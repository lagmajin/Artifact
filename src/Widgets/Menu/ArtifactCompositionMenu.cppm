module;
#include <QObject>
#include <QWidget>
#include <wobjectimpl.h>
#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QDialog>

module Menu.Composition;


import  Artifact.Project.Manager;

import Dialog.Composition;

import ArtifactMainWindow;

import Utils.Path;

namespace Artifact {

 W_OBJECT_IMPL(ArtifactCompositionMenu)

 class ArtifactCompositionMenu::Impl {
 //W_OBJECT(ArtifactCompositionMenu::Impl)
 private:

 public:
  Impl(ArtifactCompositionMenu* menu, ArtifactMainWindow* mainWindow);
  ~Impl();
  ArtifactMainWindow* mainWindow_ = nullptr;
  QAction* createCompositionAction=nullptr;
  QAction* createCompositionFromFootage = nullptr;
  QAction* changeCompositionSettingsAction = nullptr;

  QAction* saveAsFrameAction = nullptr;

  QAction* addToRenderQueueAction = nullptr;

  void showCreateCompositionSettingDialog();
  void handleAddRenderQueueRequest();
  void handleSaveAsImageRequest();
 };

 //W_OBJECT_IMPL(ArtifactCompositionMenu::Impl)

 ArtifactCompositionMenu::Impl::Impl(ArtifactCompositionMenu* menu,ArtifactMainWindow* mainWindow)
{
  createCompositionAction = new QAction("Create composition");
  //createCompositionAction->setShortcut(QKeySequence::New);
  createCompositionAction->setIcon(QIcon(ArtifactCore::getIconPath() + "/composition.png"));

  

  createCompositionFromFootage = new QAction("Create composition from footage");
  //createCompositionAction->setText()
  createCompositionFromFootage->setDisabled(true);

 

  changeCompositionSettingsAction = new QAction("Change composition settings");

  changeCompositionSettingsAction->setDisabled(true);
  
  saveAsFrameAction = new QAction("Save as image file");
  //createCompositionAction->setText()
  saveAsFrameAction->setDisabled(true);

  addToRenderQueueAction = new QAction();
  addToRenderQueueAction->setText("AddRenderQueue");

  connect(createCompositionAction, &QAction::triggered, menu, [this]() {
   this->showCreateCompositionSettingDialog();
   }
   );
  menu->addAction(createCompositionAction);
  menu->addAction(createCompositionFromFootage);
  menu->addSeparator();
  menu->addAction(addToRenderQueueAction);


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
   auto& projectManager = ArtifactProjectManager::getInstance();

   projectManager.createNewComposition();
  }

 }

 void ArtifactCompositionMenu::Impl::handleAddRenderQueueRequest()
 {

 }

 void ArtifactCompositionMenu::Impl::handleSaveAsImageRequest()
 {

 }

 ArtifactCompositionMenu::ArtifactCompositionMenu(ArtifactMainWindow* mainWindow, QWidget* parent/*=nullptr*/) :QMenu(parent), impl_(new Impl(this,mainWindow))
 {
  setObjectName("CompositionMenu(&C)");

  setTitle("Composition");
  setTearOffEnabled(true);
  setSeparatorsCollapsible(true);
  setMinimumWidth(160);
  //addAction(impl_->createCompositionAction);



  addAction(impl_->saveAsFrameAction);




  connect(this, &QMenu::aboutToShow, this, &ArtifactCompositionMenu::rebuildMenu);
 }

 ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* parent /*= nullptr*/):QMenu(parent)
 {

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
