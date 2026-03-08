module;
#include <QObject>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QLineEdit>
#include <QFileInfo>
#include <wobjectimpl.h>

module Artifact.Menu.File;

import Artifact.Project.Manager;
import Artifact.Service.Project;

namespace Artifact {

class ArtifactFileMenu::Impl {
public:
    Impl(ArtifactFileMenu* menu);
    ~Impl() = default;

    QAction* createProjectAction = nullptr;
    QAction* openProjectAction = nullptr;
    QAction* saveProjectAction = nullptr;
    QAction* saveProjectAsAction = nullptr;
    QAction* closeProjectAction = nullptr;
    QAction* newCompositionAction = nullptr;
    QAction* importAssetsAction = nullptr;
    QAction* revealProjectFolderAction = nullptr;
    QAction* quitAction = nullptr;
    QMenu* recentProjectsMenu = nullptr;
    ArtifactFileMenu* menu_ = nullptr;

    void rebuildMenu();
    void handleCreateProject();
    void handleOpenProject();
    void handleSaveProject();
    void handleSaveProjectAs();
    void handleNewComposition();
    void handleImportAssets();
    void handleRevealProjectFolder();
};

ArtifactFileMenu::Impl::Impl(ArtifactFileMenu* menu)
    : menu_(menu)
{
    createProjectAction = new QAction("新規プロジェクト(&N)...");
    createProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N));

    openProjectAction = new QAction("プロジェクトを開く(&O)...");
    openProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));

    saveProjectAction = new QAction("保存(&S)");
    saveProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));

    saveProjectAsAction = new QAction("名前を付けて保存(&A)...");
    saveProjectAsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));

    closeProjectAction = new QAction("プロジェクトを閉じる");
    newCompositionAction = new QAction("新規コンポジション(&C)...");
    newCompositionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    importAssetsAction = new QAction("アセットを読み込み(&I)...");
    importAssetsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    revealProjectFolderAction = new QAction("プロジェクトフォルダを開く");
    
    quitAction = new QAction("終了(&Q)");
    quitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));

    menu->addAction(createProjectAction);
    menu->addAction(openProjectAction);
    menu->addSeparator();
    menu->addAction(saveProjectAction);
    menu->addAction(saveProjectAsAction);
    menu->addSeparator();
    menu->addAction(newCompositionAction);
    menu->addAction(importAssetsAction);
    menu->addSeparator();
    menu->addAction(closeProjectAction);
    menu->addAction(revealProjectFolderAction);
    recentProjectsMenu = menu->addMenu("最近使ったプロジェクト");
    menu->addSeparator();
    menu->addAction(quitAction);

    QObject::connect(createProjectAction, &QAction::triggered, menu, [this]() { handleCreateProject(); });
    QObject::connect(openProjectAction, &QAction::triggered, menu, [this]() { handleOpenProject(); });
    QObject::connect(saveProjectAction, &QAction::triggered, menu, [this]() { handleSaveProject(); });
    QObject::connect(saveProjectAsAction, &QAction::triggered, menu, [this]() { handleSaveProjectAs(); });
    QObject::connect(newCompositionAction, &QAction::triggered, menu, [this]() { handleNewComposition(); });
    QObject::connect(importAssetsAction, &QAction::triggered, menu, [this]() { handleImportAssets(); });
    QObject::connect(revealProjectFolderAction, &QAction::triggered, menu, [this]() { handleRevealProjectFolder(); });
    QObject::connect(closeProjectAction, &QAction::triggered, menu, &ArtifactFileMenu::projectClosed);
    QObject::connect(quitAction, &QAction::triggered, menu, &ArtifactFileMenu::quitApplication);
}

void ArtifactFileMenu::Impl::handleCreateProject()
{
    if (!menu_) return;
    auto dir = QFileDialog::getExistingDirectory(menu_, "新規プロジェクトの保存先を選択");
    if (dir.isEmpty()) return;
    const QString name = QFileInfo(dir).fileName();
    auto& manager = ArtifactProjectManager::getInstance();
    auto result = manager.createProject(UniString(name), true);
    if (!result.isSuccess) {
        qWarning() << "Create project failed";
    }
}

