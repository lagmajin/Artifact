module;
#include <QString>
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
export module Artifact.Composition.Setting;



import Color.Float;
import Core.AspectRatio;
import Utils.String.UniString;

export namespace Artifact {

 class ArtifactCompositionSetting {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactCompositionSetting();
  ~ArtifactCompositionSetting();
 };



};