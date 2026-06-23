module;
#include <utility>
#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QMessageBox>
#include <QTextStream>
#include <QUrl>
#include <wobjectimpl.h>

module Artifact.Menu.Script;

import std;

import Artifact.Script.Hooks;
import Script.Python.Engine;
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
 QAction* openMenuScriptAction = nullptr;
 QAction* openHooksFolderAction = nullptr;
 QAction* openMacrosFolderAction = nullptr;
 QMenu* aeUtilityMenu = nullptr;
 QMenu* hooksMenu = nullptr;
 QMenu* macrosMenu = nullptr;
 std::vector<QAction*> hookActions;
 std::vector<QAction*> macroActions;
 std::vector<QAction*> aeUtilityActions;

 QString scriptsRootPath() const;
 QString menuScriptPath() const;
 QString macrosRootPath() const;
 QString aeUtilityRootPath() const;
 QString aeUtilityScriptPath(const QString& fileName) const;
 QString hooksRootPath() const;
 bool ensureScriptsWorkspaceScaffold() const;
 bool ensureTextFile(const QString& path, const QString& contents) const;
 void openFolder(const QString& path) const;
 void openFile(const QString& path) const;
 void runHook(const QString& hookName);
 void runMacroFile(const QString& filePath);
 void refreshAeUtilityActions();
 void refreshHookActions();
 void refreshMacroActions();
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

QString ArtifactScriptMenu::Impl::menuScriptPath() const
{
 return QDir(scriptsRootPath()).filePath(QStringLiteral("menu.py"));
}

QString ArtifactScriptMenu::Impl::macrosRootPath() const
{
  return QDir(scriptsRootPath()).filePath(QStringLiteral("macros"));
}

QString ArtifactScriptMenu::Impl::aeUtilityRootPath() const
{
  return QDir(macrosRootPath()).filePath(QStringLiteral("ae_utility_pack"));
}

QString ArtifactScriptMenu::Impl::aeUtilityScriptPath(const QString& fileName) const
{
  return QDir(aeUtilityRootPath()).filePath(fileName);
}

bool ArtifactScriptMenu::Impl::ensureTextFile(const QString& path,
                                              const QString& contents) const
{
 QFileInfo info(path);
 QDir dir = info.dir();
 if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
  return false;
 }
 if (info.exists()) {
  return true;
 }

 QFile file(path);
 if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
  return false;
 }
 QTextStream stream(&file);
 stream << contents;
 file.close();
 return true;
}

bool ArtifactScriptMenu::Impl::ensureScriptsWorkspaceScaffold() const
{
 const QString root = scriptsRootPath();
 QDir rootDir(root);
 if (!rootDir.exists() && !rootDir.mkpath(QStringLiteral("."))) {
  return false;
 }

 QDir(root).mkpath(QStringLiteral("hooks"));
 QDir(root).mkpath(QStringLiteral("macros"));

 const QString menuStub =
     QStringLiteral(
         "# ArtifactStudio script menu entry point.\n"
         "# This file is executed from the scripts root when present.\n"
         "# Keep custom imports and shared helpers here.\n"
         "\n"
         "# Example:\n"
         "# print(\"Artifact menu.py loaded\")\n");
 if (!ensureTextFile(menuScriptPath(), menuStub)) {
  return false;
 }

 const QString macroReadme =
     QStringLiteral(
         "ArtifactStudio macros folder\n"
         "===========================\n"
         "\n"
         "Put reusable Python scripts in this folder.\n"
         "Future Script menu / macro entry integration will pick them up from here.\n");
 if (!ensureTextFile(QDir(macrosRootPath()).filePath(QStringLiteral("README.txt")),
                     macroReadme)) {
  return false;
 }

 QString hookReadme =
     QStringLiteral(
         "ArtifactStudio hook scripts\n"
         "===========================\n"
         "\n"
         "Create one Python file per hook name.\n"
         "Known hooks:\n");
 for (const QString& hookName : ArtifactPythonHookManager::knownHooks()) {
  hookReadme += QStringLiteral("- %1.py\n").arg(hookName);
 }
 if (!ensureTextFile(QDir(hooksRootPath()).filePath(QStringLiteral("README.txt")),
                     hookReadme)) {
  return false;
 }

 QDir(macrosRootPath()).mkpath(QStringLiteral("ae_utility_pack"));

 if (!ensureTextFile(
         aeUtilityScriptPath(QStringLiteral("quick_rename_layers.py")),
         QStringLiteral(
             "# ArtifactStudio AE utility pack\n"
             "# Quick Rename Layers\n"
             "\n"
             "print(\"Quick Rename Layers stub\")\n"))) {
  return false;
 }

 if (!ensureTextFile(
         aeUtilityScriptPath(QStringLiteral("clean_layers.py")),
         QStringLiteral(
             "# ArtifactStudio AE utility pack\n"
             "# Clean Layers\n"
             "\n"
             "print(\"Clean Layers stub\")\n"))) {
  return false;
 }

 if (!ensureTextFile(
         aeUtilityScriptPath(QStringLiteral("trim_comp_to_content.py")),
         QStringLiteral(
             "# ArtifactStudio AE utility pack\n"
             "# Trim Comp to Content\n"
             "\n"
             "print(\"Trim Comp to Content stub\")\n"))) {
  return false;
 }

 return true;
 }

