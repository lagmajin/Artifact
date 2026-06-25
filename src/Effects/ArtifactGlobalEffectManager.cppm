module;
#include <QHash>
#include <QCoreApplication>
#include <QDir>
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
import ArtifactCore.Plugin.Registry;
import ArtifactCore.Plugin.Common;
import Artifact.Plugin.Loader;

namespace Artifact
{

 class ArtifactGlobalEffectManager::Impl {
 private:
  ArtifactPluginLoader loader_;
 public:
  Impl();
  ~Impl();
  void loadPlugin();
  void unloadAllPlugins();
  void factoryByID(const EffectID& id);
 };

 ArtifactGlobalEffectManager::Impl::Impl() {}
 ArtifactGlobalEffectManager::Impl::~Impl() {}
 ArtifactGlobalEffectManager::ArtifactGlobalEffectManager() {}
 ArtifactGlobalEffectManager::~ArtifactGlobalEffectManager() {}

 void ArtifactGlobalEffectManager::loadPlugin() noexcept
 {
   impl_->loadPlugin();
 }

 void ArtifactGlobalEffectManager::Impl::loadPlugin()
 {
   const QStringList paths = {
     QDir(QCoreApplication::applicationDirPath()).filePath("plugins/effects"),
   };
   loader_.discoverAndLoad(paths, PluginLoadMode::Auto);
 }

 void ArtifactGlobalEffectManager::Impl::unloadAllPlugins()
 {
   loader_.unloadAll();
 }

 void ArtifactGlobalEffectManager::unloadAllPlugins() noexcept
 {
   impl_->unloadAllPlugins();
 }

 void ArtifactGlobalEffectManager::factoryByID(const EffectID& id)
 {
   impl_->factoryByID(id);
 }

 void ArtifactGlobalEffectManager::Impl::factoryByID(const EffectID& id)
 {
   auto& registry = ArtifactPluginRegistry::instance();
   auto opt = registry.pluginById(id.toString().toStdString());
   if (opt) {
     return;
   }
 }

ArtifactGlobalEffectManager* ArtifactGlobalEffectManager::effectManager()
 {
 static ArtifactGlobalEffectManager instance = ArtifactGlobalEffectManager();
 return &instance;
 }

};
