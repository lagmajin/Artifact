module;

#include <QStandardItemModel>
module Artifact.Project.Model;

import std;
import Artifact.Project;
import Artifact.Service.Project;

namespace Artifact
{

 class ArtifactProjectModel::Impl
 {
 private:
  

 public:
  Impl();
  ~Impl();
  ArtifactProjectWeakPtr projectPtr_;
  void refreshTree();
 };

 void ArtifactProjectModel::Impl::refreshTree()
 {
  auto projectService = ArtifactProjectService::instance();
 	
 	 
 	
 }

 ArtifactProjectModel::ArtifactProjectModel(QObject* parent/*=nullptr*/) :QAbstractItemModel(parent)
 {

 }

 ArtifactProjectModel::~ArtifactProjectModel()
 {

 }

 QVariant ArtifactProjectModel::data(const QModelIndex& index, int role) const
 {

  return QVariant();
 }

 int ArtifactProjectModel::rowCount(const QModelIndex& parent) const
 {
  return 0;
 }
	

};