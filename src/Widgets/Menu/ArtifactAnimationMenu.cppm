module;
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <wobjectimpl.h>
#include <qcoro6/qcoro/qcorotask.h>

module Menu.Animation;

W_OBJECT_IMPL(Artifact::ArtifactAnimationMenu)

namespace Artifact {

 class ArtifactAnimationMenu::Impl {
 public:
  Impl();
  ~Impl();

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
 };

 ArtifactAnimationMenu::Impl::Impl()
 {
 }

 ArtifactAnimationMenu::Impl::~Impl()
 {
 }

 ArtifactAnimationMenu::ArtifactAnimationMenu(QWidget* parent)
  : QMenu(parent), impl_(new Impl())
 {
  setTitle(u8"Animation");
  setTearOffEnabled(true);

  // ---- キーフレーム操作 ----
  impl_->addKeyframeAction = addAction(u8"Add Keyframe");
  impl_->addKeyframeAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_K));
  connect(impl_->addKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::addKeyframeRequested);

  impl_->removeKeyframeAction = addAction(u8"Remove Keyframe");
  impl_->removeKeyframeAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_K));
  connect(impl_->removeKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::removeKeyframeRequested);

  impl_->selectAllKeyframesAction = addAction(u8"Select All Keyframes");
  impl_->selectAllKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_A));
  connect(impl_->selectAllKeyframesAction, &QAction::triggered, this, &ArtifactAnimationMenu::selectAllKeyframesRequested);

  impl_->copyKeyframesAction = addAction(u8"Copy Keyframes");
  impl_->copyKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
  connect(impl_->copyKeyframesAction, &QAction::triggered, this, &ArtifactAnimationMenu::copyKeyframesRequested);

  impl_->pasteKeyframesAction = addAction(u8"Paste Keyframes");
  impl_->pasteKeyframesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
  connect(impl_->pasteKeyframesAction, &QAction::triggered, this, &ArtifactAnimationMenu::pasteKeyframesRequested);

  addSeparator();

  // ---- イージングメニュー ----
  impl_->easingMenu = addMenu(u8"Keyframe Easing");

  impl_->linearAction = impl_->easingMenu->addAction(u8"Linear");
  impl_->linearAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L));
  connect(impl_->linearAction, &QAction::triggered, this, &ArtifactAnimationMenu::applyLinearRequested);

  impl_->easeInAction = impl_->easingMenu->addAction(u8"Easy Ease In");
  impl_->easeInAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F9));
  connect(impl_->easeInAction, &QAction::triggered, this, &ArtifactAnimationMenu::applyEaseInRequested);

  impl_->easeOutAction = impl_->easingMenu->addAction(u8"Easy Ease Out");
  impl_->easeOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9));
  connect(impl_->easeOutAction, &QAction::triggered, this, &ArtifactAnimationMenu::applyEaseOutRequested);

  impl_->easeInOutAction = impl_->easingMenu->addAction(u8"Easy Ease (In/Out)");
  impl_->easeInOutAction->setShortcut(QKeySequence(Qt::Key_F9));
  connect(impl_->easeInOutAction, &QAction::triggered, this, &ArtifactAnimationMenu::applyEaseInOutRequested);

  impl_->holdAction = impl_->easingMenu->addAction(u8"Hold Keyframe");
  impl_->holdAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H));
  connect(impl_->holdAction, &QAction::triggered, this, &ArtifactAnimationMenu::applyHoldRequested);

  impl_->easingMenu->addSeparator();
  impl_->customEasingAction = impl_->easingMenu->addAction(u8"Custom Easing...");

  // ---- 補間メニュー ----
  impl_->interpolationMenu = addMenu(u8"Keyframe Interpolation");

  impl_->linearInterpolationAction = impl_->interpolationMenu->addAction(u8"Linear");
  impl_->bezierInterpolationAction = impl_->interpolationMenu->addAction(u8"Bezier");
  impl_->holdInterpolationAction = impl_->interpolationMenu->addAction(u8"Hold");
  impl_->autoInterpolationAction = impl_->interpolationMenu->addAction(u8"Auto Bezier");

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
  impl_->graphEditorMenu = addMenu(u8"Graph Editor");

  impl_->showGraphEditorAction = impl_->graphEditorMenu->addAction(u8"Show Graph Editor");
  impl_->showGraphEditorAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
  connect(impl_->showGraphEditorAction, &QAction::triggered, this, &ArtifactAnimationMenu::showGraphEditorRequested);

  impl_->graphEditorMenu->addSeparator();

  impl_->toggleVelocityGraphAction = impl_->graphEditorMenu->addAction(u8"Toggle Velocity Graph");
  impl_->toggleVelocityGraphAction->setCheckable(true);
  connect(impl_->toggleVelocityGraphAction, &QAction::triggered, this, &ArtifactAnimationMenu::toggleVelocityGraphRequested);

  impl_->toggleValueGraphAction = impl_->graphEditorMenu->addAction(u8"Toggle Value Graph");
  impl_->toggleValueGraphAction->setCheckable(true);
  impl_->toggleValueGraphAction->setChecked(true);
  connect(impl_->toggleValueGraphAction, &QAction::triggered, this, &ArtifactAnimationMenu::toggleValueGraphRequested);

  impl_->graphEditorMenu->addSeparator();

  impl_->smoothGraphAction = impl_->graphEditorMenu->addAction(u8"Smooth Selected Keyframes");
  impl_->flattenGraphAction = impl_->graphEditorMenu->addAction(u8"Flatten Handles");

  // ---- ナビゲーションメニュー ----
  impl_->navigationMenu = addMenu(u8"Keyframe Navigation");

  impl_->goToNextKeyframeAction = impl_->navigationMenu->addAction(u8"Go to Next Keyframe");
  impl_->goToNextKeyframeAction->setShortcut(QKeySequence(Qt::Key_K));
  connect(impl_->goToNextKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::goToNextKeyframeRequested);

  impl_->goToPreviousKeyframeAction = impl_->navigationMenu->addAction(u8"Go to Previous Keyframe");
  impl_->goToPreviousKeyframeAction->setShortcut(QKeySequence(Qt::Key_J));
  connect(impl_->goToPreviousKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::goToPreviousKeyframeRequested);

  impl_->navigationMenu->addSeparator();

  impl_->goToFirstKeyframeAction = impl_->navigationMenu->addAction(u8"Go to First Keyframe");
  impl_->goToFirstKeyframeAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_J));
  connect(impl_->goToFirstKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::goToFirstKeyframeRequested);

  impl_->goToLastKeyframeAction = impl_->navigationMenu->addAction(u8"Go to Last Keyframe");
  impl_->goToLastKeyframeAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_K));
  connect(impl_->goToLastKeyframeAction, &QAction::triggered, this, &ArtifactAnimationMenu::goToLastKeyframeRequested);

  addSeparator();

  // ---- タイムリマップメニュー ----
  impl_->timeRemapMenu = addMenu(u8"Time Remap");

  impl_->enableTimeRemapAction = impl_->timeRemapMenu->addAction(u8"Enable Time Remapping");
  impl_->enableTimeRemapAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T));
  connect(impl_->enableTimeRemapAction, &QAction::triggered, this, &ArtifactAnimationMenu::enableTimeRemapRequested);

  impl_->freezeFrameAction = impl_->timeRemapMenu->addAction(u8"Freeze Frame");
  impl_->freezeFrameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_J));
  connect(impl_->freezeFrameAction, &QAction::triggered, this, &ArtifactAnimationMenu::freezeFrameRequested);

  impl_->timeReverseAction = impl_->timeRemapMenu->addAction(u8"Time-Reverse Layer");
  connect(impl_->timeReverseAction, &QAction::triggered, this, &ArtifactAnimationMenu::timeReverseRequested);

  // ---- エクスプレッションメニュー ----
  impl_->expressionMenu = addMenu(u8"Expression");

  impl_->addExpressionAction = impl_->expressionMenu->addAction(u8"Add Expression");
  impl_->addExpressionAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Equal));
  connect(impl_->addExpressionAction, &QAction::triggered, this, &ArtifactAnimationMenu::addExpressionRequested);

  impl_->editExpressionAction = impl_->expressionMenu->addAction(u8"Edit Expression...");
  impl_->editExpressionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Equal));
  connect(impl_->editExpressionAction, &QAction::triggered, this, &ArtifactAnimationMenu::editExpressionRequested);

  impl_->removeExpressionAction = impl_->expressionMenu->addAction(u8"Remove Expression");
  connect(impl_->removeExpressionAction, &QAction::triggered, this, &ArtifactAnimationMenu::removeExpressionRequested);

  impl_->expressionMenu->addSeparator();

  impl_->convertToKeyframesAction = impl_->expressionMenu->addAction(u8"Convert Expression to Keyframes...");
  connect(impl_->convertToKeyframesAction, &QAction::triggered, this, &ArtifactAnimationMenu::convertToKeyframesRequested);

  addSeparator();

  // ---- プリセットメニュー ----
  impl_->presetMenu = addMenu(u8"Animation Presets");

  impl_->saveAnimationPresetAction = impl_->presetMenu->addAction(u8"Save Animation Preset...");
  connect(impl_->saveAnimationPresetAction, &QAction::triggered, this, &ArtifactAnimationMenu::saveAnimationPresetRequested);

  impl_->loadAnimationPresetAction = impl_->presetMenu->addAction(u8"Apply Animation Preset...");
  connect(impl_->loadAnimationPresetAction, &QAction::triggered, this, &ArtifactAnimationMenu::loadAnimationPresetRequested);

  impl_->presetMenu->addSeparator();

  impl_->browsePresetsAction = impl_->presetMenu->addAction(u8"Browse Presets...");
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