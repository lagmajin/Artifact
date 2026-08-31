module;
#include <utility>
#include <QObject>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QDialog>
#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QLineEdit>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QCoreApplication>
#include <QTimer>
#include <QProgressDialog>
#include <QPointer>
#include <QRegularExpression>
#include <QPainter>
#include <QImage>
#include <QVector>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <wobjectimpl.h>

module Artifact.Menu.File;
import std;

import Artifact.Project.Manager;
import Artifact.Project.Packager;
import Artifact.Project.Statistics;
import Artifact.Composition.InitParams;
import Artifact.Service.Project;
import Artifact.Widgets.ImportAssetsDialog;
import Artifact.Export.Dialog;
import Application.AppSettings;
import Utils.Path;
import Artifact.Widgets.AppDialogs;
import Undo.UndoManager;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layers.SolidImage;
import Artifact.Layer.NLETransitionBridge;
import NLE.Core;
import NLE.OTIO;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Translation.Manager;

namespace Artifact {
using namespace ArtifactCore;
namespace {
constexpr int kMaxRecentProjects = 5;

QString normalizedProjectPath(const QString& path)
{
    const QString trimmed = path.trimmed();
    return trimmed.isEmpty()
        ? QString()
        : QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());
}

QString menuText(const QString& key, const QString& fallback)
{
    return TranslationManager::instance().tr(key, fallback);
}

void addRecentProject(const QString& path)
{
    const QString normalizedPath = normalizedProjectPath(path);
    if (normalizedPath.isEmpty()) {
        return;
    }
    auto* settings = ArtifactAppSettings::instance();
    if (!settings) {
        return;
    }
    auto recent = settings->recentProjectPaths();
    recent.removeAll(normalizedPath);
    recent.prepend(normalizedPath);
    while (recent.size() > kMaxRecentProjects) {
        recent.removeLast();
    }
    settings->setRecentProjectPaths(recent);
}

QStringList pruneMissingRecentProjects(const QStringList& paths)
{
    QStringList pruned;
    QSet<QString> seen;
    pruned.reserve(paths.size());
    for (const QString& path : paths) {
        const QString normalizedPath = normalizedProjectPath(path);
        if (normalizedPath.isEmpty() || seen.contains(normalizedPath)) {
            continue;
        }
        seen.insert(normalizedPath);
        if (QFileInfo(normalizedPath).exists()) {
            pruned.push_back(normalizedPath);
        }
    }
    return pruned;
}

QString supportedAssetFilter()
{
    return QStringLiteral(
        "対応アセット (*.png *.jpg *.jpeg *.bmp *.gif *.tga *.tif *.tiff *.webp *.hdr *.exr *.ico *.dds *.ktx *.psd *.psb *.svg "
        "*.mp4 *.mov *.mkv *.avi *.webm *.mp3 *.wav *.flac *.ogg *.aac *.m4a "
        "*.obj *.fbx *.gltf *.glb *.pmd *.abc *.usd *.usda *.usdc);;"
        "画像 (*.png *.jpg *.jpeg *.bmp *.gif *.tga *.tif *.tiff *.webp *.hdr *.exr *.ico *.dds *.ktx *.psd *.psb *.svg);;"
        "動画 (*.mp4 *.mov *.mkv *.avi *.webm);;"
        "音声 (*.mp3 *.wav *.flac *.ogg *.aac *.m4a);;"
        "3D (*.obj *.fbx *.gltf *.glb *.pmd *.abc *.usd *.usda *.usdc)"
    );
}

bool confirmPotentiallyDestructiveAction(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setIcon(QMessageBox::Warning);
    box.setText(text);
    box.setMinimumWidth(760);
    box.setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* yesButton = box.addButton(QStringLiteral("はい"), QMessageBox::AcceptRole);
    auto* noButton = box.addButton(QStringLiteral("いいえ"), QMessageBox::RejectRole);
    box.setDefaultButton(noButton);
    box.exec();
    return box.clickedButton() == yesButton;
}

// P0-2: 保存確認付き終了/プロジェクトクローズ
bool confirmUnsavedChanges(QWidget* parent, const QString& actionName)
{
    auto* svc = ArtifactProjectService::instance();
    if (!svc || !svc->hasProject()) return true;

    auto project = svc->getCurrentProjectSharedPtr();
    if (!project) return true;

    // Check if project is dirty
    bool hasUnsaved = project->isDirty();
    if (!hasUnsaved) {
        if (auto* undoMgr = UndoManager::instance()) {
            hasUnsaved = undoMgr->hasUnsavedChanges();
        }
    }

    if (!hasUnsaved) return true;

    QMessageBox box(parent);
    box.setWindowTitle(QStringLiteral("保存の確認"));
    box.setIcon(QMessageBox::Warning);
    box.setText(QStringLiteral("プロジェクトに変更があります。%1 前に保存しますか？").arg(actionName));
    box.setInformativeText(QStringLiteral("未保存の変更は失われる可能性があります。"));

    auto* saveButton = box.addButton(QStringLiteral("保存"), QMessageBox::AcceptRole);
    auto* discardButton = box.addButton(QStringLiteral("破棄"), QMessageBox::DestructiveRole);
    auto* cancelButton = box.addButton(QStringLiteral("キャンセル"), QMessageBox::RejectRole);
    box.setDefaultButton(saveButton);

    box.exec();

    if (box.clickedButton() == cancelButton) return false;
    if (box.clickedButton() == discardButton) return true;

    // Save
    auto& manager = ArtifactProjectManager::getInstance();
    QString path = manager.currentProjectPath();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(parent, "プロジェクトを保存", QString(),
            "Artifact Project (*.artifact *.json);;All Files (*.*)");
        if (path.isEmpty()) return false;
    }
    auto result = manager.saveToFile(path);
    return result.success;
}
}

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
    QAction* importOtioAction = nullptr;
    QAction* revealProjectFolderAction = nullptr;
    QAction* exportFontUsageAction = nullptr;
    QAction* restartAction = nullptr;
    QAction* quitAction = nullptr;
    QMenu* exportMenu = nullptr;
    QAction* exportCurrentFrameAction = nullptr;
    QAction* exportWorkAreaAction = nullptr;
    QAction* exportProjectPackageAction = nullptr;
    QAction* exportCompositionAction = nullptr;
    QAction* exportOtioAction = nullptr;
    QMenu* recentProjectsMenu = nullptr;
    QStringList cachedRecentProjects_; // 変更がない場合にメニューを再構築しないためのキャッシュ
    ArtifactFileMenu* menu_ = nullptr;

    void rebuildMenu();
    void handleCreateProject();
    void handleOpenProject();
    void handleSaveProject();
    void handleSaveProjectAs();
    void handleNewComposition();
    void handleImportAssets();
    void handleImportOtio();
    void handleRevealProjectFolder();
    void handleExportCurrentFrame();
    void handleExportWorkArea();
    void handleExportProjectPackage();
    void handleExportComposition();
    void handleExportOtio();
    void handleExportFontUsage();
    void openProjectPath(const QString& path, bool addToRecent);
};

