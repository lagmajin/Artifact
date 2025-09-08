//module;

module;
#include <QMenu>
#include <wobjectimpl.h>
module Artifact.Menu.Edit;


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


