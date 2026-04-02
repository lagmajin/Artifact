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
#include <QRegularExpression>
#include <QFileInfo>
#include <QSet>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QCoreApplication>
#include <QTimer>
#include <QSettings>
#include <QProgressDialog>
#include <QPainter>
#include <QImage>
#include <wobjectimpl.h>

module Artifact.Menu.File;
import std;

import Artifact.Project.Manager;
import Artifact.Service.Project;
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
constexpr const char* kRecentFilesKey = "recentProjects";

QStringList readRecentProjects()
{
    QSettings settings;
    return settings.value(kRecentFilesKey, QStringList()).toStringList();
}

void writeRecentProjects(const QStringList& paths)
{
    QSettings settings;
    settings.setValue(kRecentFilesKey, paths);
}

void addRecentProject(const QString& path)
{
    auto recent = readRecentProjects();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > kMaxRecentProjects) {
        recent.removeLast();
    }
    writeRecentProjects(recent);
}

bool isSupportedAssetPath(const QString& path)
{
    static const QSet<QString> kExt = {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("bmp"), QStringLiteral("tif"), QStringLiteral("tiff"),
        QStringLiteral("exr"), QStringLiteral("hdr"),
        QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("mkv"),
        QStringLiteral("avi"), QStringLiteral("webm"),
        QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"),
        QStringLiteral("ogg"), QStringLiteral("aac"), QStringLiteral("m4a"),
        QStringLiteral("obj"), QStringLiteral("fbx"),
        QStringLiteral("gltf"), QStringLiteral("glb"),
        QStringLiteral("abc"), QStringLiteral("usd"),
        QStringLiteral("usda"), QStringLiteral("usdc")
    };
    return kExt.contains(QFileInfo(path).suffix().toLower());
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
    box.setStyleSheet(QStringLiteral("QLabel { min-width: 680px; }"));

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
};

ArtifactFileMenu::Impl::Impl(ArtifactFileMenu* menu)
    : menu_(menu)
{
    createProjectAction = new QAction("新規プロジェクト(&N)...");
    createProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N));
    createProjectAction->setIcon(QIcon(resolveIconPath("Material/create_new_folder.svg")));

    openProjectAction = new QAction("プロジェクトを開く(&O)...");
    openProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    openProjectAction->setIcon(QIcon(resolveIconPath("Material/file_open.svg")));

    saveProjectAction = new QAction("保存(&S)");
    saveProjectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    saveProjectAction->setIcon(QIcon(resolveIconPath("Material/save.svg")));

    saveProjectAsAction = new QAction("名前を付けて保存(&A)...");
    saveProjectAsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    saveProjectAsAction->setIcon(QIcon(resolveIconPath("Material/save.svg")));

    closeProjectAction = new QAction("プロジェクトを閉じる");
    closeProjectAction->setIcon(QIcon(resolveIconPath("Material/folder.svg")));

    newCompositionAction = new QAction("新規コンポジション(&C)...");
    newCompositionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    newCompositionAction->setIcon(QIcon(resolveIconPath("Material/movie_creation.svg")));

    importAssetsAction = new QAction("アセットを読み込み(&I)...");
    importAssetsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    importAssetsAction->setIcon(QIcon(resolveIconPath("Material/upload.svg")));

    revealProjectFolderAction = new QAction("プロジェクトフォルダを開く");
    revealProjectFolderAction->setIcon(QIcon(resolveIconPath("Material/folder.svg")));

    restartAction = new QAction("再起動");
    restartAction->setIcon(QIcon(resolveIconPath("Material/replay.svg")));
    
    quitAction = new QAction("終了(&Q)");
    quitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    quitAction->setIcon(QIcon());

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
    auto dir = QFileDialog::getExistingDirectory(menu_, "新規プロジェクトの保存先を選択");
    if (dir.isEmpty()) return;
    const QString name = QFileInfo(dir).fileName();
    auto& manager = ArtifactProjectManager::getInstance();
    auto result = manager.createProject(UniString(name), true);
    if (!result.isSuccess) {
        qWarning() << "Create project failed";
    } else {
        // プロジェクト作成に成功したら履歴に追加
        // manager.currentProjectPath() はディレクトリパスまたはファイルパスを返す可能性がある
        QString projectPath = manager.currentProjectPath();
        if (!projectPath.isEmpty()) {
            addRecentProject(projectPath);
        }
    }
}

void ArtifactFileMenu::Impl::handleOpenProject()
{
    if (!menu_) return;
    const QString filePath = QFileDialog::getOpenFileName(menu_, "プロジェクトを開く", QString(), "Artifact Project (*.artifact *.json);;All Files (*.*)");
    if (filePath.isEmpty()) return;
    addRecentProject(filePath);
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
        addRecentProject(path);
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
    addRecentProject(path);
    auto result = ArtifactProjectManager::getInstance().saveToFile(path);
    if (!result.success) {
        qWarning() << "Save project as failed";
    }
}