ArtifactFileMenu::Impl::Impl(ArtifactFileMenu* menu)
    : menu_(menu)
{
    createProjectAction = new QAction(menuText(QStringLiteral("menu.file.new_project"), QStringLiteral("新規プロジェクト(&N)...")));
    createProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N));
    createProjectAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_new_project.svg")));

    openProjectAction = new QAction(menuText(QStringLiteral("menu.file.open_project"), QStringLiteral("プロジェクトを開く(&O)...")));
    openProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    openProjectAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_open_project.svg")));

    saveProjectAction = new QAction(menuText(QStringLiteral("menu.file.save_project"), QStringLiteral("保存(&S)")));
    saveProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    saveProjectAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_save_project.svg")));

    saveProjectAsAction = new QAction(menuText(QStringLiteral("menu.file.save_as"), QStringLiteral("名前を付けて保存(&A)...")));
    saveProjectAsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    saveProjectAsAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_save_project_as.svg")));

    closeProjectAction = new QAction(menuText(QStringLiteral("menu.file.close_project"), QStringLiteral("プロジェクトを閉じる")));
    closeProjectAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_close_project.svg")));

    newCompositionAction = new QAction(menuText(QStringLiteral("menu.composition.new_composition"), QStringLiteral("新規コンポジション(&C)...")));
    newCompositionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    newCompositionAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_new_composition.svg")));

    importAssetsAction = new QAction(menuText(QStringLiteral("menu.file.import"), QStringLiteral("アセットを読み込み(&I)...")));
    importAssetsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    importAssetsAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_import_assets.svg")));
    importOtioAction = new QAction(QStringLiteral("OpenTimelineIOを読み込む..."), menu);

    revealProjectFolderAction = new QAction(menuText(QStringLiteral("menu.file.reveal_folder"), QStringLiteral("プロジェクトフォルダを開く")));
    revealProjectFolderAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_reveal_folder.svg")));

    exportFontUsageAction = new QAction(menuText(QStringLiteral("menu.file.export_fonts"), QStringLiteral("使用フォントレポートを書き出す...")));

    exportCompositionAction = new QAction(QStringLiteral("CompositionをゲームUI形式で書き出す..."), menu);
    exportOtioAction = new QAction(QStringLiteral("OpenTimelineIOを書き出す..."), menu);
    exportCurrentFrameAction = new QAction(QStringLiteral("現在のフレームを書き出す..."), menu);
    exportWorkAreaAction = new QAction(QStringLiteral("ワークエリアを書き出す..."), menu);
    exportProjectPackageAction = new QAction(QStringLiteral("プロジェクトをパッケージ化..."), menu);

    restartAction = new QAction(menuText(QStringLiteral("menu.file.restart"), QStringLiteral("再起動")));
    restartAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_restart.svg")));
    
    quitAction = new QAction(menuText(QStringLiteral("menu.file.quit"), QStringLiteral("終了(&Q)")));
    quitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    quitAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_quit.svg")));

    menu->addAction(createProjectAction);
    menu->addAction(openProjectAction);
    menu->addSeparator();
    menu->addAction(saveProjectAction);
    menu->addAction(saveProjectAsAction);
    menu->addSeparator();
    menu->addAction(newCompositionAction);
    menu->addAction(importAssetsAction);
    menu->addAction(importOtioAction);
    menu->addSeparator();
    menu->addAction(closeProjectAction);
    menu->addAction(revealProjectFolderAction);
    menu->addAction(exportFontUsageAction);
    exportMenu = menu->addMenu(QStringLiteral("エクスポート"));
    exportMenu->addAction(exportCompositionAction);
    exportMenu->addAction(exportOtioAction);
    exportMenu->addSeparator();
    exportMenu->addAction(exportCurrentFrameAction);
    exportMenu->addAction(exportWorkAreaAction);
    exportMenu->addSeparator();
    exportMenu->addAction(exportProjectPackageAction);
    recentProjectsMenu = menu->addMenu(menuText(QStringLiteral("menu.file.recent_projects"), QStringLiteral("最近使ったプロジェクト")));
    recentProjectsMenu->setIcon(QIcon(resolveIconPath("Studio/filemenu_recent_projects.svg")));
    menu->addSeparator();
    menu->addAction(restartAction);
    menu->addAction(quitAction);

    QObject::connect(createProjectAction, &QAction::triggered, menu, [this]() { handleCreateProject(); });
    QObject::connect(openProjectAction, &QAction::triggered, menu, [this]() { handleOpenProject(); });
    QObject::connect(saveProjectAction, &QAction::triggered, menu, [this]() { handleSaveProject(); });
    QObject::connect(saveProjectAsAction, &QAction::triggered, menu, [this]() { handleSaveProjectAs(); });
    QObject::connect(newCompositionAction, &QAction::triggered, menu, [this]() { handleNewComposition(); });
    QObject::connect(importAssetsAction, &QAction::triggered, menu, [this]() { handleImportAssets(); });
    QObject::connect(importOtioAction, &QAction::triggered, menu, [this]() { handleImportOtio(); });
    QObject::connect(revealProjectFolderAction, &QAction::triggered, menu, [this]() { handleRevealProjectFolder(); });
    QObject::connect(exportFontUsageAction, &QAction::triggered, menu,
                     [this]() { handleExportFontUsage(); });
    QObject::connect(exportCompositionAction, &QAction::triggered, menu,
                     [this]() { handleExportComposition(); });
    QObject::connect(exportOtioAction, &QAction::triggered, menu,
                     [this]() { handleExportOtio(); });
    QObject::connect(exportCurrentFrameAction, &QAction::triggered, menu,
                     [this]() { handleExportCurrentFrame(); });
    QObject::connect(exportWorkAreaAction, &QAction::triggered, menu,
                     [this]() { handleExportWorkArea(); });
    QObject::connect(exportProjectPackageAction, &QAction::triggered, menu,
                     [this]() { handleExportProjectPackage(); });
    QObject::connect(closeProjectAction, &QAction::triggered, menu, &ArtifactFileMenu::projectClosed);
    QObject::connect(restartAction, &QAction::triggered, menu, &ArtifactFileMenu::restartApplication);
    QObject::connect(quitAction, &QAction::triggered, menu, &ArtifactFileMenu::quitApplication);
}

