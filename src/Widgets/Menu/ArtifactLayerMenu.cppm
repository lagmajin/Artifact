module;
#include <QAction>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <wobjectimpl.h>

module Artifact.Menu.Layer;

import Artifact.Project.Manager;
import Artifact.Service.Project;
import Utils.Id;
import Artifact.Layer.InitParams;
import Artifact.Layer.Factory;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
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

    ArtifactAbstractLayerPtr selectedLayer() const;
    std::shared_ptr<ArtifactAbstractComposition> currentComposition() const;
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
    });
    QObject::connect(menu, &QMenu::aboutToShow, menu, [this]() {
        refreshEnabledState();
    });
}

std::shared_ptr<ArtifactAbstractComposition> ArtifactLayerMenu::Impl::currentComposition() const
{
    auto* service = ArtifactProjectService::instance();
    return service ? service->currentComposition().lock() : nullptr;
}

ArtifactAbstractLayerPtr ArtifactLayerMenu::Impl::selectedLayer() const
{
    auto comp = currentComposition();
    if (!comp) return nullptr;

    if (!selectedLayerId_.isNil()) {
        if (auto selected = comp->layerById(selectedLayerId_)) {
            return selected;
        }
    }

    const auto all = comp->allLayer();
    for (const auto& l : all) {
        if (l) return l;
    }
    return nullptr;
}

void ArtifactLayerMenu::Impl::refreshEnabledState()
{
    const auto comp = currentComposition();
    const auto layer = selectedLayer();
    const bool hasComp = static_cast<bool>(comp);
    const bool hasLayer = static_cast<bool>(layer);

    duplicateLayerAction->setEnabled(hasLayer);
    renameLayerAction->setEnabled(hasLayer);
    deleteLayerAction->setEnabled(hasLayer);
    toggleVisibleAction->setEnabled(hasLayer);
    toggleLockAction->setEnabled(hasLayer);
    toggleSoloAction->setEnabled(hasLayer);
    toggleShyAction->setEnabled(hasLayer);
    soloOnlyAction->setEnabled(hasLayer && hasComp);
    selectParentAction->setEnabled(hasLayer && layer->hasParent());
    clearParentAction->setEnabled(hasLayer && layer->hasParent());
    precomposeAction->setEnabled(hasLayer);
    splitAction->setEnabled(hasLayer);
}

void ArtifactLayerMenu::Impl::handleCreateSolid()
{
    auto service = ArtifactProjectService::instance();
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
    ArtifactTextLayerInitParams params(u8"Text 1");
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params);
}

void ArtifactLayerMenu::Impl::handleDuplicateLayer()
{
    auto comp = currentComposition();
    auto layer = selectedLayer();
    if (!comp || !layer) return;
    auto result = ArtifactProjectManager::getInstance().duplicateLayerInComposition(comp->id(), layer->id());
    if (!result.success) {
        QMessageBox::warning(menu_->window(), "Layer", "レイヤー複製に失敗しました。");
    }
}

void ArtifactLayerMenu::Impl::handleRenameLayer()
{
    auto layer = selectedLayer();
    if (!layer) return;

    bool ok = false;
    const QString newName = QInputDialog::getText(
        menu_->window(),
        "レイヤー名の変更",
        "新しい名前:",
        QLineEdit::Normal,
        layer->layerName(),
        &ok);
    if (!ok) return;
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty()) return;
    layer->setLayerName(trimmed);
}

void ArtifactLayerMenu::Impl::handleDeleteLayer()
{
    auto comp = currentComposition();
    auto layer = selectedLayer();
    if (!comp || !layer) return;

    auto* service = ArtifactProjectService::instance();
    service->removeLayerFromComposition(comp->id(), layer->id());
}

void ArtifactLayerMenu::Impl::handleToggleVisible()
{
    auto layer = selectedLayer();
    if (!layer) return;
    layer->setVisible(!layer->isVisible());
}

void ArtifactLayerMenu::Impl::handleToggleLock()
{
    auto layer = selectedLayer();
    if (!layer) return;
    layer->setLocked(!layer->isLocked());
}

void ArtifactLayerMenu::Impl::handleToggleSolo()
{
    auto layer = selectedLayer();
    if (!layer) return;
    layer->setSolo(!layer->isSolo());
}

void ArtifactLayerMenu::Impl::handleToggleShy()
{
    auto layer = selectedLayer();
    if (!layer) return;
    layer->setShy(!layer->isShy());
}

void ArtifactLayerMenu::Impl::handleSoloOnlySelected()
{
    auto comp = currentComposition();
    auto layer = selectedLayer();
    if (!comp || !layer) return;

    for (const auto& candidate : comp->allLayer()) {
        if (!candidate) continue;
        candidate->setSolo(candidate->id() == layer->id());
    }
}

void ArtifactLayerMenu::Impl::handleSelectParent()
{
    auto comp = currentComposition();
    auto layer = selectedLayer();
    if (!comp || !layer || !layer->hasParent()) return;

    auto* service = ArtifactProjectService::instance();
    service->selectLayer(layer->parentLayerId());
}

void ArtifactLayerMenu::Impl::handleClearParent()
{
    auto layer = selectedLayer();
    if (!layer) return;
    layer->clearParent();
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
