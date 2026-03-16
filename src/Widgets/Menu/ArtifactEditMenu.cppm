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
  QAction* selectNoneAction = nullptr;
  QAction* invertSelectionAction = nullptr;
  QAction* selectSameTypeAction = nullptr;
  QAction* findAction = nullptr;
  QAction* preferencesAction = nullptr;

  QAction* copyAction_ = nullptr;
  QAction* cutAction_ = nullptr;
  QAction* pasteAction_ = nullptr;
  QAction* deleteAction_ = nullptr;

  void handleCopyAction();
  void handleCutAction();
  void handlePasteAction();
  void handleDelete();
  void handleUndo();
  void handleRedo();
  void handleDuplicate();
  void handleSplit();
  void handleTrimIn();
  void handleTrimOut();
  void handleSelectAll();
  void handleSelectNone();
  void handleInvertSelection();
  void handleSelectSameType();
  void handleFind();
  void handlePreferences();
 };

 ArtifactEditMenu::Impl::Impl(QMenu* menu)
 {
  // Basic edit actions
  undoAction = new QAction("元に戻す (&U)");
  undoAction->setShortcut(QKeySequence::Undo);
  undoAction->setIcon(QIcon(resolveIconPath("Material/undo.svg")));

  redoAction = new QAction("やり直し (&R)");
  redoAction->setShortcut(QKeySequence::Redo);
  redoAction->setIcon(QIcon(resolveIconPath("Material/redo.svg")));

  duplicateAction = new QAction("複製 (&D)");
  duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
  duplicateAction->setIcon(QIcon(resolveIconPath("Material/content_copy.svg")));

  splitAction = new QAction("レイヤーを分割 (&S)");
  splitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
  splitAction->setIcon(QIcon(resolveIconPath("Material/content_cut.svg")));

  trimInAction = new QAction("インポイントを現在の時間にトリム");
  trimOutAction = new QAction("アウトポイントを現在の時間にトリム");

  selectAllAction = new QAction("すべて選択 (&A)");
  selectAllAction->setShortcut(QKeySequence::SelectAll);

  selectNoneAction = new QAction("選択解除");
  selectNoneAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));

  invertSelectionAction = new QAction("選択を反転");

  selectSameTypeAction = new QAction("同じ種類を選択");

  findAction = new QAction("検索 (&F)...");
  findAction->setShortcut(QKeySequence::Find);
  findAction->setIcon(QIcon(resolveIconPath("Material/search.svg")));

  preferencesAction = new QAction("環境設定 (&P)...");
  preferencesAction->setIcon(QIcon(resolveIconPath("Material/settings.svg")));

  copyAction_ = new QAction("コピー (&C)");
  copyAction_->setShortcut(QKeySequence::Copy);
  copyAction_->setIcon(QIcon(resolveIconPath("Material/content_copy.svg")));

  cutAction_ = new QAction("切り取り (&T)");
  cutAction_->setShortcut(QKeySequence::Cut);
  cutAction_->setIcon(QIcon(resolveIconPath("Material/content_cut.svg")));

  pasteAction_ = new QAction("貼り付け (&P)");
  pasteAction_->setShortcut(QKeySequence::Paste);
  pasteAction_->setIcon(QIcon(resolveIconPath("Material/content_paste.svg")));

  deleteAction_ = new QAction("削除 (&D)");
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
  
  // Select submenu
  QMenu* selectMenu = menu->addMenu("選択");
  selectMenu->addAction(selectAllAction);
  selectMenu->addAction(selectNoneAction);
  selectMenu->addAction(invertSelectionAction);
  selectMenu->addSeparator();
  selectMenu->addAction(selectSameTypeAction);

  menu->addSeparator();
  menu->addAction(findAction);
  menu->addSeparator();
  menu->addAction(preferencesAction);

  // Connections
  QObject::connect(undoAction, &QAction::triggered, menu, [this]() { handleUndo(); });
  QObject::connect(redoAction, &QAction::triggered, menu, [this]() { handleRedo(); });
  QObject::connect(copyAction_, &QAction::triggered, menu, [this]() { handleCopyAction(); });
  QObject::connect(cutAction_, &QAction::triggered, menu, [this]() { handleCutAction(); });
  QObject::connect(pasteAction_, &QAction::triggered, menu, [this]() { handlePasteAction(); });
  QObject::connect(deleteAction_, &QAction::triggered, menu, [this]() { handleDelete(); });
  QObject::connect(duplicateAction, &QAction::triggered, menu, [this]() { handleDuplicate(); });
  QObject::connect(splitAction, &QAction::triggered, menu, [this]() { handleSplit(); });
  QObject::connect(trimInAction, &QAction::triggered, menu, [this]() { handleTrimIn(); });
  QObject::connect(trimOutAction, &QAction::triggered, menu, [this]() { handleTrimOut(); });
  QObject::connect(selectAllAction, &QAction::triggered, menu, [this]() { handleSelectAll(); });
  QObject::connect(selectNoneAction, &QAction::triggered, menu, [this]() { handleSelectNone(); });
  QObject::connect(invertSelectionAction, &QAction::triggered, menu, [this]() { handleInvertSelection(); });
  QObject::connect(selectSameTypeAction, &QAction::triggered, menu, [this]() { handleSelectSameType(); });
  QObject::connect(findAction, &QAction::triggered, menu, [this]() { handleFind(); });
  QObject::connect(preferencesAction, &QAction::triggered, menu, [this]() { handlePreferences(); });
 }

 void ArtifactEditMenu::Impl::handleUndo() { qDebug() << "Undo"; }
 void ArtifactEditMenu::Impl::handleRedo() { qDebug() << "Redo"; }
 void ArtifactEditMenu::Impl::handleCopyAction() { qDebug() << "Copy"; }
 void ArtifactEditMenu::Impl::handleCutAction() { qDebug() << "Cut"; }
 void ArtifactEditMenu::Impl::handlePasteAction() { qDebug() << "Paste"; }
 void ArtifactEditMenu::Impl::handleDelete() { qDebug() << "Delete"; }
 void ArtifactEditMenu::Impl::handleDuplicate() { qDebug() << "Duplicate"; }
 void ArtifactEditMenu::Impl::handleSplit() { qDebug() << "Split"; }
 void ArtifactEditMenu::Impl::handleTrimIn() { qDebug() << "Trim In"; }
 void ArtifactEditMenu::Impl::handleTrimOut() { qDebug() << "Trim Out"; }
 void ArtifactEditMenu::Impl::handleSelectAll() { qDebug() << "Select All"; }
 void ArtifactEditMenu::Impl::handleSelectNone() { qDebug() << "Select None"; }
 void ArtifactEditMenu::Impl::handleInvertSelection() { qDebug() << "Invert Selection"; }
 void ArtifactEditMenu::Impl::handleSelectSameType() { qDebug() << "Select Same Type"; }
 void ArtifactEditMenu::Impl::handleFind() { qDebug() << "Find"; }
 void ArtifactEditMenu::Impl::handlePreferences() { qDebug() << "Preferences"; }

 W_OBJECT_IMPL(ArtifactEditMenu)

 ArtifactEditMenu::ArtifactEditMenu(QWidget* parent) : QMenu(parent), impl_(new Impl(this)) {
  setTitle("編集 (&E)");
 }

 ArtifactEditMenu::~ArtifactEditMenu() { delete impl_; }

 void ArtifactEditMenu::rebuildMenu() { impl_->rebuildMenu(); }

} // namespace Artifact
