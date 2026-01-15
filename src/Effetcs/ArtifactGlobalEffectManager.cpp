module;
#include <QHash>
module Artifact.Effects.Manager;

import std;
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