module;

#include <QStandardItemModel>
module Artifact.Project.Model;

import Project;


namespace Artifact
{

 class ArtifactProjectModel::Impl
 {
 private:
  

 public:
  Impl();
  ~Impl();
  ArtifactProjectWeakPtr projectPtr_;
 };


 ArtifactProjectModel::ArtifactProjectModel(QObject* parent/*=nullptr*/) :QStandardItemModel(parent)
 {

 }

 ArtifactProjectModel::~ArtifactProjectModel()
 {

 }

};