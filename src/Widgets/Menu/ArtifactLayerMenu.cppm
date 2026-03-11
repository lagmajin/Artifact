module;
#include <QAction>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <wobjectimpl.h>

module Artifact.Menu.Layer;
import std;

import Artifact.Service.Project;
import Utils.Id;
import Utils.String.UniString;
import Artifact.Layer.InitParams;
import Artifact.Layer.Factory;
import Artifact.Composition.Abstract;
import Artifact.Widgets.CreatePlaneLayerDialog;

namespace Artifact {

W_OBJECT_IMPL(ArtifactLayerMenu)

class ArtifactLayerMenu::Impl {
public:
    Impl(ArtifactLayerMenu* menu);
    ~Impl() = default;

    ArtifactLayerMenu* menu_ = nullptr;
    ArtifactCore::LayerID selectedLayerId_;

    QMenu* createMenu = nullptr;
    QMenu* switchMenu = nullptr;
    QMenu* selectMenu = nullptr;

    QAction* createSolidAction = nullptr;
    QAction* createNullAction = nullptr;
    QAction* createAdjustAction = nullptr;
    QAction* createTextAction = nullptr;

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
    QAction* splitAction = nullptr;

    void handleCreateSolid();
    void handleCreateNull();
    void handleCreateAdjust();
    void handleCreateText();

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
    void handleSplitLayer();

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

    createNullAction = new QAction("ヌルオブジェクト(&N)", createMenu);
    createNullAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Y));

    createAdjustAction = new QAction("調整レイヤー(&A)", createMenu);
    createAdjustAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Y));

    createTextAction = new QAction("テキスト(&T)", createMenu);
    createTextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_T));

    createMenu->addAction(createSolidAction);
    createMenu->addAction(createNullAction);
    createMenu->addAction(createAdjustAction);
    createMenu->addAction(createTextAction);

    duplicateLayerAction = new QAction("レイヤーを複製(&D)", menu);
    duplicateLayerAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    renameLayerAction = new QAction("レイヤー名を変更(&R)...", menu);
    renameLayerAction->setShortcut(QKeySequence(Qt::Key_F2));
    deleteLayerAction = new QAction("削除(&X)", menu);
    deleteLayerAction->setShortcut(QKeySequence(Qt::Key_Delete));

    switchMenu = new QMenu("スイッチ(&S)", menu);
    toggleVisibleAction = new QAction("表示/非表示を切替", switchMenu);
    toggleLockAction = new QAction("ロックを切替", switchMenu);
    toggleSoloAction = new QAction("ソロを切替", switchMenu);
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
    splitAction = new QAction("レイヤー分割(&L)", menu);
    splitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));

    menu->addMenu(createMenu);
    menu->addSeparator();
    menu->addAction(duplicateLayerAction);
    menu->addAction(renameLayerAction);
    menu->addAction(deleteLayerAction);
    menu->addSeparator();
    menu->addMenu(switchMenu);
    menu->addMenu(selectMenu);
    menu->addSeparator();
    menu->addAction(precomposeAction);
    menu->addAction(splitAction);

    QObject::connect(createSolidAction, &QAction::triggered, menu, [this]() { handleCreateSolid(); });
    QObject::connect(createNullAction, &QAction::triggered, menu, [this]() { handleCreateNull(); });
    QObject::connect(createAdjustAction, &QAction::triggered, menu, [this]() { handleCreateAdjust(); });
    QObject::connect(createTextAction, &QAction::triggered, menu, [this]() { handleCreateText(); });

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
    QObject::connect(splitAction, &QAction::triggered, menu, [this]() { handleSplitLayer(); });

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
    splitAction->setEnabled(hasLayer);
}

void ArtifactLayerMenu::Impl::handleCreateSolid()
{
    auto service = ArtifactProjectService::instance();
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    QWidget* parentWindow = menu_ ? menu_->window() : nullptr;
    CreateSolidLayerSettingDialog dialog(parentWindow);
    QObject::connect(&dialog, &CreateSolidLayerSettingDialog::submit, menu_, [service](const ArtifactSolidLayerInitParams& params) {
        if (service) {
            service->addLayerToCurrentComposition(params);
        }
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
    ArtifactNullLayerInitParams params(u8"Null 1");
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
    ArtifactSolidLayerInitParams params(u8"Adjustment Layer");
    auto* service = ArtifactProjectService::instance();
    if (auto comp = service->currentComposition().lock()) {
        auto size = comp->settings().compositionSize();
        params.setWidth(size.width());
        params.setHeight(size.height());
    }
    params.setColor(FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    service->addLayerToCurrentComposition(params);
}

void ArtifactLayerMenu::Impl::handleCreateText()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactTextLayerInitParams params(u8"Text 1");
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params);
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
    const auto answer = QMessageBox::question(
        menu_->window(),
        QStringLiteral("レイヤー削除"),
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
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

void ArtifactLayerMenu::Impl::handleSplitLayer()
{
    QMessageBox::information(menu_->window(), "Layer", "レイヤー分割は次のステップで実装します。");
}

ArtifactLayerMenu::ArtifactLayerMenu(QWidget* parent)
    : QMenu(parent), impl_(new Impl(this))
{
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

