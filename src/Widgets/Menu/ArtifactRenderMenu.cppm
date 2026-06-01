module;
#include <utility>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QApplication>
#include <QDialog>
#include <QRect>
#include <wobjectimpl.h>

module Menu.Render;
import std;

import Event.Bus;
import Artifact.Event.Types;
import Artifact.Service.Project;
import Artifact.Render.Queue.Service;
import Artifact.MainWindow;
import Artifact.Widgets.RenderCenterWindow;
import Artifact.Widget.Dialog.RenderOutputSetting;
import Utils.Path;
import Artifact.Widgets.Test.ScrollPoC;

namespace Artifact {
using namespace ArtifactCore;

namespace {
 QWidget* findWidgetByClassHint(const QString& classHint)
 {
  const auto widgets = QApplication::allWidgets();
  for (QWidget* w : widgets) {
   if (!w) continue;
   const QString className = QString::fromLatin1(w->metaObject()->className());
   if (className.contains(classHint, Qt::CaseInsensitive)) {
    return w;
   }
  }
  return nullptr;
 }
}

W_OBJECT_IMPL(ArtifactRenderMenu)

class ArtifactRenderMenu::Impl {
public:
 Impl(ArtifactRenderMenu* menu, QWidget* mainWindow);
 ~Impl() = default;

 ArtifactRenderMenu* menu_ = nullptr;
 QWidget* mainWindow_ = nullptr;
 QAction* addCurrentToQueueAction = nullptr;
 QAction* addAllCompositionsAction = nullptr;
 QAction* showQueueAction = nullptr;
 QAction* showRenderManagerAction = nullptr;
 QAction* renderSettingsAction = nullptr;
 QAction* startRenderAction = nullptr;
 QAction* clearAllAction = nullptr;
 QAction* scrollPoCAction = nullptr;
 std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

 void addCurrentToQueue();
 void addAllCompositions();
 void showQueue();
 void showRenderManager();
 void showRenderSettings();
 void startRender();
 void clearAll();
 void showScrollPoC();
};

ArtifactRenderMenu::Impl::Impl(ArtifactRenderMenu* menu, QWidget* mainWindow)
 : menu_(menu), mainWindow_(mainWindow)
{
 addCurrentToQueueAction = new QAction("現在のコンポジションをレンダーキューに追加(&A)");
 addCurrentToQueueAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
 addCurrentToQueueAction->setIcon(QIcon(resolveIconPath("Studio/rendermenu_add_current.svg")));

 showQueueAction = new QAction("レンダーキューを表示(&Q)...");
 showQueueAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R));
 showQueueAction->setIcon(QIcon(resolveIconPath("Studio/rendermenu_queue.svg")));

 showRenderManagerAction = new QAction("レンダーマネージャーを表示(&M)...");
 showRenderManagerAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
 showRenderManagerAction->setIcon(QIcon(resolveIconPath("Studio/rendermenu_manager.svg")));

 renderSettingsAction = new QAction("レンダー出力設定(&S)...");
 renderSettingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
 renderSettingsAction->setIcon(QIcon(resolveIconPath("Studio/rendermenu_settings.svg")));

 startRenderAction = new QAction("レンダリングを開始(&S)");
 startRenderAction->setShortcut(QKeySequence(Qt::Key_F12));
 startRenderAction->setIcon(QIcon(resolveIconPath("Studio/rendermenu_start.svg")));

 clearAllAction = new QAction("すべてのジョブをクリア(&C)");
 clearAllAction->setIcon(QIcon(resolveIconPath("Studio/rendermenu_clear_all.svg")));

 scrollPoCAction = new QAction("Scroll PoC (Floating)", menu);
 scrollPoCAction->setIcon(QIcon(resolveIconPath("Studio/rendermenu_scroll_poc.svg")));

 menu->addAction(addCurrentToQueueAction);
 addAllCompositionsAction = new QAction("全コンポジションをキューに追加(&A)", menu);
 addAllCompositionsAction->setIcon(QIcon(resolveIconPath("Studio/rendermenu_add_all.svg")));
 menu->addAction(addAllCompositionsAction);
 menu->addSeparator();
 menu->addAction(showQueueAction);
 menu->addAction(showRenderManagerAction);
 menu->addAction(renderSettingsAction);
 menu->addSeparator();
 menu->addAction(startRenderAction);
 menu->addAction(clearAllAction);
 menu->addSeparator();
 menu->addAction(scrollPoCAction);

 QObject::connect(addCurrentToQueueAction, &QAction::triggered, menu, [this]() { addCurrentToQueue(); });
 QObject::connect(addAllCompositionsAction, &QAction::triggered, menu, [this]() { addAllCompositions(); });
 QObject::connect(showQueueAction, &QAction::triggered, menu, [this]() { showQueue(); });
 QObject::connect(showRenderManagerAction, &QAction::triggered, menu, [this]() { showRenderManager(); });
 QObject::connect(renderSettingsAction, &QAction::triggered, menu, [this]() { showRenderSettings(); });
 QObject::connect(startRenderAction, &QAction::triggered, menu, [this]() { startRender(); });
 QObject::connect(clearAllAction, &QAction::triggered, menu, [this]() { clearAll(); });
 QObject::connect(scrollPoCAction, &QAction::triggered, menu, [this]() { showScrollPoC(); });

 auto& eventBus = ArtifactCore::globalEventBus();
