module;
#include <utility>
#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <QActionGroup>
#include <QPointer>
#include <QApplication>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QImage>


#include <wobjectimpl.h>

#include <QMessageBox>

module Artifact.Menu.View;
import std;

import Artifact.Service.Project;
import Artifact.Service.Playback;
import Application.AppSettings;
import Artifact.MainWindow;
import Artifact.Workspace.Manager;
import Artifact.Widgets.ColorPaletteWidget;
import Artifact.Widgets.ColorSciencePanel;
import Artifact.Widgets.SecondaryPreviewWindow;
import Widgets.AssetBrowser;
import Widgets.ToolBar;
import Artifact.Widgets.ReactiveEventEditorWindow;
import Utils.Path;
import Event.Bus;
import Artifact.Event.Types;
import Image.ImageF32x4_RGBA;

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
   QActionGroup* resolutionGroup = nullptr;
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
   QAction* deleteWorkspacePresetAction = nullptr;
   QAction* restoreWorkspaceSessionAction = nullptr;
   QMenu* windowPanelsMenu = nullptr;
   QStringList cachedWorkspacePresetNames_;
   QStringList cachedDockTitles_;
   ArtifactMainWindow* mainWindow = nullptr;
   QPointer<ArtifactReactiveEventEditorWindow> reactiveEventEditorWindow;
     int newBrowserCount_ = 1;
     QAction* openContentsViewerAction = nullptr;
     QAction* openProjectPanelAction = nullptr;
     QAction* openColorPaletteAction = nullptr;
     QAction* openAutoColorGradingAction = nullptr;
     QAction* openReactiveEventEditorAction = nullptr;
     QAction* secondaryPreviewAction = nullptr;
     QPointer<ArtifactSecondaryPreviewWindow> secondaryPreviewWindow;
     ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
     std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

   void refreshEnabledState();
   void refreshWorkspaceState();
   void refreshWorkspacePresetMenu();
   void rebuildWindowPanelsMenu();
   void showProjectPanel();
   void showSecondaryPreview();
   void refreshSecondaryPreview();
   };

  ArtifactViewMenu::Impl::Impl(ArtifactViewMenu* menu)
  {
   zoomInAction = new QAction("ズームイン(&I)");
   zoomInAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
   zoomInAction->setIcon(QIcon(resolveIconPath("Studio/zoom_in.svg")));
   
   zoomOutAction = new QAction("ズームアウト(&O)");
   zoomOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
   zoomOutAction->setIcon(QIcon(resolveIconPath("Studio/zoom_out.svg")));

   defaultZoomAction = new QAction("100% ズーム");
   defaultZoomAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));
   defaultZoomAction->setIcon(QIcon(resolveIconPath("Studio/aspect_ratio.svg")));

   fitToScreenAction = new QAction("画面に合わせる(&F)");
   fitToScreenAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Slash));
   fitToScreenAction->setIcon(QIcon(resolveIconPath("Studio/fit_screen.svg")));

   resolutionMenu = new QMenu("解像度(&R)");
   resolutionMenu->setIcon(QIcon(resolveIconPath("Studio/resolution_full.svg")));
   resolutionGroup = new QActionGroup(menu);
   resolutionGroup->setExclusive(true);
   resFullAction = resolutionMenu->addAction("フル画質");
   resFullAction->setIcon(QIcon(resolveIconPath("Studio/resolution_full.svg")));
   resHalfAction = resolutionMenu->addAction("1/2画質");
   resHalfAction->setIcon(QIcon(resolveIconPath("Studio/resolution_half.svg")));
   resThirdAction = resolutionMenu->addAction("1/3画質");
   resThirdAction->setIcon(QIcon(resolveIconPath("Studio/resolution_third.svg")));
   resQuarterAction = resolutionMenu->addAction("1/4画質");
   resQuarterAction->setIcon(QIcon(resolveIconPath("Studio/resolution_quarter.svg")));
   resFullAction->setCheckable(true);
   resFullAction->setChecked(true);
   resHalfAction->setCheckable(true);
   resThirdAction->setCheckable(true);
   resQuarterAction->setCheckable(true);
   resolutionGroup->addAction(resFullAction);
   resolutionGroup->addAction(resHalfAction);
   resolutionGroup->addAction(resThirdAction);
   resolutionGroup->addAction(resQuarterAction);

   showGridAction = new QAction("グリッドを表示(&G)");
   showGridAction->setShortcut(QKeySequence("Ctrl+'"));
   showGridAction->setCheckable(true);
   showGridAction->setIcon(QIcon(resolveIconPath("Studio/grid_on.svg")));

   snapToGridAction = new QAction("グリッドにスナップ(&S)");
   snapToGridAction->setShortcut(QKeySequence("Ctrl+Shift+'"));
   snapToGridAction->setCheckable(true);
   snapToGridAction->setIcon(QIcon(resolveIconPath("Studio/grid_on.svg")));

   showGuidesAction = new QAction("ガイドを表示");
   showGuidesAction->setShortcut(QKeySequence("Ctrl+;"));
   showGuidesAction->setCheckable(true);
   showGuidesAction->setIcon(QIcon(resolveIconPath("Studio/straighten.svg")));

   snapToGuidesAction = new QAction("ガイドにスナップ");
   snapToGuidesAction->setShortcut(QKeySequence("Ctrl+Shift+;"));
   snapToGuidesAction->setCheckable(true);
   snapToGuidesAction->setIcon(QIcon(resolveIconPath("Studio/linear_scale.svg")));

   showRulersAction = new QAction("定規を表示(&R)");
   showRulersAction->setShortcut(QKeySequence("Ctrl+R"));
   showRulersAction->setCheckable(true);
   showRulersAction->setIcon(QIcon(resolveIconPath("Studio/straighten.svg")));

   useDisplayColorManagementAction = new QAction("ディスプレイのカラーマネジメントを使用");
   useDisplayColorManagementAction->setCheckable(true);
   useDisplayColorManagementAction->setIcon(QIcon(resolveIconPath("Studio/color_palette.svg")));

   qualityPresetMenu = new QMenu("品質プリセット(&Q)");
   qualityPresetMenu->setIcon(QIcon(resolveIconPath("Studio/quality_preview.svg")));
   qualityGroup = new QActionGroup(menu);
   qualityGroup->setExclusive(true);

   qualityDraftAction = qualityPresetMenu->addAction("Draft (編集優先)");
   qualityDraftAction->setIcon(QIcon(resolveIconPath("Studio/quality_draft.svg")));
   qualityPreviewAction = qualityPresetMenu->addAction("Preview (標準)");
   qualityPreviewAction->setIcon(QIcon(resolveIconPath("Studio/quality_preview.svg")));
   qualityFinalAction = qualityPresetMenu->addAction("Final (品質優先)");
   qualityFinalAction->setIcon(QIcon(resolveIconPath("Studio/quality_final.svg")));

   qualityDraftAction->setCheckable(true);
   qualityPreviewAction->setCheckable(true);
   qualityFinalAction->setCheckable(true);
   qualityPreviewAction->setChecked(true);

   qualityGroup->addAction(qualityDraftAction);
   qualityGroup->addAction(qualityPreviewAction);
   qualityGroup->addAction(qualityFinalAction);

   QObject::connect(resFullAction, &QAction::triggered, menu, []() {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
     settings->setPreviewResolutionPercent(100);
    }
   });
   QObject::connect(resHalfAction, &QAction::triggered, menu, []() {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
     settings->setPreviewResolutionPercent(50);
    }
   });
   QObject::connect(resThirdAction, &QAction::triggered, menu, []() {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
     settings->setPreviewResolutionPercent(33);
    }
   });
   QObject::connect(resQuarterAction, &QAction::triggered, menu, []() {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
     settings->setPreviewResolutionPercent(25);
    }
   });

   if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
    QObject::connect(settings, &ArtifactCore::ArtifactAppSettings::settingsChanged, menu,
                     [this]() {
                      refreshEnabledState();
                     });
   }

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
          break;
         case ::PreviewQualityPreset::Preview:
          qualityPreviewAction->setChecked(true);
          break;
         case ::PreviewQualityPreset::Final:
          qualityFinalAction->setChecked(true);
          break;
         }
        }));

    eventBusSubscriptions_.push_back(eventBus_.subscribe<ProjectChangedEvent>(
        [this](const ProjectChangedEvent&) {
         refreshEnabledState();
         refreshSecondaryPreview();
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<CompositionCreatedEvent>(
        [this](const CompositionCreatedEvent&) {
         refreshEnabledState();
         refreshSecondaryPreview();
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<CurrentCompositionChangedEvent>(
        [this](const CurrentCompositionChangedEvent&) {
         refreshSecondaryPreview();
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<FrameChangedEvent>(
        [this](const FrameChangedEvent&) {
         refreshSecondaryPreview();
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<PlaybackStateChangedEvent>(
        [this](const PlaybackStateChangedEvent&) {
         refreshSecondaryPreview();
        }));
   }

   workspaceMenu = new QMenu("ワークスペース(&K)");
   workspaceMenu->setIcon(QIcon(resolveIconPath("Studio/workspace.svg")));
   workspaceGroup = new QActionGroup(menu);
   workspaceGroup->setExclusive(true);

   workspaceDefaultAction = workspaceMenu->addAction("Default");
   workspaceDefaultAction->setIcon(QIcon(resolveIconPath("Studio/workspace_default.svg")));
   workspaceAnimationAction = workspaceMenu->addAction("Animation");
   workspaceAnimationAction->setIcon(QIcon(resolveIconPath("Studio/workspace_animation.svg")));
   workspaceVfxAction = workspaceMenu->addAction("VFX");
   workspaceVfxAction->setIcon(QIcon(resolveIconPath("Studio/workspace_vfx.svg")));
   workspaceCompositingAction = workspaceMenu->addAction("Compositing");
   workspaceCompositingAction->setIcon(QIcon(resolveIconPath("Studio/workspace_compositing.svg")));
   workspaceAudioAction = workspaceMenu->addAction("Audio");
   workspaceAudioAction->setIcon(QIcon(resolveIconPath("Studio/workspace_audio.svg")));

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
   workspacePresetMenu->setIcon(QIcon(resolveIconPath("Studio/presets.svg")));
   saveWorkspacePresetAction = workspacePresetMenu->addAction("現在のレイアウトを保存...");
   saveWorkspacePresetAction->setIcon(QIcon(resolveIconPath("Studio/save_layout.svg")));
   deleteWorkspacePresetAction = workspacePresetMenu->addAction("プリセットを削除...");
   deleteWorkspacePresetAction->setIcon(QIcon(resolveIconPath("Studio/delete.svg")));
   restoreWorkspaceSessionAction = workspacePresetMenu->addAction("最後のセッションを復元");
   restoreWorkspaceSessionAction->setIcon(QIcon(resolveIconPath("Studio/restore_session.svg")));
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
   QObject::connect(deleteWorkspacePresetAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    ArtifactWorkspaceManager manager;
    const QStringList presets = manager.presetNames();
    if (presets.isEmpty()) {
     QMessageBox::information(mainWindow, QStringLiteral("ワークスペース"),
                              QStringLiteral("削除できるプリセットがありません。"));
     return;
    }
    bool ok = false;
    const QString presetName = QInputDialog::getItem(
        mainWindow, QStringLiteral("プリセットを削除"),
        QStringLiteral("削除するプリセットを選択してください"),
        presets, 0, false, &ok);
    if (!ok || presetName.trimmed().isEmpty()) {
     return;
    }
    const QString confirmMessage = QStringLiteral("プリセット「%1」を削除しますか？").arg(presetName);
    if (QMessageBox::question(mainWindow, QStringLiteral("プリセットを削除"),
                              confirmMessage,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
     return;
    }
    if (!manager.deletePreset(presetName)) {
     QMessageBox::warning(mainWindow, QStringLiteral("プリセットを削除"),
                          QStringLiteral("プリセットの削除に失敗しました。"));
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
   openContentsViewerAction->setIcon(QIcon(resolveIconPath("Studio/contents_viewer.svg")));
   QObject::connect(openContentsViewerAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    mainWindow->setDockVisible(QStringLiteral("Contents Viewer"), true);
    mainWindow->activateDock(QStringLiteral("Contents Viewer"));
   });

   openProjectPanelAction = menu->addAction("Project パネル(&P)");
   openProjectPanelAction->setIcon(QIcon(resolveIconPath("Studio/panels.svg")));
   QObject::connect(openProjectPanelAction, &QAction::triggered, menu, [this]() {
    showProjectPanel();
   });

   openColorPaletteAction = menu->addAction("カラーパレット(&P)");
   openColorPaletteAction->setIcon(QIcon(resolveIconPath("Studio/color_palette.svg")));
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

   openAutoColorGradingAction = menu->addAction("自動カラーグレーディング(&G)");
   openAutoColorGradingAction->setIcon(QIcon(resolveIconPath("Studio/color_palette.svg")));
   QObject::connect(openAutoColorGradingAction, &QAction::triggered, menu, [this]() {
    if (!mainWindow) return;
    const QString dockTitle = QStringLiteral("Auto Color Grading");
    if (mainWindow->hasDock(dockTitle)) {
     mainWindow->setDockVisible(dockTitle, true);
     mainWindow->activateDock(dockTitle);
     return;
    }
    auto* gradingWidget = new ArtifactColorSciencePanel(mainWindow);
    gradingWidget->analyzeCurrentFrame();
    mainWindow->addDockedWidgetFloating(
        dockTitle,
        QStringLiteral("auto_color_grading_dock"),
        gradingWidget,
        QRect(150, 150, 580, 720));
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
    windowPanelsMenu->setIcon(QIcon(resolveIconPath("Studio/panels.svg")));

    // Dynamically rebuild the panels menu each time it opens
   QObject::connect(windowPanelsMenu, &QMenu::aboutToShow, menu, [this]() {
     rebuildWindowPanelsMenu();
    });

    menu->addSeparator();
    openReactiveEventEditorAction = menu->addAction("リアクティブイベントエディタ(&E)...");
    openReactiveEventEditorAction->setIcon(QIcon(resolveIconPath("Studio/reactive_events.svg")));
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
     newBrowserAction->setIcon(QIcon(resolveIconPath("Studio/asset_browser.svg")));
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
     secondaryPreviewAction->setIcon(QIcon(resolveIconPath("Studio/secondary_preview.svg")));
     QObject::connect(secondaryPreviewAction, &QAction::triggered, menu, [this]() {
      showSecondaryPreview();
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
  if (resFullAction) {
    resFullAction->setEnabled(hasComp);
  }
  if (resHalfAction) {
    resHalfAction->setEnabled(hasComp);
  }
  if (resThirdAction) {
    resThirdAction->setEnabled(hasComp);
  }
  if (resQuarterAction) {
    resQuarterAction->setEnabled(hasComp);
  }
  qualityPresetMenu->setEnabled(hasComp);
  if (hasComp) {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
      const int percent = settings->previewResolutionPercent();
      if (resFullAction) {
        resFullAction->setChecked(percent >= 88);
      }
      if (resHalfAction) {
        resHalfAction->setChecked(percent >= 42 && percent < 88);
      }
      if (resThirdAction) {
        resThirdAction->setChecked(percent >= 28 && percent < 42);
      }
      if (resQuarterAction) {
        resQuarterAction->setChecked(percent < 28);
      }
    }
  }
  
  showGridAction->setEnabled(hasComp);
  snapToGridAction->setEnabled(hasComp);
  showGuidesAction->setEnabled(hasComp);
  snapToGuidesAction->setEnabled(hasComp);
  showRulersAction->setEnabled(hasComp);
  useDisplayColorManagementAction->setEnabled(hasComp);
  if (openContentsViewerAction) {
   openContentsViewerAction->setEnabled(true);
  }
  if (openProjectPanelAction) {
   openProjectPanelAction->setEnabled(static_cast<bool>(mainWindow));
  }
  if (openColorPaletteAction) {
   openColorPaletteAction->setEnabled(true);
  }
  if (openAutoColorGradingAction) {
   openAutoColorGradingAction->setEnabled(hasComp);
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
  saveWorkspacePresetAction->setIcon(QIcon(resolveIconPath("Studio/save_layout.svg")));
  deleteWorkspacePresetAction =
      workspacePresetMenu->addAction("プリセットを削除...");
  deleteWorkspacePresetAction->setIcon(QIcon(resolveIconPath("Studio/delete.svg")));
  restoreWorkspaceSessionAction =
      workspacePresetMenu->addAction("最後のセッションを復元");
  restoreWorkspaceSessionAction->setIcon(QIcon(resolveIconPath("Studio/restore_session.svg")));

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

  QObject::connect(deleteWorkspacePresetAction, &QAction::triggered,
                   mainWindow, [mw = mainWindow]() {
                     if (!mw) {
                       return;
                     }
                     ArtifactWorkspaceManager manager;
                     const QStringList presets = manager.presetNames();
                     if (presets.isEmpty()) {
                       QMessageBox::information(
                           mw, QStringLiteral("ワークスペース"),
                           QStringLiteral("削除できるプリセットがありません。"));
                       return;
                     }
                     bool ok = false;
                     const QString presetName = QInputDialog::getItem(
                         mw, QStringLiteral("プリセットを削除"),
                         QStringLiteral("削除するプリセットを選択してください"),
                         presets, 0, false, &ok);
                     if (!ok || presetName.trimmed().isEmpty()) {
                       return;
                     }
                     const QString confirmMessage =
                         QStringLiteral("プリセット「%1」を削除しますか？").arg(presetName);
                     if (QMessageBox::question(
                             mw, QStringLiteral("プリセットを削除"),
                             confirmMessage, QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No) != QMessageBox::Yes) {
                       return;
                     }
                     if (!manager.deletePreset(presetName)) {
                       QMessageBox::warning(
                           mw, QStringLiteral("プリセットを削除"),
                           QStringLiteral("プリセットの削除に失敗しました。"));
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
   empty->setIcon(QIcon(resolveIconPath("Studio/empty_state.svg")));
   empty->setEnabled(false);
   return;
  }

  for (const QString& preset : presets) {
   QAction* action = workspacePresetMenu->addAction(preset);
   action->setIcon(QIcon(resolveIconPath("Studio/presets.svg")));
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
  action->setIcon(QIcon(resolveIconPath("Studio/panels.svg")));
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
   action->setIcon(QIcon(resolveIconPath("Studio/panels.svg")));
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
   none->setIcon(QIcon(resolveIconPath("Studio/empty_state.svg")));
   none->setEnabled(false);
  }
}

void ArtifactViewMenu::Impl::showProjectPanel()
{
 if (!mainWindow) {
  return;
 }

 const QString dockTitle = QStringLiteral("Project");
 if (mainWindow->hasDock(dockTitle)) {
  mainWindow->setDockVisible(dockTitle, true);
  mainWindow->activateDock(dockTitle);
  return;
 }

 QMessageBox::information(mainWindow, QStringLiteral("Project パネル"),
                          QStringLiteral("Project パネルが見つかりません。"));
}

void ArtifactViewMenu::Impl::refreshSecondaryPreview()
{
 if (!secondaryPreviewWindow || !secondaryPreviewWindow->isVisible()) {
  return;
 }

 auto* playback = ArtifactPlaybackService::instance();
 if (!playback || !playback->currentComposition()) {
  secondaryPreviewWindow->updatePreviewImage(QImage());
  secondaryPreviewWindow->setStatusMessage(
      QStringLiteral("RAM preview: unavailable"));
  return;
 }

 const auto currentFrame = playback->currentFrame().framePosition();
 const auto range = playback->frameRange();
 const auto currentComp = playback->currentComposition();
 const QString compName = currentComp ? currentComp->settings().compositionName().toQString()
                                      : QString();
 const auto state = playback->ramPreviewFrameState(currentFrame);
 const QString stateReason = ramPreviewNotReadyReason(state);

 secondaryPreviewWindow->updateFrameInfo(currentFrame, range.duration(), compName);
 secondaryPreviewWindow->setStatusMessage(
     QStringLiteral("RAM preview: frame %1 / %2 | %3%4")
         .arg(currentFrame)
         .arg(range.duration())
         .arg(state.playable ? QStringLiteral("ready") : stateReason)
         .arg(compName.isEmpty() ? QString() : QStringLiteral(" | %1").arg(compName)));

 ArtifactCore::ImageF32x4_RGBA previewImage;
 if (playback->tryGetRamPreviewFrameImage(currentFrame, previewImage)) {
  secondaryPreviewWindow->updatePreviewImage(previewImage.toQImage());
 } else {
  secondaryPreviewWindow->updatePreviewImage(QImage());
  if (!state.playable) {
    secondaryPreviewWindow->setStatusMessage(
        QStringLiteral("RAM preview: frame %1 / %2 | %3")
            .arg(currentFrame)
            .arg(range.duration())
            .arg(stateReason));
  }
 }
}

void ArtifactViewMenu::Impl::showSecondaryPreview()
{
 if (!mainWindow) {
  return;
 }

 const auto screens = QGuiApplication::screens();
 if (screens.size() < 2) {
  QMessageBox::information(mainWindow, QStringLiteral("セカンドモニタープレビュー"),
                           QStringLiteral("2つ目のモニターが検出されていません。\nマルチディスプレイ環境でご利用ください。"));
  return;
 }

  if (!secondaryPreviewWindow) {
  secondaryPreviewWindow = new ArtifactSecondaryPreviewWindow(mainWindow);
  secondaryPreviewWindow->setAttribute(Qt::WA_DeleteOnClose, false);
 }

 secondaryPreviewWindow->showOnScreen(1);
 refreshSecondaryPreview();
}

};

