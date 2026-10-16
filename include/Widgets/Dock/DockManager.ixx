module;

#include <algorithm>
#include <QRect>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QList>
#include <QString>

export module Dock.Manager;

export namespace Dock {

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

inline constexpr int kAllDockAreaDropTargets = 0x1f;

inline constexpr int dockAreaDropTargetBit(DockArea area) {
  return 1 << static_cast<int>(area);
}

enum class DockBackendKind {
  Native,
};

struct DockBackendCapabilities {
  bool supportsFloating = false;
  bool supportsDragDrop = false;
  bool supportsMultipleTabGroups = false;
};

inline QString dockBackendKindToString(DockBackendKind kind) {
  static_cast<void>(kind);
  return QStringLiteral("native");
}

inline DockBackendKind dockBackendKindFromString(const QString &value) {
  static_cast<void>(value);
  return DockBackendKind::Native;
}

struct DockLayoutEntry {
  QString dockId;
  DockArea area = DockArea::Center;
  QString tabGroup;
  // Optional path for a nested split within the root DockArea. Each pair is
  // an orientation (H/V) followed by the child side (0/1). Older documents
  // omit this field and continue to use the area's root tab surface.
  QString splitPath;
  // Per split-path segment, the first child's share in ten-thousandths.
  QList<int> splitRatios;
  // Optional path in the complete dock container, outside the area's local
  // split tree. Each pair is an orientation (H/V) and child side (0/1).
  QString containerSplitPath;
  QList<int> containerSplitRatios;
  // Stable identity for a floating tab group. Older layouts may omit it and
  // continue to identify groups through the legacy tabGroup member list.
  QString floatingGroupId;
  QRect floatingGeometry;
  bool visible = true;
  bool active = false;
  bool pinned = false;
  bool closable = true;
  bool movable = true;
  bool floatable = true;
  bool focusable = true;
  bool noTab = false;
  bool autoHideable = true;
  bool locked = false;
  // QADS CDockAreaWidget::allowedAreas equivalent. Older layouts omit this
  // field and retain all five local dock-area targets.
  int allowedDropAreas = kAllDockAreaDropTargets;
  // QADS CDockAreaWidget::HideSingleWidgetTitleBar. This is copied onto each
  // member entry so tab-surface flags survive portable layout restoration.
  bool hideSingleWidgetTitleBar = false;
  bool floating = false;
  bool autoHidden = false;
  DockArea autoHideArea = DockArea::Left;
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
  if (entry.splitPath.size() > 128 || entry.splitPath.size() % 2 != 0 ||
      entry.splitRatios.size() != entry.splitPath.size() / 2) {
    return false;
  }
  if (entry.containerSplitPath.size() > 128 ||
      entry.containerSplitPath.size() % 2 != 0 ||
      entry.containerSplitRatios.size() !=
          entry.containerSplitPath.size() / 2) {
    return false;
  }
  if (entry.floatingGroupId.size() > 128 ||
      (!entry.floating && !entry.floatingGroupId.isEmpty())) {
    return false;
  }
  for (qsizetype index = 0; index < entry.splitPath.size(); index += 2) {
    const QChar orientation = entry.splitPath.at(index);
    const QChar side = entry.splitPath.at(index + 1);
    const int ratio = entry.splitRatios.at(index / 2);
    if ((orientation != QLatin1Char('H') &&
         orientation != QLatin1Char('V')) ||
        (side != QLatin1Char('0') && side != QLatin1Char('1')) ||
        ratio < 1 || ratio > 9999) {
      return false;
    }
  }
  for (qsizetype index = 0; index < entry.containerSplitPath.size();
       index += 2) {
    const QChar orientation = entry.containerSplitPath.at(index);
    const QChar side = entry.containerSplitPath.at(index + 1);
    const int ratio = entry.containerSplitRatios.at(index / 2);
    if ((orientation != QLatin1Char('H') &&
         orientation != QLatin1Char('V')) ||
        (side != QLatin1Char('0') && side != QLatin1Char('1')) ||
        ratio < 1 || ratio > 9999) {
      return false;
    }
  }
  if (entry.floating &&
      (entry.floatingGeometry.width() < 0 ||
       entry.floatingGeometry.height() < 0)) {
    return false;
  }
  if (entry.allowedDropAreas < 0 ||
      (entry.allowedDropAreas & ~kAllDockAreaDropTargets) != 0) {
    return false;
  }
  if (entry.autoHidden &&
      (entry.floating || entry.autoHideArea == DockArea::Center)) {
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
  if (!entry.splitPath.isEmpty()) {
    json[QStringLiteral("splitPath")] = entry.splitPath;
    QJsonArray ratios;
    for (const int ratio : entry.splitRatios) {
      ratios.push_back(ratio);
    }
    json[QStringLiteral("splitRatios")] = ratios;
  }
  if (!entry.containerSplitPath.isEmpty()) {
    json[QStringLiteral("containerSplitPath")] = entry.containerSplitPath;
    QJsonArray ratios;
    for (const int ratio : entry.containerSplitRatios) {
      ratios.push_back(ratio);
    }
    json[QStringLiteral("containerSplitRatios")] = ratios;
  }
  if (entry.floating && !entry.floatingGroupId.isEmpty()) {
    json[QStringLiteral("floatingGroupId")] = entry.floatingGroupId;
  }
  json[QStringLiteral("floatingGeometry")] = QJsonObject{
      {QStringLiteral("x"), entry.floatingGeometry.x()},
      {QStringLiteral("y"), entry.floatingGeometry.y()},
      {QStringLiteral("width"), entry.floatingGeometry.width()},
      {QStringLiteral("height"), entry.floatingGeometry.height()}};
  json[QStringLiteral("visible")] = entry.visible;
  json[QStringLiteral("active")] = entry.active;
  json[QStringLiteral("pinned")] = entry.pinned;
  if (!entry.closable) {
    json[QStringLiteral("closable")] = false;
  }
  if (!entry.movable) {
    json[QStringLiteral("movable")] = false;
  }
  if (!entry.floatable) {
    json[QStringLiteral("floatable")] = false;
  }
  if (!entry.focusable) {
    json[QStringLiteral("focusable")] = false;
  }
  if (entry.noTab) {
    json[QStringLiteral("noTab")] = true;
  }
  if (!entry.autoHideable) {
    json[QStringLiteral("autoHideable")] = false;
  }
  if (entry.locked) {
    json[QStringLiteral("locked")] = true;
  }
  if (entry.allowedDropAreas != kAllDockAreaDropTargets) {
    json[QStringLiteral("allowedDropAreas")] = entry.allowedDropAreas;
  }
  if (entry.hideSingleWidgetTitleBar) {
    json[QStringLiteral("hideSingleWidgetTitleBar")] = true;
  }
  json[QStringLiteral("floating")] = entry.floating;
  if (entry.autoHidden) {
    json[QStringLiteral("autoHidden")] = true;
    json[QStringLiteral("autoHideArea")] =
        dockAreaToString(entry.autoHideArea);
  }
  return json;
}

inline DockLayoutEntry dockLayoutEntryFromJson(const QJsonObject &json) {
  DockLayoutEntry entry;
  entry.dockId = json.value(QStringLiteral("dockId")).toString();
  entry.area = dockAreaFromString(json.value(QStringLiteral("area")).toString());
  entry.tabGroup = json.value(QStringLiteral("tabGroup")).toString();
  entry.splitPath = json.value(QStringLiteral("splitPath")).toString();
  for (const auto &ratio :
       json.value(QStringLiteral("splitRatios")).toArray()) {
    entry.splitRatios.push_back(ratio.toInt());
  }
  entry.containerSplitPath =
      json.value(QStringLiteral("containerSplitPath")).toString();
  for (const auto &ratio :
       json.value(QStringLiteral("containerSplitRatios")).toArray()) {
    entry.containerSplitRatios.push_back(ratio.toInt());
  }
  entry.floatingGroupId =
      json.value(QStringLiteral("floatingGroupId")).toString();
  const QJsonObject geometry =
      json.value(QStringLiteral("floatingGeometry")).toObject();
  entry.floatingGeometry = QRect(
      geometry.value(QStringLiteral("x")).toInt(),
      geometry.value(QStringLiteral("y")).toInt(),
      geometry.value(QStringLiteral("width")).toInt(),
      geometry.value(QStringLiteral("height")).toInt());
  entry.visible = json.value(QStringLiteral("visible")).toBool(true);
  entry.active = json.value(QStringLiteral("active")).toBool(false);
  entry.pinned = json.value(QStringLiteral("pinned")).toBool(false);
  entry.closable = json.value(QStringLiteral("closable")).toBool(true);
  entry.movable = json.value(QStringLiteral("movable")).toBool(true);
  entry.floatable = json.value(QStringLiteral("floatable")).toBool(true);
  entry.focusable = json.value(QStringLiteral("focusable")).toBool(true);
  entry.noTab = json.value(QStringLiteral("noTab")).toBool(false);
  entry.autoHideable = json.value(QStringLiteral("autoHideable")).toBool(true);
  entry.locked = json.value(QStringLiteral("locked")).toBool(false);
  const int allowedDropAreas =
      json.value(QStringLiteral("allowedDropAreas"))
          .toInt(kAllDockAreaDropTargets);
  entry.allowedDropAreas = allowedDropAreas >= 0 &&
          (allowedDropAreas & ~kAllDockAreaDropTargets) == 0
      ? allowedDropAreas
      : kAllDockAreaDropTargets;
  entry.hideSingleWidgetTitleBar =
      json.value(QStringLiteral("hideSingleWidgetTitleBar")).toBool(false);
  entry.floating = json.value(QStringLiteral("floating")).toBool(false);
  entry.autoHidden = json.value(QStringLiteral("autoHidden")).toBool(false);
  if (entry.autoHidden) {
    entry.autoHideArea =
        dockAreaFromString(json.value(QStringLiteral("autoHideArea")).toString());
  }
  return entry;
}

struct DockLayoutDocument {
  int version = kDockLayoutDocumentVersion;
  QList<DockLayoutEntry> entries;
  // Optional, area-level preference. Missing and unknown values retain the
  // historical North/top placement for backward compatibility.
  QHash<int, bool> areaTabsAtBottom;
  // Optional ratios for the built-in vertical and horizontal root splitters:
  // top/center/bottom followed by left/center/right, each summing to 10000.
  QList<int> rootSplitterRatios;
  // Optional QADS-style tab behavior; disabled by default to match its base
  // configuration flags.
  bool middleMouseButtonClosesTab = false;
  // Optional QADS-style Auto-Hide button mode; false toggles only the active
  // panel and true toggles every open panel in the tab area.
  bool autoHideButtonTogglesArea = false;

