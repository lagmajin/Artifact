module;
#include <utility>
#include <QAction>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QtSVG/QSvgRenderer>
#include <QTimer>
#include <QSet>
#include <wobjectimpl.h>

module Artifact.Menu.Layer;
import std;

import Artifact.Service.Project;
import Utils.Path;
import Utils.Id;
import Utils.String.UniString;
import Artifact.Application.Manager;
import Artifact.Layer.InitParams;
import Artifact.Layer.Factory;
import Artifact.Composition.Abstract;
import Artifact.Widgets.CreatePlaneLayerDialog;
import Artifact.Widgets.AppDialogs;
import Artifact.Tool.CameraTracker;

namespace Artifact {
using namespace ArtifactCore;

namespace {

QSet<QString> currentLayerNames()
{
    QSet<QString> names;
    if (auto* service = ArtifactProjectService::instance()) {
        if (auto comp = service->currentComposition().lock()) {
            for (const auto& layer : comp->allLayer()) {
                if (!layer) {
                    continue;
                }
                const QString name = layer->layerName().trimmed();
                if (!name.isEmpty()) {
                    names.insert(name);
                }
            }
        }
    }
    return names;
}

QString makeUniqueSequentialName(QString baseName, const QSet<QString>& occupied)
{
    baseName = baseName.trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("Layer 1");
    }
    if (!occupied.contains(baseName)) {
        return baseName;
    }

    QString prefix = baseName;
    int startNumber = 2;
    int end = baseName.size();
    while (end > 0 && baseName.at(end - 1).isDigit()) {
        --end;
    }
    if (end < baseName.size()) {
        int start = end;
        while (start > 0 && baseName.at(start - 1).isSpace()) {
            --start;
        }
        bool ok = false;
        const int current = baseName.mid(end).toInt(&ok);
        if (ok) {
            prefix = baseName.left(start);
            startNumber = current + 1;
        }
    }
    if (prefix == baseName && !prefix.endsWith(QLatin1Char(' '))) {
        prefix += QLatin1Char(' ');
    }
    for (int index = startNumber; index < 10000; ++index) {
        const QString candidate = prefix + QString::number(index);
        if (!occupied.contains(candidate)) {
            return candidate;
        }
    }
    return baseName;
}

QString uniqueLayerName(const QString& baseName)
{
    return makeUniqueSequentialName(baseName, currentLayerNames());
}

} // namespace

W_OBJECT_IMPL(ArtifactLayerMenu)

class ArtifactLayerMenu::Impl {
public:
    Impl(ArtifactLayerMenu* menu);
    ~Impl() = default;

    ArtifactLayerMenu* menu_ = nullptr;
    QWidget* mainWindow_ = nullptr;
    ArtifactCore::LayerID selectedLayerId_;

    QMenu* createMenu = nullptr;
    QMenu* switchMenu = nullptr;
    QMenu* selectMenu = nullptr;

    QAction* createSolidAction = nullptr;
    QAction* createNullAction = nullptr;
    QAction* createAdjustAction = nullptr;
    QAction* createTextAction = nullptr;
    QAction* createParticleAction = nullptr;
    QAction* createCameraAction = nullptr;
    QAction* createAudioAction = nullptr;
    QAction* createSvgAction = nullptr;
    QAction* trackCameraAction = nullptr;

    QAction* duplicateLayerAction = nullptr;
    QAction* renameLayerAction = nullptr;
    QAction* deleteLayerAction = nullptr;

    QAction* toggleVisibleAction = nullptr;
    QAction* toggleLockAction = nullptr;
    QAction* toggleSoloAction = nullptr;
    QAction* toggleShyAction = nullptr;
    QAction* soloOnlyAction = nullptr;

    QAction* selectParentAction = nullptr;
    QAction* clearParentAction = nullptr;

    QAction* precomposeAction = nullptr;
    QAction* groupSelectionAction = nullptr;
    QAction* ungroupAction = nullptr;
    QAction* splitAction = nullptr;

    void handleCreateSolid();
    void handleCreateNull();
    void handleCreateAdjust();
    void handleCreateText();
    void handleCreateParticle();
    void handleCreateCamera();
    void handleCreateAudio();
    void handleCreateSvg();

    void handleDuplicateLayer();
    void handleRenameLayer();
    void handleDeleteLayer();

