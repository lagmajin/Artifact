module;

#include <algorithm>
#include <QRect>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QList>
#include <QString>

export module Artifact.DockManager;

export namespace Artifact {

inline constexpr int kDockLayoutDocumentVersion = 1;

// Backend-neutral placement intent. QADS and the future native backend both
// translate this value into their own surface/layout representation.
enum class DockArea {
  Left,
  Right,
  Top,
  Bottom,
  Center,
};

enum class DockBackendKind {
  QadsAdapter,
  Native,
};

struct DockBackendCapabilities {
  bool supportsFloating = false;
  bool supportsDragDrop = false;
  bool supportsMultipleTabGroups = false;
};

inline QString dockBackendKindToString(DockBackendKind kind) {
  return kind == DockBackendKind::Native ? QStringLiteral("native")
                                         : QStringLiteral("qads");
}

inline DockBackendKind dockBackendKindFromString(const QString &value) {
  return value.compare(QStringLiteral("native"), Qt::CaseInsensitive) == 0
             ? DockBackendKind::Native
             : DockBackendKind::QadsAdapter;
}

struct DockLayoutEntry {
  QString dockId;
  DockArea area = DockArea::Center;
  QString tabGroup;
  QRect floatingGeometry;
  bool visible = true;
  bool pinned = false;
  bool floating = false;
};

inline bool isValidDockLayoutEntry(const DockLayoutEntry &entry);

// Backend-neutral registry.  The native backend can reuse this model without
// depending on a QADS widget pointer or container type.
class DockLayoutRegistry {
public:
  void upsert(const DockLayoutEntry &entry) {
    if (isValidDockLayoutEntry(entry)) {
      entries_[entry.dockId] = entry;
    }
  }

  bool contains(const QString &dockId) const {
    return entries_.contains(dockId);
  }

  DockLayoutEntry value(const QString &dockId) const {
    return entries_.value(dockId);
  }

  void remove(const QString &dockId) { entries_.remove(dockId); }

  QList<DockLayoutEntry> values() const {
    auto values = entries_.values();
    std::sort(values.begin(), values.end(),
              [](const DockLayoutEntry &left, const DockLayoutEntry &right) {
                return left.dockId < right.dockId;
              });
    return values;
  }

  void clear() { entries_.clear(); }

private:
  QHash<QString, DockLayoutEntry> entries_;
};

inline bool isValidDockLayoutEntry(const DockLayoutEntry &entry) {
  if (entry.dockId.trimmed().isEmpty()) {
    return false;
  }
  if (entry.floating &&
      (entry.floatingGeometry.width() < 0 ||
       entry.floatingGeometry.height() < 0)) {
    return false;
  }
  return true;
}

inline bool canApplyDockLayoutEntry(const DockBackendCapabilities &capabilities,
                                    const DockLayoutEntry &entry) {
  return isValidDockLayoutEntry(entry) &&
         (!entry.floating || capabilities.supportsFloating);
}

inline QString dockAreaToString(DockArea area) {
  switch (area) {
  case DockArea::Left: return QStringLiteral("left");
  case DockArea::Right: return QStringLiteral("right");
  case DockArea::Top: return QStringLiteral("top");
  case DockArea::Bottom: return QStringLiteral("bottom");
  case DockArea::Center: return QStringLiteral("center");
  }
  return QStringLiteral("center");
}

inline DockArea dockAreaFromString(const QString &value) {
  if (value.compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0) {
    return DockArea::Left;
  }
  if (value.compare(QStringLiteral("right"), Qt::CaseInsensitive) == 0) {
    return DockArea::Right;
  }
  if (value.compare(QStringLiteral("top"), Qt::CaseInsensitive) == 0) {
    return DockArea::Top;
  }
  if (value.compare(QStringLiteral("bottom"), Qt::CaseInsensitive) == 0) {
    return DockArea::Bottom;
  }
  return DockArea::Center;
}

inline QJsonObject dockLayoutEntryToJson(const DockLayoutEntry &entry) {
  QJsonObject json;
  json[QStringLiteral("dockId")] = entry.dockId;
  json[QStringLiteral("area")] = dockAreaToString(entry.area);
  json[QStringLiteral("tabGroup")] = entry.tabGroup;
  json[QStringLiteral("floatingGeometry")] = QJsonObject{
      {QStringLiteral("x"), entry.floatingGeometry.x()},
      {QStringLiteral("y"), entry.floatingGeometry.y()},
      {QStringLiteral("width"), entry.floatingGeometry.width()},
      {QStringLiteral("height"), entry.floatingGeometry.height()}};
  json[QStringLiteral("visible")] = entry.visible;
  json[QStringLiteral("pinned")] = entry.pinned;
  json[QStringLiteral("floating")] = entry.floating;
  return json;
}

inline DockLayoutEntry dockLayoutEntryFromJson(const QJsonObject &json) {
  DockLayoutEntry entry;
  entry.dockId = json.value(QStringLiteral("dockId")).toString();
  entry.area = dockAreaFromString(json.value(QStringLiteral("area")).toString());
  entry.tabGroup = json.value(QStringLiteral("tabGroup")).toString();
  const QJsonObject geometry =
      json.value(QStringLiteral("floatingGeometry")).toObject();
  entry.floatingGeometry = QRect(
      geometry.value(QStringLiteral("x")).toInt(),
      geometry.value(QStringLiteral("y")).toInt(),
      geometry.value(QStringLiteral("width")).toInt(),
      geometry.value(QStringLiteral("height")).toInt());
  entry.visible = json.value(QStringLiteral("visible")).toBool(true);
  entry.pinned = json.value(QStringLiteral("pinned")).toBool(false);
  entry.floating = json.value(QStringLiteral("floating")).toBool(false);
  return entry;
}

struct DockLayoutDocument {
  int version = kDockLayoutDocumentVersion;
  QList<DockLayoutEntry> entries;

  QJsonObject toJson() const {
    QJsonArray jsonEntries;
    for (const auto &entry : entries) {
      jsonEntries.push_back(dockLayoutEntryToJson(entry));
    }
    return QJsonObject{{QStringLiteral("version"), version},
                       {QStringLiteral("entries"), jsonEntries}};
  }

  static DockLayoutDocument fromJson(const QJsonObject &json) {
    DockLayoutDocument document;
    document.version = json.value(QStringLiteral("version")).toInt(0);
    if (document.version != kDockLayoutDocumentVersion) {
      document.entries.clear();
      return document;
    }
    for (const auto &value :
         json.value(QStringLiteral("entries")).toArray()) {
      if (value.isObject()) {
        const DockLayoutEntry entry = dockLayoutEntryFromJson(value.toObject());
        if (isValidDockLayoutEntry(entry)) {
          document.entries.push_back(entry);
        }
      }
    }
    return document;
  }
};

} // namespace Artifact
