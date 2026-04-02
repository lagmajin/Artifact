module;
#include <QAbstractListModel>
#include <QHash>
#include <QByteArray>
#include <QList>
#include <QIcon>
#include <QStringList>

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
export module AssetMenuModel;




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
  bool isSequence = false;
  int sequenceFrameCount = 0;
  int sequenceStartFrame = 0;
  int sequencePadding = 0;
  QStringList sequencePaths;
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
  class QMimeData* mimeData(const QModelIndexList& indexes) const override;

  void setItems(const QList<AssetMenuItem>& items);
  void addItem(const AssetMenuItem& item);
  bool updateItemIconByPath(const QString& path, const QIcon& icon);
  void refreshIcons();
  void clear();
  AssetMenuItem itemAt(int row) const;
 };
}
