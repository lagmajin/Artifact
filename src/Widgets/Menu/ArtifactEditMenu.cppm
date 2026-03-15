module;
#include <QMenu>
#include <wobjectimpl.h>
#include <QAction>
#include <QKeySequence>
#include <QDebug>
module Artifact.Menu.Edit;
import std;

import Artifact.Project.Manager;
import Utils.Path;

namespace Artifact {
 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactEditMenu)

 class  ArtifactEditMenu::Impl {
 private:

  bool projectCreated_ = false;
 public:
  Impl(QMenu* menu);
  void rebuildMenu();
  QAction* undoAction = nullptr;
  QAction* redoAction = nullptr;
  QAction* duplicateAction = nullptr;
  QAction* splitAction = nullptr;
  QAction* trimInAction = nullptr;
  QAction* trimOutAction = nullptr;
  QAction* selectAllAction = nullptr;
  QAction* findAction = nullptr;
  QAction* preferencesAction = nullptr;

  QAction* copyAction_ = nullptr;
  QAction* cutAction_ = nullptr;
  QAction* pasteAction_ = nullptr;
  QAction* deleteAction_ = nullptr;

  void handleCopyAction();
  void handleCutAction();
  void handlePasteAction();
  void handleUndo();
  void handleRedo();
  void handleDuplicate();
  void handleSplit();
  void handleTrimIn();
  void handleTrimOut();
  void handleSelectAll();
  void handleFind();
  void handlePreferences();
 };

 ArtifactEditMenu::Impl::Impl(QMenu* menu)
 {
  // Basic edit actions
  undoAction = new QAction("元に戻す(&U)");
  undoAction->setShortcut(QKeySequence::Undo);
  undoAction->setIcon(QIcon(resolveIconPath("Material/undo.svg")));

  redoAction = new QAction("やり直し(&R)");
  redoAction->setShortcut(QKeySequence::Redo);
  redoAction->setIcon(QIcon(resolveIconPath("Material/redo.svg")));

  duplicateAction = new QAction("複製(&D)");
  duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
  duplicateAction->setIcon(QIcon(resolveIconPath("Material/content_copy.svg")));

  splitAction = new QAction("レイヤーを分割(&S)");
  splitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
  splitAction->setIcon(QIcon(resolveIconPath("Material/content_cut.svg")));

  trimInAction = new QAction("インポイントを現在の時間にトリム");
  trimOutAction = new QAction("アウトポイントを現在の時間にトリム");

  selectAllAction = new QAction("すべて選択(&A)");
  selectAllAction->setShortcut(QKeySequence::SelectAll);

  findAction = new QAction("検索(&F)...");
  findAction->setShortcut(QKeySequence::Find);
  findAction->setIcon(QIcon(resolveIconPath("Material/search.svg")));

  preferencesAction = new QAction("環境設定(&P)...");
  preferencesAction->setIcon(QIcon(resolveIconPath("Material/settings.svg")));

  copyAction_ = new QAction("コピー(&C)");
  copyAction_->setShortcut(QKeySequence::Copy);
  copyAction_->setIcon(QIcon(resolveIconPath("Material/content_copy.svg")));

  cutAction_ = new QAction("切り取り(&T)");
  cutAction_->setShortcut(QKeySequence::Cut);
  cutAction_->setIcon(QIcon(resolveIconPath("Material/content_cut.svg")));

  pasteAction_ = new QAction("貼り付け(&P)");
  pasteAction_->setShortcut(QKeySequence::Paste);
  pasteAction_->setIcon(QIcon(resolveIconPath("Material/content_paste.svg")));

  deleteAction_ = new QAction("削除(&D)");
  deleteAction_->setShortcut(QKeySequence::Delete);
  deleteAction_->setIcon(QIcon(resolveIconPath("Material/delete.svg")));

  // Build menu
  menu->addAction(undoAction);
  menu->addAction(redoAction);
  menu->addSeparator();
  menu->addAction(copyAction_);
  menu->addAction(cutAction_);
  menu->addAction(pasteAction_);
  menu->addAction(deleteAction_);
  menu->addAction(duplicateAction);
  menu->addSeparator();
  menu->addAction(splitAction);
  menu->addAction(trimInAction);
  menu->addAction(trimOutAction);
  menu->addSeparator();
  menu->addAction(selectAllAction);
  menu->addAction(findAction);
  menu->addSeparator();
  menu->addAction(preferencesAction);

  // Connections
  QObject::connect(copyAction_, &QAction::triggered, [this]() { handleCopyAction(); });
  QObject::connect(cutAction_, &QAction::triggered, [this]() { handleCutAction(); });
  QObject::connect(pasteAction_, &QAction::triggered, [this]() { handlePasteAction(); });
  QObject::connect(undoAction, &QAction::triggered, [this]() { handleUndo(); });
  QObject::connect(redoAction, &QAction::triggered, [this]() { handleRedo(); });
  QObject::connect(duplicateAction, &QAction::triggered, [this]() { handleDuplicate(); });
  QObject::connect(splitAction, &QAction::triggered, [this]() { handleSplit(); });
  QObject::connect(trimInAction, &QAction::triggered, [this]() { handleTrimIn(); });
  QObject::connect(trimOutAction, &QAction::triggered, [this]() { handleTrimOut(); });
  QObject::connect(selectAllAction, &QAction::triggered, [this]() { handleSelectAll(); });
  QObject::connect(findAction, &QAction::triggered, [this]() { handleFind(); });
  QObject::connect(preferencesAction, &QAction::triggered, [this]() { handlePreferences(); });
 }

 void ArtifactEditMenu::Impl::rebuildMenu()
 {
  // Enable/disable actions based on project state (simple heuristic)
  bool hasProject = ArtifactProjectManager::getInstance().isProjectCreated();
  undoAction->setEnabled(hasProject);
  redoAction->setEnabled(hasProject);
  copyAction_->setEnabled(hasProject);
  cutAction_->setEnabled(hasProject);
  pasteAction_->setEnabled(hasProject);
  deleteAction_->setEnabled(hasProject);
  duplicateAction->setEnabled(hasProject);
  splitAction->setEnabled(hasProject);
  trimInAction->setEnabled(hasProject);
  trimOutAction->setEnabled(hasProject);
  selectAllAction->setEnabled(hasProject);
  findAction->setEnabled(hasProject);
 }

 void ArtifactEditMenu::Impl::handleCopyAction()
 {
  qDebug() << "EditMenu: Copy triggered";
 }

 void ArtifactEditMenu::Impl::handleCutAction()
 {
  qDebug() << "EditMenu: Cut triggered";
 }

 void ArtifactEditMenu::Impl::handlePasteAction()
 {
  qDebug() << "EditMenu: Paste triggered";
 }

 void ArtifactEditMenu::Impl::handleUndo()
 {
  qDebug() << "EditMenu: Undo triggered";
 }

 void ArtifactEditMenu::Impl::handleRedo()
 {
  qDebug() << "EditMenu: Redo triggered";
 }

 void ArtifactEditMenu::Impl::handleDuplicate()
 {
  qDebug() << "EditMenu: Duplicate triggered";
 }

 void ArtifactEditMenu::Impl::handleSplit()
 {
  qDebug() << "EditMenu: Split Layer triggered";
 }

 void ArtifactEditMenu::Impl::handleTrimIn()
 {
  qDebug() << "EditMenu: Trim In triggered";
 }

 void ArtifactEditMenu::Impl::handleTrimOut()
 {
  qDebug() << "EditMenu: Trim Out triggered";
 }

 void ArtifactEditMenu::Impl::handleSelectAll()
 {
  qDebug() << "EditMenu: Select All triggered";
 }

 void ArtifactEditMenu::Impl::handleFind()
 {
  qDebug() << "EditMenu: Find triggered";
 }

 void ArtifactEditMenu::Impl::handlePreferences()
 {
  qDebug() << "EditMenu: Preferences triggered";
 }

 ArtifactEditMenu::ArtifactEditMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle("編集(&E)");
  setTearOffEnabled(false);
  connect(this, &QMenu::aboutToShow, this, [this]() { impl_->rebuildMenu(); });
 }
 ArtifactEditMenu::~ArtifactEditMenu()
 {
  delete impl_;
 }

};
