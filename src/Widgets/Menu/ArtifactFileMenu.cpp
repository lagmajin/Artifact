module;

#include <mutex>

#include <QMenu>

#include <wobjectimpl.h>
#include <QApplication>
module Menu:File;

import  Project.Manager;


namespace Artifact {

 W_OBJECT_IMPL(ArtifactFileMenu)

 class  ArtifactFileMenu::Impl {
 private:
 
  bool projectCreated_ = false;
 public:
  Impl();
  void rebuildMenu(QMenu* menu);
  QAction* createProjectAction;
  QAction* closeProjectAction;
  QAction* saveProjectAction;
  QAction* saveProjectAsAction;
  QAction* quitApplicationAction;
 };

 ArtifactFileMenu::Impl::Impl()
 {
  createProjectAction = new QAction("CreateProject");
  createProjectAction -> setShortcut(QKeySequence::New);
  closeProjectAction = new QAction("CloseProject");
  closeProjectAction->setShortcut(QKeySequence::Close);
  closeProjectAction->setDisabled(true);

  saveProjectAction = new QAction("�ۑ�(&S)");
  saveProjectAction->setShortcut(QKeySequence::Save);
  saveProjectAction->setDisabled(true); // �ŏ��͖��� (�܂��v���W�F�N�g���Ȃ�����)

  // --- ���O��t���ĕۑ��A�N�V���� ---
  saveProjectAsAction = new QAction("���O��t���ĕۑ�(&A)...");
  saveProjectAsAction->setShortcut(QKeySequence::SaveAs);
  saveProjectAsAction->setDisabled(true); // �ŏ��͖��� (�܂��v���W�F�N�g���Ȃ�����)

  quitApplicationAction= new QAction("�I��()...");
  quitApplicationAction->setShortcut(QKeySequence::Quit);

 }

 void ArtifactFileMenu::Impl::rebuildMenu(QMenu* menu)
 {

 }

 ArtifactFileMenu::ArtifactFileMenu(QWidget* parent /*= nullptr*/):QMenu(parent),Impl_(new Impl())
 {
  setObjectName("FileMenu");

  setTitle("File");

  QPalette p = palette();
  p.setColor(QPalette::Window, QColor(30, 30, 30));

  setPalette(p);

  setAutoFillBackground(true);


  setAttribute(Qt::WA_TranslucentBackground);
  setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

  //auto projectAction = new QAction("CreateProject");
  addAction(Impl_->createProjectAction);
  addAction(Impl_->saveProjectAction);
  addAction(Impl_->saveProjectAsAction);
  addAction(Impl_->closeProjectAction);
  addAction(Impl_->quitApplicationAction);


  connect(Impl_->createProjectAction, &QAction::triggered,
   this,&ArtifactFileMenu::projectCreateRequested);

  connect(Impl_->quitApplicationAction, &QAction::triggered,
   this, &ArtifactFileMenu::quitApplication);

  connect(this, &QMenu::aboutToShow, this, &ArtifactFileMenu::rebuildMenu);
 }

 ArtifactFileMenu::~ArtifactFileMenu()
 {

 }

 void ArtifactFileMenu::projectCreateRequested()
 {
  qDebug() << "ReceiverClass::onDataReady() ���Ăяo����܂����I";

  ArtifactProjectManager::getInstance().createProject();

 }

 

 void ArtifactFileMenu::projectClosed()
 {
  qDebug() << "ReceiverClass::projectClosed() ���Ăяo����܂����I";
 }

 void ArtifactFileMenu::quitApplication()
 {
  QApplication::quit();
 }

 void ArtifactFileMenu::rebuildMenu()
 {
  qDebug() << "Rebuild Menu";


 }

};