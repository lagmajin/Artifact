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
 };

 ArtifactEditMenu::Impl::Impl(QMenu* menu)
 {

 }

 ArtifactEditMenu::ArtifactEditMenu(QWidget* parent/*=nullptr*/):QMenu(parent)
 {
  setTitle(tr("Edit"));
 }
 ArtifactEditMenu::~ArtifactEditMenu()
 {

 }









};


