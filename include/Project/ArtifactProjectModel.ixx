module;
#include <wobjectdefs.h>
#include <QStandardItemModel>
export module Artifact.Project.Model;

import std;
import Artifact.Project;

export namespace Artifact
{

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