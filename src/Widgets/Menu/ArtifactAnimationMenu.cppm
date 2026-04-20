module;
#include <utility>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <wobjectimpl.h>

module Menu.Animation;
import std;

import Event.Bus;
import Artifact.Event.Types;
import Artifact.Service.Project;
import Utils.Id;
import Utils.Path;

W_OBJECT_IMPL(Artifact::ArtifactAnimationMenu)

namespace Artifact {

namespace {
QIcon menuIcon(const QString& path)
{
  return QIcon(resolveIconPath(path));
}
}

 class ArtifactAnimationMenu::Impl {
 public:
  Impl(ArtifactAnimationMenu* menu);
  ~Impl();

  ArtifactAnimationMenu* menu_ = nullptr;
  ArtifactCore::LayerID selectedLayerId_;
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  // ---- キーフレーム操作アクション ----
  QAction* addKeyframeAction = nullptr;
  QAction* removeKeyframeAction = nullptr;
  QAction* selectAllKeyframesAction = nullptr;
  QAction* copyKeyframesAction = nullptr;
  QAction* pasteKeyframesAction = nullptr;
  QAction* reverseKeyframesAction = nullptr;

  // ---- イージングアクション ----
  QAction* linearAction = nullptr;
  QAction* easeInAction = nullptr;
  QAction* easeOutAction = nullptr;
  QAction* easeInOutAction = nullptr;
  QAction* holdAction = nullptr;
  QAction* customEasingAction = nullptr;

  // ---- 補間アクション ----
  QAction* linearInterpolationAction = nullptr;
  QAction* bezierInterpolationAction = nullptr;
  QAction* holdInterpolationAction = nullptr;
  QAction* autoInterpolationAction = nullptr;

  // ---- カーブエディタアクション ----
  QAction* toggleVelocityGraphAction = nullptr;
  QAction* toggleValueGraphAction = nullptr;
  QAction* showGraphEditorAction = nullptr;
  QAction* smoothGraphAction = nullptr;
  QAction* flattenGraphAction = nullptr;

  // ---- ナビゲーションアクション ----
  QAction* goToNextKeyframeAction = nullptr;
  QAction* goToPreviousKeyframeAction = nullptr;
  QAction* goToFirstKeyframeAction = nullptr;
  QAction* goToLastKeyframeAction = nullptr;

  // ---- タイムリマップアクション ----
  QAction* enableTimeRemapAction = nullptr;
  QAction* freezeFrameAction = nullptr;
  QAction* timeReverseAction = nullptr;

  // ---- エクスプレッションアクション ----
  QAction* addExpressionAction = nullptr;
  QAction* editExpressionAction = nullptr;
  QAction* removeExpressionAction = nullptr;
  QAction* convertToKeyframesAction = nullptr;

  // ---- プリセットアクション ----
  QAction* saveAnimationPresetAction = nullptr;
  QAction* loadAnimationPresetAction = nullptr;
  QAction* browsePresetsAction = nullptr;

  // ---- メニュー ----
  QMenu* easingMenu = nullptr;
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
  if (reverseKeyframesAction) reverseKeyframesAction->setEnabled(hasLayer);

  easingMenu->setEnabled(hasLayer);
  interpolationMenu->setEnabled(hasLayer);
  graphEditorMenu->setEnabled(hasLayer);
  navigationMenu->setEnabled(hasLayer);
  timeRemapMenu->setEnabled(hasLayer);
  expressionMenu->setEnabled(hasLayer);
  presetMenu->setEnabled(hasLayer);
 }

