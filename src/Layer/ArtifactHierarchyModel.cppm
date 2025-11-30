module;
#include <QIcon>
#include <QAbstractItemModel>

module Artifact.Layers.Hierarchy.Model;

import std;
import Utils;
import Artifact.Composition.Abstract;
import Project.Manager;

namespace Artifact
{
 using namespace ArtifactCore;

 ArtifactHierarchyModel::ArtifactHierarchyModel(QObject* parent /*= nullptr*/):QAbstractItemModel(parent)
 {

 }

 ArtifactHierarchyModel::~ArtifactHierarchyModel()
 {

 }
 QVariant ArtifactHierarchyModel::headerData(int section, Qt::Orientation orientation, int role) const
 {
  if (orientation == Qt::Horizontal) {
   if (role == Qt::DecorationRole) {  // アイコン用
    switch (section) {
    case 0: return QIcon(resolveIconPath("visibility.png"));
    }
   }
   if (role == Qt::DisplayRole) { // テキスト用（必要なら）
    if (section == 4) return QStringLiteral("レイヤー名");
   }
  }


  return QVariant();
 }
 int ArtifactHierarchyModel::rowCount(const QModelIndex& parent /*= QModelIndex()*/) const
 {
  if (parent.isValid())
   return 0; // 子はまだ作らない
  return 1; // ルートに1行だけ
 }

 int ArtifactHierarchyModel::columnCount(const QModelIndex& parent /*= QModelIndex()*/) const
 {
  return 3;
 }

 QModelIndex ArtifactHierarchyModel::index(int row, int column, const QModelIndex& parent) const
 {
  if (!hasIndex(row, column, parent))
   return QModelIndex();

  // ツリー構造をまだ作ってないので row,column を直接保持
  return createIndex(row, column, nullptr);
 }

 QVariant ArtifactHierarchyModel::data(const QModelIndex& index, int role /*= Qt::DisplayRole*/) const
 {
 	
 	
  if (role == Qt::BackgroundRole) {
   if (index.row() % 2 == 0)
	return QColor(30, 30, 30);  // 偶数行
   else
	return QColor(45, 45, 45);  // 奇数行
  	
  	
  	
   if (role == Qt::DisplayRole) {
	return "Test";
   }
  }
 	
  return QVariant(); // 空実装
 }

 QModelIndex ArtifactHierarchyModel::parent(const QModelIndex& child) const
 {
  Q_UNUSED(child);
  return QModelIndex(); // 空実装
 }

}