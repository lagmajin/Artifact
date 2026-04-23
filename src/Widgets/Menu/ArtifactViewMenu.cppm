module;
#include <utility>
#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <QActionGroup>
#include <QPointer>
#include <QApplication>
#include <QInputDialog>
#include <QLineEdit>


#include <wobjectimpl.h>

#include <QMessageBox>

module Artifact.Menu.View;
import std;

import Artifact.Service.Project;
import Artifact.MainWindow;
import Artifact.Workspace.Manager;
import Artifact.Widgets.ColorPaletteWidget;
import Widgets.AssetBrowser;
import Widgets.ToolBar;
import Artifact.Widgets.ReactiveEventEditorWindow;
import Utils.Path;
import Event.Bus;
import Artifact.Event.Types;

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

  class ArtifactViewMenu::Impl {
  public:
   Impl(ArtifactViewMenu* menu);
   ~Impl();

   QAction* zoomInAction = nullptr;
   QAction* zoomOutAction = nullptr;
   QAction* defaultZoomAction = nullptr;
   QAction* fitToScreenAction = nullptr;
   
   QMenu* resolutionMenu = nullptr;
   QAction* resFullAction = nullptr;
   QAction* resHalfAction = nullptr;
   QAction* resThirdAction = nullptr;
   QAction* resQuarterAction = nullptr;

   QAction* showGridAction = nullptr;
   QAction* snapToGridAction = nullptr;
   QAction* showGuidesAction = nullptr;
   QAction* snapToGuidesAction = nullptr;
   QAction* showRulersAction = nullptr;
   QAction* useDisplayColorManagementAction = nullptr;

   QMenu* qualityPresetMenu = nullptr;
   QActionGroup* qualityGroup = nullptr;
   QAction* qualityDraftAction = nullptr;
   QAction* qualityPreviewAction = nullptr;
   QAction* qualityFinalAction = nullptr;
   QMenu* workspaceMenu = nullptr;
   QMenu* workspacePresetMenu = nullptr;
   QActionGroup* workspaceGroup = nullptr;
   QAction* workspaceDefaultAction = nullptr;
   QAction* workspaceAnimationAction = nullptr;
   QAction* workspaceVfxAction = nullptr;
   QAction* workspaceCompositingAction = nullptr;
   QAction* workspaceAudioAction = nullptr;
   QAction* saveWorkspacePresetAction = nullptr;
   QAction* restoreWorkspaceSessionAction = nullptr;
   QMenu* windowPanelsMenu = nullptr;
   QStringList cachedWorkspacePresetNames_;
   QStringList cachedDockTitles_;
   ArtifactMainWindow* mainWindow = nullptr;
   QPointer<ArtifactReactiveEventEditorWindow> reactiveEventEditorWindow;
     int newBrowserCount_ = 1;
     QAction* openContentsViewerAction = nullptr;
     QAction* openColorPaletteAction = nullptr;
     QAction* openReactiveEventEditorAction = nullptr;
     QAction* secondaryPreviewAction = nullptr;
     ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
     std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

   void refreshEnabledState();
   void refreshWorkspaceState();
   void refreshWorkspacePresetMenu();
   void rebuildWindowPanelsMenu();
   };

  ArtifactViewMenu::Impl::Impl(ArtifactViewMenu* menu)
  {
   zoomInAction = new QAction("ズームイン(&I)");
   zoomInAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
   zoomInAction->setIcon(QIcon(resolveIconPath("Material/zoom_in.svg")));
   
   zoomOutAction = new QAction("ズームアウト(&O)");
   zoomOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
   zoomOutAction->setIcon(QIcon(resolveIconPath("Material/zoom_out.svg")));

   defaultZoomAction = new QAction("100% ズーム");
   defaultZoomAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));
   defaultZoomAction->setIcon(QIcon(resolveIconPath("Material/aspect_ratio.svg")));

   fitToScreenAction = new QAction("画面に合わせる(&F)");
   fitToScreenAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Slash));
   fitToScreenAction->setIcon(QIcon(resolveIconPath("Material/fit_screen.svg")));

   resolutionMenu = new QMenu("解像度(&R)");
   resFullAction = resolutionMenu->addAction("フル画質");
   resHalfAction = resolutionMenu->addAction("1/2画質");
   resThirdAction = resolutionMenu->addAction("1/3画質");
   resQuarterAction = resolutionMenu->addAction("1/4画質");
   resFullAction->setCheckable(true);
   resFullAction->setChecked(true);
   resHalfAction->setCheckable(true);
   resThirdAction->setCheckable(true);
   resQuarterAction->setCheckable(true);

   showGridAction = new QAction("グリッドを表示(&G)");
   showGridAction->setShortcut(QKeySequence("Ctrl+'"));
   showGridAction->setCheckable(true);
   showGridAction->setIcon(QIcon(resolveIconPath("Material/grid_on.svg")));

   snapToGridAction = new QAction("グリッドにスナップ(&S)");
   snapToGridAction->setShortcut(QKeySequence("Ctrl+Shift+'"));
   snapToGridAction->setCheckable(true);

   showGuidesAction = new QAction("ガイドを表示");
   showGuidesAction->setShortcut(QKeySequence("Ctrl+;"));
   showGuidesAction->setCheckable(true);

   snapToGuidesAction = new QAction("ガイドにスナップ");
   snapToGuidesAction->setShortcut(QKeySequence("Ctrl+Shift+;"));
   snapToGuidesAction->setCheckable(true);

   showRulersAction = new QAction("定規を表示(&R)");
   showRulersAction->setShortcut(QKeySequence("Ctrl+R"));
   showRulersAction->setCheckable(true);
   showRulersAction->setIcon(QIcon(resolveIconPath("Material/straighten.svg")));

   useDisplayColorManagementAction = new QAction("ディスプレイのカラーマネジメントを使用");
   useDisplayColorManagementAction->setCheckable(true);

   qualityPresetMenu = new QMenu("品質プリセット(&Q)");
   qualityGroup = new QActionGroup(menu);
   qualityGroup->setExclusive(true);

   qualityDraftAction = qualityPresetMenu->addAction("Draft (編集優先)");
   qualityPreviewAction = qualityPresetMenu->addAction("Preview (標準)");
   qualityFinalAction = qualityPresetMenu->addAction("Final (品質優先)");

   qualityDraftAction->setCheckable(true);
   qualityPreviewAction->setCheckable(true);
   qualityFinalAction->setCheckable(true);
   qualityPreviewAction->setChecked(true);

   qualityGroup->addAction(qualityDraftAction);
   qualityGroup->addAction(qualityPreviewAction);
   qualityGroup->addAction(qualityFinalAction);

   auto* svc = ArtifactProjectService::instance();
   if (svc) {
    QObject::connect(qualityDraftAction, &QAction::triggered, menu, [svc]() {
     svc->setPreviewQualityPreset(::PreviewQualityPreset::Draft);
    });
    QObject::connect(qualityPreviewAction, &QAction::triggered, menu, [svc]() {
     svc->setPreviewQualityPreset(::PreviewQualityPreset::Preview);
    });
    QObject::connect(qualityFinalAction, &QAction::triggered, menu, [svc]() {
     svc->setPreviewQualityPreset(::PreviewQualityPreset::Final);
    });

    eventBusSubscriptions_.push_back(eventBus_.subscribe<PreviewQualityPresetChangedEvent>(
        [this](const PreviewQualityPresetChangedEvent& event) {
         switch (static_cast<::PreviewQualityPreset>(event.preset)) {
         case ::PreviewQualityPreset::Draft:
          qualityDraftAction->setChecked(true);
          resQuarterAction->setChecked(true);
          break;
         case ::PreviewQualityPreset::Preview:
          qualityPreviewAction->setChecked(true);
          resHalfAction->setChecked(true);
          break;
         case ::PreviewQualityPreset::Final:
          qualityFinalAction->setChecked(true);
          resFullAction->setChecked(true);
          break;
         }
        }));

    eventBusSubscriptions_.push_back(eventBus_.subscribe<ProjectChangedEvent>(
        [this](const ProjectChangedEvent&) {
         refreshEnabledState();
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<CompositionCreatedEvent>(
        [this](const CompositionCreatedEvent&) {
         refreshEnabledState();
        }));
   }

   workspaceMenu = new QMenu("ワークスペース(&K)");
   workspaceGroup = new QActionGroup(menu);
   workspaceGroup->setExclusive(true);

   workspaceDefaultAction = workspaceMenu->addAction("Default");
   workspaceAnimationAction = workspaceMenu->addAction("Animation");
   workspaceVfxAction = workspaceMenu->addAction("VFX");
   workspaceCompositingAction = workspaceMenu->addAction("Compositing");
   workspaceAudioAction = workspaceMenu->addAction("Audio");

   for (auto* action : {workspaceDefaultAction, workspaceAnimationAction,
                        workspaceVfxAction, workspaceCompositingAction,
                        workspaceAudioAction}) {
    action->setCheckable(true);
    workspaceGroup->addAction(action);
   }

   QObject::connect(workspaceDefaultAction, &QAction::triggered, menu, [this]() {
    if (mainWindow) mainWindow->setWorkspaceMode(WorkspaceMode::Default);
   });
   QObject::connect(workspaceAnimationAction, &QAction::triggered, menu, [this]() {
    if (mainWindow) mainWindow->setWorkspaceMode(WorkspaceMode::Animation);
   });
   QObject::connect(workspaceVfxAction, &QAction::triggered, menu, [this]() {
    if (mainWindow) mainWindow->setWorkspaceMode(WorkspaceMode::VFX);
   });
   QObject::connect(workspaceCompositingAction, &QAction::triggered, menu, [this]() {
    if (mainWindow) mainWindow->setWorkspaceMode(WorkspaceMode::Compositing);
   });
   QObject::connect(workspaceAudioAction, &QAction::triggered, menu, [this]() {
    if (mainWindow) mainWindow->setWorkspaceMode(WorkspaceMode::Audio);
   });

   workspacePresetMenu = new QMenu("プリセット");
   saveWorkspacePresetAction = workspacePresetMenu->addAction("現在のレイアウトを保存...");
   restoreWorkspaceSessionAction = workspacePresetMenu->addAction("最後のセッションを復元");
   QObject::connect(saveWorkspacePresetAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    const QString defaultName = QStringLiteral("Custom");
    bool ok = false;
    const QString presetName = QInputDialog::getText(
        mainWindow, QStringLiteral("ワークスペースを保存"),
        QStringLiteral("プリセット名を入力してください"), QLineEdit::Normal,
        defaultName, &ok)
                                    .trimmed();
    if (!ok || presetName.isEmpty()) {
     return;
    }
    ArtifactWorkspaceManager manager;
    if (!manager.savePreset(presetName, mainWindow)) {
     QMessageBox::warning(mainWindow, QStringLiteral("ワークスペースを保存"),
                          QStringLiteral("ワークスペースの保存に失敗しました。"));
    }
   });
   QObject::connect(restoreWorkspaceSessionAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    ArtifactWorkspaceManager manager;
    if (!manager.restoreSession(mainWindow)) {
     QMessageBox::information(mainWindow, QStringLiteral("ワークスペースを復元"),
                              QStringLiteral("復元できるセッションがありません。"));
    }
   });
   
   QObject::connect(menu, &QMenu::aboutToShow, menu, [this]() {
    refreshEnabledState();
    refreshWorkspaceState();
    refreshWorkspacePresetMenu();
   });

   menu->addAction(zoomInAction);
   menu->addAction(zoomOutAction);
   menu->addAction(defaultZoomAction);
   menu->addAction(fitToScreenAction);
   menu->addSeparator();
   menu->addMenu(resolutionMenu);
   menu->addMenu(qualityPresetMenu);
   menu->addSeparator();
   menu->addAction(useDisplayColorManagementAction);
   menu->addSeparator();
   openContentsViewerAction = menu->addAction("Contents Viewer");
   QObject::connect(openContentsViewerAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    mainWindow->setDockVisible(QStringLiteral("Contents Viewer"), true);
    mainWindow->activateDock(QStringLiteral("Contents Viewer"));
   });

   openColorPaletteAction = menu->addAction("カラーパレット(&P)");
   QObject::connect(openColorPaletteAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    const QString dockTitle = QStringLiteral("Color Palette");
    if (mainWindow->hasDock(dockTitle)) {
     mainWindow->setDockVisible(dockTitle, true);
     mainWindow->activateDock(dockTitle);
     return;
    }
    auto* paletteWidget = new ArtifactColorPaletteWidget(mainWindow);
    mainWindow->addDockedWidgetFloating(
        dockTitle,
        QStringLiteral("color_palette_dock"),
        paletteWidget,
        QRect(120, 120, 560, 640));
   });

   menu->addSeparator();
   menu->addMenu(workspaceMenu);
   menu->addMenu(workspacePresetMenu);
   menu->addSeparator();
   menu->addAction(showGridAction);
   menu->addAction(snapToGridAction);
   menu->addAction(showGuidesAction);
   menu->addAction(snapToGuidesAction);
   menu->addAction(showRulersAction);
   menu->addSeparator();
    windowPanelsMenu = menu->addMenu("ウィンドウパネル(&W)");

    // Dynamically rebuild the panels menu each time it opens
   QObject::connect(windowPanelsMenu, &QMenu::aboutToShow, menu, [this]() {
     rebuildWindowPanelsMenu();
    });

    menu->addSeparator();
    openReactiveEventEditorAction = menu->addAction("リアクティブイベントエディタ(&E)...");
    QObject::connect(openReactiveEventEditorAction, &QAction::triggered, menu, [this]() {
     if (!mainWindow) return;
     if (!reactiveEventEditorWindow) {
      reactiveEventEditorWindow = new ArtifactReactiveEventEditorWindow(mainWindow);
      reactiveEventEditorWindow->setAttribute(Qt::WA_DeleteOnClose, true);
     }
     reactiveEventEditorWindow->present();
    });

    menu->addSeparator();
     auto* newBrowserAction = menu->addAction("新規アセットブラウザ(&A)");
     QObject::connect(newBrowserAction, &QAction::triggered, menu, [this]() {
      if (!mainWindow) return;
      newBrowserCount_++;
      auto* browser = new ArtifactAssetBrowser(mainWindow);
      const QString title = QStringLiteral("Asset Browser (%1)").arg(newBrowserCount_);
      mainWindow->addDockedWidgetFloating(
       title,
       QStringLiteral("asset_browser_%1").arg(newBrowserCount_),
       browser,
       QRect(100, 100, 800, 600));
     });

     menu->addSeparator();
     secondaryPreviewAction = menu->addAction("セカンドモニタープレビュー(&S)");
     secondaryPreviewAction->setShortcut(QKeySequence(Qt::Key_F12));
      QObject::connect(secondaryPreviewAction, &QAction::triggered, menu, [this, menu]() {
       if (!mainWindow) return;
       auto screens = QGuiApplication::screens();
       if (screens.size() < 2) {
        QMessageBox::information(menu, "セカンドモニタープレビュー",
         "2つ目のモニターが検出されていません。\n"
         "マルチディスプレイ環境でご利用ください。");
        return;
       }
       // TODO: Implement secondary preview on second screen
       qWarning() << "[ViewMenu] Secondary preview not yet implemented";
      });
    }

 ArtifactViewMenu::Impl::~Impl()
 {

 }

 void ArtifactViewMenu::Impl::refreshEnabledState()
 {
  auto* svc = ArtifactProjectService::instance();
  const bool hasProject = svc && svc->hasProject();
  const bool hasComp = hasProject && static_cast<bool>(svc->currentComposition().lock());

  zoomInAction->setEnabled(hasComp);
  zoomOutAction->setEnabled(hasComp);
  defaultZoomAction->setEnabled(hasComp);
  fitToScreenAction->setEnabled(hasComp);
  
  resolutionMenu->setEnabled(hasComp);
  qualityPresetMenu->setEnabled(hasComp);
  
  showGridAction->setEnabled(hasComp);
  snapToGridAction->setEnabled(hasComp);
  showGuidesAction->setEnabled(hasComp);
  snapToGuidesAction->setEnabled(hasComp);
  showRulersAction->setEnabled(hasComp);
  useDisplayColorManagementAction->setEnabled(hasComp);
  if (openContentsViewerAction) {
   openContentsViewerAction->setEnabled(true);
  }
  if (openColorPaletteAction) {
   openColorPaletteAction->setEnabled(true);
  }
  if (openReactiveEventEditorAction) {
   openReactiveEventEditorAction->setEnabled(true);
  }
 }

 void ArtifactViewMenu::Impl::refreshWorkspaceState()
 {
  if (!workspaceDefaultAction || !workspaceAnimationAction ||
      !workspaceVfxAction || !workspaceCompositingAction ||
      !workspaceAudioAction) {
   return;
  }

  const WorkspaceMode mode = mainWindow ? mainWindow->workspaceMode()
                                        : WorkspaceMode::Default;
  workspaceDefaultAction->setChecked(mode == WorkspaceMode::Default);
  workspaceAnimationAction->setChecked(mode == WorkspaceMode::Animation);
  workspaceVfxAction->setChecked(mode == WorkspaceMode::VFX);
  workspaceCompositingAction->setChecked(mode == WorkspaceMode::Compositing);
  workspaceAudioAction->setChecked(mode == WorkspaceMode::Audio);
 }

 void ArtifactViewMenu::Impl::refreshWorkspacePresetMenu()
 {
 if (!workspacePresetMenu) {
   return;
  }

  if (!mainWindow) {
   workspacePresetMenu->clear();
   workspacePresetMenu->setEnabled(false);
   cachedWorkspacePresetNames_.clear();
   return;
  }

  ArtifactWorkspaceManager manager;
  const QStringList presets = manager.presetNames();
  workspacePresetMenu->setEnabled(true);

  if (presets == cachedWorkspacePresetNames_ && !workspacePresetMenu->actions().isEmpty()) {
   return;
  }

  cachedWorkspacePresetNames_ = presets;
  workspacePresetMenu->clear();
  saveWorkspacePresetAction =
      workspacePresetMenu->addAction("現在のレイアウトを保存...");
  restoreWorkspaceSessionAction =
      workspacePresetMenu->addAction("最後のセッションを復元");

  QObject::connect(saveWorkspacePresetAction, &QAction::triggered, mainWindow,
                   [mw = mainWindow]() {
                     if (!mw) {
                       return;
                     }
                     const QString defaultName = QStringLiteral("Custom");
                     bool ok = false;
                     const QString presetName = QInputDialog::getText(
                                                    mw,
                                                    QStringLiteral("ワークスペースを保存"),
                                                    QStringLiteral("プリセット名を入力してください"),
                                                    QLineEdit::Normal, defaultName,
                                                    &ok)
                                                    .trimmed();
                     if (!ok || presetName.isEmpty()) {
                       return;
                     }
                     ArtifactWorkspaceManager manager;
                     if (!manager.savePreset(presetName, mw)) {
                       QMessageBox::warning(
                           mw, QStringLiteral("ワークスペースを保存"),
                           QStringLiteral("ワークスペースの保存に失敗しました。"));
                     }
                   });

  QObject::connect(restoreWorkspaceSessionAction, &QAction::triggered,
                   mainWindow, [mw = mainWindow]() {
                     if (!mw) {
                       return;
                     }
                     ArtifactWorkspaceManager manager;
                     if (!manager.restoreSession(mw)) {
                       QMessageBox::information(
                           mw, QStringLiteral("ワークスペースを復元"),
                           QStringLiteral("復元できるセッションがありません。"));
                     }
                   });

  workspacePresetMenu->addSeparator();

  if (presets.isEmpty()) {
   QAction* empty = workspacePresetMenu->addAction("(no presets)");
   empty->setEnabled(false);
   return;
  }

  for (const QString& preset : presets) {
   QAction* action = workspacePresetMenu->addAction(preset);
   QObject::connect(action, &QAction::triggered, mainWindow,
                    [mw = mainWindow, preset]() {
                     ArtifactWorkspaceManager manager;
                     if (!manager.restorePreset(preset, mw)) {
                      QMessageBox::warning(mw,
                                           QStringLiteral("ワークスペース"),
                                           QStringLiteral("プリセットの復元に失敗しました。"));
                     }
                    });
  }
 }

 W_OBJECT_IMPL(ArtifactViewMenu)

 ArtifactViewMenu::ArtifactViewMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle("表示(&V)");
  setTearOffEnabled(false);
  impl_->refreshEnabledState();
 }

 ArtifactViewMenu::~ArtifactViewMenu()
 {
  delete impl_;
 }

 void ArtifactViewMenu::registerView(const QString& name, QWidget* view)
 {
  if (!impl_ || !impl_->windowPanelsMenu || !view) return;

  QAction* action = impl_->windowPanelsMenu->addAction(name);
  action->setCheckable(true);
  action->setChecked(view->isVisible());

  QPointer<QWidget> guardedView(view);
  QObject::connect(action, &QAction::toggled, this, [guardedView](bool checked) {
   if (!guardedView) return;
   guardedView->setVisible(checked);
   if (checked) {
    guardedView->raise();
    guardedView->activateWindow();
   }
  });

  QObject::connect(view, &QWidget::destroyed, this, [this, action]() {
   if (impl_ && impl_->windowPanelsMenu && action) {
    impl_->windowPanelsMenu->removeAction(action);
   }
   if (action) {
    action->deleteLater();
   }
  });

  auto syncAction = [action, guardedView]() {
   if (!action || !guardedView) return;
   const bool vis = guardedView->isVisible();
   if (action->isChecked() != vis) {
    action->setChecked(vis);
   }
  };

  if (view->isWindow()) {
   QObject::connect(view, &QWidget::windowTitleChanged, this, [action](const QString& t) {
    if (action && !t.isEmpty()) action->setText(t);
   });
  }

  QObject::connect(action, &QAction::hovered, this, [syncAction]() mutable {
   syncAction();
  });
 }

 void ArtifactViewMenu::setMainWindow(ArtifactMainWindow* mw)
 {
  impl_->mainWindow = mw;
  if (impl_) {
   impl_->cachedWorkspacePresetNames_.clear();
   impl_->cachedDockTitles_.clear();
   impl_->refreshWorkspaceState();
  }
 }

void ArtifactViewMenu::Impl::rebuildWindowPanelsMenu()
{
  if (!windowPanelsMenu || !mainWindow) return;

  const QStringList titles = mainWindow->dockTitles();
  if (titles == cachedDockTitles_) {
   for (QAction* action : windowPanelsMenu->actions()) {
    if (!action || !action->isCheckable()) {
     continue;
    }
    const QString title = action->text();
    action->setChecked(mainWindow->isDockVisible(title));
   }
   return;
  }

  cachedDockTitles_ = titles;
  windowPanelsMenu->clear();

  for (const QString& title : titles) {
   QAction* action = windowPanelsMenu->addAction(title);
   action->setCheckable(true);
   action->setChecked(mainWindow->isDockVisible(title));

   QObject::connect(action, &QAction::triggered, mainWindow, [mw = mainWindow, title](bool checked) {
    mw->setDockVisible(title, checked);
    if (checked) {
     mw->activateDock(title);
    }
   });
  }

  if (titles.isEmpty()) {
   QAction* none = windowPanelsMenu->addAction("(no panels)");
   none->setEnabled(false);
  }
}

};

