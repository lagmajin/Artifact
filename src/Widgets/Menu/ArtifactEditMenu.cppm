//module;

module;
#include <QMenu>
#include <wobjectimpl.h>
module Artifact.Menu.Edit;

import std;

namespace Artifact {

 W_OBJECT_IMPL(ArtifactEditMenu)

 class  ArtifactEditMenu::Impl {
 private:

  bool projectCreated_ = false;
 public:
  Impl(QMenu* menu);
  void rebuildMenu(QMenu* menu);
  QAction* applicationSettings = nullptr;

  QAction* copyAction_ = nullptr;
  QAction* cutAction_ = nullptr;
  QAction* pasteAction_ = nullptr;
  QAction* deleteAction_ = nullptr;

  void handleCopyAction();

  void handleCutAction();
 };

 ArtifactEditMenu::Impl::Impl(QMenu* menu)
 {
  copyAction_ = new QAction();
  copyAction_->setText("Copy");

  cutAction_ = new QAction();
  cutAction_->setText("Cut");

  pasteAction_ = new QAction();
  pasteAction_->setText("Paste");

  menu->addAction(copyAction_);
  menu->addAction(cutAction_);
  menu->addAction(pasteAction_);

  connect(copyAction_, &QAction::triggered, [this]() { handleCopyAction(); });
  connect(cutAction_, &QAction::triggered, [this]() { handleCutAction(); });

 }

 void ArtifactEditMenu::Impl::handleCopyAction()
 {
  // Implementation for copy action
 }

 void ArtifactEditMenu::Impl::handleCutAction()
 {
  // Implementation for cut action
 }

 ArtifactEditMenu::ArtifactEditMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle(tr("Edit(&E)"));
  setTearOffEnabled(true);
 }
 ArtifactEditMenu::~ArtifactEditMenu()
 {
  delete impl_;
 }









};


