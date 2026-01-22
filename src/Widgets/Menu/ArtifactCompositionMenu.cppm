module;
#include <QObject>
#include <QWidget>
#include <wobjectimpl.h>
#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QKeySequence>
#include <QColorDialog>
#include <qcoro6/qcoro/qcorotask.h>
module Menu.Composition;


import  Artifact.Project.Manager;

import Dialog.Composition;

import Artifact.MainWindow;

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
  QAction* backgroundColorAction = nullptr;

  QAction* saveAsFrameAction = nullptr;
  QMenu* saveFrameAsMenu = nullptr;

  QAction* addToRenderQueueAction = nullptr;

  QAction* trimCompToWorkAreaAction = nullptr;
  QAction* cropCompToROIAction = nullptr;

  void showCreateCompositionSettingDialog();
  void showBackgroundColorDialog();
  QCoro::Task<> showChangeCompositionSettingsDialogAsync();
  void handleAddRenderQueueRequest();
  void handleSaveAsImageRequest();
 };

 //W_OBJECT_IMPL(ArtifactCompositionMenu::Impl)

 ArtifactCompositionMenu::Impl::Impl(ArtifactCompositionMenu* menu,ArtifactMainWindow* mainWindow)
{
  createCompositionAction = new QAction("New Composition...");
  createCompositionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
  createCompositionAction->setIcon(QIcon(ArtifactCore::getIconPath() + "/composition.png"));

  createCompositionFromFootage = new QAction("New Composition from Footage...");
  createCompositionFromFootage->setDisabled(true);

  changeCompositionSettingsAction = new QAction("Composition Settings...");
  changeCompositionSettingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
  changeCompositionSettingsAction->setDisabled(true);

  backgroundColorAction = new QAction("Background Color...");
  backgroundColorAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
  backgroundColorAction->setDisabled(true);
  
  saveFrameAsMenu = new QMenu("Save Frame As");
  saveAsFrameAction = new QAction("File...");
  saveAsFrameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
  saveAsFrameAction->setDisabled(true);
  saveFrameAsMenu->addAction(saveAsFrameAction);

  addToRenderQueueAction = new QAction("Add to Render Queue");
  addToRenderQueueAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));

  trimCompToWorkAreaAction = new QAction("Trim Comp to Work Area");
  trimCompToWorkAreaAction->setDisabled(true);

  cropCompToROIAction = new QAction("Crop Comp to Region of Interest");
  cropCompToROIAction->setDisabled(true);

  connect(createCompositionAction, &QAction::triggered, menu, [this]() {
   this->showCreateCompositionSettingDialog();
   }
   );
  
  connect(backgroundColorAction, &QAction::triggered, menu, [this]() {
   this->showBackgroundColorDialog();
   }
   );

  menu->addAction(createCompositionAction);
  menu->addAction(createCompositionFromFootage);
  menu->addSeparator();
  menu->addAction(changeCompositionSettingsAction);
  menu->addAction(backgroundColorAction);
  menu->addSeparator();
  menu->addMenu(saveFrameAsMenu);
  menu->addAction(addToRenderQueueAction);
  menu->addSeparator();
  menu->addAction(trimCompToWorkAreaAction);
  menu->addAction(cropCompToROIAction);
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

   projectManager.createComposition();
  }

  createCompositionSettingDialog->deleteLater();

 }

 void ArtifactCompositionMenu::Impl::showBackgroundColorDialog()
 {
  auto& projectManager = ArtifactProjectManager::getInstance();
  auto comp = projectManager.currentComposition();
  if (!comp) return;

  QColor color = QColorDialog::getColor(Qt::black, mainWindow_, "Composition Background Color");
  if (color.isValid()) {
   // Convert QColor to FloatColor
   FloatColor floatColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
   comp->setBackGroundColor(floatColor);
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
  auto& projectManager = ArtifactProjectManager::getInstance();
  bool hasActiveComposition = projectManager.currentComposition() != nullptr;

  impl_->changeCompositionSettingsAction->setEnabled(hasActiveComposition);
  impl_->backgroundColorAction->setEnabled(hasActiveComposition);
  impl_->saveAsFrameAction->setEnabled(hasActiveComposition);
  impl_->trimCompToWorkAreaAction->setEnabled(hasActiveComposition);
  impl_->cropCompToROIAction->setEnabled(hasActiveComposition);
 }

};
