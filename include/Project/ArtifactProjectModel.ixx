module;
#include <wobjectdefs.h>
#include <QStandardItemModel>
export module Artifact.Project.Model;

import std;
import Artifact.Project;

export namespace Artifact
{

 class ArtifactProjectModel:public QStandardItemModel
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactProjectModel(QObject*parent=nullptr);
  ~ArtifactProjectModel();
  void setProject();
 };











};