 ArtifactAnimationMenu::ArtifactAnimationMenu(QWidget* parent)
  : QMenu(parent), impl_(new Impl(this))
 {
  setTitle("アニメーション(&A)");
  setTearOffEnabled(false);

  // ---- キーフレーム操作 ----
 impl_->addKeyframeAction = addAction("キーフレームを追加");
  impl_->addKeyframeAction->setIcon(menuIcon(QStringLiteral("Material/add_circle_outline.svg")));
  impl_->addKeyframeAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_K));
  impl_->removeKeyframeAction = addAction("キーフレームを削除");
  impl_->removeKeyframeAction->setIcon(menuIcon(QStringLiteral("Material/remove_circle_outline.svg")));
  impl_->removeKeyframeAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_K));

  impl_->selectAllKeyframesAction = addAction("すべてのキーフレームを選択");
  impl_->selectAllKeyframesAction->setIcon(menuIcon(QStringLiteral("Material/select_all.svg")));
  impl_->selectAllKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_A));

  impl_->copyKeyframesAction = addAction("キーフレームをコピー");
  impl_->copyKeyframesAction->setIcon(menuIcon(QStringLiteral("Material/content_copy.svg")));
  impl_->copyKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));

  impl_->pasteKeyframesAction = addAction("キーフレームをペースト");
  impl_->pasteKeyframesAction->setIcon(menuIcon(QStringLiteral("Material/content_paste.svg")));
  impl_->pasteKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));

  addSeparator();

  // ---- イージングメニュー ----
  impl_->easingMenu = addMenu("キーフレーム補助(&K)");
  impl_->easingMenu->setIcon(menuIcon(QStringLiteral("Material/tune.svg")));

  impl_->linearAction = impl_->easingMenu->addAction("リニア");
  impl_->linearAction->setIcon(menuIcon(QStringLiteral("Material/straighten.svg")));
  impl_->linearAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L));

  impl_->easeInAction = impl_->easingMenu->addAction("イージーイーズイン");
  impl_->easeInAction->setIcon(menuIcon(QStringLiteral("Material/trending_up.svg")));
  impl_->easeInAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F9));

  impl_->easeOutAction = impl_->easingMenu->addAction("イージーイーズアウト");
  impl_->easeOutAction->setIcon(menuIcon(QStringLiteral("Material/trending_down.svg")));
  impl_->easeOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9));

  impl_->easeInOutAction = impl_->easingMenu->addAction("イージーイーズ");
  impl_->easeInOutAction->setIcon(menuIcon(QStringLiteral("Material/show_chart.svg")));
  impl_->easeInOutAction->setShortcut(QKeySequence(Qt::Key_F9));

  impl_->holdAction = impl_->easingMenu->addAction("キーフレームの停止");
  impl_->holdAction->setIcon(menuIcon(QStringLiteral("Material/pause.svg")));
  impl_->holdAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H));

  impl_->easingMenu->addSeparator();
  impl_->customEasingAction = impl_->easingMenu->addAction("カスタムイージング...");
  impl_->customEasingAction->setIcon(menuIcon(QStringLiteral("Material/edit.svg")));

  // ---- 補間メニュー ----
  impl_->interpolationMenu = addMenu("キーフレーム補間(&I)");
  impl_->interpolationMenu->setIcon(menuIcon(QStringLiteral("Material/timeline.svg")));

  impl_->linearInterpolationAction = impl_->interpolationMenu->addAction("リニア");
  impl_->linearInterpolationAction->setIcon(menuIcon(QStringLiteral("Material/straighten.svg")));
  impl_->bezierInterpolationAction = impl_->interpolationMenu->addAction("ベジェ");
  impl_->bezierInterpolationAction->setIcon(menuIcon(QStringLiteral("Material/edit.svg")));
  impl_->holdInterpolationAction = impl_->interpolationMenu->addAction("停止");
  impl_->holdInterpolationAction->setIcon(menuIcon(QStringLiteral("Material/pause.svg")));
  impl_->autoInterpolationAction = impl_->interpolationMenu->addAction("オートベジェ");
  impl_->autoInterpolationAction->setIcon(menuIcon(QStringLiteral("Material/auto_fix_high.svg")));

  impl_->linearInterpolationAction->setCheckable(true);
  impl_->bezierInterpolationAction->setCheckable(true);
  impl_->holdInterpolationAction->setCheckable(true);
  impl_->autoInterpolationAction->setCheckable(true);

  addSeparator();

  // ---- カーブエディタメニュー ----
  impl_->graphEditorMenu = addMenu("カーブエディタ(&G)");
  impl_->graphEditorMenu->setIcon(menuIcon(QStringLiteral("Material/show_chart.svg")));

  impl_->showGraphEditorAction = impl_->graphEditorMenu->addAction("カーブエディタを表示");
  impl_->showGraphEditorAction->setIcon(menuIcon(QStringLiteral("Material/query_stats.svg")));
  impl_->showGraphEditorAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
  impl_->graphEditorMenu->addSeparator();

  impl_->toggleVelocityGraphAction = impl_->graphEditorMenu->addAction("速度グラフ");
  impl_->toggleVelocityGraphAction->setIcon(menuIcon(QStringLiteral("Material/speed.svg")));
  impl_->toggleVelocityGraphAction->setCheckable(true);
  impl_->toggleValueGraphAction = impl_->graphEditorMenu->addAction("値グラフ");
  impl_->toggleValueGraphAction->setIcon(menuIcon(QStringLiteral("Material/timeline.svg")));
  impl_->toggleValueGraphAction->setCheckable(true);
  impl_->toggleValueGraphAction->setChecked(true);

  impl_->graphEditorMenu->addSeparator();

  impl_->smoothGraphAction = impl_->graphEditorMenu->addAction("選択したキーフレームをスムーズに");
  impl_->smoothGraphAction->setIcon(menuIcon(QStringLiteral("Material/smoothing.svg")));
  impl_->flattenGraphAction = impl_->graphEditorMenu->addAction("ハンドルをフラットに");
  impl_->flattenGraphAction->setIcon(menuIcon(QStringLiteral("Material/straighten.svg")));

  // ---- ナビゲーションメニュー ----
  impl_->navigationMenu = addMenu("ナビゲーション(&N)");
  impl_->navigationMenu->setIcon(menuIcon(QStringLiteral("Material/skip_next.svg")));

  impl_->goToNextKeyframeAction = impl_->navigationMenu->addAction("次のキーフレームに移動");
  impl_->goToNextKeyframeAction->setIcon(menuIcon(QStringLiteral("Material/skip_next.svg")));
  impl_->goToNextKeyframeAction->setShortcut(QKeySequence(Qt::Key_K));
  impl_->goToPreviousKeyframeAction = impl_->navigationMenu->addAction("前のキーフレームに移動");
  impl_->goToPreviousKeyframeAction->setIcon(menuIcon(QStringLiteral("Material/skip_previous.svg")));
  impl_->goToPreviousKeyframeAction->setShortcut(QKeySequence(Qt::Key_J));

  impl_->navigationMenu->addSeparator();

  impl_->goToFirstKeyframeAction = impl_->navigationMenu->addAction("最初のキーフレームに移動");
  impl_->goToFirstKeyframeAction->setIcon(menuIcon(QStringLiteral("Material/first_page.svg")));
  impl_->goToFirstKeyframeAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_J));
  impl_->goToLastKeyframeAction = impl_->navigationMenu->addAction("最後のキーフレームに移動");
  impl_->goToLastKeyframeAction->setIcon(menuIcon(QStringLiteral("Material/last_page.svg")));
  impl_->goToLastKeyframeAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_K));

  addSeparator();

  // ---- タイムリマップメニュー ----
  impl_->timeRemapMenu = addMenu("タイムリマップ(&T)");
  impl_->timeRemapMenu->setIcon(menuIcon(QStringLiteral("Material/schedule.svg")));

  impl_->enableTimeRemapAction = impl_->timeRemapMenu->addAction("タイムリマップ可能にする");
  impl_->enableTimeRemapAction->setIcon(menuIcon(QStringLiteral("Material/schedule.svg")));
  impl_->enableTimeRemapAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T));
  impl_->freezeFrameAction = impl_->timeRemapMenu->addAction("フレームをフリーズ");
  impl_->freezeFrameAction->setIcon(menuIcon(QStringLiteral("Material/pause_circle.svg")));
  impl_->freezeFrameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_J));
  impl_->timeReverseAction = impl_->timeRemapMenu->addAction("時間反転レイヤー");
  impl_->timeReverseAction->setIcon(menuIcon(QStringLiteral("Material/swap_horiz.svg")));

  // ---- エクスプレッションメニュー ----
  impl_->expressionMenu = addMenu("エクスプレッション(&E)");
  impl_->expressionMenu->setIcon(menuIcon(QStringLiteral("Material/functions.svg")));

  impl_->addExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを追加");
  impl_->addExpressionAction->setIcon(menuIcon(QStringLiteral("Material/add.svg")));
  impl_->addExpressionAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Equal));
  impl_->editExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを編集...");
  impl_->editExpressionAction->setIcon(menuIcon(QStringLiteral("Material/edit.svg")));
  impl_->editExpressionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Equal));
  impl_->removeExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを削除");
  impl_->removeExpressionAction->setIcon(menuIcon(QStringLiteral("Material/delete.svg")));

  impl_->expressionMenu->addSeparator();

  impl_->convertToKeyframesAction = impl_->expressionMenu->addAction("エクスプレッションをキーフレームに変換...");
  impl_->convertToKeyframesAction->setIcon(menuIcon(QStringLiteral("Material/animation.svg")));
  addSeparator();

  // ---- プリセットメニュー ----
  impl_->presetMenu = addMenu("アニメーションプリセット(&P)");
  impl_->presetMenu->setIcon(menuIcon(QStringLiteral("Material/bookmarks.svg")));

  impl_->saveAnimationPresetAction = impl_->presetMenu->addAction("アニメーションプリセットを保存...");
  impl_->saveAnimationPresetAction->setIcon(menuIcon(QStringLiteral("Material/save.svg")));
  impl_->loadAnimationPresetAction = impl_->presetMenu->addAction("アニメーションプリセットを適用...");
  impl_->loadAnimationPresetAction->setIcon(menuIcon(QStringLiteral("Material/folder_open.svg")));
  impl_->presetMenu->addSeparator();

  impl_->browsePresetsAction = impl_->presetMenu->addAction("プリセットを参照...");
  impl_->browsePresetsAction->setIcon(menuIcon(QStringLiteral("Material/folder.svg")));

  auto dispatchAction = [this](QAction* action) {
   if (!action) {
    return;
   }
   if (action == impl_->addKeyframeAction) { Q_EMIT addKeyframeRequested(); return; }
   if (action == impl_->removeKeyframeAction) { Q_EMIT removeKeyframeRequested(); return; }
   if (action == impl_->selectAllKeyframesAction) { Q_EMIT selectAllKeyframesRequested(); return; }
   if (action == impl_->copyKeyframesAction) { Q_EMIT copyKeyframesRequested(); return; }
   if (action == impl_->pasteKeyframesAction) { Q_EMIT pasteKeyframesRequested(); return; }
   if (action == impl_->linearAction) { Q_EMIT applyLinearRequested(); return; }
   if (action == impl_->easeInAction) { Q_EMIT applyEaseInRequested(); return; }
   if (action == impl_->easeOutAction) { Q_EMIT applyEaseOutRequested(); return; }
   if (action == impl_->easeInOutAction) { Q_EMIT applyEaseInOutRequested(); return; }
   if (action == impl_->holdAction) { Q_EMIT applyHoldRequested(); return; }
   if (action == impl_->customEasingAction) { Q_EMIT applyEasingRequested(EasingType::Custom); return; }
   if (action == impl_->linearInterpolationAction) { Q_EMIT setInterpolationRequested(KeyframeInterpolation::Linear); return; }
   if (action == impl_->bezierInterpolationAction) { Q_EMIT setInterpolationRequested(KeyframeInterpolation::Bezier); return; }
   if (action == impl_->holdInterpolationAction) { Q_EMIT setInterpolationRequested(KeyframeInterpolation::Hold); return; }
   if (action == impl_->autoInterpolationAction) { Q_EMIT setInterpolationRequested(KeyframeInterpolation::Auto); return; }
   if (action == impl_->showGraphEditorAction) { Q_EMIT showGraphEditorRequested(); return; }
   if (action == impl_->toggleVelocityGraphAction) { Q_EMIT toggleVelocityGraphRequested(); return; }
   if (action == impl_->toggleValueGraphAction) { Q_EMIT toggleValueGraphRequested(); return; }
   if (action == impl_->smoothGraphAction) { return; }
   if (action == impl_->flattenGraphAction) { return; }
   if (action == impl_->goToNextKeyframeAction) { Q_EMIT goToNextKeyframeRequested(); return; }
   if (action == impl_->goToPreviousKeyframeAction) { Q_EMIT goToPreviousKeyframeRequested(); return; }
   if (action == impl_->goToFirstKeyframeAction) { Q_EMIT goToFirstKeyframeRequested(); return; }
   if (action == impl_->goToLastKeyframeAction) { Q_EMIT goToLastKeyframeRequested(); return; }
   if (action == impl_->enableTimeRemapAction) { Q_EMIT enableTimeRemapRequested(); return; }
   if (action == impl_->freezeFrameAction) { Q_EMIT freezeFrameRequested(); return; }
   if (action == impl_->timeReverseAction) { Q_EMIT timeReverseRequested(); return; }
   if (action == impl_->addExpressionAction) { Q_EMIT addExpressionRequested(); return; }
   if (action == impl_->editExpressionAction) { Q_EMIT editExpressionRequested(); return; }
   if (action == impl_->removeExpressionAction) { Q_EMIT removeExpressionRequested(); return; }
   if (action == impl_->convertToKeyframesAction) { Q_EMIT convertToKeyframesRequested(); return; }
   if (action == impl_->saveAnimationPresetAction) { Q_EMIT saveAnimationPresetRequested(); return; }
   if (action == impl_->loadAnimationPresetAction) { Q_EMIT loadAnimationPresetRequested(); return; }
   if (action == impl_->browsePresetsAction) { return; }
  };

  QObject::connect(this, &QMenu::triggered, this, dispatchAction);
  QObject::connect(impl_->easingMenu, &QMenu::triggered, this, dispatchAction);
  QObject::connect(impl_->interpolationMenu, &QMenu::triggered, this, dispatchAction);
  QObject::connect(impl_->graphEditorMenu, &QMenu::triggered, this, dispatchAction);
  QObject::connect(impl_->navigationMenu, &QMenu::triggered, this, dispatchAction);
  QObject::connect(impl_->timeRemapMenu, &QMenu::triggered, this, dispatchAction);
  QObject::connect(impl_->expressionMenu, &QMenu::triggered, this, dispatchAction);
  QObject::connect(impl_->presetMenu, &QMenu::triggered, this, dispatchAction);

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
