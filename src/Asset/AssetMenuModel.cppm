module;
#include <QAbstractListModel>
#include <QApplication>
#include <QByteArray>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QStringList>
#include <QStyle>
#include <QVariant>

module AssetMenuModel;

import std;
import Utils.String.UniString;

namespace Artifact
{
 using namespace ArtifactCore;

 class AssetMenuModel::Impl
 {
 public:
  QList<AssetMenuItem> items_;
  QIcon folderIcon_;
  QIcon fileIcon_;

  Impl();
 };

 AssetMenuModel::Impl::Impl()
 {
  if (auto style = QApplication::style()) {
   folderIcon_ = style->standardIcon(QStyle::SP_DirIcon);
   fileIcon_ = style->standardIcon(QStyle::SP_FileIcon);
  }
 }

 AssetMenuModel::AssetMenuModel(QObject* parent) : QAbstractListModel(parent), impl_(new Impl())
 {
 }

 AssetMenuModel::~AssetMenuModel()
 {
  delete impl_;
 }

 int AssetMenuModel::rowCount(const QModelIndex& parent) const
 {
  if (parent.isValid()) {
   return 0;
  }
  return impl_->items_.size();
 }

 QVariant AssetMenuModel::data(const QModelIndex& index, int role) const
 {
  if (!index.isValid()) {
   return QVariant();
  }
  if (index.row() < 0 || index.row() >= impl_->items_.size()) {
   return QVariant();
  }

  const auto& item = impl_->items_.at(index.row());
  switch (role) {
  case Qt::DisplayRole:
  case static_cast<int>(AssetMenuRole::Name):
   return item.name.toQString();
  case Qt::DecorationRole:
   // Use custom icon if available, otherwise use default
   if (!item.icon.isNull()) {
    return item.icon;
   }
   return item.isFolder ? impl_->folderIcon_ : impl_->fileIcon_;
  case Qt::ToolTipRole: {
   QStringList lines;
   if (!item.type.toQString().isEmpty()) {
    lines << item.type.toQString();
   }
   if (!item.path.toQString().isEmpty()) {
    lines << item.path.toQString();
   }
   return lines.join(QStringLiteral("\n"));
  }
  case static_cast<int>(AssetMenuRole::Type):
   return item.type.toQString();
  case static_cast<int>(AssetMenuRole::Path):
   return item.path.toQString();
  case static_cast<int>(AssetMenuRole::IsFolder):
   return item.isFolder;
  default:
   break;
  }
  return QVariant();
 }

 Qt::ItemFlags AssetMenuModel::flags(const QModelIndex& index) const
 {
  if (!index.isValid()) {
   return Qt::NoItemFlags;
  }
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
 }

 QHash<int, QByteArray> AssetMenuModel::roleNames() const
 {
  QHash<int, QByteArray> roles;
  roles[Qt::DisplayRole] = "display";
  roles[Qt::DecorationRole] = "icon";
  roles[static_cast<int>(AssetMenuRole::Name)] = "name";
  roles[static_cast<int>(AssetMenuRole::Type)] = "type";
  roles[static_cast<int>(AssetMenuRole::Path)] = "path";
  roles[static_cast<int>(AssetMenuRole::IsFolder)] = "isFolder";
  return roles;
 }

 void AssetMenuModel::setItems(const QList<AssetMenuItem>& items)
 {
  beginResetModel();
  impl_->items_ = items;
  endResetModel();
 }

 void AssetMenuModel::addItem(const AssetMenuItem& item)
 {
  int row = impl_->items_.size();
  beginInsertRows(QModelIndex(), row, row);
  impl_->items_.append(item);
  endInsertRows();
 }

 void AssetMenuModel::clear()
 {
  beginResetModel();
  impl_->items_.clear();
  endResetModel();
 }

 AssetMenuItem AssetMenuModel::itemAt(int row) const
 {
  if (row < 0 || row >= impl_->items_.size()) {
   return AssetMenuItem{};
  }
  return impl_->items_.at(row);
 }
}
