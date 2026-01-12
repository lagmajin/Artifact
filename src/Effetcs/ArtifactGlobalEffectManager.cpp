module;

module Artifact.Effects.Manager;

import std;
import Container.MultiIndex;

namespace Artifact
{

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