module;
#include <QString>

export module Artifact.Effects.Manager;

import std;

export namespace Artifact
{

 class GlobalEffectManager{
 private:
  class Impl;
  Impl* impl_;
 public:
  GlobalEffectManager();
  ~GlobalEffectManager();
  void loadPlugin() noexcept;
  void unloadAllPlugins() noexcept;
 	
  static GlobalEffectManager* effectManager();
 };





};