void ArtifactFileMenu::Impl::handleNewComposition()
{
    if (!menu_) return;

    // Preset selection
    QStringList presets;
    presets << "1920 x 1080  @ 30fps (Full HD)"
            << "1920 x 1080  @ 24fps (Cinema)"
            << "3840 x 2160  @ 30fps (4K UHD)"
            << "1280 x 720   @ 30fps (HD)"
            << "1080 x 1920  @ 30fps (Vertical HD)"
            << "1080 x 1920  @ 60fps (Vertical 60)"
            << "1080 x 1080  @ 30fps (Square)"
            << "1920 x 1080  @ 60fps (Full HD 60)";

    bool ok = false;
    const QString preset = QInputDialog::getItem(menu_, "新規コンポジション", "プリセット:", presets, 0, false, &ok);
    if (!ok) return;

    // Parse preset: "WxH @ fps (name)"
    int w = 1920, h = 1080;
    double fps = 30.0;
    QRegularExpression re(R"((\d+)\s*x\s*(\d+)\s*@\s*(\d+)fps)");
    auto match = re.match(preset);
    if (match.hasMatch()) {
        w = match.captured(1).toInt();
        h = match.captured(2).toInt();
        fps = match.captured(3).toDouble();
    }

    const QString name = QInputDialog::getText(menu_, "コンポジション名", "名前:", QLineEdit::Normal, "Composition", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    if (auto* svc = ArtifactProjectService::instance()) {
        svc->createComposition(UniString(name.trimmed()));
    }
}

void ArtifactFileMenu::Impl::handleImportAssets()
{
    if (!menu_) return;
    const QStringList files = QFileDialog::getOpenFileNames(
        menu_,
        QStringLiteral("アセットを読み込み"),
        QString(),
        supportedAssetFilter()
    );
    if (files.isEmpty()) return;
    QStringList supported;
    supported.reserve(files.size());
    for (const QString& path : files) {
        if (isSupportedAssetPath(path)) {
            supported.push_back(path);
        }
    }
    if (supported.isEmpty()) {
        qWarning() << "No supported asset files selected";
        return;
    }
    if (auto* svc = ArtifactProjectService::instance()) {
        svc->importAssetsFromPaths(supported);
    }
}

void ArtifactFileMenu::Impl::handleRevealProjectFolder()
{
    const QString path = ArtifactProjectManager::getInstance().currentProjectPath();
    if (path.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
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
    const int64_t totalFrames = endFrame - startFrame + 1;
    
    // 進捗ダイアログを表示
    QProgressDialog progress(menu_);
    progress.setWindowTitle("レンダリング中");
    progress.setLabelText("フレームを描画中...");
    progress.setRange(0, static_cast<int>(totalFrames));
    progress.setCancelButton(nullptr);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    
    const QSize compSize = comp->settings().compositionSize();
    int renderedCount = 0;
    
    // 各フレームをレンダリング
    for (int64_t frame = startFrame; frame <= endFrame; ++frame) {
        // 進捗更新
        progress.setValue(static_cast<int>(frame - startFrame));
        QApplication::processEvents();
        if (progress.wasCanceled()) break;
        
        // キャンバスをクリア
        QImage canvas(compSize, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(QColor(18, 20, 24));
        
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        
        // 全レイヤーを描画
        const auto layers = comp->allLayer();
        for (const auto& layer : layers) {
            if (!layer || !layer->isVisible()) continue;
            
            // 現在のフレーム位置にシーク
            layer->goToFrame(frame);

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
        
        // ファイル名を生成（連番）
        QString frameFilePath;
        if (filePath.endsWith(".png", Qt::CaseInsensitive)) {
            // PNG シーケンスの場合
            QFileInfo fi(filePath);
            frameFilePath = fi.absolutePath() + "/" + fi.completeBaseName() + 
                           QString("_%1").arg(static_cast<int>(frame), 4, 10, QChar('0')) + ".png";
        } else {
            // デフォルトは PNG シーケンス
            frameFilePath = filePath + QString("_%1.png").arg(static_cast<int>(frame), 4, 10, QChar('0'));
        }
        
        canvas.save(frameFilePath, "PNG");
        renderedCount++;
    }
    
    progress.setValue(static_cast<int>(totalFrames));
    
    QMessageBox::information(menu_, "エクスポート",
        QString("%1 フレームを保存しました:\n%2").arg(renderedCount).arg(filePath));
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
    
    // TODO: プロジェクトをパッケージ化
    QMessageBox::information(menu_, "エクスポート", "プロジェクトのパッケージ化機能は現在開発中です。");
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
        recentProjectsMenu->clear();
        auto recent = readRecentProjects();
        if (recent.isEmpty()) {
            auto* noRecent = recentProjectsMenu->addAction("なし");
            noRecent->setEnabled(false);
        } else {
            for (const auto& path : recent) {
                QFileInfo fi(path);
                QString displayName = fi.fileName();
                QString displayPath = fi.absolutePath();

                // P0-1: Show full path in submenu for clarity
                auto* fileAction = recentProjectsMenu->addAction(displayName);
                fileAction->setData(path);
                fileAction->setStatusTip(path);
                fileAction->setToolTip(path);

                // P0-1: Add path as a disabled sub-item for visual clarity
                auto* pathAction = recentProjectsMenu->addAction(QStringLiteral("  %1").arg(displayPath));
                pathAction->setEnabled(false);
                pathAction->setToolTip(path);

                QObject::connect(fileAction, &QAction::triggered, menu_, [path]() {
                    ArtifactProjectManager::getInstance().loadFromFile(path);
                });
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
}

} // namespace Artifact
