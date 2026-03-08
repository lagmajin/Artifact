module;
#include <QObject>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QApplication>
#include <QDebug>
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
    QAction* quitAction = nullptr;

    void rebuildMenu();
};

ArtifactFileMenu::Impl::Impl(ArtifactFileMenu* menu)
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
    
    quitAction = new QAction("終了(&Q)");
    quitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));

    menu->addAction(createProjectAction);
    menu->addAction(openProjectAction);
    menu->addSeparator();
    menu->addAction(saveProjectAction);
    menu->addAction(saveProjectAsAction);
    menu->addSeparator();
    menu->addAction(closeProjectAction);
    menu->addSeparator();
    menu->addAction(quitAction);

    QObject::connect(createProjectAction, &QAction::triggered, menu, &ArtifactFileMenu::projectCreateRequested);
    QObject::connect(closeProjectAction, &QAction::triggered, menu, &ArtifactFileMenu::projectClosed);
    QObject::connect(quitAction, &QAction::triggered, menu, &ArtifactFileMenu::quitApplication);
}

void ArtifactFileMenu::Impl::rebuildMenu()
{
    auto service = ArtifactProjectService::instance();
    bool hasProject = service && service->hasProject();
    saveProjectAction->setEnabled(hasProject);
    saveProjectAsAction->setEnabled(hasProject);
    closeProjectAction->setEnabled(hasProject);
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
    // Implementation details...
}

void ArtifactFileMenu::projectClosed()
{
    qDebug() << "Project closed";
    // Implementation details...
}

void ArtifactFileMenu::quitApplication()
{
    QApplication::quit();
}

void ArtifactFileMenu::resetRecentFilesMenu()
{
}

} // namespace Artifact
