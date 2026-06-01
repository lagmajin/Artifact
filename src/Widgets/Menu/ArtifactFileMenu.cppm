module;
#include <utility>
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
#include <QSet>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QCoreApplication>
#include <QTimer>
#include <QProgressDialog>
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
import Artifact.Composition.InitParams;
import Artifact.Service.Project;
import Application.AppSettings;
import Utils.Path;
import Artifact.Widgets.AppDialogs;
import Undo.UndoManager;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layers.SolidImage;

namespace Artifact {
using namespace ArtifactCore;
namespace {
constexpr int kMaxRecentProjects = 5;

void addRecentProject(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }
    auto* settings = ArtifactAppSettings::instance();
    if (!settings) {
        return;
    }
    auto recent = settings->recentProjectPaths();
    recent.removeAll(path);
    recent.prepend(path);
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
        if (path.isEmpty() || seen.contains(path)) {
            continue;
        }
        seen.insert(path);
        if (QFileInfo(path).exists()) {
            pruned.push_back(path);
        }
    }
    return pruned;
}

QString supportedAssetFilter()
{
    return QStringLiteral(
        "対応アセット (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.exr *.hdr "
        "*.mp4 *.mov *.mkv *.avi *.webm *.mp3 *.wav *.flac *.ogg *.aac *.m4a "
        "*.obj *.fbx *.gltf *.glb *.abc *.usd *.usda *.usdc);;"
        "画像 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.exr *.hdr);;"
        "動画 (*.mp4 *.mov *.mkv *.avi *.webm);;"
        "音声 (*.mp3 *.wav *.flac *.ogg *.aac *.m4a);;"
        "3D (*.obj *.fbx *.gltf *.glb *.abc *.usd *.usda *.usdc)"
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
    QAction* revealProjectFolderAction = nullptr;
    QAction* restartAction = nullptr;
    QAction* quitAction = nullptr;
    QMenu* exportMenu = nullptr;
    QAction* exportCurrentFrameAction = nullptr;
    QAction* exportWorkAreaAction = nullptr;
    QAction* exportProjectPackageAction = nullptr;
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
    void handleRevealProjectFolder();
    void handleExportCurrentFrame();
    void handleExportWorkArea();
    void handleExportProjectPackage();
    void openProjectPath(const QString& path, bool addToRecent);
};

ArtifactFileMenu::Impl::Impl(ArtifactFileMenu* menu)
    : menu_(menu)
{
    createProjectAction = new QAction("新規プロジェクト(&N)...");
    createProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N));
    createProjectAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_new_project.svg")));

    openProjectAction = new QAction("プロジェクトを開く(&O)...");
    openProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    openProjectAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_open_project.svg")));

    saveProjectAction = new QAction("保存(&S)");
    saveProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    saveProjectAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_save_project.svg")));

    saveProjectAsAction = new QAction("名前を付けて保存(&A)...");
    saveProjectAsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    saveProjectAsAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_save_project_as.svg")));

    closeProjectAction = new QAction("プロジェクトを閉じる");
    closeProjectAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_close_project.svg")));

    newCompositionAction = new QAction("新規コンポジション(&C)...");
    newCompositionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    newCompositionAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_new_composition.svg")));

    importAssetsAction = new QAction("アセットを読み込み(&I)...");
    importAssetsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    importAssetsAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_import_assets.svg")));

    revealProjectFolderAction = new QAction("プロジェクトフォルダを開く");
    revealProjectFolderAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_reveal_folder.svg")));

    restartAction = new QAction("再起動");
    restartAction->setIcon(QIcon(resolveIconPath("Studio/filemenu_restart.svg")));
    
    quitAction = new QAction("終了(&Q)");
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
    menu->addSeparator();
    menu->addAction(closeProjectAction);
    menu->addAction(revealProjectFolderAction);
    recentProjectsMenu = menu->addMenu("最近使ったプロジェクト");
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
    QObject::connect(revealProjectFolderAction, &QAction::triggered, menu, [this]() { handleRevealProjectFolder(); });
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
    auto result = manager.saveToFile(path);
    if (!result.success) {
        qWarning() << "Save project failed";
        return;
    }
    addRecentProject(path);
}

void ArtifactFileMenu::Impl::handleSaveProjectAs()
{
    if (!menu_) return;
    const QString path = QFileDialog::getSaveFileName(menu_, "名前を付けて保存", QString(), "Artifact Project (*.artifact *.json);;All Files (*.*)");
    if (path.isEmpty()) return;
    auto result = ArtifactProjectManager::getInstance().saveToFile(path);
    if (!result.success) {
        qWarning() << "Save project as failed";
        return;
    }
    addRecentProject(path);
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
    const QStringList imported = svc->importAssetsFromPaths(files);
    if (imported.isEmpty()) {
        QMessageBox::warning(menu_, QStringLiteral("アセットを読み込み"),
                             QStringLiteral("読み込めるアセットがありませんでした。"));
    }
}

void ArtifactFileMenu::Impl::handleRevealProjectFolder()
{
    const QString path = ArtifactProjectManager::getInstance().currentProjectPath();
    if (path.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
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

    auto& manager = ArtifactProjectManager::getInstance();
    const QString beforePath = manager.currentProjectPath();
    manager.loadFromFile(path);
    const QString afterPath = manager.currentProjectPath();
    const bool opened = afterPath == path || (beforePath != afterPath && !afterPath.isEmpty());
    if (!opened) {
        QMessageBox::warning(menu_, QStringLiteral("プロジェクトを開く"),
                             QStringLiteral("プロジェクトを開けませんでした。\n%1").arg(path));
        return;
    }
    if (addToRecent) {
        addRecentProject(afterPath.isEmpty() ? path : afterPath);
    }
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
    const QSize compSize = comp->settings().compositionSize();
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
        if (auto imageLayer = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
            QImage img = imageLayer->toQImage();
            if (!img.isNull()) {
                const auto size = layer->sourceSize();
                painter.drawImage(QRectF(0, 0, size.width, size.height), img);
            }
        } else if (auto svgLayer = std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
            QImage img = svgLayer->toQImage();
            if (!img.isNull()) {
                const auto size = layer->sourceSize();
                painter.drawImage(QRectF(0, 0, size.width, size.height), img);
            }
        } else if (auto solidLayer = std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
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
    const QSize compSize = comp->settings().compositionSize();
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
    auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
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

                if (auto imageLayer = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
                    QImage img = imageLayer->toQImage();
                    if (!img.isNull()) {
                        const auto size = layer->sourceSize();
                        painter.drawImage(QRectF(0, 0, size.width, size.height), img);
                    }
                } else if (auto svgLayer = std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
                    QImage img = svgLayer->toQImage();
                    if (!img.isNull()) {
                        const auto size = layer->sourceSize();
                        painter.drawImage(QRectF(0, 0, size.width, size.height), img);
                    }
                } else if (auto solidLayer = std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
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

void ArtifactFileMenu::Impl::rebuildMenu()
{
    auto service = ArtifactProjectService::instance();
    bool hasProject = service && service->hasProject();
    saveProjectAction->setEnabled(hasProject);
    saveProjectAsAction->setEnabled(hasProject);
    closeProjectAction->setEnabled(hasProject);
    newCompositionAction->setEnabled(service != nullptr);
    importAssetsAction->setEnabled(hasProject);
    revealProjectFolderAction->setEnabled(hasProject);

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
