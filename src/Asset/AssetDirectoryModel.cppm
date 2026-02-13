module;

#include <QAbstractItemModel>
#include <QString>
#include <QHash>
#include <QVector>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QStyle>
#include <QApplication>
#include <QUuid>

module AssetDirectoryModel;

import std;

namespace Artifact {

 class AssetDirectoryModel::Impl {
 public:
  TreeItem* rootItem = nullptr;
  TreeItem* assetsNode = nullptr;
  TreeItem* packagesNode = nullptr;
  QString assetRootPath;
  QString packageRootPath;
  QHash<QString, TreeItem*> guidToItem;

  Impl();
  ~Impl();
  void buildRootTree();
  TreeItem* createNode(const QString& guid, const QString& name, const QString& path, TreeItem* parent, bool isVirtual);
  void loadChildrenForNode(TreeItem* node);
  TreeItem* findItemByGuid(const QString& guid) const;
 };

 AssetDirectoryModel::Impl::Impl() {
  rootItem = new TreeItem();
  rootItem->guid = "root";
  rootItem->name = "Root";
  rootItem->isVirtual = true;
  guidToItem["root"] = rootItem;
 }

 AssetDirectoryModel::Impl::~Impl() {
  delete rootItem;
 }

 void AssetDirectoryModel::Impl::buildRootTree() {
  if (!rootItem) return;
  for (auto child : rootItem->children) {
   delete child;
  }
  rootItem->children.clear();
  if (!assetRootPath.isEmpty()) {
   assetsNode = createNode("assets", "Assets", assetRootPath, rootItem, false);
  }
  if (!packageRootPath.isEmpty()) {
   packagesNode = createNode("packages", "Packages", packageRootPath, rootItem, false);
  }
 }

 TreeItem* AssetDirectoryModel::Impl::createNode(const QString& guid, const QString& name, const QString& path, TreeItem* parent, bool isVirtual) {
  auto item = new TreeItem();
  item->guid = guid.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : guid;
  item->name = name;
  item->physicalPath = path;
  item->isVirtual = isVirtual;
  item->parent = parent;
  item->isFolder = isVirtual || QFileInfo(path).isDir();
  item->childrenLoaded = false;
  if (parent) {
   parent->children.append(item);
  }
  guidToItem[item->guid] = item;
  return item;
 }