    void handleToggleVisible();
    void handleToggleLock();
    void handleToggleSolo();
    void handleToggleShy();
    void handleSoloOnlySelected();

    void handleSelectParent();
    void handleClearParent();

    void handlePrecompose();
    void handleGroupSelection();
    void handleUngroup();
    void handleSplitLayer();
    void handleTrackCamera();

    bool hasCurrentComposition() const;
    bool ensureCurrentComposition();
    bool hasSelectedLayer() const;
    void refreshEnabledState();
};

ArtifactLayerMenu::Impl::Impl(ArtifactLayerMenu* menu) : menu_(menu)
{
    createMenu = new QMenu("新規(&N)", menu);
    createSolidAction = new QAction("平面(&Y)...", createMenu);
    createSolidAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    createSolidAction->setIcon(QIcon(resolveIconPath("Material/palette.svg")));

    createNullAction = new QAction("ヌルオブジェクト(&N)", createMenu);
    createNullAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Y));
    createNullAction->setIcon(QIcon(resolveIconPath("Material/aspect_ratio.svg")));

    createAdjustAction = new QAction("調整レイヤー(&A)", createMenu);
    createAdjustAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Y));
    createAdjustAction->setIcon(QIcon(resolveIconPath("Material/blur_on.svg")));

    createTextAction = new QAction("テキスト(&T)", createMenu);
    createTextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_T));
    createTextAction->setIcon(QIcon(resolveIconPath("Material/title.svg")));

    createParticleAction = new QAction("パーティクル(&P)", createMenu);

    createCameraAction = new QAction("カメラ(&C)", createMenu);
    createCameraAction->setIcon(QIcon(resolveIconPath("Material/videocam.svg")));

    createAudioAction = new QAction("オーディオ(&U)...", createMenu);
    createAudioAction->setIcon(QIcon(resolveIconPath("Material/audiotrack.svg")));

    createSvgAction = new QAction("SVG シェイプレイヤー(&V)...", createMenu);

    trackCameraAction = new QAction("3Dカメラトラッキング(&T)", menu);
    trackCameraAction->setIcon(QIcon(resolveIconPath("Material/videocam.svg")));

    createMenu->addAction(createSolidAction);
    createMenu->addAction(createNullAction);
    createMenu->addAction(createAdjustAction);
    createMenu->addAction(createTextAction);
    createMenu->addAction(createParticleAction);
    createMenu->addAction(createCameraAction);
    createMenu->addAction(createAudioAction);
    createMenu->addAction(createSvgAction);

    duplicateLayerAction = new QAction("レイヤーを複製(&D)", menu);
    duplicateLayerAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    duplicateLayerAction->setIcon(QIcon(resolveIconPath("Material/content_copy.svg")));
    renameLayerAction = new QAction("レイヤー名を変更(&R)...", menu);
    renameLayerAction->setShortcut(QKeySequence(Qt::Key_F2));
    deleteLayerAction = new QAction("削除(&X)", menu);
    deleteLayerAction->setShortcut(QKeySequence(Qt::Key_Delete));
    deleteLayerAction->setIcon(QIcon(resolveIconPath("Material/delete.svg")));

    switchMenu = new QMenu("スイッチ(&S)", menu);
    toggleVisibleAction = new QAction("表示/非表示を切替", switchMenu);
    toggleVisibleAction->setIcon(QIcon(resolveIconPath("Material/visibility.svg")));
    toggleLockAction = new QAction("ロックを切替", switchMenu);
    toggleLockAction->setIcon(QIcon(resolveIconPath("Material/lock.svg")));
    toggleSoloAction = new QAction("ソロを切替", switchMenu);
    toggleSoloAction->setIcon(QIcon(resolveIconPath("Material/headset.svg")));
    toggleShyAction = new QAction("シャイを切替", switchMenu);
    soloOnlyAction = new QAction("選択レイヤーのみソロ", switchMenu);
    switchMenu->addAction(toggleVisibleAction);
    switchMenu->addAction(toggleLockAction);
    switchMenu->addAction(toggleSoloAction);
    switchMenu->addAction(toggleShyAction);
    switchMenu->addSeparator();
    switchMenu->addAction(soloOnlyAction);

    selectMenu = new QMenu("選択(&E)", menu);
    selectParentAction = new QAction("親を選択", selectMenu);
    clearParentAction = new QAction("親を解除", selectMenu);
    selectMenu->addAction(selectParentAction);
    selectMenu->addAction(clearParentAction);

    precomposeAction = new QAction("プリコンポーズ(&P)...", menu);
    groupSelectionAction = new QAction("グループ化(&G)...", menu);
    ungroupAction = new QAction("グループ解除(&U)", menu);
    splitAction = new QAction("レイヤー分割(&L)", menu);
    splitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    splitAction->setIcon(QIcon(resolveIconPath("Material/content_cut.svg")));

    menu->addMenu(createMenu);
    menu->addAction(trackCameraAction);
    menu->addSeparator();
    menu->addAction(duplicateLayerAction);
    menu->addAction(renameLayerAction);
    menu->addAction(deleteLayerAction);
    menu->addSeparator();
    menu->addMenu(switchMenu);
    menu->addMenu(selectMenu);
    menu->addSeparator();
    menu->addAction(precomposeAction);
    menu->addAction(groupSelectionAction);
    menu->addAction(ungroupAction);
    menu->addAction(splitAction);

    QObject::connect(createSolidAction, &QAction::triggered, menu, [this]() { handleCreateSolid(); });
    QObject::connect(createNullAction, &QAction::triggered, menu, [this]() { handleCreateNull(); });
    QObject::connect(createAdjustAction, &QAction::triggered, menu, [this]() { handleCreateAdjust(); });
    QObject::connect(createTextAction, &QAction::triggered, menu, [this]() { handleCreateText(); });
    QObject::connect(createParticleAction, &QAction::triggered, menu, [this]() { handleCreateParticle(); });
    QObject::connect(createCameraAction, &QAction::triggered, menu, [this]() { handleCreateCamera(); });
    QObject::connect(createAudioAction, &QAction::triggered, menu, [this]() { handleCreateAudio(); });
    QObject::connect(createSvgAction, &QAction::triggered, menu, [this]() { handleCreateSvg(); });

    QObject::connect(duplicateLayerAction, &QAction::triggered, menu, [this]() { handleDuplicateLayer(); });
    QObject::connect(renameLayerAction, &QAction::triggered, menu, [this]() { handleRenameLayer(); });
    QObject::connect(deleteLayerAction, &QAction::triggered, menu, [this]() { handleDeleteLayer(); });

    QObject::connect(toggleVisibleAction, &QAction::triggered, menu, [this]() { handleToggleVisible(); });
    QObject::connect(toggleLockAction, &QAction::triggered, menu, [this]() { handleToggleLock(); });
    QObject::connect(toggleSoloAction, &QAction::triggered, menu, [this]() { handleToggleSolo(); });
    QObject::connect(toggleShyAction, &QAction::triggered, menu, [this]() { handleToggleShy(); });
    QObject::connect(soloOnlyAction, &QAction::triggered, menu, [this]() { handleSoloOnlySelected(); });

    QObject::connect(selectParentAction, &QAction::triggered, menu, [this]() { handleSelectParent(); });
    QObject::connect(clearParentAction, &QAction::triggered, menu, [this]() { handleClearParent(); });

    QObject::connect(precomposeAction, &QAction::triggered, menu, [this]() { handlePrecompose(); });
    QObject::connect(groupSelectionAction, &QAction::triggered, menu, [this]() { handleGroupSelection(); });
    QObject::connect(ungroupAction, &QAction::triggered, menu, [this]() { handleUngroup(); });
    QObject::connect(splitAction, &QAction::triggered, menu, [this]() { handleSplitLayer(); });
    QObject::connect(trackCameraAction, &QAction::triggered, menu, [this]() { handleTrackCamera(); });

    auto* service = ArtifactProjectService::instance();
    QObject::connect(service, &ArtifactProjectService::layerSelected, menu, [this](const ArtifactCore::LayerID& id) {
        selectedLayerId_ = id;
        refreshEnabledState();
    });
    QObject::connect(service, &ArtifactProjectService::layerRemoved, menu, [this](const ArtifactCore::CompositionID&, const ArtifactCore::LayerID& id) {
        if (selectedLayerId_ == id) {
            selectedLayerId_ = {};
        }
        refreshEnabledState();
    });
    QObject::connect(service, &ArtifactProjectService::compositionCreated, menu, [this](const ArtifactCore::CompositionID&) {
        refreshEnabledState();
    });
    QObject::connect(service, &ArtifactProjectService::projectChanged, menu, [this]() {
        refreshEnabledState();
    });
    QObject::connect(menu, &QMenu::aboutToShow, menu, [this]() {
        refreshEnabledState();
    });
}