void ArtifactScriptMenu::Impl::openFolder(const QString& path) const
{
 if (path.trimmed().isEmpty()) {
  return;
 }
 ensureScriptsWorkspaceScaffold();
 const QString absolute = QFileInfo(path).absoluteFilePath();
 QDir dir(absolute);
 if (!dir.exists()) {
  dir.mkpath(QStringLiteral("."));
 }
 QDesktopServices::openUrl(QUrl::fromLocalFile(absolute));
}

void ArtifactScriptMenu::Impl::openFile(const QString& path) const
{
 if (path.trimmed().isEmpty()) {
  return;
 }
 ensureScriptsWorkspaceScaffold();
 QFileInfo info(path);
 QDir dir = info.dir();
 if (!dir.exists()) {
  dir.mkpath(QStringLiteral("."));
 }
 QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
}

void ArtifactScriptMenu::Impl::runHook(const QString& hookName)
{
 if (hookName.trimmed().isEmpty()) {
  return;
 }
 const bool exists = ArtifactPythonHookManager::hookScriptExists(hookName);
 const bool enabled = ArtifactPythonHookManager::isHookEnabled(hookName);
 if (!exists) {
  const auto result = QMessageBox::question(
      menu_,
      tr("Script"),
      tr("Hook script not found: %1\nCreate a stub file and open it now?")
          .arg(hookName),
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::Yes);
  if (result == QMessageBox::Yes) {
   const QString hookPath = ArtifactPythonHookManager::hookScriptPath(hookName);
   const QString stub =
       QStringLiteral(
           "# ArtifactStudio hook script\n"
           "# Hook: %1\n"
           "\n"
           "print(\"Running hook: %1\")\n")
           .arg(hookName);
   ensureTextFile(hookPath, stub);
   openFile(hookPath);
   refreshHookActions();
  }
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

void ArtifactScriptMenu::Impl::runMacroFile(const QString& filePath)
{
 if (filePath.trimmed().isEmpty()) {
  return;
 }

 const QFileInfo info(filePath);
 if (!info.exists()) {
  QMessageBox::information(
      menu_, tr("Script"),
      tr("Macro script not found:\n%1").arg(info.absoluteFilePath()));
  return;
 }

 auto& py = ArtifactCore::PythonEngine::instance();
 if (!py.isInitialized()) {
  QMessageBox::warning(
      menu_, tr("Script"),
      tr("Python engine is not initialized. Macro execution is unavailable."));
  return;
 }

 py.setGlobalString("artifact_macro_name", info.baseName().toStdString());
 py.setGlobalString("artifact_macro_file", info.absoluteFilePath().toStdString());
 const bool ok = py.executeFile(info.absoluteFilePath().toStdString());
 if (!ok) {
  QMessageBox::warning(
      menu_, tr("Script"),
      tr("Failed to run macro:\n%1").arg(info.absoluteFilePath()));
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
  action->setEnabled(true);
  action->setToolTip(
      exists
          ? tr("Hook script exists at %1 and is %2.")
                .arg(ArtifactPythonHookManager::hookScriptPath(hookName),
                     enabled ? tr("enabled") : tr("disabled"))
          : tr("Hook script not found. Trigger to create %1.")
                .arg(ArtifactPythonHookManager::hookScriptPath(hookName)));
  if (exists) {
   action->setText(enabled ? hookName : QStringLiteral("%1 [disabled]").arg(hookName));
  } else {
   action->setText(QStringLiteral("%1 [missing]").arg(hookName));
  }
 }
}

void ArtifactScriptMenu::Impl::refreshMacroActions()
{
 if (!macrosMenu) {
  return;
 }

 for (QAction* action : macroActions) {
  if (action) {
   macrosMenu->removeAction(action);
   delete action;
  }
 }
 macroActions.clear();

 ensureScriptsWorkspaceScaffold();
 const QDir dir(macrosRootPath());
 const QFileInfoList files =
     dir.entryInfoList(QStringList() << "*.py", QDir::Files, QDir::Name);

 if (files.isEmpty()) {
  QAction* emptyAction = macrosMenu->addAction(tr("No Macros Yet"));
  emptyAction->setEnabled(false);
  emptyAction->setToolTip(
      tr("Put reusable Python files in %1").arg(macrosRootPath()));
  macroActions.push_back(emptyAction);
  return;
 }

 for (const QFileInfo& fileInfo : files) {
  QAction* action = macrosMenu->addAction(fileInfo.baseName());
  action->setIcon(QIcon(resolveIconPath("Studio/scriptmenu_run_macro.svg")));
  action->setData(fileInfo.absoluteFilePath());
  action->setToolTip(fileInfo.absoluteFilePath());
  QObject::connect(action, &QAction::triggered, menu_, [this, path = fileInfo.absoluteFilePath()]() {
   runMacroFile(path);
  });
  macroActions.push_back(action);
 }
}

void ArtifactScriptMenu::Impl::refreshAeUtilityActions()
{
 if (!aeUtilityMenu) {
  return;
 }

 for (QAction* action : aeUtilityActions) {
  if (action) {
   aeUtilityMenu->removeAction(action);
   delete action;
  }
 }
 aeUtilityActions.clear();

 ensureScriptsWorkspaceScaffold();
 const QStringList fileNames = {
     QStringLiteral("quick_rename_layers.py"),
     QStringLiteral("clean_layers.py"),
     QStringLiteral("trim_comp_to_content.py")};

 bool hasAny = false;
 for (const QString& fileName : fileNames) {
  const QString path = aeUtilityScriptPath(fileName);
  const QFileInfo info(path);
  QAction* action = aeUtilityMenu->addAction(info.baseName());
  action->setIcon(QIcon(resolveIconPath("Studio/scriptmenu_run_macro.svg")));
  action->setData(info.absoluteFilePath());
  action->setToolTip(info.absoluteFilePath());
  action->setEnabled(info.exists());
  QObject::connect(action, &QAction::triggered, menu_, [this, path]() {
   runMacroFile(path);
  });
  aeUtilityActions.push_back(action);
  hasAny = hasAny || info.exists();
 }

 if (!hasAny) {
  QAction* emptyAction = aeUtilityMenu->addAction(tr("No AE Utility Scripts Yet"));
  emptyAction->setEnabled(false);
  emptyAction->setToolTip(tr("Use the scaffolded stubs under %1").arg(aeUtilityRootPath()));
  aeUtilityActions.push_back(emptyAction);
 }
}

ArtifactScriptMenu::Impl::Impl(ArtifactScriptMenu* menu)
 : menu_(menu)
{
 openScriptsFolderAction = new QAction(tr("Open User Scripts Workspace"));
 openScriptsFolderAction->setIcon(QIcon(resolveIconPath("Studio/scriptmenu_workspace.svg")));
 openScriptsFolderAction->setToolTip(
     tr("Open the canonical user scripts root and scaffold menu.py, hooks, and macros folders."));

 openMenuScriptAction = new QAction(tr("Open menu.py"));
 openMenuScriptAction->setIcon(QIcon(resolveIconPath("Studio/scriptmenu_menu_py.svg")));
 openMenuScriptAction->setToolTip(
     tr("Open the script menu entry file from the user scripts workspace."));

 openHooksFolderAction = new QAction(tr("Open Hook Scripts Folder"));
 openHooksFolderAction->setIcon(QIcon(resolveIconPath("Studio/scriptmenu_hooks_folder.svg")));
 openHooksFolderAction->setToolTip(
     tr("Open the hooks folder inside the user scripts workspace."));

 openMacrosFolderAction = new QAction(tr("Open Macros Folder"));
 openMacrosFolderAction->setIcon(QIcon(resolveIconPath("Studio/scriptmenu_macros_folder.svg")));
 openMacrosFolderAction->setToolTip(
     tr("Open the macros folder inside the user scripts workspace."));

 hooksMenu = new QMenu(tr("Hook Commands"));
 hooksMenu->setIcon(QIcon(resolveIconPath("Studio/scriptmenu_hooks.svg")));

 macrosMenu = new QMenu(tr("Macro Commands"));
 macrosMenu->setIcon(QIcon(resolveIconPath("Studio/scriptmenu_macros.svg")));

 aeUtilityMenu = new QMenu(tr("AE Utility Pack"));
 aeUtilityMenu->setIcon(QIcon(resolveIconPath("Studio/scriptmenu_macros.svg")));

 const QStringList hookNames = ArtifactPythonHookManager::knownHooks();
 for (const QString& hookName : hookNames) {
  QAction* action = hooksMenu->addAction(hookName);
  action->setIcon(QIcon(resolveIconPath("Studio/scriptmenu_hooks.svg")));
  action->setData(hookName);
  action->setToolTip(hookName);
  hookActions.push_back(action);
  QObject::connect(action, &QAction::triggered, menu, [this, hookName]() {
   runHook(hookName);
  });
 }

 menu->addAction(openScriptsFolderAction);
 menu->addAction(openMenuScriptAction);
 menu->addAction(openHooksFolderAction);
 menu->addAction(openMacrosFolderAction);
 menu->addSeparator();
 menu->addMenu(macrosMenu);
 menu->addMenu(aeUtilityMenu);
 menu->addMenu(hooksMenu);
 refreshMacroActions();
 refreshAeUtilityActions();
 refreshHookActions();

 QObject::connect(openScriptsFolderAction, &QAction::triggered, menu, [this]() {
  openFolder(scriptsRootPath());
 });
 QObject::connect(openMenuScriptAction, &QAction::triggered, menu, [this]() {
  openFile(menuScriptPath());
 });
 QObject::connect(openHooksFolderAction, &QAction::triggered, menu, [this]() {
  openFolder(hooksRootPath());
 });
 QObject::connect(openMacrosFolderAction, &QAction::triggered, menu, [this]() {
  openFolder(macrosRootPath());
 });
 QObject::connect(macrosMenu, &QMenu::aboutToShow, menu, [this]() {
  refreshMacroActions();
 });
 QObject::connect(aeUtilityMenu, &QMenu::aboutToShow, menu, [this]() {
  refreshAeUtilityActions();
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
 setIcon(QIcon(resolveIconPath("Studio/menubar_script.svg")));
 setTearOffEnabled(true);
}

ArtifactScriptMenu::~ArtifactScriptMenu()
{
 delete impl_;
}

}
