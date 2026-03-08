module;
#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <QActionGroup>
#include <QPointer>
#include <QApplication>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Menu.View;




import Artifact.Service.Project;

namespace Artifact {
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
   QMenu* windowPanelsMenu = nullptr;
   QAction* openProjectPanelAction = nullptr;
   QAction* openInspectorPanelAction = nullptr;

  };

  ArtifactViewMenu::Impl::Impl(ArtifactViewMenu* menu)
  {
   zoomInAction = new QAction("ズームイン(&I)");
   zoomInAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
   
   zoomOutAction = new QAction("ズームアウト(&O)");
   zoomOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));

   defaultZoomAction = new QAction("100% ズーム");
   defaultZoomAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));

   fitToScreenAction = new QAction("画面に合わせる(&F)");
   fitToScreenAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Slash));

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
     svc->setPreviewQualityPreset(PreviewQualityPreset::Draft);
    });
    QObject::connect(qualityPreviewAction, &QAction::triggered, menu, [svc]() {
     svc->setPreviewQualityPreset(PreviewQualityPreset::Preview);
    });
    QObject::connect(qualityFinalAction, &QAction::triggered, menu, [svc]() {
     svc->setPreviewQualityPreset(PreviewQualityPreset::Final);
    });

    QObject::connect(svc, &ArtifactProjectService::previewQualityPresetChanged, menu, [this](PreviewQualityPreset preset) {
     switch (preset) {
     case PreviewQualityPreset::Draft:
      qualityDraftAction->setChecked(true);
      resQuarterAction->setChecked(true);
      break;
     case PreviewQualityPreset::Preview:
      qualityPreviewAction->setChecked(true);
      resHalfAction->setChecked(true);
      break;
     case PreviewQualityPreset::Final:
      qualityFinalAction->setChecked(true);
      resFullAction->setChecked(true);
      break;
     }
    });
   }

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
   menu->addAction(showGridAction);
   menu->addAction(snapToGridAction);
   menu->addAction(showGuidesAction);
   menu->addAction(snapToGuidesAction);
   menu->addAction(showRulersAction);
   menu->addSeparator();
   windowPanelsMenu = menu->addMenu("ウィンドウパネル(&W)");
   openProjectPanelAction = windowPanelsMenu->addAction("プロジェクト");
   openInspectorPanelAction = windowPanelsMenu->addAction("インスペクター");

   QObject::connect(openProjectPanelAction, &QAction::triggered, menu, []() {
    if (QWidget* w = findWidgetByClassHint("ArtifactProjectManagerWidget")) {
     w->show();
     w->raise();
     w->activateWindow();
    }
   });
   QObject::connect(openInspectorPanelAction, &QAction::triggered, menu, []() {
    if (QWidget* w = findWidgetByClassHint("ArtifactInspectorWidget")) {
     w->show();
     w->raise();
     w->activateWindow();
    }
   });
  }

 ArtifactViewMenu::Impl::~Impl()
 {

 }

 ArtifactViewMenu::ArtifactViewMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle("表示(&V)");
  setTearOffEnabled(false);
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

};
