module;
#include <utility>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <wobjectimpl.h>

module Menu.Animation;
import std;

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

  // ---- グラフエディタアクション ----
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
  auto* service = ArtifactProjectService::instance();
  if (service) {
   QObject::connect(service, &ArtifactProjectService::layerSelected, menu, [this](const ArtifactCore::LayerID& id) {
    selectedLayerId_ = id;
    refreshEnabledState();
   });
   QObject::connect(service, &ArtifactProjectService::layerRemoved, menu, [this](const ArtifactCore::CompositionID&, const ArtifactCore::LayerID& id) {
    if (selectedLayerId_ == id) {
     selectedLayerId_ = {};
    }
    refreshEnabledState();
   });
   QObject::connect(service, &ArtifactProjectService::projectChanged, menu, [this]() {
    refreshEnabledState();
   });
  }
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
  connect(impl_->addKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::addKeyframeRequested);

  impl_->removeKeyframeAction = addAction("キーフレームを削除");
  impl_->removeKeyframeAction->setIcon(menuIcon(QStringLiteral("Material/remove_circle_outline.svg")));
  impl_->removeKeyframeAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_K));
  connect(impl_->removeKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::removeKeyframeRequested);

  impl_->selectAllKeyframesAction = addAction("すべてのキーフレームを選択");
  impl_->selectAllKeyframesAction->setIcon(menuIcon(QStringLiteral("Material/select_all.svg")));
  impl_->selectAllKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_A));
  connect(impl_->selectAllKeyframesAction, &QAction::triggered, this, &ArtifactAnimationMenu::selectAllKeyframesRequested);

  impl_->copyKeyframesAction = addAction("キーフレームをコピー");
  impl_->copyKeyframesAction->setIcon(menuIcon(QStringLiteral("Material/content_copy.svg")));
  impl_->copyKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
  connect(impl_->copyKeyframesAction, &QAction::triggered, this, &ArtifactAnimationMenu::copyKeyframesRequested);

  impl_->pasteKeyframesAction = addAction("キーフレームをペースト");
  impl_->pasteKeyframesAction->setIcon(menuIcon(QStringLiteral("Material/content_paste.svg")));
  impl_->pasteKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
  connect(impl_->pasteKeyframesAction, &QAction::triggered, this, &ArtifactAnimationMenu::pasteKeyframesRequested);

  addSeparator();

  // ---- イージングメニュー ----
  impl_->easingMenu = addMenu("キーフレーム補助(&K)");
  impl_->easingMenu->setIcon(menuIcon(QStringLiteral("Material/tune.svg")));

  impl_->linearAction = impl_->easingMenu->addAction("リニア");
  impl_->linearAction->setIcon(menuIcon(QStringLiteral("Material/straighten.svg")));
  impl_->linearAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L));
  connect(impl_->linearAction, &QAction::triggered, this, &ArtifactAnimationMenu::applyLinearRequested);

  impl_->easeInAction = impl_->easingMenu->addAction("イージーイーズイン");
  impl_->easeInAction->setIcon(menuIcon(QStringLiteral("Material/trending_up.svg")));
  impl_->easeInAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F9));
  connect(impl_->easeInAction, &QAction::triggered, this, &ArtifactAnimationMenu::applyEaseInRequested);

  impl_->easeOutAction = impl_->easingMenu->addAction("イージーイーズアウト");
  impl_->easeOutAction->setIcon(menuIcon(QStringLiteral("Material/trending_down.svg")));
  impl_->easeOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9));
  connect(impl_->easeOutAction, &QAction::triggered, this, &ArtifactAnimationMenu::applyEaseOutRequested);

  impl_->easeInOutAction = impl_->easingMenu->addAction("イージーイーズ");
  impl_->easeInOutAction->setIcon(menuIcon(QStringLiteral("Material/show_chart.svg")));
  impl_->easeInOutAction->setShortcut(QKeySequence(Qt::Key_F9));
  connect(impl_->easeInOutAction, &QAction::triggered, this, &ArtifactAnimationMenu::applyEaseInOutRequested);

  impl_->holdAction = impl_->easingMenu->addAction("キーフレームの停止");
  impl_->holdAction->setIcon(menuIcon(QStringLiteral("Material/pause.svg")));
  impl_->holdAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H));
  connect(impl_->holdAction, &QAction::triggered, this, &ArtifactAnimationMenu::applyHoldRequested);

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

  connect(impl_->linearInterpolationAction, &QAction::triggered, this, [this]() {
   Q_EMIT setInterpolationRequested(KeyframeInterpolation::Linear);
  });
  connect(impl_->bezierInterpolationAction, &QAction::triggered, this, [this]() {
   Q_EMIT setInterpolationRequested(KeyframeInterpolation::Bezier);
  });
  connect(impl_->holdInterpolationAction, &QAction::triggered, this, [this]() {
   Q_EMIT setInterpolationRequested(KeyframeInterpolation::Hold);
  });
  connect(impl_->autoInterpolationAction, &QAction::triggered, this, [this]() {
   Q_EMIT setInterpolationRequested(KeyframeInterpolation::Auto);
  });

  addSeparator();

  // ---- グラフエディタメニュー ----
  impl_->graphEditorMenu = addMenu("グラフエディタ(&G)");
  impl_->graphEditorMenu->setIcon(menuIcon(QStringLiteral("Material/show_chart.svg")));

  impl_->showGraphEditorAction = impl_->graphEditorMenu->addAction("グラフエディタを表示");
  impl_->showGraphEditorAction->setIcon(menuIcon(QStringLiteral("Material/query_stats.svg")));
  impl_->showGraphEditorAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
  connect(impl_->showGraphEditorAction, &QAction::triggered, this, &ArtifactAnimationMenu::showGraphEditorRequested);

  impl_->graphEditorMenu->addSeparator();

  impl_->toggleVelocityGraphAction = impl_->graphEditorMenu->addAction("速度グラフ");
  impl_->toggleVelocityGraphAction->setIcon(menuIcon(QStringLiteral("Material/speed.svg")));
  impl_->toggleVelocityGraphAction->setCheckable(true);
  connect(impl_->toggleVelocityGraphAction, &QAction::triggered, this, &ArtifactAnimationMenu::toggleVelocityGraphRequested);

  impl_->toggleValueGraphAction = impl_->graphEditorMenu->addAction("値グラフ");
  impl_->toggleValueGraphAction->setIcon(menuIcon(QStringLiteral("Material/timeline.svg")));
  impl_->toggleValueGraphAction->setCheckable(true);
  impl_->toggleValueGraphAction->setChecked(true);
  connect(impl_->toggleValueGraphAction, &QAction::triggered, this, &ArtifactAnimationMenu::toggleValueGraphRequested);

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
  connect(impl_->goToNextKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::goToNextKeyframeRequested);

  impl_->goToPreviousKeyframeAction = impl_->navigationMenu->addAction("前のキーフレームに移動");
  impl_->goToPreviousKeyframeAction->setIcon(menuIcon(QStringLiteral("Material/skip_previous.svg")));
  impl_->goToPreviousKeyframeAction->setShortcut(QKeySequence(Qt::Key_J));
  connect(impl_->goToPreviousKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::goToPreviousKeyframeRequested);

  impl_->navigationMenu->addSeparator();

  impl_->goToFirstKeyframeAction = impl_->navigationMenu->addAction("最初のキーフレームに移動");
  impl_->goToFirstKeyframeAction->setIcon(menuIcon(QStringLiteral("Material/first_page.svg")));
  impl_->goToFirstKeyframeAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_J));
  connect(impl_->goToFirstKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::goToFirstKeyframeRequested);

  impl_->goToLastKeyframeAction = impl_->navigationMenu->addAction("最後のキーフレームに移動");
  impl_->goToLastKeyframeAction->setIcon(menuIcon(QStringLiteral("Material/last_page.svg")));
  impl_->goToLastKeyframeAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_K));
  connect(impl_->goToLastKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::goToLastKeyframeRequested);

  addSeparator();

  // ---- タイムリマップメニュー ----
  impl_->timeRemapMenu = addMenu("タイムリマップ(&T)");
  impl_->timeRemapMenu->setIcon(menuIcon(QStringLiteral("Material/schedule.svg")));

  impl_->enableTimeRemapAction = impl_->timeRemapMenu->addAction("タイムリマップ可能にする");
  impl_->enableTimeRemapAction->setIcon(menuIcon(QStringLiteral("Material/schedule.svg")));
  impl_->enableTimeRemapAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T));
  connect(impl_->enableTimeRemapAction, &QAction::triggered, this, &ArtifactAnimationMenu::enableTimeRemapRequested);

  impl_->freezeFrameAction = impl_->timeRemapMenu->addAction("フレームをフリーズ");
  impl_->freezeFrameAction->setIcon(menuIcon(QStringLiteral("Material/pause_circle.svg")));
  impl_->freezeFrameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_J));
  connect(impl_->freezeFrameAction, &QAction::triggered, this, &ArtifactAnimationMenu::freezeFrameRequested);

  impl_->timeReverseAction = impl_->timeRemapMenu->addAction("時間反転レイヤー");
  impl_->timeReverseAction->setIcon(menuIcon(QStringLiteral("Material/swap_horiz.svg")));
  connect(impl_->timeReverseAction, &QAction::triggered, this, &ArtifactAnimationMenu::timeReverseRequested);

  // ---- エクスプレッションメニュー ----
  impl_->expressionMenu = addMenu("エクスプレッション(&E)");
  impl_->expressionMenu->setIcon(menuIcon(QStringLiteral("Material/functions.svg")));

  impl_->addExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを追加");
  impl_->addExpressionAction->setIcon(menuIcon(QStringLiteral("Material/add.svg")));
  impl_->addExpressionAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Equal));
  connect(impl_->addExpressionAction, &QAction::triggered, this, &ArtifactAnimationMenu::addExpressionRequested);

  impl_->editExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを編集...");
  impl_->editExpressionAction->setIcon(menuIcon(QStringLiteral("Material/edit.svg")));
  impl_->editExpressionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Equal));
  connect(impl_->editExpressionAction, &QAction::triggered, this, &ArtifactAnimationMenu::editExpressionRequested);

  impl_->removeExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを削除");
  impl_->removeExpressionAction->setIcon(menuIcon(QStringLiteral("Material/delete.svg")));
  connect(impl_->removeExpressionAction, &QAction::triggered, this, &ArtifactAnimationMenu::removeExpressionRequested);

  impl_->expressionMenu->addSeparator();

  impl_->convertToKeyframesAction = impl_->expressionMenu->addAction("エクスプレッションをキーフレームに変換...");
  impl_->convertToKeyframesAction->setIcon(menuIcon(QStringLiteral("Material/animation.svg")));
  connect(impl_->convertToKeyframesAction, &QAction::triggered, this, &ArtifactAnimationMenu::convertToKeyframesRequested);

  addSeparator();

  // ---- プリセットメニュー ----
  impl_->presetMenu = addMenu("アニメーションプリセット(&P)");
  impl_->presetMenu->setIcon(menuIcon(QStringLiteral("Material/bookmarks.svg")));

  impl_->saveAnimationPresetAction = impl_->presetMenu->addAction("アニメーションプリセットを保存...");
  impl_->saveAnimationPresetAction->setIcon(menuIcon(QStringLiteral("Material/save.svg")));
  connect(impl_->saveAnimationPresetAction, &QAction::triggered, this, &ArtifactAnimationMenu::saveAnimationPresetRequested);

  impl_->loadAnimationPresetAction = impl_->presetMenu->addAction("アニメーションプリセットを適用...");
  impl_->loadAnimationPresetAction->setIcon(menuIcon(QStringLiteral("Material/folder_open.svg")));
  connect(impl_->loadAnimationPresetAction, &QAction::triggered, this, &ArtifactAnimationMenu::loadAnimationPresetRequested);

  impl_->presetMenu->addSeparator();

  impl_->browsePresetsAction = impl_->presetMenu->addAction("プリセットを参照...");
  impl_->browsePresetsAction->setIcon(menuIcon(QStringLiteral("Material/folder.svg")));

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
