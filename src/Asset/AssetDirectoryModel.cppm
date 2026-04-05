module;

#include <QAbstractItemModel>
#include <QString>
#include <QHash>
#include <QVector>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMimeData>
#include <QPainter>
#include <QStyle>
#include <QUrl>
#include <QApplication>
#include <QSettings>
#include <QUuid>
#include <wobjectimpl.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module AssetDirectoryModel;





namespace Artifact {

 W_OBJECT_IMPL(AssetDirectoryModel)

 class AssetDirectoryModel::Impl {
 public:
  TreeItem* rootItem = nullptr;
  TreeItem* favoritesNode = nullptr;
  TreeItem* assetsNode = nullptr;
  TreeItem* packagesNode = nullptr;
  QString assetRootPath;
  QString packageRootPath;
  QHash<QString, TreeItem*> guidToItem;
  QVector<FavoriteEntry> favorites_;

  Impl();
  ~Impl();
  void buildRootTree();
  TreeItem* createNode(const QString& guid, const QString& name, const QString& path, TreeItem* parent, bool isVirtual);
  void loadChildrenForNode(TreeItem* node);
  TreeItem* findItemByGuid(const QString& guid) const;
  QString normalizedPath(const QString& path) const;
  void loadFavoritesFromSettings();
  void saveFavoritesToSettings() const;
  void calculateIntelligence(TreeItem* node);

  static bool isImageFile(const QString& fileName) {
    const QString ext = QFileInfo(fileName).suffix().toLower();
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tif" || ext == "tiff" || ext == "bmp" || ext == "exr" || ext == "tga" || ext == "psd";
  }
  static bool isVideoFile(const QString& fileName) {
    const QString ext = QFileInfo(fileName).suffix().toLower();
    return ext == "mp4" || ext == "mov" || ext == "avi" || ext == "mkv" || ext == "webm" || ext == "m4v" || ext == "flv";
  }
  static bool isAudioFile(const QString& fileName) {
    const QString ext = QFileInfo(fileName).suffix().toLower();
    return ext == "wav" || ext == "mp3" || ext == "aac" || ext == "flac" || ext == "m4a" || ext == "ogg";
  }
  static bool is3DFile(const QString& fileName) {
    const QString ext = QFileInfo(fileName).suffix().toLower();
    return ext == "obj" || ext == "fbx" || ext == "glb" || ext == "gltf" || ext == "abc" || ext == "stl";
  }
 };

 AssetDirectoryModel::Impl::Impl() {
  rootItem = new TreeItem();
  rootItem->guid = "root";
  rootItem->name = "Root";
  rootItem->isVirtual = true;
  guidToItem["root"] = rootItem;
  loadFavoritesFromSettings();
 }

 AssetDirectoryModel::Impl::~Impl() {
  delete rootItem;
 }

 void AssetDirectoryModel::Impl::buildRootTree() {
  if (!rootItem) return;
  for (auto* child : rootItem->children) {
   delete child;
  }
  rootItem->children.clear();
  guidToItem.clear();
  guidToItem.insert("root", rootItem);
  favoritesNode = nullptr;
  assetsNode = nullptr;
  packagesNode = nullptr;

  favoritesNode = createNode("favorites", "Favorites", QString(), rootItem, true);
  favoritesNode->childrenLoaded = true;
  for (const auto& favorite : favorites_) {
   if (favorite.path.trimmed().isEmpty()) {
    continue;
   }
   const QFileInfo info(favorite.path);
   const QString displayName = favorite.name.trimmed().isEmpty() ? info.fileName() : favorite.name.trimmed();
   createNode(favorite.guid, displayName, info.absoluteFilePath(), favoritesNode, info.isDir());
  }
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
  
  if (item->isFolder && !item->isVirtual && !item->physicalPath.isEmpty()) {
    calculateIntelligence(item);
  }

  return item;
 }