eventBusSubscriptions_.push_back(eventBus.subscribe<ProjectChangedEvent>(
     [this](const ProjectChangedEvent&) { menu_->rebuildMenu(); }));
eventBusSubscriptions_.push_back(eventBus.subscribe<CompositionCreatedEvent>(
     [this](const CompositionCreatedEvent&) { menu_->rebuildMenu(); }));
eventBusSubscriptions_.push_back(eventBus.subscribe<CurrentCompositionChangedEvent>(
     [this](const CurrentCompositionChangedEvent&) { menu_->rebuildMenu(); }));
eventBusSubscriptions_.push_back(eventBus.subscribe<RenderQueueChangedEvent>(
     [this](const RenderQueueChangedEvent&) { menu_->rebuildMenu(); }));
}

void ArtifactRenderMenu::Impl::showScrollPoC()
{
 if (auto* mw = qobject_cast<ArtifactMainWindow*>(mainWindow_)) {
  auto* poc = new ArtifactScrollPoCWidget();
  mw->addDockedWidgetFloating(
   QStringLiteral("Scroll PoC"),
   QStringLiteral("scroll_poc_dock"),
   poc,
   QRect(100, 100, 600, 400)
  );
 }
}

void ArtifactRenderMenu::Impl::addCurrentToQueue()
{
 auto* projectService = ArtifactProjectService::instance();
 auto currentComp = projectService->currentComposition().lock();
 if (!currentComp) {
  QMessageBox::information(mainWindow_ ? mainWindow_ : menu_, "Render", "アクティブなコンポジションがありません。");
  return;
 }

 auto* queueService = ArtifactRenderQueueService::instance();
 if (queueService) {
  queueService->addRenderQueueForComposition(currentComp->id(), currentComp->settings().compositionName().toQString());
 }
 
 showQueue();
}

void ArtifactRenderMenu::Impl::addAllCompositions()
{
 auto* queueService = ArtifactRenderQueueService::instance();
 if (!queueService) return;

 const int added = queueService->addAllCompositions();
 if (added > 0) {
  QMessageBox::information(mainWindow_ ? mainWindow_ : menu_, "Render",
   QString("%1 個のコンポジションをキューに追加しました。").arg(added));
 } else {
  QMessageBox::information(mainWindow_ ? mainWindow_ : menu_, "Render",
   "追加できるコンポジションがありません。");
 }

 showQueue();
}

void ArtifactRenderMenu::Impl::showQueue()
{
 // まず既存の独立ウィンドウを探す
 if (auto* window = qobject_cast<ArtifactRenderCenterWindow*>(findWidgetByClassHint("ArtifactRenderCenterWindow"))) {
  window->present();
  return;
 }

 // なければ新しく作成（MainWindowを親にするが、Windowフラグで独立させる）
 auto* newWindow = new ArtifactRenderCenterWindow(mainWindow_);
 newWindow->setAttribute(Qt::WA_DeleteOnClose);
 newWindow->present();
}

void ArtifactRenderMenu::Impl::showRenderManager()
{
 showQueue();
}

void ArtifactRenderMenu::Impl::showRenderSettings()
{
 ArtifactRenderOutputSettingDialog dialog(mainWindow_ ? mainWindow_ : menu_);
 dialog.setWindowTitle(QStringLiteral("Render Output Settings"));
 dialog.exec();
}

void ArtifactRenderMenu::Impl::startRender()
{
 auto* queueService = ArtifactRenderQueueService::instance();
 if (queueService) {
  queueService->startAllJobs();
 }
}

void ArtifactRenderMenu::Impl::clearAll()
{
 auto* queueService = ArtifactRenderQueueService::instance();
 if (queueService) {
  queueService->removeAllRenderQueues();
 }
}

ArtifactRenderMenu::ArtifactRenderMenu(QWidget* mainWindow, QWidget* parent)
 : QMenu(parent), impl_(new Impl(this, mainWindow))
{
 setTitle("レンダー(&R)");
 connect(this, &QMenu::aboutToShow, this, &ArtifactRenderMenu::rebuildMenu);
}

ArtifactRenderMenu::~ArtifactRenderMenu()
{
 delete impl_;
}

void ArtifactRenderMenu::rebuildMenu()
{
 if (!impl_) return;
 auto* projectService = ArtifactProjectService::instance();
 const bool hasComp = projectService && !projectService->currentComposition().expired();
 impl_->addCurrentToQueueAction->setEnabled(hasComp);
 if (impl_->addAllCompositionsAction) {
  impl_->addAllCompositionsAction->setEnabled(ArtifactRenderQueueService::instance() != nullptr);
 }
 if (impl_->showQueueAction) {
  impl_->showQueueAction->setEnabled(true);
 }
 if (impl_->showRenderManagerAction) {
  impl_->showRenderManagerAction->setEnabled(true);
 }
 if (impl_->renderSettingsAction) {
  impl_->renderSettingsAction->setEnabled(true);
 }
 
 auto* queueService = ArtifactRenderQueueService::instance();
 const bool hasJobs = queueService && queueService->jobCount() > 0;
 impl_->startRenderAction->setEnabled(hasJobs);
 impl_->clearAllAction->setEnabled(hasJobs);
 if (impl_->scrollPoCAction) {
  impl_->scrollPoCAction->setEnabled(static_cast<bool>(impl_->mainWindow_));
 }
}

}
