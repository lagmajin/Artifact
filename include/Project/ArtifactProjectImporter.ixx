module;
#include <QString>

export module Artifact.Project.Importer;

import std;
import Artifact.Project;
import Utils.String.UniString;

export namespace Artifact
{
 using namespace ArtifactCore;

 struct ArtifactProjectImporterResult
 {
  bool success = false;
  ArtifactProjectPtr project;
  UniString errorMessage;
  int compositionsLoaded = 0;
  int layersLoaded = 0;
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

  // バリデーション
  bool validateFile(const QString& path);
  UniString getFileFormatVersion(const QString& path);
 };

};