bool ArtifactLayerMenu::Impl::hasCurrentComposition() const
{
    auto* service = ArtifactProjectService::instance();
    return service && static_cast<bool>(service->currentComposition().lock());
}

bool ArtifactLayerMenu::Impl::ensureCurrentComposition()
{
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return false;
    }
    if (service->currentComposition().lock()) {
        return true;
    }
    if (!service->hasProject()) {
        return false;
    }

    service->createComposition(UniString(QStringLiteral("Composition")));
    return static_cast<bool>(service->currentComposition().lock());
}

bool ArtifactLayerMenu::Impl::hasSelectedLayer() const
{
    return hasCurrentComposition() && !selectedLayerId_.isNil();
}

void ArtifactLayerMenu::Impl::refreshEnabledState()
{
    auto* service = ArtifactProjectService::instance();
    const bool hasProject = service && service->hasProject();
    const bool hasComp = hasCurrentComposition();
    const bool hasLayer = hasSelectedLayer();
    const bool hasParent = hasLayer && service && service->layerHasParentInCurrentComposition(selectedLayerId_);

    // Creation actions can auto-create first composition when a project exists.
    createSolidAction->setEnabled(hasProject);
    createNullAction->setEnabled(hasProject);
    createAdjustAction->setEnabled(hasProject);
    createTextAction->setEnabled(hasProject);
    createParticleAction->setEnabled(hasProject);
    createCameraAction->setEnabled(hasProject);
    createAudioAction->setEnabled(hasProject);
    createSvgAction->setEnabled(hasProject);

    duplicateLayerAction->setEnabled(hasLayer);
    renameLayerAction->setEnabled(hasLayer);
    deleteLayerAction->setEnabled(hasLayer);
    toggleVisibleAction->setEnabled(hasLayer);
    toggleLockAction->setEnabled(hasLayer);
    toggleSoloAction->setEnabled(hasLayer);
    toggleShyAction->setEnabled(hasLayer);
    soloOnlyAction->setEnabled(hasLayer && hasComp);
    selectParentAction->setEnabled(hasParent);
    clearParentAction->setEnabled(hasParent);
    precomposeAction->setEnabled(hasLayer);
    groupSelectionAction->setEnabled(hasLayer && hasComp);
    
    // Ungroup: 選択中のレイヤーがグループの場合のみ有効
    bool isGroupSelected = false;
    auto* app = ArtifactApplicationManager::instance();
    if (app && app->layerSelectionManager()) {
        auto current = app->layerSelectionManager()->currentLayer();
        isGroupSelected = current && current->isGroupLayer();
    }
    ungroupAction->setEnabled(isGroupSelected && hasComp);
    
    splitAction->setEnabled(hasLayer);

    // 3Dカメラトラッキング: 動画レイヤーが選択されている場合のみ有効
    bool isVideoSelected = false;
    if (hasLayer && hasComp) {
        if (auto comp = service->currentComposition().lock()) {
            if (auto layer = comp->layerById(selectedLayerId_)) {
                isVideoSelected = layer->hasVideo();
            }
        }
    }
    trackCameraAction->setEnabled(isVideoSelected);
}

