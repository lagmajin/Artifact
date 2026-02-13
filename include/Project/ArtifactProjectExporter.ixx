module;
#include <QFile>
#include <QJsonObject>
export module Artifact.Project.Exporter;

import std;
import Artifact.Project;
import Utils.String.Like;
import Utils.String.UniString;


export namespace Artifact
{
 using namespace ArtifactCore;

 enum eProjectFormat
 {
  ProjectFormat_JSON = 0,
  ProjectFormat_Binary = 1
 };

 struct ArtifactProjectExporterResult
 {
  bool success = false;
  UniString errorMessage;
 };

 class ArtifactProjectExporter {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactProjectExporter();
  ~ArtifactProjectExporter();
  void setProject(ArtifactProjectPtr& ptr);
  ArtifactProjectExporterResult exportProject();
  ArtifactProjectExporterResult exportTimelineData();
  ArtifactProjectExporterResult exportMetadata();
  void setFormat(eProjectFormat format);
  template <StringLike T>
  void setOutputPath(const T& name);
  void setOutputPath(const QString& path);
  void setOutputPath(const UniString& path);
 };

 template <StringLike T>
 void ArtifactProjectExporter::setOutputPath(const T& name)
 {
  setOutputPath(UniString(name));
 }
};
