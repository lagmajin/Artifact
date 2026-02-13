module;

#include <QAbstractItemModel>
#include <QString>
#include <QUuid>
#include <QHash>
#include <QVector>
#include <QDir>
#include <QFileInfo>

export module AssetDirectoryModel;

import std;

export namespace Artifact {

 struct TreeItem {
  QString guid;
  QString name;
  QString physicalPath;
  bool isVirtual = false;
  bool isFolder = true;
  TreeItem* parent = nullptr;
  QVector<TreeItem*> children;
  bool childrenLoaded = false;

  ~TreeItem() {
   for (auto child : children) {
    delete child;
   }
  }
 };

 class AssetDirectoryModel : public QAbstractItemModel {
  Q_OBJECT
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

 Q_SIGNALS:
  void assetSelected(const QString& guid, const QString& path, bool isFolder);
  void assetDoubleClicked(const QString& guid, const QString& path);
  void folderExpanded(const QString& guid);
 };

}
