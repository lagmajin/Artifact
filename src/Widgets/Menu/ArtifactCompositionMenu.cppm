module;
#include <QMenu>
#include <QAction>
#include <QColorDialog>
#include <QDebug>
#include <wobjectimpl.h>

module Menu.Composition;

import Artifact.Project.Manager;
import Artifact.Service.Project;
import Dialog.Composition;
import Artifact.MainWindow;

namespace Artifact {

W_OBJECT_IMPL(ArtifactCompositionMenu)

class ArtifactCompositionMenu::Impl {
public:
    Impl(ArtifactCompositionMenu* menu, ArtifactMainWindow* mainWindow);
    ~Impl() = default;

    ArtifactCompositionMenu* menu_ = nullptr;
    ArtifactMainWindow* mainWindow_ = nullptr;
    QAction* createAction = nullptr;
    QAction* settingsAction = nullptr;
    QAction* colorAction = nullptr;

    void showCreate();
    void showSettings();
    void showColor();
};

ArtifactCompositionMenu::Impl::Impl(ArtifactCompositionMenu* menu, ArtifactMainWindow* mainWindow)
    : menu_(menu), mainWindow_(mainWindow)
{
    createAction = new QAction("新規コンポジション(&N)...");
    createAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    
    settingsAction = new QAction("コンポジション設定(&S)...");
    settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    
    colorAction = new QAction("背景色(&B)...");
    colorAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));

    menu->addAction(createAction);
    menu->addAction(settingsAction);
    menu->addAction(colorAction);

    QObject::connect(createAction, &QAction::triggered, menu, [this]() { showCreate(); });
    QObject::connect(settingsAction, &QAction::triggered, menu, [this]() { showSettings(); });
    QObject::connect(colorAction, &QAction::triggered, menu, [this]() { showColor(); });
}

void ArtifactCompositionMenu::Impl::showCreate()
{
    auto dialog = new CreateCompositionDialog(reinterpret_cast<QWidget*>(mainWindow_));
    if (dialog->exec()) {
        // Handled by dialog
    }
    dialog->deleteLater();
}

void ArtifactCompositionMenu::Impl::showSettings()
{
    // TODO: Implement
}

void ArtifactCompositionMenu::Impl::showColor()
{
    auto service = ArtifactProjectService::instance();
    if (auto comp = service->currentComposition().lock()) {
        QColor color = QColorDialog::getColor(Qt::black, reinterpret_cast<QWidget*>(mainWindow_), "Background Color");
        if (color.isValid()) {
            comp->setBackGroundColor(FloatColor(color.redF(), color.greenF(), color.blueF(), color.alphaF()));
        }
    }
}

ArtifactCompositionMenu::ArtifactCompositionMenu(ArtifactMainWindow* mainWindow, QWidget* parent)
    : QMenu(parent), impl_(new Impl(this, mainWindow))
{
    setTitle("コンポジション(&C)");
    connect(this, &QMenu::aboutToShow, this, &ArtifactCompositionMenu::rebuildMenu);
}

ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* parent)
    : QMenu(parent), impl_(nullptr)
{
}

ArtifactCompositionMenu::~ArtifactCompositionMenu()
{
    delete impl_;
}

void ArtifactCompositionMenu::rebuildMenu()
{
    if (!impl_) return;
    auto service = ArtifactProjectService::instance();
    bool hasComp = !service->currentComposition().expired();
    impl_->settingsAction->setEnabled(hasComp);
    impl_->colorAction->setEnabled(hasComp);
}

void ArtifactCompositionMenu::handleCreateCompositionRequested()
{
    if (impl_) impl_->showCreate();
}

} // namespace Artifact