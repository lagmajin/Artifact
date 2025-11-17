module;
#include <QString>
#include <QApplication>
#include <entt/entt.hpp>
export  module Artifact.Application.Manager;


import EnvironmentVariable;
import Effects.Manager;
import Artifact.Render.Manager;
import Project.Manager;

namespace Artifact {

 class ArtifactApplicationManager
 {
 private:
  class Impl;
  Impl* impl_;
  ArtifactApplicationManager(const ArtifactApplicationManager&) = delete;
 public:
  ArtifactApplicationManager();
  ~ArtifactApplicationManager();
  static ArtifactApplicationManager* instance();
  GlobalEffectManager*const effectManager();
  //ArtifactProjectManager* const projectManager();
  entt::registry& registry();
 
 };













};