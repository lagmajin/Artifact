module;
#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>
#include <wobjectimpl.h>

module Artifact.Menu.Script;

import std;

import Artifact.Script.Hooks;
import Utils.Path;

namespace Artifact {

using namespace ArtifactCore;

W_OBJECT_IMPL(ArtifactScriptMenu)

class ArtifactScriptMenu::Impl {
public:
 Impl(ArtifactScriptMenu* menu);
 ~Impl() = default;

 ArtifactScriptMenu* menu_ = nullptr;
 QAction* openScriptsFolderAction = nullptr;
 QAction* openHooksFolderAction = nullptr;
 QMenu* hooksMenu = nullptr;
 std::vector<QAction*> hookActions;

 QString scriptsRootPath() const;
 QString hooksRootPath() const;
 void openFolder(const QString& path) const;
 void runHook(const QString& hookName);
 void refreshHookActions();
};

QString ArtifactScriptMenu::Impl::scriptsRootPath() const
{
 const QString appDir = QCoreApplication::applicationDirPath();
 const QString localScripts = QDir(appDir).filePath(QStringLiteral("scripts"));
 if (QDir(localScripts).exists()) {
  return QDir(localScripts).absolutePath();
 }
 const QString currentScripts = QDir(QDir::currentPath()).filePath(QStringLiteral("scripts"));
 if (QDir(currentScripts).exists()) {
  return QDir(currentScripts).absolutePath();
 }
 return QDir(localScripts).absolutePath();
}

QString ArtifactScriptMenu::Impl::hooksRootPath() const
{
 return QDir(scriptsRootPath()).filePath(QStringLiteral("hooks"));
}

void ArtifactScriptMenu::Impl::openFolder(const QString& path) const
{
 if (path.trimmed().isEmpty()) {
  return;
 }
 const QString absolute = QFileInfo(path).absoluteFilePath();
 QDir dir(absolute);
 if (!dir.exists()) {
  dir.mkpath(QStringLiteral("."));
 }
 QDesktopServices::openUrl(QUrl::fromLocalFile(absolute));
}

void ArtifactScriptMenu::Impl::runHook(const QString& hookName)
{
 if (hookName.trimmed().isEmpty()) {
  return;
 }
 const bool exists = ArtifactPythonHookManager::hookScriptExists(hookName);
 const bool enabled = ArtifactPythonHookManager::isHookEnabled(hookName);
 if (!exists) {
  QMessageBox::information(menu_, tr("Script"), tr("Hook script not found: %1").arg(hookName));
  return;
 }
 if (!enabled) {
  const auto result = QMessageBox::question(
      menu_,
      tr("Script"),
      tr("Hook %1 is disabled. Enable it and run it now?").arg(hookName),
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::Yes);
  if (result != QMessageBox::Yes) {
   return;
  }
  ArtifactPythonHookManager::setHookEnabled(hookName, true);
 }
 const bool ok = ArtifactPythonHookManager::runHook(hookName);
 if (!ok) {
  QMessageBox::warning(menu_, tr("Script"), tr("Failed to run hook: %1").arg(hookName));
 }
}

void ArtifactScriptMenu::Impl::refreshHookActions()
{
 for (QAction* action : hookActions) {
  if (!action) {
   continue;
  }
  const QString hookName = action->data().toString();
  const bool exists = ArtifactPythonHookManager::hookScriptExists(hookName);
  const bool enabled = ArtifactPythonHookManager::isHookEnabled(hookName);
  action->setEnabled(exists);
  action->setToolTip(exists
                         ? tr("Hook script exists and is %1.").arg(enabled ? tr("enabled") : tr("disabled"))
                         : tr("Hook script not found."));
  if (exists) {
   action->setText(enabled ? hookName : QStringLiteral("%1 [disabled]").arg(hookName));
  } else {
   action->setText(hookName);
  }
 }
}

ArtifactScriptMenu::Impl::Impl(ArtifactScriptMenu* menu)
 : menu_(menu)
{
 openScriptsFolderAction = new QAction(tr("Open Scripts Folder"));
 openScriptsFolderAction->setIcon(QIcon(resolveIconPath("Material/folder_open.svg")));

 openHooksFolderAction = new QAction(tr("Open Hooks Folder"));
 openHooksFolderAction->setIcon(QIcon(resolveIconPath("Material/folder.svg")));

 hooksMenu = new QMenu(tr("Hooks"));

 const QStringList hookNames = ArtifactPythonHookManager::knownHooks();
 for (const QString& hookName : hookNames) {
  QAction* action = hooksMenu->addAction(hookName);
  action->setData(hookName);
  action->setToolTip(hookName);
  hookActions.push_back(action);
  QObject::connect(action, &QAction::triggered, menu, [this, hookName]() {
   runHook(hookName);
  });
 }

 menu->addAction(openScriptsFolderAction);
 menu->addAction(openHooksFolderAction);
 menu->addSeparator();
 menu->addMenu(hooksMenu);
 refreshHookActions();

 QObject::connect(openScriptsFolderAction, &QAction::triggered, menu, [this]() {
  openFolder(scriptsRootPath());
 });
 QObject::connect(openHooksFolderAction, &QAction::triggered, menu, [this]() {
  openFolder(hooksRootPath());
 });
 QObject::connect(hooksMenu, &QMenu::aboutToShow, menu, [this]() {
  refreshHookActions();
 });
}

ArtifactScriptMenu::ArtifactScriptMenu(QWidget* parent)
 : QMenu(parent), impl_(new Impl(this))
{
 setObjectName(QStringLiteral("ScriptMenu"));
 setTitle(tr("Script(&S)"));
 setTearOffEnabled(true);
}

ArtifactScriptMenu::~ArtifactScriptMenu()
{
 delete impl_;
}

}
