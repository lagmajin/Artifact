module;
#include <QAbstractListModel>
#include <QHash>
#include <QByteArray>
#include <QList>
#include <QIcon>

export module AssetMenuModel;

import std;
import Utils.String.UniString;

export namespace Artifact
{
 using namespace ArtifactCore;

 struct AssetMenuItem
 {
  UniString name;
  UniString type;
  UniString path;
  bool isFolder = false;
  QIcon icon;  // Optional: custom icon/thumbnail
 };

 enum class AssetMenuRole
 {
  Name = Qt::UserRole + 1,
  Type,
  Path,
  IsFolder
 };

 class AssetMenuModel : public QAbstractListModel
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit AssetMenuModel(QObject* parent = nullptr);
  ~AssetMenuModel();

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setItems(const QList<AssetMenuItem>& items);
  void addItem(const AssetMenuItem& item);
  void clear();
  AssetMenuItem itemAt(int row) const;
 };
}
