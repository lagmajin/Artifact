module;

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <wobjectdefs.h>
#include <QMenu>
export module Menu.Animation;

import Math.Interpolate;

export namespace Artifact {

  class ArtifactAnimationMenu : public QMenu {
   W_OBJECT(ArtifactAnimationMenu)
  private:
   class Impl;
   Impl* impl_;

  public:
   explicit ArtifactAnimationMenu(QWidget* parent = nullptr);
   ~ArtifactAnimationMenu();

   QAction* getAddKeyframeAction() const;
   QAction* getRemoveKeyframeAction() const;
   QAction* getSelectAllKeyframesAction() const;
   QAction* getCopyKeyframesAction() const;
   QAction* getPasteKeyframesAction() const;

   void addKeyframeRequested() W_SIGNAL(addKeyframeRequested);
   void removeKeyframeRequested() W_SIGNAL(removeKeyframeRequested);
   void selectAllKeyframesRequested() W_SIGNAL(selectAllKeyframesRequested);
   void copyKeyframesRequested() W_SIGNAL(copyKeyframesRequested);
   void pasteKeyframesRequested() W_SIGNAL(pasteKeyframesRequested);

   void applyInterpolationRequested(ArtifactCore::InterpolationType type) W_SIGNAL(applyInterpolationRequested, type);

   void toggleVelocityGraphRequested() W_SIGNAL(toggleVelocityGraphRequested);
   void toggleValueGraphRequested() W_SIGNAL(toggleValueGraphRequested);
   void showGraphEditorRequested() W_SIGNAL(showGraphEditorRequested);

   void goToNextKeyframeRequested() W_SIGNAL(goToNextKeyframeRequested);
   void goToPreviousKeyframeRequested() W_SIGNAL(goToPreviousKeyframeRequested);
   void goToFirstKeyframeRequested() W_SIGNAL(goToFirstKeyframeRequested);
   void goToLastKeyframeRequested() W_SIGNAL(goToLastKeyframeRequested);

   void enableTimeRemapRequested() W_SIGNAL(enableTimeRemapRequested);
   void freezeFrameRequested() W_SIGNAL(freezeFrameRequested);
   void timeReverseRequested() W_SIGNAL(timeReverseRequested);

   void addExpressionRequested() W_SIGNAL(addExpressionRequested);
   void editExpressionRequested() W_SIGNAL(editExpressionRequested);
   void removeExpressionRequested() W_SIGNAL(removeExpressionRequested);
   void convertToKeyframesRequested() W_SIGNAL(convertToKeyframesRequested);

   void saveAnimationPresetRequested() W_SIGNAL(saveAnimationPresetRequested);
   void loadAnimationPresetRequested() W_SIGNAL(loadAnimationPresetRequested);
  };

} // namespace Artifact

W_REGISTER_ARGTYPE(ArtifactCore::InterpolationType)
