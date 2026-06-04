module;
#include <utility>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QCursor>
#include <QPoint>
#include <QSignalBlocker>
#include <wobjectimpl.h>

module Menu.Animation;
import std;

import Event.Bus;
import Application.AppSettings;
import Artifact.Event.Types;
import Artifact.Service.Project;
import Artifact.Widgets.ArtifactPropertyWidget;
import Artifact.Widgets.ExpressionCopilotWidget;
import Artifact.Widgets.Timeline;
import Artifact.Widgets.Timeline.EasingLab;
import Utils.Id;
import Utils.Path;
import Math.Interpolate;

W_OBJECT_IMPL(Artifact::ArtifactAnimationMenu)

namespace Artifact {

namespace {
QIcon menuIcon(const QString& path)
{
  return QIcon(resolveIconPath(path));
}

ArtifactTimelineWidget* activeTimelineWidget(QWidget* root)
{
 if (!root) {
  return nullptr;
 }

 const auto widgets = root->findChildren<ArtifactTimelineWidget*>();
 for (auto* widget : widgets) {
  if (widget && widget->hasFocus()) {
   return widget;
  }
 }
 for (auto* widget : widgets) {
  if (widget && widget->isVisible()) {
   return widget;
  }
 }
 return widgets.isEmpty() ? nullptr : widgets.front();
}

bool openActiveExpressionCopilot(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget() &&
      propertyWidget->openActiveExpressionCopilot()) {
   return true;
  }
 }

 return false;
}

bool openNewExpressionCopilot(QWidget* root)
{
 if (!root) {
  return false;
 }

 auto* copilot = new ArtifactExpressionCopilotWidget(root);
 copilot->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::Tool);
 copilot->setWindowTitle(QStringLiteral("Expression Copilot"));
 copilot->setAttribute(Qt::WA_DeleteOnClose);
 copilot->move(QCursor::pos() - QPoint(150, 200));
 copilot->show();
 return true;
}

bool clearActiveExpression(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget() &&
      propertyWidget->clearActiveExpression()) {
   return true;
  }
 }
 return false;
}

bool convertActiveExpressionToKeyframes(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget() &&
      propertyWidget->convertActiveExpressionToKeyframes()) {
   return true;
  }
 }
 return false;
}

bool saveActiveExpressionPreset(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget() &&
      propertyWidget->saveActiveExpressionPreset()) {
   return true;
  }
 }
 return false;
}

bool loadActiveExpressionPreset(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget() &&
      propertyWidget->loadActiveExpressionPreset()) {
   return true;
  }
 }
 return false;
}

