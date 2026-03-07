module;
#include <QHash>
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
module Artifact.Effects.Manager;




import Container.MultiIndex;

namespace Artifact
{

 class ArtifactGlobalEffectManager::Impl {
 private:
  QHash<EffectID, EffectFactory> effectFactory_;
 public:
  Impl();
  ~Impl();
   
 };

 ArtifactGlobalEffectManager::Impl::Impl()
 {

 }

 ArtifactGlobalEffectManager::Impl::~Impl()
 {

 }

 ArtifactGlobalEffectManager::ArtifactGlobalEffectManager()
 {

 }

 ArtifactGlobalEffectManager::~ArtifactGlobalEffectManager()
 {

 }

 void ArtifactGlobalEffectManager::loadPlugin() noexcept
 {

 }

 void ArtifactGlobalEffectManager::unloadAllPlugins() noexcept
 {

 }

ArtifactGlobalEffectManager* ArtifactGlobalEffectManager::effectManager()
 {
 static ArtifactGlobalEffectManager instance = ArtifactGlobalEffectManager();
 return &instance;
 }

};