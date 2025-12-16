module;

#include <QStandardItemModel>
module Artifact.Project.Model;

import std;
import Artifact.Project;


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
  
 	 
 	
 }

 ArtifactProjectModel::ArtifactProjectModel(QObject* parent/*=nullptr*/) :QStandardItemModel(parent)
 {

 }

 ArtifactProjectModel::~ArtifactProjectModel()
 {

 }

};