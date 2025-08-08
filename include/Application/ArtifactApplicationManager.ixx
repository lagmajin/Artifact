module;
#include <Qstring>
#include <QApplication>

export  module Artifact.Application.Manager;


import EnvironmentVariable;



namespace Artifact {

 class ArtifactApplicationManager
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactApplicationManager();
  ~ArtifactApplicationManager();
  static ArtifactApplicationManager* instance();

 };













};