module;

#include <QAbstractItemModel>
export module Artifact.Layers.Hierarchy.Model;

import std;
import Utils.Id;


export namespace Artifact
{

 class ArtifactHierarchyModel:public QAbstractItemModel
 {
 private:
  class Impl;
  Impl* impl_;
 protected:
 	
 public:
  ArtifactHierarchyModel(QObject* parent = nullptr);
  ~ArtifactHierarchyModel();
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  QModelIndex parent(const QModelIndex& child) const override;
 };




}
