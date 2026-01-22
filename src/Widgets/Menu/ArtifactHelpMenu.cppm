module;
#include <wobjectimpl.h>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

module Menu.Help;

import std;

namespace Artifact {
 W_OBJECT_IMPL(ArtifactHelpMenu)
 

 class ArtifactHelpMenu::Impl {
 public:
  Impl();
  ~Impl();

  QAction* versionInfoAction_ = nullptr;
  QAction* aboutAction_ = nullptr;
  QAction* docsAction_ = nullptr;
  QAction* checkUpdatesAction_ = nullptr;
 };

 ArtifactHelpMenu::Impl::Impl()
 {
  versionInfoAction_ = new QAction(u8"Version Info");
  aboutAction_ = new QAction(u8"About Artifact");
  docsAction_ = new QAction(u8"Documentation");
  checkUpdatesAction_ = new QAction(u8"Check for Updates");
 }

 ArtifactHelpMenu::Impl::~Impl()
 {
  delete versionInfoAction_;
  delete aboutAction_;
  delete docsAction_;
  delete checkUpdatesAction_;
 }

 ArtifactHelpMenu::ArtifactHelpMenu(QWidget* parent /*= nullptr*/):QMenu(parent),impl_(new Impl())
 {
  setObjectName("HelpMenu");
  setTitle(tr("Help(&H)"));
  setTearOffEnabled(true);

  // add actions
  addAction(impl_->versionInfoAction_);
  addAction(impl_->aboutAction_);
  addAction(impl_->docsAction_);
  addSeparator();
  addAction(impl_->checkUpdatesAction_);

  // connections
  connect(impl_->versionInfoAction_, &QAction::triggered, this, [this]() {
    QMessageBox::information(this, tr("Version"), tr("Artifact Version: %1").arg("1.0.0"));
  });

  connect(impl_->aboutAction_, &QAction::triggered, this, [this]() {
    QMessageBox::about(this, tr("About Artifact"), tr("Artifact - A creative tool."));
  });

  connect(impl_->docsAction_, &QAction::triggered, this, [this]() {
    QDesktopServices::openUrl(QUrl("https://github.com/lagmajin/Artifact"));
  });

  connect(impl_->checkUpdatesAction_, &QAction::triggered, this, [this]() {
    QMessageBox::information(this, tr("Updates"), tr("No updates available."));
  });
 }

 ArtifactHelpMenu::~ArtifactHelpMenu()
 {
  delete impl_;
 }

};
