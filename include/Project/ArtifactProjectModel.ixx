module;

#include <QStandardItemModel>
export module Artifact.Project.Model;



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
 };











};