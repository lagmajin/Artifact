module;

#include <QAbstractItemModel>
#include <QString>
#include <QUuid>
#include <QHash>
#include <QVector>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <wobjectdefs.h>

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
export module AssetDirectoryModel;





export namespace Artifact {

 struct FavoriteEntry {
  QString guid;
  QString name;
  QString path;
 };

 struct TreeItem {
  QString guid;
  QString name;
  QString physicalPath;
  bool isVirtual = false;
  bool isFolder = true;
  TreeItem* parent = nullptr;
  QVector<TreeItem*> children;
  bool childrenLoaded = false;

  // Folder Intelligence
  float imageRatio = 0.0f;
  float videoRatio = 0.0f;
  float audioRatio = 0.0f;
  float threeDRatio = 0.0f;
  bool intelligenceLoaded = false;

  ~TreeItem() {
   for (auto child : children) {
    delete child;
   }
  }
 };

 class AssetDirectoryModel : public QAbstractItemModel {
  W_OBJECT(AssetDirectoryModel)
 private:
  class Impl;
  Impl* impl_;

 public:
  explicit AssetDirectoryModel(QObject* parent = nullptr);
  ~AssetDirectoryModel();

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
  bool canFetchMore(const QModelIndex& parent) const override;
  void fetchMore(const QModelIndex& parent) override;

  Qt::DropActions supportedDropActions() const override;
  QStringList mimeTypes() const override;
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

  void setAssetRootPath(const QString& path);
  void setPackageRootPath(const QString& path);

  QModelIndex indexFromGuid(const QString& guid) const;
  QString guidFromIndex(const QModelIndex& index) const;
  QString pathFromIndex(const QModelIndex& index) const;
  QString nameFromIndex(const QModelIndex& index) const;
  bool isVirtualNode(const QModelIndex& index) const;
  bool isFolderNode(const QModelIndex& index) const;

  void addFavorite(const QString& path, const QString& displayName = "");
  void removeFavorite(const QString& guid);
  bool isFavoritePath(const QString& path) const;
  QString favoriteGuidForPath(const QString& path) const;
  QVector<FavoriteEntry> favoriteEntries() const;

 public:
  //signals
  void assetSelected(const QString& guid, const QString& path, bool isFolder)
   W_SIGNAL(assetSelected, guid, path, isFolder);
  void assetDoubleClicked(const QString& guid, const QString& path)
   W_SIGNAL(assetDoubleClicked, guid, path);
  void folderExpanded(const QString& guid)
   W_SIGNAL(folderExpanded, guid);
 };

}
