module;
#include <wobjectimpl.h>
#include <mutex>
#include <QFile>
#include <QMenu>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QFileDialog>
#include <QDir>
#include <QDirIterator>

#include <QApplication>

module Artifact.Menu.File;

import  Artifact.Project.Manager;
import  Artifact.Service.Project;

import Utils;

using namespace Artifact;

namespace Artifact {

 W_OBJECT_IMPL(ArtifactFileMenu)

  class  ArtifactFileMenu::Impl {
  private:

   bool projectCreated_ = false;
  public:
   Impl();
   void rebuildMenu();
   QAction* createProjectAction=nullptr;
   QAction* closeProjectAction=nullptr;
   QAction* saveProjectAction=nullptr;
   QAction* saveProjectAsAction=nullptr;
   QAction* quitApplicationAction=nullptr;

   QAction* openProjectAction=nullptr;
   QAction* projectSettingsAction=nullptr;
   
   QMenu* importMenu=nullptr;
   QAction* importFileAction=nullptr;
   QAction* importFolderAction=nullptr;
   
   QMenu* exportMenu=nullptr;
   QAction* addToRenderQueueAction=nullptr;

   void handleCreateProject();
   void handleOpenProject();
   void handleSaveProject();
   void handleSaveAsProject();
   void handleCloseProject();
   void handleImportFile();
   void handleImportFolder();

 };

 ArtifactFileMenu::Impl::Impl()
 {
  createProjectAction = new QAction("CreateProject");
  createProjectAction->setShortcut(QKeySequence::New);
  createProjectAction->setIcon(QIcon(ArtifactCore::getIconPath() + "/new.png"));



  closeProjectAction = new QAction("CloseProject");
  closeProjectAction->setShortcut(QKeySequence::Close);
  closeProjectAction->setDisabled(true);

  saveProjectAction = new QAction(u8"保存(&S)");
  saveProjectAction->setShortcut(QKeySequence::Save);
  saveProjectAction->setDisabled(true); // 最初は無効 (まだプロジェクトがないため)

  // --- 名前を付けて保存アクション ---
  saveProjectAsAction = new QAction("名前を付けて保存(&A)...");
  saveProjectAsAction->setShortcut(QKeySequence::SaveAs);
  saveProjectAsAction->setDisabled(true); // 最初は無効 (まだプロジェクトがないため)

  quitApplicationAction = new QAction("終了(&Q)");
  quitApplicationAction->setShortcut(QKeySequence::Quit);
  quitApplicationAction->setIcon(QIcon(ArtifactCore::getIconPath() + "/Png/close_red.png"));

  openProjectAction = new QAction("開く(&O)...");
  openProjectAction->setShortcut(QKeySequence::Open);

  projectSettingsAction = new QAction("プロジェクト設定(&P)...");
  projectSettingsAction->setShortcut(QKeySequence("Alt+Shift+Ctrl+P"));
  projectSettingsAction->setDisabled(true);

  // Import Submenu
  importMenu = new QMenu("読み込み(&I)");
  importFileAction = new QAction("ファイル(&F)...");
  importFileAction->setShortcut(QKeySequence("Ctrl+I"));
  importFolderAction = new QAction("フォルダ(&D)...");
  importMenu->addAction(importFileAction);
  importMenu->addAction(importFolderAction);
  importMenu->setDisabled(true);

  // Export Submenu
  exportMenu = new QMenu("書き出し(&E)");
  addToRenderQueueAction = new QAction("レンダーキューに追加");
  addToRenderQueueAction->setShortcut(QKeySequence("Ctrl+M"));
  exportMenu->addAction(addToRenderQueueAction);
  exportMenu->setDisabled(true);
 }

 void ArtifactFileMenu::Impl::rebuildMenu()
 {
  // Update action states based on project status
  bool hasProject = ArtifactProjectManager::getInstance().isProjectCreated();
  saveProjectAction->setEnabled(hasProject);
  saveProjectAsAction->setEnabled(hasProject);
  closeProjectAction->setEnabled(hasProject);
  projectSettingsAction->setEnabled(hasProject);
  importMenu->setEnabled(hasProject);
  exportMenu->setEnabled(hasProject);
  createProjectAction->setEnabled(!hasProject); // Disable if project already exists
  openProjectAction->setEnabled(!hasProject);
 }

 void ArtifactFileMenu::Impl::handleCreateProject()
 {
  ArtifactProjectManager::getInstance().createProject();
 }

 void ArtifactFileMenu::Impl::handleOpenProject()
 {
  // TODO: Implement open project dialog
  QString fileName = QFileDialog::getOpenFileName(nullptr, "Open Project", "", "Artifact Project (*.art)");
  if (!fileName.isEmpty()) {
    ArtifactProjectManager::getInstance().loadFromFile(fileName);
  }
 }

 void ArtifactFileMenu::Impl::handleCloseProject()
 {
  ArtifactProjectManager::getInstance().closeCurrentProject();
 }

 void ArtifactFileMenu::Impl::handleSaveProject()
 {
  if (!ArtifactProjectManager::getInstance().isProjectCreated()) {
   return;
  }
  QString path = ArtifactProjectManager::getInstance().currentProjectPath();
  if (path.isEmpty()) {
   handleSaveAsProject();
   return;
  }
  auto result = ArtifactProjectManager::getInstance().saveToFile(path);
  if (!result.success) {
   // TODO: Show error message to user
  }
 }

