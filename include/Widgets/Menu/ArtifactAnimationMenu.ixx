module;

#include <QMenu>
#include <wobjectdefs.h>

export module Menu.Animation;

import std;

export namespace Artifact {

 // イージングタイプ（After Effects風）
 enum class EasingType {
  Linear,
  EaseIn,
  EaseOut,
  EaseInOut,
  Hold,
  Custom
 };

 // キーフレーム補間タイプ
 enum class KeyframeInterpolation {
  Linear,
  Bezier,
  Hold,
  Auto
 };

 class ArtifactAnimationMenu : public QMenu {
  W_OBJECT(ArtifactAnimationMenu)
 private:
  class Impl;
  Impl* impl_;

 public:
  explicit ArtifactAnimationMenu(QWidget* parent = nullptr);
  ~ArtifactAnimationMenu();

  // ---- メニューアクション取得 ----
  
  QAction* getAddKeyframeAction() const;
  QAction* getRemoveKeyframeAction() const;
  QAction* getSelectAllKeyframesAction() const;
  QAction* getCopyKeyframesAction() const;
  QAction* getPasteKeyframesAction() const;

  // ---- シグナル ----

  // キーフレーム操作
  void addKeyframeRequested() W_SIGNAL(addKeyframeRequested);
  void removeKeyframeRequested() W_SIGNAL(removeKeyframeRequested);
  void selectAllKeyframesRequested() W_SIGNAL(selectAllKeyframesRequested);
  void copyKeyframesRequested() W_SIGNAL(copyKeyframesRequested);
  void pasteKeyframesRequested() W_SIGNAL(pasteKeyframesRequested);

  // イージング適用
  void applyEasingRequested(EasingType type) W_SIGNAL(applyEasingRequested, type);
  void applyLinearRequested() W_SIGNAL(applyLinearRequested);
  void applyEaseInRequested() W_SIGNAL(applyEaseInRequested);
  void applyEaseOutRequested() W_SIGNAL(applyEaseOutRequested);
  void applyEaseInOutRequested() W_SIGNAL(applyEaseInOutRequested);
  void applyHoldRequested() W_SIGNAL(applyHoldRequested);

  // キーフレーム補間
  void setInterpolationRequested(KeyframeInterpolation type) W_SIGNAL(setInterpolationRequested, type);

  // グラフエディタ
  void toggleVelocityGraphRequested() W_SIGNAL(toggleVelocityGraphRequested);
  void toggleValueGraphRequested() W_SIGNAL(toggleValueGraphRequested);
  void showGraphEditorRequested() W_SIGNAL(showGraphEditorRequested);

  // キーフレームナビゲーション
  void goToNextKeyframeRequested() W_SIGNAL(goToNextKeyframeRequested);
  void goToPreviousKeyframeRequested() W_SIGNAL(goToPreviousKeyframeRequested);
  void goToFirstKeyframeRequested() W_SIGNAL(goToFirstKeyframeRequested);
  void goToLastKeyframeRequested() W_SIGNAL(goToLastKeyframeRequested);

  // タイムリマップ
  void enableTimeRemapRequested() W_SIGNAL(enableTimeRemapRequested);
  void freezeFrameRequested() W_SIGNAL(freezeFrameRequested);
  void timeReverseRequested() W_SIGNAL(timeReverseRequested);

  // エクスプレッション
  void addExpressionRequested() W_SIGNAL(addExpressionRequested);
  void editExpressionRequested() W_SIGNAL(editExpressionRequested);
  void removeExpressionRequested() W_SIGNAL(removeExpressionRequested);
  void convertToKeyframesRequested() W_SIGNAL(convertToKeyframesRequested);

  // プリセット
  void saveAnimationPresetRequested() W_SIGNAL(saveAnimationPresetRequested);
  void loadAnimationPresetRequested() W_SIGNAL(loadAnimationPresetRequested);
 };

} // namespace Artifact