 void AssetDirectoryModel::Impl::loadChildrenForNode(TreeItem* node) {
  if (!node || node->childrenLoaded || node->physicalPath.isEmpty()) return;
  QDir dir(node->physicalPath);
  if (!dir.exists()) {
   node->childrenLoaded = true;
   return;
  }
  QStringList dirList = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QString& dirName : dirList) {
   QString childPath = dir.absoluteFilePath(dirName);
   createNode("", dirName, childPath, node, false);
  }
  node->childrenLoaded = true;
 }

 TreeItem* AssetDirectoryModel::Impl::findItemByGuid(const QString& guid) const {
  return guidToItem.value(guid, nullptr);
 }

 AssetDirectoryModel::AssetDirectoryModel(QObject* parent)
  : QAbstractItemModel(parent), impl_(new Impl()) {
 }

 AssetDirectoryModel::~AssetDirectoryModel() {
  delete impl_;
 }

 int AssetDirectoryModel::rowCount(const QModelIndex& parent) const {
  TreeItem* item = parent.isValid() ? static_cast<TreeItem*>(parent.internalPointer()) : impl_->rootItem;
  return item ? item->children.count() : 0;
 }

 int AssetDirectoryModel::columnCount(const QModelIndex& parent) const {
  return 1;
 }

 QVariant AssetDirectoryModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) return QVariant();
  TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
  if (!item) return QVariant();
  switch (role) {
  case Qt::DisplayRole:
   return item->name;
  case Qt::DecorationRole:
   if (auto style = QApplication::style()) {
    return style->standardIcon(QStyle::SP_DirIcon);
   }
   return QIcon();
  case Qt::ToolTipRole:
   return item->physicalPath.isEmpty() ? item->name : item->physicalPath;
  default:
   return QVariant();
  }
 }

 Qt::ItemFlags AssetDirectoryModel::flags(const QModelIndex& index) const {
  return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;
 }

 QModelIndex AssetDirectoryModel::index(int row, int column, const QModelIndex& parent) const {
  if (!hasIndex(row, column, parent)) return QModelIndex();
  TreeItem* parentItem = parent.isValid() ? static_cast<TreeItem*>(parent.internalPointer()) : impl_->rootItem;
  if (!parentItem || row >= parentItem->children.count()) return QModelIndex();
  return createIndex(row, column, parentItem->children[row]);
 }

 QModelIndex AssetDirectoryModel::parent(const QModelIndex& index) const {
  if (!index.isValid()) return QModelIndex();
  TreeItem* childItem = static_cast<TreeItem*>(index.internalPointer());
  TreeItem* parentItem = childItem ? childItem->parent : nullptr;
  if (!parentItem || parentItem == impl_->rootItem) return QModelIndex();
  TreeItem* grandParent = parentItem->parent;
  int row = grandParent ? grandParent->children.indexOf(parentItem) : -1;
  return row >= 0 ? createIndex(row, 0, parentItem) : QModelIndex();
 }

 bool AssetDirectoryModel::hasChildren(const QModelIndex& parent) const {
  TreeItem* item = parent.isValid() ? static_cast<TreeItem*>(parent.internalPointer()) : impl_->rootItem;
  return item && (item->isVirtual || item->isFolder);
 }

 bool AssetDirectoryModel::canFetchMore(const QModelIndex& parent) const {
  TreeItem* item = parent.isValid() ? static_cast<TreeItem*>(parent.internalPointer()) : nullptr;
  return item && !item->childrenLoaded && item->isFolder;
 }

 void AssetDirectoryModel::fetchMore(const QModelIndex& parent) {
  TreeItem* item = parent.isValid() ? static_cast<TreeItem*>(parent.internalPointer()) : nullptr;
  if (!item || item->childrenLoaded) return;
  impl_->loadChildrenForNode(item);
  if (item->children.count() > 0) {
   beginInsertRows(parent, 0, item->children.count() - 1);
   endInsertRows();
  }
  Q_EMIT folderExpanded(item->guid);
 }

 void AssetDirectoryModel::setAssetRootPath(const QString& path) {
  impl_->assetRootPath = path;
  beginResetModel();
  impl_->buildRootTree();
  endResetModel();
 }

 void AssetDirectoryModel::setPackageRootPath(const QString& path) {
  impl_->packageRootPath = path;
  beginResetModel();
  impl_->buildRootTree();
  endResetModel();
 }

 QModelIndex AssetDirectoryModel::indexFromGuid(const QString& guid) const {
  TreeItem* item = impl_->findItemByGuid(guid);
  if (!item || !item->parent) return QModelIndex();
  int row = item->parent->children.indexOf(item);
  return row >= 0 ? createIndex(row, 0, item) : QModelIndex();
 }

 QString AssetDirectoryModel::guidFromIndex(const QModelIndex& index) const {
  if (!index.isValid()) return "";
  TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
  return item ? item->guid : "";
 }

 QString AssetDirectoryModel::pathFromIndex(const QModelIndex& index) const {
  if (!index.isValid()) return "";
  TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
  return item ? item->physicalPath : "";
 }

 QString AssetDirectoryModel::nameFromIndex(const QModelIndex& index) const {
  if (!index.isValid()) return "";
  TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
  return item ? item->name : "";
 }

 bool AssetDirectoryModel::isVirtualNode(const QModelIndex& index) const {
  if (!index.isValid()) return false;
  TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
  return item ? item->isVirtual : false;
 }

 bool AssetDirectoryModel::isFolderNode(const QModelIndex& index) const {
  if (!index.isValid()) return false;
  TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
  return item ? item->isFolder : false;
 }

 void AssetDirectoryModel::addFavorite(const QString& path, const QString& displayName) {
 }

 void AssetDirectoryModel::removeFavorite(const QString& guid) {
 }

}
