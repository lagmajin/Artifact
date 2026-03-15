module;
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QApplication>
#include <wobjectimpl.h>

module Menu.Render;
import std;

import Artifact.Service.Project;
import Artifact.Render.Queue.Service;
import Utils.Path;

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
 QAction* showQueueAction = nullptr;
 QAction* renderSettingsAction = nullptr;
 QAction* startRenderAction = nullptr;
 QAction* clearAllAction = nullptr;

 void addCurrentToQueue();
 void showQueue();
 void showRenderSettings();
 void startRender();
 void clearAll();
};

ArtifactRenderMenu::Impl::Impl(ArtifactRenderMenu* menu, QWidget* mainWindow)
 : menu_(menu), mainWindow_(mainWindow)
{
 addCurrentToQueueAction = new QAction("現在のコンポジションをレンダーキューに追加(&A)");
 addCurrentToQueueAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
 addCurrentToQueueAction->setIcon(QIcon(resolveIconPath("Material/playlist_add.svg")));

 showQueueAction = new QAction("レンダーキューを表示(&Q)...");
 showQueueAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R));
 showQueueAction->setIcon(QIcon(resolveIconPath("Material/view_list.svg")));

 renderSettingsAction = new QAction("レンダー出力設定(&S)...");
 renderSettingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
 renderSettingsAction->setIcon(QIcon(resolveIconPath("Material/settings.svg")));

 startRenderAction = new QAction("レンダリングを開始(&S)");
 startRenderAction->setShortcut(QKeySequence(Qt::Key_F12));
 startRenderAction->setIcon(QIcon(resolveIconPath("Material/play_arrow.svg")));

 clearAllAction = new QAction("すべてのジョブをクリア(&C)");
 clearAllAction->setIcon(QIcon(resolveIconPath("Material/clear_all.svg")));

 menu->addAction(addCurrentToQueueAction);
 menu->addSeparator();
 menu->addAction(showQueueAction);
 menu->addAction(renderSettingsAction);
 menu->addSeparator();
 menu->addAction(startRenderAction);
 menu->addAction(clearAllAction);

 QObject::connect(addCurrentToQueueAction, &QAction::triggered, menu, [this]() { addCurrentToQueue(); });
 QObject::connect(showQueueAction, &QAction::triggered, menu, [this]() { showQueue(); });
 QObject::connect(renderSettingsAction, &QAction::triggered, menu, [this]() { showRenderSettings(); });
 QObject::connect(startRenderAction, &QAction::triggered, menu, [this]() { startRender(); });
 QObject::connect(clearAllAction, &QAction::triggered, menu, [this]() { clearAll(); });
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

void ArtifactRenderMenu::Impl::showQueue()
{
 if (QWidget* w = findWidgetByClassHint("RenderQueueManagerWidget")) {
  w->show();
  w->raise();
  w->activateWindow();
 }
}

void ArtifactRenderMenu::Impl::showRenderSettings()
{
 QMessageBox::information(mainWindow_ ? mainWindow_ : menu_, "Render Settings", "出力設定ダイアログはレンダーキューパネルの「詳細設定」からアクセスできます。");
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
 
 auto* queueService = ArtifactRenderQueueService::instance();
 const bool hasJobs = queueService && queueService->jobCount() > 0;
 impl_->startRenderAction->setEnabled(hasJobs);
 impl_->clearAllAction->setEnabled(hasJobs);
}

}
