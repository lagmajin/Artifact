module;
#include <QString>

export module Artifact.Effects.Manager;

import std;
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