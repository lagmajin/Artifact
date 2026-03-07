module ;
#include <QSize>
#include <QString>
#include "../ArtifactCore/include/Define/DllExportMacro.hpp"


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
export module Composition.Settings;






import Color.Float;
import Core.AspectRatio;
import Utils.String.UniString;


export namespace ArtifactCore {


 class LIBRARY_DLL_API  CompositionSettings {
 private:
  class Impl;
  Impl* impl_;
 public:
  CompositionSettings();
  CompositionSettings(const CompositionSettings& settings);
  ~CompositionSettings();
  UniString compositionName() const;
  void setCompositionName(const UniString& string);
  QSize compositionSize() const;
  void setCompositionSize(const QSize& size);


 };







};