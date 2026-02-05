module;
#include <QString>

export module Artifact.Project.Importer;

import std;
import Artifact.Project;

export namespace Artifact
{
 using namespace ArtifactCore;

 struct ArtifactProjectImporterResult
 {
  bool success = false;
  ArtifactProjectPtr project;
 };

 class ArtifactProjectImporter {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactProjectImporter();
  ~ArtifactProjectImporter();
  void setInputPath(const QString& path);
  ArtifactProjectImporterResult importProject();
 };

};
