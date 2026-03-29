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
import Artifact.MainWindow;
import Artifact.Widgets.RenderCenterWindow;
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
 QAction* showQueueAction = nullptr;
 QAction* showRenderManagerAction = nullptr;
 QAction* renderSettingsAction = nullptr;
 QAction* startRenderAction = nullptr;
 QAction* clearAllAction = nullptr;

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
 addCurrentToQueueAction->setIcon(QIcon(resolveIconPath("Material/playlist_add.svg")));

 showQueueAction = new QAction("レンダーキューを表示(&Q)...");
 showQueueAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R));
 showQueueAction->setIcon(QIcon(resolveIconPath("Material/view_list.svg")));

 showRenderManagerAction = new QAction("レンダーマネージャーを表示(&M)...");
 showRenderManagerAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
 showRenderManagerAction->setIcon(QIcon(resolveIconPath("Material/settings_suggest.svg")));

 renderSettingsAction = new QAction("レンダー出力設定(&S)...");
 renderSettingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
 renderSettingsAction->setIcon(QIcon(resolveIconPath("Material/settings.svg")));

 startRenderAction = new QAction("レンダリングを開始(&S)");
 startRenderAction->setShortcut(QKeySequence(Qt::Key_F12));
 startRenderAction->setIcon(QIcon(resolveIconPath("Material/play_arrow.svg")));

 clearAllAction = new QAction("すべてのジョブをクリア(&C)");
 clearAllAction->setIcon(QIcon(resolveIconPath("Material/clear_all.svg")));

 auto* scrollPoCAction = new QAction("Scroll PoC (Floating)", menu);

 menu->addAction(addCurrentToQueueAction);
 auto* addAllCompsAction = new QAction("全コンポジションをキューに追加(&A)", menu);
 menu->addAction(addAllCompsAction);
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
 QObject::connect(addAllCompsAction, &QAction::triggered, menu, [this]() { addAllCompositions(); });
 QObject::connect(showQueueAction, &QAction::triggered, menu, [this]() { showQueue(); });
 QObject::connect(showRenderManagerAction, &QAction::triggered, menu, [this]() { showRenderManager(); });
 QObject::connect(renderSettingsAction, &QAction::triggered, menu, [this]() { showRenderSettings(); });
 QObject::connect(startRenderAction, &QAction::triggered, menu, [this]() { startRender(); });
 QObject::connect(clearAllAction, &QAction::triggered, menu, [this]() { clearAll(); });
 QObject::connect(scrollPoCAction, &QAction::triggered, menu, [this]() { showScrollPoC(); });
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