void ArtifactFileMenu::Impl::handleOpenProject()
{
    if (!menu_) return;
    const QString filePath = QFileDialog::getOpenFileName(menu_, "プロジェクトを開く", QString(), "Artifact Project (*.artifact *.json);;All Files (*.*)");
    if (filePath.isEmpty()) return;
    ArtifactProjectManager::getInstance().loadFromFile(filePath);
}

void ArtifactFileMenu::Impl::handleSaveProject()
{
    if (!menu_) return;
    auto& manager = ArtifactProjectManager::getInstance();
    QString path = manager.currentProjectPath();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(menu_, "プロジェクトを保存", QString(), "Artifact Project (*.artifact *.json);;All Files (*.*)");
        if (path.isEmpty()) return;
    }
    auto result = manager.saveToFile(path);
    if (!result.success) {
        qWarning() << "Save project failed";
    }
}

void ArtifactFileMenu::Impl::handleSaveProjectAs()
{
    if (!menu_) return;
    const QString path = QFileDialog::getSaveFileName(menu_, "名前を付けて保存", QString(), "Artifact Project (*.artifact *.json);;All Files (*.*)");
    if (path.isEmpty()) return;
    auto result = ArtifactProjectManager::getInstance().saveToFile(path);
    if (!result.success) {
        qWarning() << "Save project as failed";
    }
}

void ArtifactFileMenu::Impl::handleNewComposition()
{
    if (!menu_) return;
    bool ok = false;
    const QString name = QInputDialog::getText(menu_, "新規コンポジション", "名前:", QLineEdit::Normal, "Composition", &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    if (auto* svc = ArtifactProjectService::instance()) {
        svc->createComposition(UniString(name.trimmed()));
    }
}

void ArtifactFileMenu::Impl::handleImportAssets()
{
    if (!menu_) return;
    const QStringList files = QFileDialog::getOpenFileNames(menu_, "アセットを読み込み", QString(), "All Files (*.*)");
    if (files.isEmpty()) return;
    if (auto* svc = ArtifactProjectService::instance()) {
        svc->importAssetsFromPaths(files);
    }
}

void ArtifactFileMenu::Impl::handleRevealProjectFolder()
{
    const QString path = ArtifactProjectManager::getInstance().currentProjectPath();
    if (path.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

void ArtifactFileMenu::Impl::rebuildMenu()
{
    auto service = ArtifactProjectService::instance();
    bool hasProject = service && service->hasProject();
    saveProjectAction->setEnabled(hasProject);
    saveProjectAsAction->setEnabled(hasProject);
    closeProjectAction->setEnabled(hasProject);
    newCompositionAction->setEnabled(hasProject);
    importAssetsAction->setEnabled(hasProject);
    revealProjectFolderAction->setEnabled(hasProject);

    if (recentProjectsMenu) {
        recentProjectsMenu->clear();
        auto* noRecent = recentProjectsMenu->addAction("(近日対応)");
        noRecent->setEnabled(false);
    }
}

W_OBJECT_IMPL(ArtifactFileMenu)

ArtifactFileMenu::ArtifactFileMenu(QWidget* parent)
    : QMenu(parent), Impl_(new Impl(this))
{
    setTitle("ファイル(&F)");
    connect(this, &QMenu::aboutToShow, this, &ArtifactFileMenu::rebuildMenu);
}

ArtifactFileMenu::~ArtifactFileMenu()
{
    delete Impl_;
}

void ArtifactFileMenu::rebuildMenu()
{
    Impl_->rebuildMenu();
}

void ArtifactFileMenu::projectCreateRequested()
{
    qDebug() << "Project create requested";
}

void ArtifactFileMenu::projectClosed()
{
    ArtifactProjectManager::getInstance().closeCurrentProject();
}

void ArtifactFileMenu::quitApplication()
{
    QApplication::quit();
}

void ArtifactFileMenu::resetRecentFilesMenu()
{
}

} // namespace Artifact
