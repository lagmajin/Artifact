module;
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <wobjectimpl.h>

module Artifact.Menu.Layer;

import Artifact.Project.Manager;
import Artifact.Service.Project;
import Artifact.Layer.InitParams;
import Artifact.Layer.Factory;
import Artifact.Widgets.CreatePlaneLayerDialog;

namespace Artifact {

W_OBJECT_IMPL(ArtifactLayerMenu)

class ArtifactLayerMenu::Impl {
public:
    Impl(ArtifactLayerMenu* menu);
    ~Impl() = default;

    ArtifactLayerMenu* menu_ = nullptr;
    QMenu* createMenu = nullptr;
    QAction* createSolidAction = nullptr;
    QAction* createNullAction = nullptr;
    QAction* createAdjustAction = nullptr;
    QAction* createTextAction = nullptr;
    QAction* deleteLayerAction = nullptr;

    void handleCreateSolid();
    void handleCreateNull();
    void handleCreateAdjust();
    void handleCreateText();
};

ArtifactLayerMenu::Impl::Impl(ArtifactLayerMenu* menu) : menu_(menu)
{
    createMenu = new QMenu("新規(&N)");
    createSolidAction = new QAction("平面(&Y)...");
    createSolidAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    
    createNullAction = new QAction("ヌルオブジェクト(&N)");
    createNullAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Y));
    
    createAdjustAction = new QAction("調整レイヤー(&A)");
    createAdjustAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Y));
    
    createTextAction = new QAction("テキスト(&T)");
    createTextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_T));
    
    createMenu->addAction(createSolidAction);
    createMenu->addAction(createNullAction);
    createMenu->addAction(createAdjustAction);
    createMenu->addAction(createTextAction);
    
    menu->addMenu(createMenu);
    
    deleteLayerAction = new QAction("削除(&D)");
    deleteLayerAction->setShortcut(QKeySequence(Qt::Key_Delete));
    menu->addAction(deleteLayerAction);

    QObject::connect(createSolidAction, &QAction::triggered, menu, [this]() { handleCreateSolid(); });
    QObject::connect(createNullAction, &QAction::triggered, menu, [this]() { handleCreateNull(); });
    QObject::connect(createAdjustAction, &QAction::triggered, menu, [this]() { handleCreateAdjust(); });
    QObject::connect(createTextAction, &QAction::triggered, menu, [this]() { handleCreateText(); });
}

void ArtifactLayerMenu::Impl::handleCreateSolid()
{
    auto dialog = new CreateSolidLayerSettingDialog(menu_);
    auto service = ArtifactProjectService::instance();
    QObject::connect(dialog, &CreateSolidLayerSettingDialog::submit, menu_, [service](const ArtifactSolidLayerInitParams& params) {
        service->addLayerToCurrentComposition(params);
    });
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void ArtifactLayerMenu::Impl::handleCreateNull()
{
    ArtifactNullLayerInitParams params(u8"Null 1");
    auto service = ArtifactProjectService::instance();
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
    auto service = ArtifactProjectService::instance();
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