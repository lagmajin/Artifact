module;

module Artifact.Effects.Manager;

import std;

namespace Artifact
{

 GlobalEffectManager::GlobalEffectManager()
 {

 }

 GlobalEffectManager::~GlobalEffectManager()
 {

 }

 void GlobalEffectManager::loadPlugin() noexcept
 {

 }

 void GlobalEffectManager::unloadAllPlugins() noexcept
 {

 }

GlobalEffectManager* GlobalEffectManager::effectManager()
 {
 static GlobalEffectManager instance = GlobalEffectManager();
 return &instance;
 }

};