void ArtifactLayerMenu::Impl::handleCreateSolid()
{
    auto service = ArtifactProjectService::instance();
    if (!ensureCurrentComposition()) {
        QWidget* parentWindow = mainWindow_ ? mainWindow_ : (menu_ ? menu_->window() : nullptr);
        QMessageBox::warning(parentWindow, "Layer", "コンポジションが選択されていません。");
        return;
    }
    auto* const menu = menu_;
    QWidget* parentWindow = mainWindow_ ? mainWindow_ : (menu_ ? menu_->window() : nullptr);
    CreateSolidLayerSettingDialog dialog(parentWindow);
    QObject::connect(&dialog, &CreateSolidLayerSettingDialog::submit, menu, [service, menu](const ArtifactSolidLayerInitParams& params) {
        if (!service) {
            return;
        }
        QTimer::singleShot(0, menu, [service, params, menu]() {
            Q_UNUSED(menu);
            service->addLayerToCurrentComposition(params);
        });
    });
    dialog.setModal(true);
    dialog.exec();
}

void ArtifactLayerMenu::Impl::handleCreateNull()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactNullLayerInitParams params(uniqueLayerName(u8"Null 1"));
    auto* service = ArtifactProjectService::instance();
    if (auto comp = service->currentComposition().lock()) {
        auto size = comp->settings().compositionSize();
        params.setWidth(size.width());
        params.setHeight(size.height());
    }
    service->addLayerToCurrentComposition(params);
}