 void AssetDirectoryModel::Impl::calculateIntelligence(TreeItem* node) {
  if (!node || node->isVirtual || node->physicalPath.isEmpty()) return;
  
  QDir dir(node->physicalPath);
  if (!dir.exists()) return;
  
  QStringList entries = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
  if (entries.isEmpty()) {
    node->intelligenceLoaded = true;
    return;
  }
  
  int imageCount = 0;
  int videoCount = 0;
  int audioCount = 0;
  int threeDCount = 0;
  int totalCount = entries.size();
  
  for (const QString& entry : entries) {
    if (isImageFile(entry)) imageCount++;
    else if (isVideoFile(entry)) videoCount++;
    else if (isAudioFile(entry)) audioCount++;
    else if (is3DFile(entry)) threeDCount++;
  }
  
  node->imageRatio = static_cast<float>(imageCount) / totalCount;
  node->videoRatio = static_cast<float>(videoCount) / totalCount;
  node->audioRatio = static_cast<float>(audioCount) / totalCount;
  node->threeDRatio = static_cast<float>(threeDCount) / totalCount;
  node->intelligenceLoaded = true;
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

 QString AssetDirectoryModel::Impl::normalizedPath(const QString& path) const {
  if (path.trimmed().isEmpty()) {
   return {};
  }
  QFileInfo info(path);
  const QString canonical = info.canonicalFilePath();
  return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
 }

 void AssetDirectoryModel::Impl::loadFavoritesFromSettings() {
  favorites_.clear();
  QSettings settings;
  settings.beginGroup(QStringLiteral("AssetBrowser/Favorites"));
  const int count = settings.beginReadArray(QStringLiteral("items"));
  for (int i = 0; i < count; ++i) {
   settings.setArrayIndex(i);
   FavoriteEntry entry;
   entry.guid = settings.value(QStringLiteral("guid")).toString();
   entry.name = settings.value(QStringLiteral("name")).toString();
   entry.path = settings.value(QStringLiteral("path")).toString();
   if (entry.path.trimmed().isEmpty()) {
    continue;
   }
   if (entry.guid.trimmed().isEmpty()) {
    entry.guid = QUuid::createUuid().toString(QUuid::WithoutBraces);
   }
   favorites_.append(entry);
  }
  settings.endArray();
  settings.endGroup();
 }

 void AssetDirectoryModel::Impl::saveFavoritesToSettings() const {
  QSettings settings;
  settings.beginGroup(QStringLiteral("AssetBrowser/Favorites"));
  settings.beginWriteArray(QStringLiteral("items"));
  for (int i = 0; i < favorites_.size(); ++i) {
   settings.setArrayIndex(i);
   settings.setValue(QStringLiteral("guid"), favorites_[i].guid);
   settings.setValue(QStringLiteral("name"), favorites_[i].name);
   settings.setValue(QStringLiteral("path"), favorites_[i].path);
  }
  settings.endArray();
  settings.endGroup();
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
    QIcon icon = style->standardIcon(QStyle::SP_DirIcon);
    if (item->intelligenceLoaded) {
      QColor tint;
      float maxRatio = 0.0f;
      // Priority: Video > Image > Audio > 3D
      if (item->videoRatio > maxRatio) { tint = QColor("#a57aff"); maxRatio = item->videoRatio; }
      if (item->imageRatio > maxRatio) { tint = QColor("#ffb347"); maxRatio = item->imageRatio; }
      if (item->audioRatio > maxRatio) { tint = QColor("#47ffb3"); maxRatio = item->audioRatio; }
      if (item->threeDRatio > maxRatio) { tint = QColor("#47d1ff"); maxRatio = item->threeDRatio; }
      
      if (maxRatio > 0.05f) {
        QPixmap pixmap = icon.pixmap(16, 16);
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        tint.setAlpha(120);
        painter.fillRect(pixmap.rect(), tint);
        painter.end();
        return QIcon(pixmap);
      }
    }
    return icon;
   }
   return QIcon();
  case Qt::ToolTipRole:
   {
    QString tooltip = item->physicalPath.isEmpty() ? item->name : item->physicalPath;
    if (item->intelligenceLoaded && (item->imageRatio > 0 || item->videoRatio > 0 || item->audioRatio > 0 || item->threeDRatio > 0)) {
      tooltip += "\nContent Ratio:";
      if (item->videoRatio > 0) tooltip += QString("\n - Video: %1%").arg(static_cast<int>(item->videoRatio * 100));
      if (item->imageRatio > 0) tooltip += QString("\n - Image: %1%").arg(static_cast<int>(item->imageRatio * 100));
      if (item->audioRatio > 0) tooltip += QString("\n - Audio: %1%").arg(static_cast<int>(item->audioRatio * 100));
      if (item->threeDRatio > 0) tooltip += QString("\n - 3D: %1%").arg(static_cast<int>(item->threeDRatio * 100));
    }
    return tooltip;
   }
  default:
   return QVariant();
  }
 }

 Qt::ItemFlags AssetDirectoryModel::flags(const QModelIndex& index) const {
  Qt::ItemFlags defaultFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  if (isVirtualNode(index) && guidFromIndex(index) == "favorites") {
    return defaultFlags | Qt::ItemIsDropEnabled;
  }
  return index.isValid() ? defaultFlags : Qt::NoItemFlags;
 }

 Qt::DropActions AssetDirectoryModel::supportedDropActions() const {
  return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
 }

 QStringList AssetDirectoryModel::mimeTypes() const {
  return {"text/uri-list"};
 }

 bool AssetDirectoryModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) {
  if (!data || !data->hasUrls()) return false;
  if (!parent.isValid()) return false;
  
  TreeItem* targetItem = static_cast<TreeItem*>(parent.internalPointer());
  if (!targetItem || targetItem->guid != "favorites") return false;

  bool added = false;
  for (const QUrl& url : data->urls()) {
    if (url.isLocalFile()) {
      QString path = url.toLocalFile();
      if (QFileInfo(path).isDir()) {
        addFavorite(path, QFileInfo(path).fileName());
        added = true;
      }
    }
  }
  return added;
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
  return item && !item->childrenLoaded && item->isFolder && !item->physicalPath.isEmpty();
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
  const QString normalized = impl_->normalizedPath(path);
  if (normalized.isEmpty()) {
   return;
  }

  for (auto& favorite : impl_->favorites_) {
   if (impl_->normalizedPath(favorite.path) == normalized) {
    if (!displayName.trimmed().isEmpty()) {
     favorite.name = displayName.trimmed();
    }
    impl_->saveFavoritesToSettings();
    beginResetModel();
    impl_->buildRootTree();
    endResetModel();
    return;
   }
  }

   FavoriteEntry entry;
  entry.guid = QUuid::createUuid().toString(QUuid::WithoutBraces);
  entry.name = displayName.trimmed();
  entry.path = normalized;
  impl_->favorites_.append(entry);
  impl_->saveFavoritesToSettings();
  beginResetModel();
  impl_->buildRootTree();
  endResetModel();
 }

 void AssetDirectoryModel::removeFavorite(const QString& guid) {
  if (guid.trimmed().isEmpty()) {
   return;
  }
  const int before = impl_->favorites_.size();
  impl_->favorites_.erase(
      std::remove_if(impl_->favorites_.begin(), impl_->favorites_.end(),
                      [&guid](const FavoriteEntry& entry) {
                        return entry.guid == guid;
                      }),
      impl_->favorites_.end());
  if (impl_->favorites_.size() == before) {
   return;
  }
  impl_->saveFavoritesToSettings();
  beginResetModel();
  impl_->buildRootTree();
  endResetModel();
 }

 bool AssetDirectoryModel::isFavoritePath(const QString& path) const {
  const QString normalized = impl_->normalizedPath(path);
  if (normalized.isEmpty()) {
   return false;
  }
  for (const auto& favorite : impl_->favorites_) {
   if (impl_->normalizedPath(favorite.path) == normalized) {
    return true;
   }
  }
  return false;
 }

 QString AssetDirectoryModel::favoriteGuidForPath(const QString& path) const {
  const QString normalized = impl_->normalizedPath(path);
  if (normalized.isEmpty()) {
   return {};
  }
  for (const auto& favorite : impl_->favorites_) {
   if (impl_->normalizedPath(favorite.path) == normalized) {
    return favorite.guid;
   }
  }
  return {};
 }

 QVector<FavoriteEntry> AssetDirectoryModel::favoriteEntries() const {
  if (!impl_) {
   return {};
  }
  QVector<FavoriteEntry> entries;
  entries.reserve(impl_->favorites_.size());
  for (const auto& favorite : impl_->favorites_) {
   entries.push_back(favorite);
  }
  return entries;
 }

}
