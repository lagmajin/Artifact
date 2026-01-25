//module;

module;
#include <QMenu>
#include <wobjectimpl.h>
#include <QAction>
#include <QKeySequence>
#include <QDebug>
module Artifact.Menu.Edit;

import std;
import Artifact.Project.Manager;

namespace Artifact {

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
  undoAction = new QAction(tr("Undo"));
  undoAction->setShortcut(QKeySequence::Undo);

  redoAction = new QAction(tr("Redo"));
  redoAction->setShortcut(QKeySequence::Redo);

  duplicateAction = new QAction(tr("Duplicate"));
  duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));

  splitAction = new QAction(tr("Split Layer"));
  splitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));

  trimInAction = new QAction(tr("Trim In"));
  trimOutAction = new QAction(tr("Trim Out"));

  selectAllAction = new QAction(tr("Select All"));
  selectAllAction->setShortcut(QKeySequence::SelectAll);

  findAction = new QAction(tr("Find..."));

  preferencesAction = new QAction(tr("Preferences..."));

  copyAction_ = new QAction(tr("Copy"));
  copyAction_->setShortcut(QKeySequence::Copy);

  cutAction_ = new QAction(tr("Cut"));
  cutAction_->setShortcut(QKeySequence::Cut);

  pasteAction_ = new QAction(tr("Paste"));
  pasteAction_->setShortcut(QKeySequence::Paste);

  deleteAction_ = new QAction(tr("Delete"));
  deleteAction_->setShortcut(QKeySequence::Delete);

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
  connect(copyAction_, &QAction::triggered, [this]() { handleCopyAction(); });
  connect(cutAction_, &QAction::triggered, [this]() { handleCutAction(); });
  connect(pasteAction_, &QAction::triggered, [this]() { handlePasteAction(); });
  connect(undoAction, &QAction::triggered, [this]() { handleUndo(); });
  connect(redoAction, &QAction::triggered, [this]() { handleRedo(); });
  connect(duplicateAction, &QAction::triggered, [this]() { handleDuplicate(); });
  connect(splitAction, &QAction::triggered, [this]() { handleSplit(); });
  connect(trimInAction, &QAction::triggered, [this]() { handleTrimIn(); });
  connect(trimOutAction, &QAction::triggered, [this]() { handleTrimOut(); });
  connect(selectAllAction, &QAction::triggered, [this]() { handleSelectAll(); });
  connect(findAction, &QAction::triggered, [this]() { handleFind(); });
  connect(preferencesAction, &QAction::triggered, [this]() { handlePreferences(); });
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
  setTitle(tr("Edit(&E)"));
  setTearOffEnabled(true);
  connect(this, &QMenu::aboutToShow, this, [this]() { impl_->rebuildMenu(); });
 }
 ArtifactEditMenu::~ArtifactEditMenu()
 {
  delete impl_;
 }









};