void ArtifactLayerMenu::Impl::handleCreateAdjust()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactLayerInitParams params(uniqueLayerName(u8"Adjustment Layer 1"), LayerType::Adjustment);
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params);
}

void ArtifactLayerMenu::Impl::handleCreateText()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactTextLayerInitParams params(uniqueLayerName(u8"Text 1"));
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params);
}

void ArtifactLayerMenu::Impl::handleCreateParticle()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactLayerInitParams params(uniqueLayerName(u8"Particle 1"), LayerType::Particle);
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params);
}

void ArtifactLayerMenu::Impl::handleCreateCamera()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactCameraLayerInitParams params;
    params.setName(UniString(uniqueLayerName(u8"Camera 1")));
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params);
}

void ArtifactLayerMenu::Impl::handleCreateAudio()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        menu_ ? menu_->window() : nullptr,
        QStringLiteral("オーディオを選択"),
        QString(),
        QStringLiteral("WAV Audio (*.wav);;All Files (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    ArtifactAudioInitParams params(uniqueLayerName(QFileInfo(path).baseName()));
    params.setAudioPath(path);
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params);
}

void ArtifactLayerMenu::Impl::handleCreateSvg()
{
    auto* service = ArtifactProjectService::instance();
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    if (!service) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        menu_ ? menu_->window() : nullptr,
        QStringLiteral("SVGを選択"),
        QString(),
        QStringLiteral("SVG (*.svg);;All Files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }
    if (!filePath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr,
                             QStringLiteral("Layer"),
                             QStringLiteral("SVG ファイルを選択してください。"));
        return;
    }
    QSvgRenderer validator(filePath);
    if (!validator.isValid()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr,
                             QStringLiteral("Layer"),
                             QStringLiteral("SVG を読み込めませんでした。"));
        return;
    }

    const QString layerName = uniqueLayerName(QFileInfo(filePath).completeBaseName());
    service->importAssetsFromPathsAsync(QStringList{filePath}, [service, layerName](QStringList importedPaths) {
        if (!service || importedPaths.isEmpty()) {
            return;
        }
        ArtifactSvgInitParams params(layerName);
        params.setSvgPath(importedPaths.first());
        service->addLayerToCurrentComposition(params);
    });
}

void ArtifactLayerMenu::Impl::handleDuplicateLayer()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    if (!service->duplicateLayerInCurrentComposition(selectedLayerId_)) {
        QMessageBox::warning(menu_->window(), "Layer", "レイヤー複製に失敗しました。");
    }
}

void ArtifactLayerMenu::Impl::handleRenameLayer()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const QString layerName = service->layerNameInCurrentComposition(selectedLayerId_);

    bool ok = false;
    const QString newName = QInputDialog::getText(
        menu_->window(),
        "レイヤー名の変更",
        "新しい名前:",
        QLineEdit::Normal,
        layerName,
        &ok);
    if (!ok) return;
    if (!service->renameLayerInCurrentComposition(selectedLayerId_, newName)) {
        QMessageBox::warning(menu_->window(), "Layer", "レイヤー名の変更に失敗しました。");
    }
}

void ArtifactLayerMenu::Impl::handleDeleteLayer()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    auto comp = service->currentComposition().lock();
    if (!comp) return;
    const QString message = service->layerRemovalConfirmationMessage(comp->id(), selectedLayerId_);
    if (!ArtifactMessageBox::confirmDelete(menu_->window(), QStringLiteral("レイヤー削除"), message)) {
        return;
    }
    if (!service->removeLayerFromComposition(comp->id(), selectedLayerId_)) {
        QMessageBox::warning(menu_->window(), QStringLiteral("削除失敗"), QStringLiteral("レイヤー削除に失敗しました。"));
    }
}