  QJsonObject toJson() const {
    QJsonArray jsonEntries;
    for (const auto &entry : entries) {
      jsonEntries.push_back(dockLayoutEntryToJson(entry));
    }
    QJsonObject json{{QStringLiteral("version"), version},
                     {QStringLiteral("entries"), jsonEntries}};
    if (middleMouseButtonClosesTab) {
      json[QStringLiteral("middleMouseButtonClosesTab")] = true;
    }
    if (autoHideButtonTogglesArea) {
      json[QStringLiteral("autoHideButtonTogglesArea")] = true;
    }
    QJsonObject tabPositions;
    for (const DockArea area : {DockArea::Left, DockArea::Right,
                                DockArea::Top, DockArea::Bottom,
                                DockArea::Center}) {
      if (areaTabsAtBottom.value(static_cast<int>(area), false)) {
        tabPositions[dockAreaToString(area)] = QStringLiteral("bottom");
      }
    }
    if (!tabPositions.isEmpty()) {
      json[QStringLiteral("areaTabPositions")] = tabPositions;
    }
    if (rootSplitterRatios.size() == 6 &&
        std::all_of(rootSplitterRatios.cbegin(), rootSplitterRatios.cend(),
                    [](int ratio) { return ratio >= 0 && ratio <= 10000; }) &&
        rootSplitterRatios.at(0) + rootSplitterRatios.at(1) +
                rootSplitterRatios.at(2) ==
            10000 &&
        rootSplitterRatios.at(3) + rootSplitterRatios.at(4) +
                rootSplitterRatios.at(5) ==
            10000) {
      QJsonArray ratios;
      for (const int ratio : rootSplitterRatios) ratios.push_back(ratio);
      json[QStringLiteral("rootSplitterRatios")] = ratios;
    }
    return json;
  }

