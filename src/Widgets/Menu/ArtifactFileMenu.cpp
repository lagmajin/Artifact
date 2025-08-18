module;
#include <wobjectimpl.h>
#include <mutex>
#include <QFile>
#include <QMenu>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

#include <QApplication>
module Artifact.Menu.File;

import  Project.Manager;

import Utils;

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
  createProjectAction->setIcon(QIcon(ArtifactCore::getIconPath() + "/new.png"));



  closeProjectAction = new QAction("CloseProject");
  closeProjectAction->setShortcut(QKeySequence::Close);
  closeProjectAction->setDisabled(true);

  saveProjectAction = new QAction(u8"保存(&S)");
  saveProjectAction->setShortcut(QKeySequence::Save);
  saveProjectAction->setDisabled(true); // 最初は無効 (まだプロジェクトがないため)

  // --- 名前を付けて保存アクション ---
  saveProjectAsAction = new QAction("名前を付けて保存(&A)...");
  saveProjectAsAction->setShortcut(QKeySequence::SaveAs);
  saveProjectAsAction->setDisabled(true); // 最初は無効 (まだプロジェクトがないため)

  quitApplicationAction= new QAction("終了()...");
  quitApplicationAction->setShortcut(QKeySequence::Quit);

 }

 void ArtifactFileMenu::Impl::rebuildMenu(QMenu* menu)
 {

 }

 ArtifactFileMenu::ArtifactFileMenu(QWidget* parent /*= nullptr*/):QMenu(parent),Impl_(new Impl())
 {
  setObjectName("FileMenu");

  setTitle("File(&F)");

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
  qDebug() << "ReceiverClass::onDataReady() が呼び出されました！";

  ArtifactProjectManager::getInstance().createProject();

 }

 

 void ArtifactFileMenu::projectClosed()
 {
  qDebug() << "ReceiverClass::projectClosed() が呼び出されました！";
 }

 void ArtifactFileMenu::quitApplication()
 {
  QApplication::quit();
 }

 void ArtifactFileMenu::rebuildMenu()
 {
  QGraphicsOpacityEffect* opacityEffect = new QGraphicsOpacityEffect(this);
  opacityEffect->setOpacity(0.0); // 最初は透明
  setGraphicsEffect(opacityEffect);

  // 2. QPropertyAnimation を作成
  QPropertyAnimation* animation = new QPropertyAnimation(opacityEffect, "opacity", this);
  animation->setDuration(500); // アニメーション時間 (ミリ秒)
  animation->setStartValue(0.0);
  animation->setEndValue(1.0); // 不透明にする
  animation->setEasingCurve(QEasingCurve::OutQuad); // イージングカーブ (アニメーションの滑らかさ)

  // アニメーションが完了したらエフェクトを削除 (残しておくとパフォーマンスに影響)
  QObject::connect(animation, &QPropertyAnimation::finished, this, [opacityEffect,this]() {
   //setGraphicsEffect(nullptr); // エフェクトを解除
   opacityEffect->deleteLater(); // エフェクトオブジェクトを削除
   });

  // 3. メニューを表示してアニメーションを開始
  //menu->popup(pos); // メニューを表示
  animation->start(QAbstractAnimation::DeleteWhenStopped);


 }

};