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
  std::unique_ptr<ArtifactAbstractEffect> factoryByID(const EffectID& id);
 };

 ArtifactGlobalEffectManager::Impl::Impl() {}
 ArtifactGlobalEffectManager::Impl::~Impl() {}
 ArtifactGlobalEffectManager::ArtifactGlobalEffectManager()
   : impl_(new Impl()) {}
 ArtifactGlobalEffectManager::~ArtifactGlobalEffectManager()
 {
   delete impl_;
   impl_ = nullptr;
 }

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
   auto& registry = ArtifactCore::ArtifactPluginRegistry::instance();
   for (const auto& descriptor :
        registry.pluginsOfCategory(ArtifactCore::PluginCategory::Effect)) {
     const std::string id = ArtifactCore::toStdString(descriptor.id);
     if (!id.empty()) registry.activatePlugin(id);
   }
 }

 void ArtifactGlobalEffectManager::Impl::unloadAllPlugins()
 {
   loader_.unloadAll();
 }

 void ArtifactGlobalEffectManager::unloadAllPlugins() noexcept
 {
   impl_->unloadAllPlugins();
 }

 std::unique_ptr<ArtifactAbstractEffect> ArtifactGlobalEffectManager::factoryByID(const EffectID& id)
 {
   return impl_->factoryByID(id);
 }

 std::unique_ptr<ArtifactAbstractEffect> ArtifactGlobalEffectManager::Impl::factoryByID(const EffectID& id)
 {
   auto& registry = ArtifactPluginRegistry::instance();
   auto opt = registry.pluginById(id.toString().toStdString());
   if (!opt || opt->category != ArtifactCore::PluginCategory::Effect ||
       !registry.isActive(id.toString().toStdString())) {
     return nullptr;
   }

   auto effect = std::make_unique<ArtifactAbstractEffect>();
   effect->setEffectID(id);
   effect->setDisplayName(QStringLiteral("Plugin Effect: %1").arg(id.toString()));
   return effect;
 }

ArtifactGlobalEffectManager* ArtifactGlobalEffectManager::effectManager()
 {
 static ArtifactGlobalEffectManager instance = ArtifactGlobalEffectManager();
 return &instance;
 }

};