  static DockLayoutDocument fromJson(const QJsonObject &json) {
    DockLayoutDocument document;
    document.version = json.value(QStringLiteral("version")).toInt(0);
    if (document.version != kDockLayoutDocumentVersion) {
      document.entries.clear();
      return document;
    }
    document.middleMouseButtonClosesTab =
        json.value(QStringLiteral("middleMouseButtonClosesTab")).toBool(false);
    document.autoHideButtonTogglesArea =
        json.value(QStringLiteral("autoHideButtonTogglesArea")).toBool(false);
    const QJsonObject tabPositions =
        json.value(QStringLiteral("areaTabPositions")).toObject();
    for (const DockArea area : {DockArea::Left, DockArea::Right,
                                DockArea::Top, DockArea::Bottom,
                                DockArea::Center}) {
      const QString position =
          tabPositions.value(dockAreaToString(area)).toString();
      if (position.compare(QStringLiteral("bottom"), Qt::CaseInsensitive) ==
          0) {
        document.areaTabsAtBottom.insert(static_cast<int>(area), true);
      }
    }
    QList<int> rootRatios;
    for (const auto &ratio :
         json.value(QStringLiteral("rootSplitterRatios")).toArray()) {
      rootRatios.push_back(ratio.toInt(-1));
    }
    if (rootRatios.size() == 6 &&
        std::all_of(rootRatios.cbegin(), rootRatios.cend(),
                    [](int ratio) { return ratio >= 0 && ratio <= 10000; }) &&
        rootRatios.at(0) + rootRatios.at(1) + rootRatios.at(2) == 10000 &&
        rootRatios.at(3) + rootRatios.at(4) + rootRatios.at(5) == 10000) {
      document.rootSplitterRatios = rootRatios;
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

} // namespace Dock

export namespace Artifact {
using Dock::DockArea;
using Dock::DockBackendCapabilities;
using Dock::DockBackendKind;
using Dock::DockLayoutDocument;
using Dock::DockLayoutEntry;
using Dock::DockLayoutRegistry;
using Dock::canApplyDockLayoutEntry;
using Dock::dockAreaDropTargetBit;
using Dock::dockAreaFromString;
using Dock::dockAreaToString;
using Dock::dockBackendKindFromString;
using Dock::dockBackendKindToString;
using Dock::dockLayoutEntryFromJson;
using Dock::dockLayoutEntryToJson;
using Dock::isValidDockLayoutEntry;
inline constexpr int kDockLayoutDocumentVersion = Dock::kDockLayoutDocumentVersion;
inline constexpr int kAllDockAreaDropTargets = Dock::kAllDockAreaDropTargets;
}
