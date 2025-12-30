module;
#include <QMenu>
#include <QAction>
module Menu.Help;

import std;

namespace Artifact {

 class ArtifactHelpMenu::Impl {
 private:

 public:
  Impl();
  ~Impl();
  QAction* versionInfoAction_ = nullptr;
 };

 ArtifactHelpMenu::Impl::Impl()
 {

 }

 ArtifactHelpMenu::Impl::~Impl()
 {

 }

 ArtifactHelpMenu::ArtifactHelpMenu(QWidget* parent /*= nullptr*/):QMenu(parent),impl_(new Impl())
 {

 }

 ArtifactHelpMenu::~ArtifactHelpMenu()
 {
  delete impl_;
 }

};