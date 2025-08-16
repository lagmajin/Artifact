module;

#include <QAbstractItemModel>
module Artifact.Layers.Hierarchy.Model;


namespace Artifact
{


 ArtifactHierarchyModel::ArtifactHierarchyModel(QObject* parent /*= nullptr*/):QAbstractItemModel(parent)
 {

 }

 ArtifactHierarchyModel::~ArtifactHierarchyModel()
 {

 }
 QVariant ArtifactHierarchyModel::headerData(int section, Qt::Orientation orientation, int role) const
 {
  if (role != Qt::DisplayRole)
   return QVariant();

  if (orientation == Qt::Horizontal) {
   switch (section) {
   case 0: return QString("目玉");
   case 1: return QString("スピーカー");
   case 2: return QString("ソロ");
   case 3: return QString("ロック");
   case 4: return QString("レイヤー名");
   default: break;
   }
  }

  return QVariant();
 }
 int ArtifactHierarchyModel::rowCount(const QModelIndex& parent /*= QModelIndex()*/) const
 {
  return 0;
 }

 int ArtifactHierarchyModel::columnCount(const QModelIndex& parent /*= QModelIndex()*/) const
 {
  return 3;
 }

 QModelIndex ArtifactHierarchyModel::index(int row, int column, const QModelIndex& parent /*= QModelIndex()*/) const
 {
  if (!hasIndex(row, column, parent))
   return QModelIndex();
  return QModelIndex();
 }

 QVariant ArtifactHierarchyModel::data(const QModelIndex& index, int role /*= Qt::DisplayRole*/) const
 {
  Q_UNUSED(index);
  Q_UNUSED(role);
  return QVariant(); // 空実装
 }

 QModelIndex ArtifactHierarchyModel::parent(const QModelIndex& child) const
 {
  Q_UNUSED(child);
  return QModelIndex(); // 空実装
 }

}