void ArtifactFileMenu::Impl::handleCreateProject()
{
    if (!menu_) return;
    if (!confirmUnsavedChanges(menu_, QStringLiteral("新規プロジェクトを作成"))) {
        return;
    }

    const QStringList starterChoices = {
        QStringLiteral("Blank Project"),
        QStringLiteral("Starter: Full HD Composition"),
        QStringLiteral("Starter: Vertical Ad Composition"),
        QStringLiteral("Starter: Square Social Composition")
    };
    bool starterOk = false;
    const QString starterChoice = QInputDialog::getItem(
        menu_, QStringLiteral("新規プロジェクト"),
        QStringLiteral("スターター:"), starterChoices, 0, false, &starterOk);
    if (!starterOk || starterChoice.trimmed().isEmpty()) {
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(
        menu_, QStringLiteral("新規プロジェクト"),
        QStringLiteral("プロジェクト名:"),
        QLineEdit::Normal, QStringLiteral("UntitledProject"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    auto& manager = ArtifactProjectManager::getInstance();
    auto result = manager.createProject(UniString(name.trimmed()), true);
    if (!result.isSuccess) {
        QMessageBox::warning(menu_, QStringLiteral("新規プロジェクト"),
                             QStringLiteral("プロジェクトを作成できませんでした。"));
        return;
    }
    const QString projectPath = manager.currentProjectPath();
    addRecentProject(projectPath);

    auto* svc = ArtifactProjectService::instance();
    if (!svc || starterChoice == starterChoices.front()) {
        return;
    }

    ArtifactCompositionInitParams starterParams = ArtifactCompositionInitParams::hdPreset();
    QString compName = QStringLiteral("Main");
    if (starterChoice == starterChoices.at(1)) {
        starterParams = ArtifactCompositionInitParams::hdPreset();
        compName = QStringLiteral("Main");
    } else if (starterChoice == starterChoices.at(2)) {
        starterParams = ArtifactCompositionInitParams::verticalPreset();
        compName = QStringLiteral("Vertical Ad");
    } else if (starterChoice == starterChoices.at(3)) {
        starterParams = ArtifactCompositionInitParams::squarePreset();
        compName = QStringLiteral("Square Social");
    }

    starterParams.setCompositionName(UniString(compName));
    svc->createComposition(starterParams);
}

void ArtifactFileMenu::Impl::handleOpenProject()
{
    if (!menu_) return;
    if (!confirmUnsavedChanges(menu_, QStringLiteral("別のプロジェクトを開く"))) {
        return;
    }
    const QString filePath = QFileDialog::getOpenFileName(menu_, "プロジェクトを開く", QString(), "Artifact Project (*.artifact *.json);;All Files (*.*)");
    if (filePath.isEmpty()) return;
    openProjectPath(filePath, true);
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
    const QPointer<ArtifactFileMenu> menuGuard(menu_);
    manager.saveToFileAsync(
        path,
        [menuGuard, path](const ArtifactProjectExporterResult& result) {
            if (result.success) {
                addRecentProject(path);
                return;
            }
            qWarning() << "Save project failed" << result.errorMessage;
            if (menuGuard) {
                QMessageBox::warning(menuGuard, QStringLiteral("プロジェクトを保存"),
                                     QStringLiteral("プロジェクトを保存できませんでした。\n%1")
                                         .arg(result.errorMessage));
            }
        });
}

void ArtifactFileMenu::Impl::handleSaveProjectAs()
{
    if (!menu_) return;
    const QString path = QFileDialog::getSaveFileName(menu_, "名前を付けて保存", QString(), "Artifact Project (*.artifact *.json);;All Files (*.*)");
    if (path.isEmpty()) return;
    const QPointer<ArtifactFileMenu> menuGuard(menu_);
    ArtifactProjectManager::getInstance().saveToFileAsync(
        path,
        [menuGuard, path](const ArtifactProjectExporterResult& result) {
            if (result.success) {
                addRecentProject(path);
                return;
            }
            qWarning() << "Save project as failed" << result.errorMessage;
            if (menuGuard) {
                QMessageBox::warning(menuGuard, QStringLiteral("プロジェクトを保存"),
                                     QStringLiteral("プロジェクトを保存できませんでした。\n%1")
                                         .arg(result.errorMessage));
            }
        });
}

void ArtifactFileMenu::Impl::handleNewComposition()
{
    if (!menu_) return;

    // Preset selection
    struct PresetEntry {
        QString label;
        ArtifactCompositionInitParams params;
    };
    ArtifactCompositionInitParams hd720Preset = ArtifactCompositionInitParams::hdPreset();
    hd720Preset.setResolution(1280, 720);
    hd720Preset.setFrameRate(30.0);
    ArtifactCompositionInitParams vertical60Preset = ArtifactCompositionInitParams::verticalPreset();
    vertical60Preset.setFrameRate(60.0);
    const QVector<PresetEntry> presets = {
        {QStringLiteral("1920 x 1080  @ 30fps (Full HD)"), ArtifactCompositionInitParams::hdPreset()},
        {QStringLiteral("2048 x 858   @ 24fps (Cinema)"), ArtifactCompositionInitParams::cinemaPreset()},
        {QStringLiteral("3840 x 2160  @ 30fps (4K UHD)"), ArtifactCompositionInitParams::fourKPreset()},
        {QStringLiteral("1280 x 720   @ 30fps (HD)"), hd720Preset},
        {QStringLiteral("1080 x 1920  @ 30fps (Vertical HD)"), ArtifactCompositionInitParams::verticalPreset()},
        {QStringLiteral("1080 x 1920  @ 60fps (Vertical 60)"), vertical60Preset},
        {QStringLiteral("1080 x 1080  @ 30fps (Square)"), ArtifactCompositionInitParams::squarePreset()},
        {QStringLiteral("1920 x 1080  @ 60fps (Full HD 60)"), ArtifactCompositionInitParams::fullHd60Preset()}
    };

    bool ok = false;
    QStringList presetLabels;
    presetLabels.reserve(presets.size());
    for (const auto& preset : presets) {
        presetLabels.push_back(preset.label);
    }
    const QString preset = QInputDialog::getItem(menu_, "新規コンポジション", "プリセット:", presetLabels, 0, false, &ok);
    if (!ok) return;

    ArtifactCompositionInitParams params = ArtifactCompositionInitParams::hdPreset();
    for (const auto& entry : presets) {
        if (entry.label == preset) {
            params = entry.params;
            break;
        }
    }

    const QString name = QInputDialog::getText(menu_, "コンポジション名", "名前:", QLineEdit::Normal, "Composition", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    params.setCompositionName(UniString(name.trimmed()));
    if (auto* svc = ArtifactProjectService::instance()) {
        svc->createComposition(params);
    }
}

void ArtifactFileMenu::Impl::handleImportAssets()
{
    if (!menu_) return;
    auto* svc = ArtifactProjectService::instance();
    if (!svc || !svc->hasProject()) {
        QMessageBox::warning(menu_, QStringLiteral("アセットを読み込み"),
                             QStringLiteral("先にプロジェクトを開いてください。"));
        return;
    }
    const QStringList files = QFileDialog::getOpenFileNames(
        menu_,
        QStringLiteral("アセットを読み込み"),
        QString(),
        supportedAssetFilter()
    );
    if (files.isEmpty()) return;
    ArtifactImportAssetsDialog dialog(files, menu_);
    if (dialog.exec() != QDialog::Accepted) return;
    const QStringList filtered = dialog.selectedPaths();
    if (filtered.isEmpty()) {
      return;
    }
    const QPointer<ArtifactFileMenu> menuGuard(menu_);
    svc->importAssetsFromPathsAsync(
        filtered,
        [menuGuard](const QStringList& imported) {
            if (!menuGuard || !imported.isEmpty()) {
                return;
            }
            QMessageBox::warning(menuGuard, QStringLiteral("アセットを読み込み"),
                                 QStringLiteral("読み込めるアセットがありませんでした。"));
        });
}

void ArtifactFileMenu::Impl::handleRevealProjectFolder()
{
    const QString path = ArtifactProjectManager::getInstance().currentProjectPath();
    if (path.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

void ArtifactFileMenu::Impl::handleImportOtio()
{
    if (!menu_) return;
    const QString filePath = QFileDialog::getOpenFileName(
        menu_, QStringLiteral("OpenTimelineIOを読み込む"), QString(),
        QStringLiteral("OpenTimelineIO (*.otio);;All Files (*.*)"));
    if (filePath.isEmpty()) return;

    ArtifactCore::NLE::NLEProjectStore store;
    ArtifactCore::NLE::SequenceId sequenceId;
    QVector<QString> warnings;
    if (!ArtifactCore::NLE::OtioAdapter::importTimelineFile(
            store, filePath, &sequenceId, &warnings)) {
        QMessageBox::warning(menu_, QStringLiteral("OpenTimelineIO"),
                             QStringLiteral("OTIOを読み込めませんでした。\n%1")
                                 .arg(warnings.join(QStringLiteral("\n"))));
        return;
    }

    const auto* sequence = store.sequence(sequenceId);
    const int trackCount = sequence ? sequence->trackOrder.size() : 0;
    const int clipCount = sequence ? store.clipIdsInSequence(sequenceId).size() : 0;
    qint64 importedDuration = sequence ? sequence->duration.duration() : 0;
    int transitionCount = 0;
    int unsupportedTransitionCount = 0;
    if (sequence) {
        for (const auto& trackId : sequence->trackOrder) {
            const auto* track = store.track(trackId);
            if (!track) continue;
            transitionCount += track->transitions.size();
            for (const auto& transitionId : track->transitions) {
                const auto* transition = store.transition(transitionId);
                if (transition && transition->kind != ArtifactCore::NLE::TransitionKind::Cut &&
                    transition->kind != ArtifactCore::NLE::TransitionKind::Crossfade &&
                    transition->kind != ArtifactCore::NLE::TransitionKind::Dissolve) {
                    ++unsupportedTransitionCount;
                }
            }
            for (const auto& clipId : track->clipOrder) {
                if (const auto* clip = store.clip(clipId)) {
                    importedDuration = qMax(importedDuration, clip->timelineRange.end());
                }
            }
        }
    }
    if (transitionCount > 0 && unsupportedTransitionCount > 0) {
        warnings.push_back(QStringLiteral(
                               "%1 transition(s) are preserved as timeline metadata; %2 complex transition(s) still await dedicated render application")
                               .arg(transitionCount)
                               .arg(unsupportedTransitionCount));
    }
    QString message = QStringLiteral("OTIOを読み込みました。\n\nシーケンス: %1\nトラック: %2\nクリップ: %3")
        .arg(sequence ? sequence->name : QStringLiteral("(unknown)"))
        .arg(trackCount)
        .arg(clipCount);
    if (!warnings.isEmpty()) {
        message += QStringLiteral("\n\n警告:\n") + warnings.join(QStringLiteral("\n"));
    }
    const auto choice = QMessageBox::question(
        menu_, QStringLiteral("OpenTimelineIO"),
        message + QStringLiteral("\n\n新しいCompositionを作成しますか？\n既存Compositionは変更されません。"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (choice != QMessageBox::Yes || !sequence) return;

    ArtifactCompositionInitParams params;
    params.setCompositionName(UniString(sequence->name.isEmpty()
        ? QStringLiteral("OTIO Import") : sequence->name));
    params.setFrameRate(sequence->timeBase.fps());
    params.setDurationFrames(qMax<qint64>(1, importedDuration));
    auto* service = ArtifactProjectService::instance();
    if (!service) return;
    service->createComposition(params);
    const auto composition = service->currentComposition().lock();
    if (!composition) {
        QMessageBox::warning(menu_, QStringLiteral("OpenTimelineIO"),
                             QStringLiteral("Import用Compositionを作成できませんでした。"));
        return;
    }

    int importedCount = 0;
    ArtifactLayerFactory layerFactory;
    for (const auto& trackId : sequence->trackOrder) {
        const auto* track = store.track(trackId);
        if (!track) continue;
        for (const auto& clipId : track->clipOrder) {
            const auto* clip = store.clip(clipId);
            const auto* source = clip ? store.source(clip->sourceId) : nullptr;
            if (!clip || !source) continue;
            const QString uri = source->uri;
            const QString path = uri.startsWith(QStringLiteral("file:///"))
                ? QUrl(uri).toLocalFile() : uri;
            const QString name = clip->name.isEmpty() ? source->displayName : clip->name;
            ArtifactAbstractLayerPtr layer;
            const QString suffix = QFileInfo(path).suffix().toLower();
            const bool isPlaceholder = uri.startsWith(QStringLiteral("artifact://"));
            if (isPlaceholder) {
                ArtifactNullLayerInitParams layerParams(name);
                layer = layerFactory.createNewLayer(layerParams);
                warnings.push_back(QStringLiteral("Created placeholder layer for unavailable source: %1")
                                       .arg(name));
            } else if (track->kind == ArtifactCore::NLE::TrackKind::Audio) {
                ArtifactAudioInitParams layerParams(name);
                layerParams.setAudioPath(path);
                layer = layerFactory.createNewLayer(layerParams);
            } else if (suffix == QStringLiteral("svg")) {
                ArtifactSvgInitParams layerParams(name);
                layerParams.setSvgPath(path);
                layer = layerFactory.createNewLayer(layerParams);
            } else if (QStringList{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                  QStringLiteral("exr"), QStringLiteral("tif"), QStringLiteral("tiff"),
                                  QStringLiteral("webp")}.contains(suffix)) {
                ArtifactImageInitParams layerParams(name);
                layerParams.setImagePath(path);
                layer = layerFactory.createNewLayer(layerParams);
            } else {
                ArtifactVideoInitParams layerParams(name);
                layerParams.setVideoPath(path);
                layer = layerFactory.createNewLayer(layerParams);
            }
            if (!layer) {
                warnings.push_back(QStringLiteral("Failed to create imported layer: %1").arg(name));
                continue;
            }
            layer->setInPoint(FramePosition(clip->timelineRange.start()));
            layer->setOutPoint(FramePosition(clip->timelineRange.end()));
            layer->setVisible(clip->enabled);
            composition->appendLayerTop(layer);
            ++importedCount;
        }
    }
    for (const auto& trackId : sequence->trackOrder) {
        const auto* track = store.track(trackId);
        if (!track) continue;
        for (const auto& transitionId : track->transitions) {
            const auto* transition = store.transition(transitionId);
            const auto* leftClip = transition ? store.clip(transition->leftClipId) : nullptr;
            const auto* rightClip = transition ? store.clip(transition->rightClipId) : nullptr;
            if (!transition || !leftClip || !rightClip) continue;
            CompositionTimelineTransition importedTransition;
            switch (transition->kind) {
            case ArtifactCore::NLE::TransitionKind::Cut: importedTransition.kind = QStringLiteral("Cut"); break;
            case ArtifactCore::NLE::TransitionKind::Crossfade: importedTransition.kind = QStringLiteral("Crossfade"); break;
            case ArtifactCore::NLE::TransitionKind::Dissolve: importedTransition.kind = QStringLiteral("Dissolve"); break;
            case ArtifactCore::NLE::TransitionKind::Wipe: importedTransition.kind = QStringLiteral("Wipe"); break;
            case ArtifactCore::NLE::TransitionKind::Slide: importedTransition.kind = QStringLiteral("Slide"); break;
            case ArtifactCore::NLE::TransitionKind::Zoom: importedTransition.kind = QStringLiteral("Zoom"); break;
            case ArtifactCore::NLE::TransitionKind::GlitchDisplace: importedTransition.kind = QStringLiteral("GlitchDisplace"); break;
            case ArtifactCore::NLE::TransitionKind::Spin: importedTransition.kind = QStringLiteral("Spin"); break;
            case ArtifactCore::NLE::TransitionKind::LinearWipe: importedTransition.kind = QStringLiteral("LinearWipe"); break;
            case ArtifactCore::NLE::TransitionKind::RadialWipe: importedTransition.kind = QStringLiteral("RadialWipe"); break;
            case ArtifactCore::NLE::TransitionKind::Flip: importedTransition.kind = QStringLiteral("Flip"); break;
            case ArtifactCore::NLE::TransitionKind::Cube: importedTransition.kind = QStringLiteral("Cube"); break;
            case ArtifactCore::NLE::TransitionKind::Doors: importedTransition.kind = QStringLiteral("Doors"); break;
            case ArtifactCore::NLE::TransitionKind::LightLeak: importedTransition.kind = QStringLiteral("LightLeak"); break;
            case ArtifactCore::NLE::TransitionKind::GradientWipe: importedTransition.kind = QStringLiteral("GradientWipe"); break;
            case ArtifactCore::NLE::TransitionKind::IrisWipe: importedTransition.kind = QStringLiteral("IrisWipe"); break;
            case ArtifactCore::NLE::TransitionKind::BlockDissolve: importedTransition.kind = QStringLiteral("BlockDissolve"); break;
            }
            importedTransition.leftClipName = leftClip->name;
            importedTransition.rightClipName = rightClip->name;
            importedTransition.range = transition->range;
            importedTransition.enabled = transition->enabled;
            composition->addTimelineTransition(importedTransition);
        }
    }
    message += QStringLiteral("\n\n新規Compositionを作成しました。レイヤー: %1").arg(importedCount);
    if (!warnings.isEmpty()) message += QStringLiteral("\n\n警告:\n") + warnings.join(QStringLiteral("\n"));
    QMessageBox::information(menu_, QStringLiteral("OpenTimelineIO"), message);
}

void ArtifactFileMenu::Impl::openProjectPath(const QString& path, bool addToRecent)
{
    if (!menu_ || path.isEmpty()) {
        return;
    }
    if (!QFileInfo(path).exists()) {
        QMessageBox::warning(menu_, QStringLiteral("プロジェクトを開く"),
                             QStringLiteral("ファイルが見つかりません。\n%1").arg(path));
        auto* settings = ArtifactAppSettings::instance();
        if (!settings) {
            return;
        }
        auto recent = settings->recentProjectPaths();
        recent.removeAll(path);
        settings->setRecentProjectPaths(recent);
        cachedRecentProjects_.clear();
        return;
    }

    const QPointer<ArtifactFileMenu> menuGuard(menu_);
    ArtifactProjectManager::getInstance().loadFromFileAsync(
        path,
        [menuGuard, path, addToRecent](const ArtifactProjectImporterResult& result) {
            if (!menuGuard) {
                return;
            }
            if (!result.success) {
                const QString error = result.errorMessage.toQString();
                QMessageBox::warning(
                    menuGuard, QStringLiteral("プロジェクトを開く"),
                    error.isEmpty()
                        ? QStringLiteral("プロジェクトを開けませんでした。\n%1").arg(path)
                        : QStringLiteral("プロジェクトを開けませんでした。\n%1").arg(error));
                return;
            }
            if (addToRecent) {
                addRecentProject(path);
            }
        });
}

void ArtifactFileMenu::Impl::handleExportCurrentFrame()
{
    if (!menu_) return;
    auto* svc = ArtifactProjectService::instance();
    if (!svc || !svc->hasProject()) {
        QMessageBox::warning(menu_, "エクスポート", "プロジェクトが開かれていません。");
        return;
    }
    
    auto comp = svc->currentComposition().lock();
    if (!comp) {
        QMessageBox::warning(menu_, "エクスポート", "コンポジションが選択されていません。");
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(menu_, "現在のフレームを書き出し",
        QString(), "PNG Image (*.png);;JPEG Image (*.jpg);;All Files (*.*)");
    if (filePath.isEmpty()) return;

    // 現在のフレームをレンダリング
    const QSize compSize = comp->effectiveCompositionSize();
    QImage canvas(compSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(QColor(18, 20, 24));
    
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 全レイヤーを描画
    const auto layers = comp->allLayer();
    for (const auto& layer : layers) {
        if (!layer || !layer->isVisible()) continue;
        
        // レイヤーを現在のフレーム位置にシーク
        layer->goToFrame(static_cast<int64_t>(comp->framePosition().framePosition()));

        // レイヤーサーフェスを取得して描画
        if (auto imageLayer = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer)) {
            QImage img = imageLayer->toQImage();
            if (!img.isNull()) {
                const auto size = layer->sourceSize();
                painter.drawImage(QRectF(0, 0, size.width, size.height), img);
            }
        } else if (auto svgLayer = ArtifactCore::dynamicPointerCast<ArtifactSvgLayer>(layer)) {
            QImage img = svgLayer->toQImage();
            if (!img.isNull()) {
                const auto size = layer->sourceSize();
                painter.drawImage(QRectF(0, 0, size.width, size.height), img);
            }
        } else if (auto solidLayer = ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(layer)) {
            QImage img(compSize, QImage::Format_ARGB32_Premultiplied);
            const FloatColor solidColor = solidLayer->color();
            img.fill(QColor(
                static_cast<int>(solidColor.r() * 255),
                static_cast<int>(solidColor.g() * 255),
                static_cast<int>(solidColor.b() * 255)));
            painter.drawImage(0, 0, img);
        }
    }
    
    // 画像を保存
    if (filePath.endsWith(".jpg", Qt::CaseInsensitive)) {
        canvas.save(filePath, "JPG", 95);
    } else {
        canvas.save(filePath, "PNG");
    }
    
    QMessageBox::information(menu_, "エクスポート", 
        QString("現在のフレームを保存しました:\n%1").arg(filePath));
}

void ArtifactFileMenu::Impl::handleExportWorkArea()
{
    if (!menu_) return;
    auto* svc = ArtifactProjectService::instance();
    if (!svc || !svc->hasProject()) {
        QMessageBox::warning(menu_, "エクスポート", "プロジェクトが開かれていません。");
        return;
    }

    auto comp = svc->currentComposition().lock();
    if (!comp) {
        QMessageBox::warning(menu_, "エクスポート", "コンポジションが選択されていません。");
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(menu_, "ワークエリアをレンダリング",
        QString(), "PNG Sequence (*.png);;MP4 Video (*.mp4);;All Files (*.*)");
    if (filePath.isEmpty()) return;

    // ワークエリア範囲を取得
    const FrameRange workArea = comp->workAreaRange();
    const int64_t startFrame = workArea.start();
    const int64_t endFrame = workArea.end();
    const int64_t totalFrames = std::max<int64_t>(1, endFrame - startFrame);
    const QSize compSize = comp->effectiveCompositionSize();
    const auto layers = comp->allLayer();

    // 進捗ダイアログを表示
    QProgressDialog* progress = new QProgressDialog(menu_);
    progress->setWindowTitle("レンダリング中");
    progress->setLabelText("フレームをレンダリング中...");
    progress->setRange(0, static_cast<int>(totalFrames));
    progress->setCancelButtonText("キャンセル");
    progress->setWindowModality(Qt::WindowModal);
    progress->setAttribute(Qt::WA_DeleteOnClose, false);
    progress->show();

    // バックグラウンドでレンダリング実行
    auto cancelFlag = ArtifactCore::makeShared<std::atomic<bool>>(false);
    QObject::connect(progress, &QProgressDialog::canceled, [cancelFlag]() {
        *cancelFlag = true;
    });

    auto* watcher = new QFutureWatcher<int>(menu_);
    QObject::connect(watcher, &QFutureWatcher<int>::progressValueChanged, progress, [progress](int value) {
        progress->setValue(value);
    });
    QObject::connect(watcher, &QFutureWatcher<int>::finished, menu_, [this, progress, watcher, filePath]() {
        progress->close();
        const int renderedCount = watcher->result();
        watcher->deleteLater();

        if (renderedCount > 0) {
            QMessageBox::information(menu_, "エクスポート完了",
                QString("%1 フレームを保存しました:\n%2").arg(renderedCount).arg(filePath));
        }
    });

    // Run rendering in background thread with progress reporting
    watcher->setFuture(QtConcurrent::run([startFrame, endFrame, compSize, layers, filePath, cancelFlag, totalFrames]() -> int {
        int renderedCount = 0;

        for (int64_t frame = startFrame; frame < endFrame; ++frame) {
            if (*cancelFlag) break;

            // キャンバスをクリア
            QImage canvas(compSize, QImage::Format_ARGB32_Premultiplied);
            canvas.fill(QColor(18, 20, 24));

            QPainter painter(&canvas);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

            // 全レイヤーを描画
            for (const auto& layer : layers) {
                if (!layer || !layer->isVisible()) continue;

                if (auto imageLayer = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer)) {
                    QImage img = imageLayer->toQImage();
                    if (!img.isNull()) {
                        const auto size = layer->sourceSize();
                        painter.drawImage(QRectF(0, 0, size.width, size.height), img);
                    }
                } else if (auto svgLayer = ArtifactCore::dynamicPointerCast<ArtifactSvgLayer>(layer)) {
                    QImage img = svgLayer->toQImage();
                    if (!img.isNull()) {
                        const auto size = layer->sourceSize();
                        painter.drawImage(QRectF(0, 0, size.width, size.height), img);
                    }
                } else if (auto solidLayer = ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(layer)) {
                    QImage img(compSize, QImage::Format_ARGB32_Premultiplied);
                    const FloatColor solidColor = solidLayer->color();
                    img.fill(QColor(
                        static_cast<int>(solidColor.r() * 255),
                        static_cast<int>(solidColor.g() * 255),
                        static_cast<int>(solidColor.b() * 255)));
                    painter.drawImage(0, 0, img);
                }
            }

            painter.end();

            // ファイル名を生成（連番）
            QString frameFilePath;
            if (filePath.endsWith(".png", Qt::CaseInsensitive)) {
                QFileInfo fi(filePath);
                frameFilePath = fi.absolutePath() + "/" + fi.completeBaseName() +
                               QString("_%1").arg(static_cast<int>(frame), 4, 10, QChar('0')) + ".png";
            } else {
                frameFilePath = filePath + QString("_%1.png").arg(static_cast<int>(frame), 4, 10, QChar('0'));
            }

            canvas.save(frameFilePath, "PNG");
            renderedCount++;
        }

        return renderedCount;
    }));
}

void ArtifactFileMenu::Impl::handleExportProjectPackage()
{
    if (!menu_) return;
    auto* svc = ArtifactProjectService::instance();
    if (!svc || !svc->hasProject()) {
        QMessageBox::warning(menu_, "エクスポート", "プロジェクトが開かれていません。");
        return;
    }
    
    const QString dirPath = QFileDialog::getExistingDirectory(menu_, "プロジェクトをパッケージ化", 
        QString(), QFileDialog::ShowDirsOnly);
    if (dirPath.isEmpty()) return;

    auto project = svc->getCurrentProjectSharedPtr();
    if (!project) {
        QMessageBox::warning(menu_, QStringLiteral("エクスポート"),
                             QStringLiteral("プロジェクトデータを取得できませんでした。"));
        return;
    }

    const PackageSettings settings{dirPath, false, false};
    if (!ArtifactProjectPackager::collectAndPackage(project.get(), settings)) {
        QMessageBox::warning(menu_, QStringLiteral("エクスポート"),
                             QStringLiteral("プロジェクトのパッケージ化に失敗しました。"));
        return;
    }
    QMessageBox::information(menu_, QStringLiteral("エクスポート"),
                             QStringLiteral("プロジェクトをパッケージ化しました。\n%1").arg(dirPath));
}

void ArtifactFileMenu::Impl::handleExportComposition()
{
    if (!menu_) return;
    auto* service = ArtifactProjectService::instance();
    if (!service || !service->hasProject()) {
        QMessageBox::warning(menu_, QStringLiteral("Composition Export"),
                             QStringLiteral("プロジェクトが開かれていません。"));
        return;
    }
    const auto composition = service->currentComposition().lock();
    if (!composition) {
        QMessageBox::warning(menu_, QStringLiteral("Composition Export"),
                             QStringLiteral("コンポジションが選択されていません。"));
        return;
    }
    QString errorMessage;
    if (ArtifactExportDialog::run(menu_, composition, &errorMessage)) {
        QMessageBox::information(menu_, QStringLiteral("Composition Export"),
                                 QStringLiteral("コンポジションを書き出しました。"));
    }
}

void ArtifactFileMenu::Impl::handleExportOtio()
{
    if (!menu_) return;
    auto* service = ArtifactProjectService::instance();
    if (!service || !service->hasProject()) {
        QMessageBox::warning(menu_, QStringLiteral("OpenTimelineIO"), QStringLiteral("プロジェクトが開かれていません。"));
        return;
    }
    const auto composition = service->currentComposition().lock();
    if (!composition) {
        QMessageBox::warning(menu_, QStringLiteral("OpenTimelineIO"), QStringLiteral("コンポジションが選択されていません。"));
        return;
    }
    const QString filePath = QFileDialog::getSaveFileName(
        menu_, QStringLiteral("OpenTimelineIOを書き出す"), QStringLiteral("composition.otio"),
        QStringLiteral("OpenTimelineIO (*.otio);;All Files (*.*)"));
    if (filePath.isEmpty()) return;
    QVector<QString> warnings;
    if (!exportCompositionToOtioFile(*composition, filePath, &warnings)) {
        QMessageBox::warning(menu_, QStringLiteral("OpenTimelineIO"),
                             QStringLiteral("OTIOを書き出せませんでした。\n%1").arg(warnings.join(QStringLiteral("\n"))));
        return;
    }
    QString message = QStringLiteral("OTIOを書き出しました。\n%1").arg(filePath);
    if (!warnings.isEmpty()) message += QStringLiteral("\n\n警告:\n") + warnings.join(QStringLiteral("\n"));
    QMessageBox::information(menu_, QStringLiteral("OpenTimelineIO"), message);
}

void ArtifactFileMenu::Impl::handleExportFontUsage()
{
    if (!menu_) return;
    auto* service = ArtifactProjectService::instance();
    if (!service || !service->hasProject()) {
        QMessageBox::warning(menu_, QStringLiteral("フォントレポート"),
                             QStringLiteral("プロジェクトが開かれていません。"));
        return;
    }
    const QString directory = QFileDialog::getExistingDirectory(
        menu_, QStringLiteral("使用フォントレポートの出力先"),
        QString(), QFileDialog::ShowDirsOnly);
    if (directory.isEmpty()) return;
    auto project = service->getCurrentProjectSharedPtr();
    if (!project || !ArtifactProjectStatistics::exportFontUsagePackage(
                        project.get(), directory)) {
        QMessageBox::warning(menu_, QStringLiteral("フォントレポート"),
                             QStringLiteral("フォントレポートの出力に失敗しました。"));
        return;
    }
    QMessageBox::information(
        menu_, QStringLiteral("フォントレポート"),
        QStringLiteral("font-usage.json / font-usage.csv とフォント実体を出力しました。\n%1")
            .arg(directory));
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
    if (importOtioAction) importOtioAction->setEnabled(true);
    revealProjectFolderAction->setEnabled(hasProject);
    exportFontUsageAction->setEnabled(hasProject);
    const bool hasComposition = hasProject && service &&
                                 static_cast<bool>(service->currentComposition().lock());
    if (exportMenu) exportMenu->setEnabled(hasProject);
    if (exportCompositionAction) exportCompositionAction->setEnabled(hasComposition);
    if (exportOtioAction) exportOtioAction->setEnabled(hasComposition);
    if (exportCurrentFrameAction) exportCurrentFrameAction->setEnabled(hasComposition);
    if (exportWorkAreaAction) exportWorkAreaAction->setEnabled(hasComposition);
    if (exportProjectPackageAction) exportProjectPackageAction->setEnabled(hasProject);

    // 最近使ったプロジェクトメニューを更新
    if (recentProjectsMenu) {
        auto* settings = ArtifactAppSettings::instance();
        const auto currentRecent = settings ? settings->recentProjectPaths() : QStringList{};
        auto recent = pruneMissingRecentProjects(currentRecent);
        if (settings && recent != currentRecent) {
            settings->setRecentProjectPaths(recent);
        }
        // リストが変わっていなければ再構築しない
        if (recent != cachedRecentProjects_) {
            cachedRecentProjects_ = recent;
            recentProjectsMenu->clear();
            if (recent.isEmpty()) {
                auto* noRecent = recentProjectsMenu->addAction("なし");
                noRecent->setIcon(QIcon(resolveIconPath("Studio/filemenu_empty_recent.svg")));
                noRecent->setEnabled(false);
            } else {
                for (const auto& path : recent) {
                    QFileInfo fi(path);
                    QString displayName = fi.fileName();

                    auto* fileAction = recentProjectsMenu->addAction(displayName);
                    fileAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_open_project.svg")));
                    fileAction->setData(path);
                    fileAction->setStatusTip(path);
                    fileAction->setToolTip(path);

                    QObject::connect(fileAction, &QAction::triggered, menu_, [this, path]() {
                        if (!confirmUnsavedChanges(menu_, QStringLiteral("最近使ったプロジェクトを開く"))) {
                            return;
                        }
                        openProjectPath(path, true);
                    });
                }
            }
        }
    }
}

W_OBJECT_IMPL(ArtifactFileMenu)

ArtifactFileMenu::ArtifactFileMenu(QWidget* parent)
    : QMenu(parent), Impl_(new Impl(this))
{
    setTitle(menuText(QStringLiteral("menu.file.label"), QStringLiteral("ファイル(&F)")));
    setIcon(QIcon(resolveIconPath("Studio/menubar_file.svg")));
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
    if (auto* svc = ArtifactProjectService::instance()) {
        if (!confirmUnsavedChanges(this, QStringLiteral("プロジェクトを閉じる"))) {
            return;
        }
    }
    ArtifactProjectManager::getInstance().closeCurrentProject();
    if (auto* svc = ArtifactProjectService::instance()) {
        svc->projectChanged();
    }
}

void ArtifactFileMenu::quitApplication()
{
    if (auto* svc = ArtifactProjectService::instance()) {
        if (!confirmUnsavedChanges(this, QStringLiteral("終了"))) {
            return;
        }
    }
    QApplication::quit();
}

void ArtifactFileMenu::restartApplication()
{
    if (auto* svc = ArtifactProjectService::instance()) {
        if (!confirmUnsavedChanges(this, QStringLiteral("再起動"))) {
            return;
        }
    }

    const QString program = QCoreApplication::applicationFilePath();
    QStringList args = QCoreApplication::arguments();
    if (!args.isEmpty()) {
        args.removeFirst();
    }
    const bool launched = QProcess::startDetached(program, args);
    if (!launched) {
        qWarning() << "Failed to restart application:" << program;
        return;
    }
    QTimer::singleShot(0, []() {
        QApplication::quit();
    });
}

void ArtifactFileMenu::resetRecentFilesMenu()
{
    if (Impl_) {
        Impl_->cachedRecentProjects_.clear();
        if (Impl_->recentProjectsMenu) {
            Impl_->recentProjectsMenu->clear();
        }
    }
}

} // namespace Artifact
