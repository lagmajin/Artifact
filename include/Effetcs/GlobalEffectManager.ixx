module;
#include <QString>

export module Effects.Manager;

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
 };





};