module;
#include <QString>
#include <QApplication>
#include <entt/entt.hpp>
export  module Artifact.Application.Manager;


import EnvironmentVariable;



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
  entt::registry& registry();
 };













};