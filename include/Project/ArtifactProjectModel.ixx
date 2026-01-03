module;
#include <wobjectdefs.h>
#include <QStandardItemModel>
export module Artifact.Project.Model;

import std;
import Artifact.Project;
import Utils.Id;
import Utils.String.UniString;


export namespace Artifact
{
 using namespace ArtifactCore;

 enum eProjectItemType {
  eProjectItemType_Unknown = 0,
  eProjectItemType_Folder,
  eProjectItemType_Composition,
  
 };

 struct ProjectItem {
  UniString name;
  Id id;
  UniString description;
  //std::vector<ProjectItem> children;

 };

 class ArtifactProjectModel:public QAbstractItemModel
 {
 public:
	 QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	 int rowCount(const QModelIndex& parent) const override;
	 QVariant data(const QModelIndex& index, int role) const override;

 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactProjectModel(QObject*parent=nullptr);
  ~ArtifactProjectModel();
  void setProject();
 };











};