bool hasActiveExpressionTarget(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget()) {
   return true;
  }
 }
 return false;
}
}

 class ArtifactAnimationMenu::Impl {
 public:
  Impl(ArtifactAnimationMenu* menu);
  ~Impl();

  ArtifactAnimationMenu* menu_ = nullptr;
  ArtifactCore::LayerID selectedLayerId_;
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  QAction* addKeyframeAction = nullptr;
  QAction* removeKeyframeAction = nullptr;
  QAction* selectAllKeyframesAction = nullptr;
  QAction* copyKeyframesAction = nullptr;
  QAction* pasteKeyframesAction = nullptr;

  QAction* linearInterpolationAction = nullptr;
  QAction* bezierInterpolationAction = nullptr;
  QAction* holdInterpolationAction = nullptr;
  QAction* easeInAction = nullptr;
  QAction* easeOutAction = nullptr;
  QAction* easeInOutAction = nullptr;

  QAction* showGraphEditorAction = nullptr;
  QAction* toggleValueGraphAction = nullptr;
  QAction* toggleVelocityGraphAction = nullptr;
  QAction* easingLabAction = nullptr;
  QAction* keyPatternAction = nullptr;
  QActionGroup* graphModeGroup = nullptr;

  QAction* goToNextKeyframeAction = nullptr;
  QAction* goToPreviousKeyframeAction = nullptr;
  QAction* goToFirstKeyframeAction = nullptr;
  QAction* goToLastKeyframeAction = nullptr;

  QAction* enableTimeRemapAction = nullptr;
  QAction* freezeFrameAction = nullptr;
  QAction* timeReverseAction = nullptr;

  QAction* addExpressionAction = nullptr;
  QAction* editExpressionAction = nullptr;
  QAction* removeExpressionAction = nullptr;
  QAction* convertToKeyframesAction = nullptr;

  QAction* saveAnimationPresetAction = nullptr;
  QAction* loadAnimationPresetAction = nullptr;

  QMenu* interpolationMenu = nullptr;
  QMenu* graphEditorMenu = nullptr;
  QMenu* navigationMenu = nullptr;
  QMenu* timeRemapMenu = nullptr;
  QMenu* expressionMenu = nullptr;
  QMenu* presetMenu = nullptr;

  void refreshEnabledState();
 };

 ArtifactAnimationMenu::Impl::Impl(ArtifactAnimationMenu* menu) : menu_(menu)
 {
  auto& eventBus = ArtifactCore::globalEventBus();
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<LayerSelectionChangedEvent>(
          [this](const LayerSelectionChangedEvent& event) {
            const ArtifactCore::LayerID layerId(event.layerId);
            if (!event.compositionId.isEmpty()) {
              auto* service = ArtifactProjectService::instance();
              if (service) {
                if (const auto comp = service->currentComposition().lock()) {
                  if (comp->id().toString() != event.compositionId) {
                    return;
                  }
                }
              }
            }
            selectedLayerId_ = layerId;
            refreshEnabledState();
          }));
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<LayerChangedEvent>(
          [this](const LayerChangedEvent& event) {
            if (!event.compositionId.isEmpty()) {
              auto* service = ArtifactProjectService::instance();
              if (service) {
                if (const auto comp = service->currentComposition().lock()) {
                  if (comp->id().toString() != event.compositionId) {
                    return;
                  }
                }
              }
            }
            if (event.changeType == LayerChangedEvent::ChangeType::Removed &&
                selectedLayerId_ == ArtifactCore::LayerID(event.layerId)) {
              selectedLayerId_ = {};
            }
            refreshEnabledState();
          }));
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent&) {
            refreshEnabledState();
          }));
  QObject::connect(menu, &QMenu::aboutToShow, menu, [this]() {
   refreshEnabledState();
  });
 }

 ArtifactAnimationMenu::Impl::~Impl()
 {
 }

 void ArtifactAnimationMenu::Impl::refreshEnabledState()
 {
  auto* service = ArtifactProjectService::instance();
  bool hasLayer = service && service->hasProject() && static_cast<bool>(service->currentComposition().lock()) && !selectedLayerId_.isNil();

  addKeyframeAction->setEnabled(hasLayer);
  removeKeyframeAction->setEnabled(hasLayer);
  selectAllKeyframesAction->setEnabled(hasLayer);
  copyKeyframesAction->setEnabled(hasLayer);
  pasteKeyframesAction->setEnabled(hasLayer);

 interpolationMenu->setEnabled(hasLayer);
 graphEditorMenu->setEnabled(hasLayer);
 navigationMenu->setEnabled(hasLayer);
 timeRemapMenu->setEnabled(hasLayer);
 expressionMenu->setEnabled(hasLayer);
 presetMenu->setEnabled(hasLayer);
  const bool hasExpressionTarget = hasActiveExpressionTarget(menu_ ? menu_->window() : nullptr);
  if (addExpressionAction) {
   addExpressionAction->setEnabled(hasLayer);
  }
  if (editExpressionAction) {
   editExpressionAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (removeExpressionAction) {
   removeExpressionAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (convertToKeyframesAction) {
   convertToKeyframesAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (saveAnimationPresetAction) {
   saveAnimationPresetAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (loadAnimationPresetAction) {
   loadAnimationPresetAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (keyPatternAction) {
   keyPatternAction->setEnabled(hasLayer);
  }
}

 ArtifactAnimationMenu::ArtifactAnimationMenu(QWidget* parent)
  : QMenu(parent), impl_(new Impl(this))
 {
  setTitle("アニメーション(&A)");
  setIcon(menuIcon(QStringLiteral("Studio/animation.svg")));
  setTearOffEnabled(false);

  impl_->addKeyframeAction = addAction("キーフレームを追加");
  impl_->addKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/add_circle.svg")));
  impl_->addKeyframeAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_K));
  impl_->removeKeyframeAction = addAction("キーフレームを削除");
  impl_->removeKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/remove_circle.svg")));
  impl_->removeKeyframeAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_K));

  impl_->selectAllKeyframesAction = addAction("すべてのキーフレームを選択");
  impl_->selectAllKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/select_all.svg")));
  impl_->selectAllKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_A));

  impl_->copyKeyframesAction = addAction("キーフレームをコピー");
  impl_->copyKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/content_copy.svg")));
  impl_->copyKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));

  impl_->pasteKeyframesAction = addAction("キーフレームをペースト");
  impl_->pasteKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/content_paste.svg")));
  impl_->pasteKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));

  addSeparator();

  impl_->interpolationMenu = addMenu("キーフレーム補間(&I)");
  impl_->interpolationMenu->setIcon(menuIcon(QStringLiteral("Studio/timeline.svg")));

  impl_->linearInterpolationAction = impl_->interpolationMenu->addAction("リニア");
  impl_->linearInterpolationAction->setIcon(menuIcon(QStringLiteral("Studio/straighten.svg")));
  impl_->linearInterpolationAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_1));
  impl_->easeInAction = impl_->interpolationMenu->addAction("イージーイーズイン");
  impl_->easeInAction->setIcon(menuIcon(QStringLiteral("Studio/trending_up.svg")));
  impl_->easeInAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_2));
  impl_->easeOutAction = impl_->interpolationMenu->addAction("イージーイーズアウト");
  impl_->easeOutAction->setIcon(menuIcon(QStringLiteral("Studio/trending_down.svg")));
  impl_->easeOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_3));
  impl_->easeInOutAction = impl_->interpolationMenu->addAction("イージーイーズ");
  impl_->easeInOutAction->setIcon(menuIcon(QStringLiteral("Studio/show_chart.svg")));
  impl_->easeInOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_4));
  impl_->holdInterpolationAction = impl_->interpolationMenu->addAction("停止");
  impl_->holdInterpolationAction->setIcon(menuIcon(QStringLiteral("Studio/pause.svg")));
  impl_->holdInterpolationAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_5));
  impl_->bezierInterpolationAction = impl_->interpolationMenu->addAction("ベジェ");
  impl_->bezierInterpolationAction->setIcon(menuIcon(QStringLiteral("Studio/edit.svg")));
  impl_->bezierInterpolationAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_6));

  impl_->linearInterpolationAction->setCheckable(true);
  impl_->bezierInterpolationAction->setCheckable(true);
  impl_->holdInterpolationAction->setCheckable(true);

  addSeparator();

  impl_->graphEditorMenu = addMenu("カーブエディタ(&G)");
  impl_->graphEditorMenu->setIcon(menuIcon(QStringLiteral("Studio/show_chart.svg")));

  impl_->showGraphEditorAction = impl_->graphEditorMenu->addAction("カーブエディタを表示");
  impl_->showGraphEditorAction->setIcon(menuIcon(QStringLiteral("Studio/query_stats.svg")));
  impl_->showGraphEditorAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
  impl_->graphEditorMenu->addSeparator();
  impl_->graphModeGroup = new QActionGroup(impl_->menu_);
  impl_->graphModeGroup->setExclusive(true);
  impl_->toggleValueGraphAction = impl_->graphEditorMenu->addAction("値グラフを表示");
  impl_->toggleValueGraphAction->setIcon(menuIcon(QStringLiteral("Studio/show_chart.svg")));
  impl_->toggleValueGraphAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_G));
  impl_->toggleValueGraphAction->setCheckable(true);
  impl_->graphModeGroup->addAction(impl_->toggleValueGraphAction);
  impl_->toggleVelocityGraphAction = impl_->graphEditorMenu->addAction("速度グラフを表示");
  impl_->toggleVelocityGraphAction->setIcon(menuIcon(QStringLiteral("Studio/speed.svg")));
  impl_->toggleVelocityGraphAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_G));
  impl_->toggleVelocityGraphAction->setCheckable(true);
  impl_->graphModeGroup->addAction(impl_->toggleVelocityGraphAction);
  impl_->easingLabAction = impl_->graphEditorMenu->addAction("EasingLab を開く");
  impl_->easingLabAction->setIcon(menuIcon(QStringLiteral("Studio/tune.svg")));
  impl_->keyPatternAction = impl_->graphEditorMenu->addAction("Key Pattern Dialog を開く");
  impl_->keyPatternAction->setIcon(menuIcon(QStringLiteral("Studio/animation.svg")));

  impl_->navigationMenu = addMenu("ナビゲーション(&N)");
  impl_->navigationMenu->setIcon(menuIcon(QStringLiteral("Studio/skip_next.svg")));

  impl_->goToNextKeyframeAction = impl_->navigationMenu->addAction("次のキーフレームに移動");
  impl_->goToNextKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/skip_next.svg")));
  impl_->goToNextKeyframeAction->setShortcut(QKeySequence(Qt::Key_K));
  impl_->goToPreviousKeyframeAction = impl_->navigationMenu->addAction("前のキーフレームに移動");
  impl_->goToPreviousKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/skip_previous.svg")));
  impl_->goToPreviousKeyframeAction->setShortcut(QKeySequence(Qt::Key_J));

  impl_->navigationMenu->addSeparator();

  impl_->goToFirstKeyframeAction = impl_->navigationMenu->addAction("最初のキーフレームに移動");
  impl_->goToFirstKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/fast_rewind.svg")));
  impl_->goToFirstKeyframeAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_J));
  impl_->goToLastKeyframeAction = impl_->navigationMenu->addAction("最後のキーフレームに移動");
  impl_->goToLastKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/fast_forward.svg")));
  impl_->goToLastKeyframeAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_K));

  addSeparator();

  impl_->timeRemapMenu = addMenu("タイムリマップ(&T)");
  impl_->timeRemapMenu->setIcon(menuIcon(QStringLiteral("Studio/schedule.svg")));

  impl_->enableTimeRemapAction = impl_->timeRemapMenu->addAction("タイムリマップ可能にする");
  impl_->enableTimeRemapAction->setIcon(menuIcon(QStringLiteral("Studio/schedule.svg")));
  impl_->enableTimeRemapAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T));
  impl_->freezeFrameAction = impl_->timeRemapMenu->addAction("フレームをフリーズ");
  impl_->freezeFrameAction->setIcon(menuIcon(QStringLiteral("Studio/pause_circle.svg")));
  impl_->freezeFrameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_J));
  impl_->timeReverseAction = impl_->timeRemapMenu->addAction("時間反転レイヤー");
  impl_->timeReverseAction->setIcon(menuIcon(QStringLiteral("Studio/swap_horiz.svg")));

  impl_->expressionMenu = addMenu("エクスプレッション(&E)");
  impl_->expressionMenu->setIcon(menuIcon(QStringLiteral("Studio/functions.svg")));

  impl_->addExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを追加...");
  impl_->addExpressionAction->setIcon(menuIcon(QStringLiteral("Studio/add.svg")));
  impl_->addExpressionAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Equal));
  impl_->editExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを編集...");
  impl_->editExpressionAction->setIcon(menuIcon(QStringLiteral("Studio/edit.svg")));
  impl_->editExpressionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Equal));
  impl_->removeExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを削除");
  impl_->removeExpressionAction->setIcon(menuIcon(QStringLiteral("Studio/delete.svg")));

  impl_->expressionMenu->addSeparator();

  impl_->convertToKeyframesAction = impl_->expressionMenu->addAction("エクスプレッションをキーフレームに変換...");
  impl_->convertToKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animation.svg")));
  addSeparator();

  impl_->presetMenu = addMenu("アニメーションプリセット(&P)");
  impl_->presetMenu->setIcon(menuIcon(QStringLiteral("Studio/bookmarks.svg")));

  impl_->saveAnimationPresetAction = impl_->presetMenu->addAction("アニメーションプリセットを保存...");
  impl_->saveAnimationPresetAction->setIcon(menuIcon(QStringLiteral("Studio/save.svg")));
  impl_->loadAnimationPresetAction = impl_->presetMenu->addAction("アニメーションプリセットを適用...");
  impl_->loadAnimationPresetAction->setIcon(menuIcon(QStringLiteral("Studio/folder_open.svg")));
  impl_->presetMenu->addSeparator();

  auto dispatchAction = [this](QAction* action) {
   if (!action) {
    return;
   }
   if (action == impl_->addKeyframeAction) { Q_EMIT addKeyframeRequested(); return; }
   if (action == impl_->removeKeyframeAction) { Q_EMIT removeKeyframeRequested(); return; }
   if (action == impl_->selectAllKeyframesAction) { Q_EMIT selectAllKeyframesRequested(); return; }
   if (action == impl_->copyKeyframesAction) { Q_EMIT copyKeyframesRequested(); return; }
   if (action == impl_->pasteKeyframesAction) { Q_EMIT pasteKeyframesRequested(); return; }
   if (action == impl_->linearInterpolationAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::Linear); return; }
   if (action == impl_->easeInAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::EaseIn); return; }
   if (action == impl_->easeOutAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::EaseOut); return; }
   if (action == impl_->easeInOutAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::EaseInOut); return; }
   if (action == impl_->holdInterpolationAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::Constant); return; }
   if (action == impl_->bezierInterpolationAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::Bezier); return; }
   if (action == impl_->showGraphEditorAction) { Q_EMIT showGraphEditorRequested(); return; }
   if (action == impl_->toggleValueGraphAction) { Q_EMIT toggleValueGraphRequested(); return; }
   if (action == impl_->toggleVelocityGraphAction) { Q_EMIT toggleVelocityGraphRequested(); return; }
  if (action == impl_->easingLabAction) {
    EasingLabDialog dialog(
        this,
        [this](ArtifactCore::InterpolationType type) {
          if (auto* timeline = activeTimelineWidget(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr)) {
            timeline->applyInterpolationToSelectedKeyframes(type);
          }
        });
    dialog.exec();
    return;
  }
  if (action == impl_->keyPatternAction) {
    if (auto* timeline = activeTimelineWidget(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr)) {
      timeline->showKeyPatternDialog();
    }
    return;
  }
   if (action == impl_->goToNextKeyframeAction) { Q_EMIT goToNextKeyframeRequested(); return; }
   if (action == impl_->goToPreviousKeyframeAction) { Q_EMIT goToPreviousKeyframeRequested(); return; }
   if (action == impl_->goToFirstKeyframeAction) { Q_EMIT goToFirstKeyframeRequested(); return; }
   if (action == impl_->goToLastKeyframeAction) { Q_EMIT goToLastKeyframeRequested(); return; }
   if (action == impl_->enableTimeRemapAction) { Q_EMIT enableTimeRemapRequested(); return; }
   if (action == impl_->freezeFrameAction) { Q_EMIT freezeFrameRequested(); return; }
   if (action == impl_->timeReverseAction) { Q_EMIT timeReverseRequested(); return; }
   if (action == impl_->addExpressionAction) { openNewExpressionCopilot(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->editExpressionAction) { openActiveExpressionCopilot(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->removeExpressionAction) { clearActiveExpression(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->convertToKeyframesAction) { convertActiveExpressionToKeyframes(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->saveAnimationPresetAction) { saveActiveExpressionPreset(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->loadAnimationPresetAction) { loadActiveExpressionPreset(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
  };

  QObject::connect(this, &QMenu::triggered, this, dispatchAction);

  if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
    connect(settings, &ArtifactCore::ArtifactAppSettings::settingsChanged, impl_->menu_, [this]() {
      if (!impl_ || !impl_->toggleValueGraphAction || !impl_->toggleVelocityGraphAction) {
        return;
      }
      if (auto* appSettings = ArtifactCore::ArtifactAppSettings::instance()) {
        const bool speedMode =
            appSettings->timelineGraphEditorModeText().compare(
                QStringLiteral("Speed"), Qt::CaseInsensitive) == 0;
        const QSignalBlocker blockValue(impl_->toggleValueGraphAction);
        const QSignalBlocker blockSpeed(impl_->toggleVelocityGraphAction);
        impl_->toggleValueGraphAction->setChecked(!speedMode);
        impl_->toggleVelocityGraphAction->setChecked(speedMode);
      }
    });
    const bool speedMode =
        settings->timelineGraphEditorModeText().compare(
            QStringLiteral("Speed"), Qt::CaseInsensitive) == 0;
    const QSignalBlocker blockValue(impl_->toggleValueGraphAction);
    const QSignalBlocker blockSpeed(impl_->toggleVelocityGraphAction);
    impl_->toggleValueGraphAction->setChecked(!speedMode);
    impl_->toggleVelocityGraphAction->setChecked(speedMode);
  }

  impl_->refreshEnabledState();
 }

 ArtifactAnimationMenu::~ArtifactAnimationMenu()
 {
  delete impl_;
 }

 QAction* ArtifactAnimationMenu::getAddKeyframeAction() const
 {
  return impl_->addKeyframeAction;
 }

 QAction* ArtifactAnimationMenu::getRemoveKeyframeAction() const
 {
  return impl_->removeKeyframeAction;
 }

 QAction* ArtifactAnimationMenu::getSelectAllKeyframesAction() const
 {
  return impl_->selectAllKeyframesAction;
 }

 QAction* ArtifactAnimationMenu::getCopyKeyframesAction() const
 {
  return impl_->copyKeyframesAction;
 }

 QAction* ArtifactAnimationMenu::getPasteKeyframesAction() const
 {
  return impl_->pasteKeyframesAction;
 }

} // namespace Artifact