 void ArtifactFileMenu::Impl::handleSaveAsProject()
 {
  if (!ArtifactProjectManager::getInstance().isProjectCreated()) {
   return;
  }
  QString fileName = QFileDialog::getSaveFileName(nullptr, "名前を付けて保存", "", "Artifact Project (*.art)");
  if (!fileName.isEmpty()) {
   if (!fileName.endsWith(".art", Qt::CaseInsensitive)) {
    fileName += ".art";
   }
   auto result = ArtifactProjectManager::getInstance().saveToFile(fileName);
   if (!result.success) {
    // TODO: Show error message to user
   }
  }
 }

 void ArtifactFileMenu::Impl::handleImportFile()
 {
  if (!ArtifactProjectManager::getInstance().isProjectCreated()) {
   return;
  }

  QStringList paths = QFileDialog::getOpenFileNames(nullptr, "Import Files", QString(), "All Files (*.*)");
  if (paths.isEmpty()) {
   return;
  }

  auto& projectManager = ArtifactProjectManager::getInstance();
  QStringList copied = projectManager.copyFilesToProjectAssets(paths);
  if (!copied.isEmpty()) {
   projectManager.addAssetsFromFilePaths(copied);
  }
 }

 void ArtifactFileMenu::Impl::handleImportFolder()
 {
  if (!ArtifactProjectManager::getInstance().isProjectCreated()) {
   return;
  }

  QString folderPath = QFileDialog::getExistingDirectory(nullptr, "Import Folder");
  if (folderPath.isEmpty()) {
   return;
  }

  QStringList files;
  QDirIterator it(folderPath, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
   files.append(it.next());
  }

  if (files.isEmpty()) {
   return;
  }

  auto& projectManager = ArtifactProjectManager::getInstance();
  QStringList copied = projectManager.copyFilesToProjectAssets(files);
  if (!copied.isEmpty()) {
   projectManager.addAssetsFromFilePaths(copied);
  }
 }

 ArtifactFileMenu::ArtifactFileMenu(QWidget* parent /*= nullptr*/) :QMenu(parent), Impl_(new Impl())
 {
  setObjectName("FileMenu");

  setTitle("File(&F)");
  setTearOffEnabled(false);

  //auto projectAction = new QAction("CreateProject");
  addAction(Impl_->createProjectAction);
  addAction(Impl_->openProjectAction);
  addSeparator();
  addAction(Impl_->saveProjectAction);
  addAction(Impl_->saveProjectAsAction);
  addAction(Impl_->closeProjectAction);
  addSeparator();
  addMenu(Impl_->importMenu);
  addMenu(Impl_->exportMenu);
  addSeparator();
  addAction(Impl_->projectSettingsAction);
  addSeparator();
  addAction(Impl_->quitApplicationAction);
  addSeparator();


  connect(Impl_->createProjectAction, &QAction::triggered,
   [this]() { Impl_->handleCreateProject(); });

  connect(Impl_->openProjectAction, &QAction::triggered,
   [this]() { Impl_->handleOpenProject(); });

  connect(Impl_->closeProjectAction, &QAction::triggered,
   [this]() { Impl_->handleCloseProject(); });

  connect(Impl_->saveProjectAction, &QAction::triggered,
   [this]() { Impl_->handleSaveProject(); });

  connect(Impl_->saveProjectAsAction, &QAction::triggered,
   [this]() { Impl_->handleSaveAsProject(); });

  connect(Impl_->importFileAction, &QAction::triggered,
   [this]() { Impl_->handleImportFile(); });

  connect(Impl_->importFolderAction, &QAction::triggered,
   [this]() { Impl_->handleImportFolder(); });

  connect(Impl_->quitApplicationAction, &QAction::triggered,
   this, &ArtifactFileMenu::quitApplication);

  connect(this, &QMenu::aboutToShow, this, &ArtifactFileMenu::rebuildMenu);

  // プロジェクト作成/変更時にメニュー状態を更新
  auto* projectService = ArtifactProjectService::instance();
  if (projectService) {
    QObject::connect(projectService, &ArtifactProjectService::projectCreated, this, [this]() {
      this->rebuildMenu();
    });
    QObject::connect(projectService, &ArtifactProjectService::projectChanged, this, [this]() {
      this->rebuildMenu();
    });
    // projectClosed シグナルがあれば同様に接続
  }
 }

 ArtifactFileMenu::~ArtifactFileMenu()
 {
  delete Impl_;
 }

 void ArtifactFileMenu::projectCreateRequested()
 {
  qDebug() << "ReceiverClass::onDataReady() が呼び出されました！";

  ArtifactProjectManager::getInstance().createProject();

 }



 void ArtifactFileMenu::projectClosed()
 {
  qDebug() << "ReceiverClass::projectClosed() が呼び出されました！";
 }

 void ArtifactFileMenu::quitApplication()
 {
  QApplication::quit();
 }

 void ArtifactFileMenu::rebuildMenu()
 {
  Impl_->rebuildMenu();
 }

 void ArtifactFileMenu::resetRecentFilesMenu()
 {

 }

};
