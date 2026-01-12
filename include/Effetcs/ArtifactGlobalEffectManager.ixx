module;
#include <QString>

export module Artifact.Effects.Manager;

import std;
import Utils.String.UniString;
import Utils.String.UniString;

export namespace Artifact
{
 struct EffectFactory {
   
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
 	
  static ArtifactGlobalEffectManager* effectManager();
 };





};