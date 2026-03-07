module;
#include <QString>

export module Artifact.Effects.Manager;

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



import Utils.Id;
import Utils.String.UniString;

import Artifact.Effect.Abstract;

export namespace Artifact
{
 using namespace ArtifactCore;

 using Creator = std::function<std::unique_ptr<ArtifactAbstractEffect>()>;
struct EffectFactoryResult {
  
};



 struct EffectFactory {
  EffectID id;
  UniString displayName;
   
 };

 class ArtifactGlobalEffectManager{
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactGlobalEffectManager();
  ~ArtifactGlobalEffectManager();
  void loadPlugin() noexcept;
  void unloadAllPlugins() noexcept;
  void factoryByID(const EffectID& id);
   
  static ArtifactGlobalEffectManager* effectManager();
 };





};