void ArtifactLayerMenu::Impl::handleToggleVisible()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const bool current = service->isLayerVisibleInCurrentComposition(selectedLayerId_);
    service->setLayerVisibleInCurrentComposition(selectedLayerId_, !current);
}

void ArtifactLayerMenu::Impl::handleToggleLock()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const bool current = service->isLayerLockedInCurrentComposition(selectedLayerId_);
    service->setLayerLockedInCurrentComposition(selectedLayerId_, !current);
}

void ArtifactLayerMenu::Impl::handleToggleSolo()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const bool current = service->isLayerSoloInCurrentComposition(selectedLayerId_);
    service->setLayerSoloInCurrentComposition(selectedLayerId_, !current);
}

void ArtifactLayerMenu::Impl::handleToggleShy()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const bool current = service->isLayerShyInCurrentComposition(selectedLayerId_);
    service->setLayerShyInCurrentComposition(selectedLayerId_, !current);
}

void ArtifactLayerMenu::Impl::handleSoloOnlySelected()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    service->soloOnlyLayerInCurrentComposition(selectedLayerId_);
}

void ArtifactLayerMenu::Impl::handleSelectParent()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const auto parentId = service->layerParentIdInCurrentComposition(selectedLayerId_);
    if (parentId.isNil()) return;
    service->selectLayer(parentId);
}

void ArtifactLayerMenu::Impl::handleClearParent()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    service->clearLayerParentInCurrentComposition(selectedLayerId_);
}

void ArtifactLayerMenu::Impl::handlePrecompose()
{
    QMessageBox::information(menu_->window(), "Layer", "プリコンポーズは次のステップで実装します。");
}

void ArtifactLayerMenu::Impl::handleGroupSelection()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || !hasCurrentComposition()) {
        return;
    }

    bool ok = false;
    const QString groupName = QInputDialog::getText(
        menu_->window(),
        "グループ化",
        "グループ名:",
        QLineEdit::Normal,
        QStringLiteral("Group 1"),
        &ok);
    if (!ok) {
        return;
    }

    if (!service->groupSelectedLayersInCurrentComposition(UniString(groupName))) {
        QMessageBox::warning(menu_->window(), "グループ化", "選択レイヤーをグループ化できませんでした。");
    }
}

void ArtifactLayerMenu::Impl::handleUngroup()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || !hasCurrentComposition()) {
        return;
    }

    if (!service->ungroupSelectedGroupInCurrentComposition()) {
        QMessageBox::warning(menu_->window(), "グループ解除", "グループを解除できませんでした。グループを選択してください。");
    }
}

void ArtifactLayerMenu::Impl::handleSplitLayer()
{
    QMessageBox::information(menu_->window(), "Layer", "レイヤー分割は次のステップで実装します。");
}

void ArtifactLayerMenu::Impl::handleTrackCamera()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;

    auto comp = service->currentComposition().lock();
    if (!comp) return;

    auto layer = comp->layerById(selectedLayerId_);
    if (!layer || !layer->hasVideo()) {
        QMessageBox::warning(menu_->window(), "3D Tracker", "動画レイヤーを選択してください。");
        return;
    }

    // 実行
    bool success = ArtifactCameraTrackerTool::run(comp.get(), layer, [this](const ArtifactCameraTrackerTool::ProgressUpdate& update) {
        // TODO: ステータスバーへの表示など
        qDebug() << "Tracking progress:" << update.currentFrame << "/" << update.totalFrames << update.message;
    });

    if (success) {
        QMessageBox::information(menu_->window(), "3D Tracker", "トラッキングが完了しました。カメラと特徴点レイヤーが作成されました。");
    } else {
        QMessageBox::warning(menu_->window(), "3D Tracker", "トラッキングに失敗しました。十分な特徴点が見つからなかった可能性があります。");
    }
}

ArtifactLayerMenu::ArtifactLayerMenu(QWidget* mainWindow, QWidget* parent)
    : QMenu(parent), impl_(new Impl(this))
{
    impl_->mainWindow_ = mainWindow ? mainWindow->window() : nullptr;
    setTitle("レイヤー(&L)");
}

ArtifactLayerMenu::~ArtifactLayerMenu()
{
    delete impl_;
}

QMenu* ArtifactLayerMenu::newLayerMenu() const
{
    return impl_->createMenu;
}

} // namespace Artifact

