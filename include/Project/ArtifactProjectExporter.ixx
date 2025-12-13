module;
#include <QFile>
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

 };

 struct ArtifactProjectExporterResult
 {
  bool success = false;
 
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
  void exportProject2();
  template <StringLike T>
  void setOutputPath(const T& name);
  void setOutputPath(const QString& path);
  void setFormat();
  // ...
 };

 template <StringLike T>
 void ArtifactProjectExporter::setOutputPath(const T& name)
 {
  //setOutputPath(name);
 }










};