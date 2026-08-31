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

  };

} // namespace Artifact
