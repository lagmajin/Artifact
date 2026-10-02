module;
#include <algorithm>
#include <functional>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSaveFile>
#include <QStringList>
#include <QTemporaryFile>
#include <QTimer>
#include <QSet>
#include <QUuid>
#include <QVector>

module Artifact.Project.RevisionService;

import Artifact.Project;
import Artifact.Render.Queue.Service;
import Core.Diagnostics.SessionLedger;

namespace
{
constexpr qint64 kMaxRevisionLedgerBytes = 16LL * 1024LL * 1024LL;
constexpr qint64 kMaxRevisionSnapshotBytes = 256LL * 1024LL * 1024LL;
constexpr qsizetype kMaxRevisionRecords = 100000;
}

namespace Artifact {
namespace {

QString projectContextKey(const QString &projectPath, const QString &projectName) {
  const QString basis = !projectPath.trimmed().isEmpty()
                            ? QFileInfo(projectPath).absoluteFilePath()
                            : projectName.trimmed();
  const QByteArray digest =
      QCryptographicHash::hash(basis.toUtf8(), QCryptographicHash::Sha256);
  return QString::fromLatin1(digest.toHex().left(16));
}

QString compactJsonValueString(const QJsonValue &value) {
  if (value.isObject()) {
    return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
  }
  if (value.isArray()) {
    return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
  }
  if (value.isString()) {
    return value.toString();
  }
  if (value.isDouble()) {
    return QString::number(value.toDouble(), 'g', 16);
  }
  if (value.isBool()) {
    return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  }
  return QStringLiteral("null");
}

void stripVolatileFields(QJsonObject &root) {
  root.remove(QStringLiteral("savedAt"));
}

bool qJsonValueEquals(const QJsonValue &lhs, const QJsonValue &rhs) {
  if (lhs.type() != rhs.type()) {
    return false;
  }
  if (lhs.isObject()) {
    return lhs.toObject() == rhs.toObject();
  }
  if (lhs.isArray()) {
    return lhs.toArray() == rhs.toArray();
  }
  if (lhs.isString()) {
    return lhs.toString() == rhs.toString();
  }
  if (lhs.isDouble()) {
    return std::abs(lhs.toDouble() - rhs.toDouble()) < 0.000001;
  }
  if (lhs.isBool()) {
    return lhs.toBool() == rhs.toBool();
  }
  return lhs.isNull() && rhs.isNull();
}

bool setJsonValueAtPath(QJsonObject &object, const QStringList &parts, int index,
                        const QJsonValue &value) {
  if (index < 0 || index >= parts.size()) {
    return false;
  }
  const QString &key = parts.at(index);
  if (index + 1 == parts.size()) {
    if (!object.contains(key)) {
      return false;
    }
    object.insert(key, value);
    return true;
  }

  const QJsonValue childValue = object.value(key);
  if (!childValue.isObject()) {
    return false;
  }
  QJsonObject child = childValue.toObject();
  if (!setJsonValueAtPath(child, parts, index + 1, value)) {
    return false;
  }
  object.insert(key, child);
  return true;
}

// Identity key used to pair array entries. Returns an empty string when the
// entry carries no usable identity, in which case the caller falls back to
// positional comparison.
QString jsonIdentityOf(const QJsonValue &value) {
  if (!value.isObject()) {
    return {};
  }
  const QJsonObject obj = value.toObject();
  const QString id = obj.value(QStringLiteral("id")).toString().trimmed();
  if (!id.isEmpty()) {
    return QStringLiteral("id:") + id;
  }
  // Footage and other items are keyed by their logical asset identity when
  // available so a relinked path does not read as an unrelated pair.
  const QString assetId =
      obj.value(QStringLiteral("assetId")).toString().trimmed();
  if (!assetId.isEmpty()) {
    return QStringLiteral("asset:") + assetId;
  }
  return {};
}

QString jsonDisplayName(const QJsonValue &value) {
  if (!value.isObject()) {
    return {};
  }
  return value.toObject().value(QStringLiteral("name")).toString();
}

void appendDiffRecursive(const QString &path, const QJsonValue &lhs,
                         const QJsonValue &rhs, QJsonArray &out);

// Pairs entries that expose an identity field so that inserting, removing, or
// reordering one layer does not mark every following layer as modified.
// Entries without identity keep the previous positional behaviour.
void appendDiffIdentityAware(const QString &path, const QJsonArray &leftArr,
                             const QJsonArray &rightArr, QJsonArray &out) {
  auto indexByIdentity = [](const QJsonArray &arr) {
    QHash<QString, QJsonValue> map;
    for (const auto &entry : arr) {
      const QString identity = jsonIdentityOf(entry);
      if (identity.isEmpty()) {
        continue;
      }
      if (!map.contains(identity)) {
        map.insert(identity, entry);
      }
    }
    return map;
  };

  const QHash<QString, QJsonValue> leftByIdentity = indexByIdentity(leftArr);
  const QHash<QString, QJsonValue> rightByIdentity = indexByIdentity(rightArr);
  const bool identityComparable = !leftByIdentity.isEmpty() ||
                                 !rightByIdentity.isEmpty();

  if (!identityComparable) {
    const int maxCount = std::max(leftArr.size(), rightArr.size());
    for (int i = 0; i < maxCount; ++i) {
      const QString childPath = QStringLiteral("%1[%2]").arg(path, QString::number(i));
      const QJsonValue leftValue = i < leftArr.size() ? leftArr.at(i) : QJsonValue();
      const QJsonValue rightValue = i < rightArr.size() ? rightArr.at(i) : QJsonValue();
      appendDiffRecursive(childPath, leftValue, rightValue, out);
    }
    return;
  }

  QSet<QString> identities;
  for (auto it = leftByIdentity.cbegin(); it != leftByIdentity.cend(); ++it) {
    identities.insert(it.key());
  }
  for (auto it = rightByIdentity.cbegin(); it != rightByIdentity.cend(); ++it) {
    identities.insert(it.key());
  }
  QStringList sortedIdentities = identities.values();
  std::sort(sortedIdentities.begin(), sortedIdentities.end());

  for (const QString &identity : sortedIdentities) {
    const bool inLeft = leftByIdentity.contains(identity);
    const bool inRight = rightByIdentity.contains(identity);
    const QString entryName = jsonDisplayName(
        inLeft ? leftByIdentity.value(identity) : rightByIdentity.value(identity));
    const QString childPath = entryName.isEmpty()
                                  ? QStringLiteral("%1{%2}").arg(path, identity)
                                  : QStringLiteral("%1{%2: %3}")
                                        .arg(path, identity, entryName);

    if (inLeft && inRight) {
      appendDiffRecursive(childPath, leftByIdentity.value(identity),
                          rightByIdentity.value(identity), out);
    } else if (inRight) {
      QJsonObject diff;
      diff[QStringLiteral("path")] = childPath;
      diff[QStringLiteral("before")] = QJsonValue();
      diff[QStringLiteral("after")] = rightByIdentity.value(identity);
      diff[QStringLiteral("beforeText")] = QString();
      diff[QStringLiteral("afterText")] =
          compactJsonValueString(rightByIdentity.value(identity));
      diff[QStringLiteral("change")] = QStringLiteral("added");
      out.push_back(diff);
    } else {
      QJsonObject diff;
      diff[QStringLiteral("path")] = childPath;
      diff[QStringLiteral("before")] = leftByIdentity.value(identity);
      diff[QStringLiteral("after")] = QJsonValue();
      diff[QStringLiteral("beforeText")] =
          compactJsonValueString(leftByIdentity.value(identity));
      diff[QStringLiteral("afterText")] = QString();
      diff[QStringLiteral("change")] = QStringLiteral("removed");
      out.push_back(diff);
    }
  }

  // Entries without identity are compared positionally among themselves so
  // arrays that mix identified and anonymous values stay readable.
  const int leftAnonymous = leftArr.size() - static_cast<int>(leftByIdentity.size());
  const int rightAnonymous = rightArr.size() - static_cast<int>(rightByIdentity.size());
  if (leftAnonymous > 0 || rightAnonymous > 0) {
    const int maxAnonymous = std::max(leftAnonymous, rightAnonymous);
    for (int i = 0; i < maxAnonymous; ++i) {
      const QString childPath =
          QStringLiteral("%1[%2]").arg(path, QString::number(i));
      const QJsonValue leftValue = i < leftAnonymous ? leftArr.at(i) : QJsonValue();
      const QJsonValue rightValue = i < rightAnonymous ? rightArr.at(i) : QJsonValue();
      appendDiffRecursive(childPath, leftValue, rightValue, out);
    }
  }
}

void appendDiffRecursive(const QString &path, const QJsonValue &lhs,
                         const QJsonValue &rhs, QJsonArray &out) {
  if (qJsonValueEquals(lhs, rhs)) {
    return;
  }

  if (lhs.isObject() && rhs.isObject()) {
    const auto leftObj = lhs.toObject();
    const auto rightObj = rhs.toObject();
    QSet<QString> keys;
    for (const auto &key : leftObj.keys()) {
      keys.insert(key);
    }
    for (const auto &key : rightObj.keys()) {
      keys.insert(key);
    }
    auto sortedKeys = keys.values();
    std::sort(sortedKeys.begin(), sortedKeys.end());
    for (const auto &key : sortedKeys) {
      const QString childPath = path.isEmpty() ? key : path + QStringLiteral(".") + key;
      appendDiffRecursive(childPath, leftObj.value(key), rightObj.value(key), out);
    }
    return;
  }

  if (lhs.isArray() && rhs.isArray()) {
    appendDiffIdentityAware(path, lhs.toArray(), rhs.toArray(), out);
    return;
  }

  QJsonObject diff;
  diff[QStringLiteral("path")] = path;
  diff[QStringLiteral("before")] = lhs;
  diff[QStringLiteral("after")] = rhs;
  diff[QStringLiteral("beforeText")] = compactJsonValueString(lhs);
  diff[QStringLiteral("afterText")] = compactJsonValueString(rhs);
  if (lhs.isUndefined() || lhs.isNull()) {
    diff[QStringLiteral("change")] = QStringLiteral("added");
  } else if (rhs.isUndefined() || rhs.isNull()) {
    diff[QStringLiteral("change")] = QStringLiteral("removed");
  } else {
    diff[QStringLiteral("change")] = QStringLiteral("modified");
  }
  out.push_back(diff);
}

// ---- three-way merge helpers ------------------------------------------------
//
// Layer order is meaningful in a DCC timeline, so the merge reconciles
// sequence changes instead of only membership. The rules below were fixed by
// exercising a prototype before writing this version.
//
//  * Membership is tracked by identity (id, else assetId).
//  * A reorder counts as an edit. When exactly one side reordered the shared
//    set, that side wins.
//  * An addition is anchored to the element that preceded it on the side that
//    added it. When a reorder and an insertion cross, no answer is objectively
//    correct; this rule at least stays deterministic.
//  * A delete only conflicts when the surviving side also changed the entry.
//  * Conflicts resolve to ours so the result is always defined.

// ---- automatic resolution of content conflicts ------------------------------
//
// Only the "both sides changed the same path to different values" case is
// automated. delete/modify and order stay manual on purpose: a resurrected
// or silently discarded layer costs more than one extra prompt.
//
// The rules below are deliberately conservative. Anything that cannot be
// decided without guessing returns false and leaves the conflict for the user.

bool autoResolveConflictValue(const QJsonValue &base, const QJsonValue &ours,
                              const QJsonValue &theirs, QJsonValue &resolved,
                              QString &reason) {
  if (qJsonValueEquals(ours, theirs)) {
    resolved = ours;
    reason = QStringLiteral("identical");
    return true;
  }
  if (qJsonValueEquals(ours, base)) {
    resolved = theirs;
    reason = QStringLiteral("theirs-only");
    return true;
  }
  if (qJsonValueEquals(theirs, base)) {
    resolved = ours;
    reason = QStringLiteral("ours-only");
    return true;
  }

  const bool numeric = base.isDouble() && ours.isDouble() && theirs.isDouble();
  if (numeric) {
    const double baseValue = base.toDouble();
    const double oursValue = ours.toDouble();
    const double theirsValue = theirs.toDouble();

    // Two artists dragging a position slider end up a fraction apart. A
    // relative tolerance keeps that out of the conflict list; ours wins the
    // tie so repeated merges stay reproducible.
    const double scale =
        std::max({std::abs(baseValue), std::abs(oursValue),
                  std::abs(theirsValue), 1.0});
    if (std::abs(oursValue - theirsValue) <= 1e-6 * scale) {
      resolved = ours;
      reason = QStringLiteral("numeric-tolerance");
      return true;
    }

    // Both moved the same way from base, so neither endpoint is more
    // justified. Keep the larger edit instead of dropping it silently.
    const auto direction = [](double from, double to) {
      if (to > from) {
        return 1;
      }
      return to < from ? -1 : 0;
    };
    const int oursDirection = direction(baseValue, oursValue);
    const int theirsDirection = direction(baseValue, theirsValue);
    if (oursDirection != 0 && oursDirection == theirsDirection) {
      const double oursDistance = std::abs(oursValue - baseValue);
      const double theirsDistance = std::abs(theirsValue - baseValue);
      resolved = oursDistance >= theirsDistance ? ours : theirs;
      reason = QStringLiteral("same-direction");
      return true;
    }
    return false;
  }

  if (ours.isString() && theirs.isString()) {
    // Whitespace-only differences are not an editing disagreement.
    if (ours.toString().trimmed() == theirs.toString().trimmed()) {
      resolved = ours;
      reason = QStringLiteral("string-ignoring-space");
      return true;
    }
    // A blanked field on one side is ambiguous: it can mean "reset to
    // default" or "mid-edit". Leave it to the user.
    return false;
  }

  if (ours.isBool() && theirs.isBool()) {
    // A flag flipped both ways has no majority rule.
    return false;
  }

  return false;
}

QJsonObject mergeById(const QJsonObject &baseMap, const QJsonObject &oursMap,
                      const QJsonObject &theirsMap, const QString &path,
                      QVector<RevisionMergeConflict> &conflicts,
                      QVector<QPair<QString, QString>> &autoResolvedOut) {
  QSet<QString> keys;
  for (const auto &key : baseMap.keys()) {
    keys.insert(key);
  }
  for (const auto &key : oursMap.keys()) {
    keys.insert(key);
  }
  for (const auto &key : theirsMap.keys()) {
    keys.insert(key);
  }
  QStringList sortedKeys = keys.values();
  std::sort(sortedKeys.begin(), sortedKeys.end());

  QJsonObject merged;
  for (const QString &key : sortedKeys) {
    const QJsonValue baseValue = baseMap.value(key);
    const QJsonValue oursValue = oursMap.value(key);
    const QJsonValue theirsValue = theirsMap.value(key);
    const bool oursChanged = !qJsonValueEquals(oursValue, baseValue);
    const bool theirsChanged = !qJsonValueEquals(theirsValue, baseValue);

    QJsonValue chosen = oursValue;
    if (oursChanged && theirsChanged && !qJsonValueEquals(oursValue, theirsValue)) {
      QJsonValue autoResolved;
      QString reason;
      if (autoResolveConflictValue(baseValue, oursValue, theirsValue,
                                   autoResolved, reason)) {
        chosen = autoResolved;
        // "ours-only" / "theirs-only" / "identical" are the ordinary case of
        // one side moving or both agreeing, not a surprise. Only record the
        // rules a user would want explained.
        if (reason != QStringLiteral("ours-only") &&
            reason != QStringLiteral("theirs-only") &&
            reason != QStringLiteral("identical")) {
          autoResolvedOut.push_back({path + QLatin1String(".") + key, reason});
        }
      } else {
        RevisionMergeConflict conflict;
        conflict.kind = RevisionMergeConflict::Kind::Content;
        conflict.path = path + QLatin1String(".") + key;
        conflict.oursLayerId = key;
        conflict.theirsLayerId = key;
        conflict.baseValue = baseValue;
        conflict.oursValue = oursValue;
        conflict.theirsValue = theirsValue;
        conflicts.push_back(conflict);
        chosen = oursValue;
      }
    } else if (oursChanged) {
      chosen = oursValue;
    } else if (theirsChanged) {
      chosen = theirsValue;
    }

    if (chosen.isUndefined() || chosen.isNull()) {
      continue;
    }
    merged.insert(key, chosen);
  }
  return merged;
}

// Recomputes a merged identity array from the merged content map. Items only
// present in one side keep the position that side asked for.
QJsonArray mergeIdentityOrder(const QJsonArray &baseArr,
                              const QJsonArray &oursArr,
                              const QJsonArray &theirsArr,
                              const QString &path,
                              QVector<RevisionMergeConflict> &conflicts) {
  const auto toIds = [](const QJsonArray &arr) {
    QStringList ids;
    for (const auto &entry : arr) {
      const QString id = jsonIdentityOf(entry);
      if (!id.isEmpty()) {
        ids.push_back(id);
      }
    }
    return ids;
  };

  const QStringList baseIds = toIds(baseArr);
  const QStringList oursIds = toIds(oursArr);
  const QStringList theirsIds = toIds(theirsArr);
  const QSet<QString> baseSet(baseIds.cbegin(), baseIds.cend());
  const QSet<QString> oursSet(oursIds.cbegin(), oursIds.cend());
  const QSet<QString> theirsSet(theirsIds.cbegin(), theirsIds.cend());

  const auto byId = [](const QJsonArray &arr) {
    QHash<QString, QJsonObject> map;
    for (const auto &entry : arr) {
      const QString id = jsonIdentityOf(entry);
      if (!id.isEmpty() && !map.contains(id)) {
        map.insert(id, entry.toObject());
      }
    }
    return map;
  };
  const auto baseMap = byId(baseArr);
  const auto oursMap = byId(oursArr);
  const auto theirsMap = byId(theirsArr);

  QStringList addedOurs;
  for (const auto &id : oursIds) {
    if (!baseSet.contains(id)) {
      addedOurs.push_back(id);
    }
  }
  QStringList addedTheirs;
  for (const auto &id : theirsIds) {
    if (!baseSet.contains(id)) {
      addedTheirs.push_back(id);
    }
  }
  QStringList deletedOurs;
  for (const auto &id : baseIds) {
    if (!oursSet.contains(id)) {
      deletedOurs.push_back(id);
    }
  }
  QStringList deletedTheirs;
  for (const auto &id : baseIds) {
    if (!theirsSet.contains(id)) {
      deletedTheirs.push_back(id);
    }
  }

  // delete/modify: only a conflict when the surviving side also changed it.
  const auto reportDeleteModify = [&](const QStringList &deleted,
                                      const QHash<QString, QJsonObject> &survivors,
                                      const QHash<QString, QJsonObject> &other) {
    for (const auto &id : deleted) {
      if (survivors.contains(id) && other.contains(id) &&
          !qJsonValueEquals(survivors.value(id), other.value(id))) {
        RevisionMergeConflict conflict;
        conflict.kind = RevisionMergeConflict::Kind::DeleteModify;
        conflict.path = path + QStringLiteral("{id: %1}").arg(id);
        conflict.oursLayerId = survivors.contains(id) ? id : QString();
        conflict.theirsLayerId = other.contains(id) ? id : QString();
        conflict.oursValue = survivors.contains(id)
                                 ? QJsonValue(survivors.value(id))
                                 : QJsonValue();
        conflict.theirsValue = QJsonValue(other.value(id));
        conflicts.push_back(conflict);
      }
    }
  };
  reportDeleteModify(deletedTheirs, oursMap, theirsMap);
  reportDeleteModify(deletedOurs, theirsMap, oursMap);

  QStringList merged;
  for (const auto &id : baseIds) {
    if (oursSet.contains(id) && theirsSet.contains(id)) {
      merged.push_back(id);
    }
  }

  const QSet<QString> mergedSet(merged.cbegin(), merged.cend());
  const auto sharedOrder = [&](const QStringList &seq) {
    QStringList out;
    for (const auto &id : seq) {
      if (mergedSet.contains(id)) {
        out.push_back(id);
      }
    }
    return out;
  };
  QStringList baseShared = sharedOrder(baseIds);
  const QStringList oursShared = sharedOrder(oursIds);
  const QStringList theirsShared = sharedOrder(theirsIds);
  const bool oursReordered = oursShared != baseShared;
  const bool theirsReordered = theirsShared != baseShared;
  if (oursReordered && !theirsReordered) {
    merged = oursShared;
  } else if (theirsReordered && !oursReordered) {
    merged = theirsShared;
  } else if (oursReordered && theirsReordered && oursShared != theirsShared) {
    RevisionMergeConflict conflict;
    conflict.kind = RevisionMergeConflict::Kind::Order;
    conflict.path = path;
    merged = oursShared;
    conflicts.push_back(conflict);
  }

  const auto place = [&](const QStringList &items, const QStringList &ref) {
    for (const auto &id : items) {
      const int index = ref.indexOf(id);
      if (index < 0) {
        continue;
      }
      QString after;
      for (int i = index - 1; i >= 0; --i) {
        if (merged.contains(ref.at(i))) {
          after = ref.at(i);
          break;
        }
      }
      if (!after.isEmpty()) {
        merged.insert(merged.indexOf(after) + 1, id);
        continue;
      }
      QString before;
      for (int i = index + 1; i < ref.size(); ++i) {
        if (merged.contains(ref.at(i))) {
          before = ref.at(i);
          break;
        }
      }
      if (!before.isEmpty()) {
        merged.insert(merged.indexOf(before), id);
      } else {
        merged.push_back(id);
      }
    }
  };
  place(addedOurs, oursIds);
  place(addedTheirs, theirsIds);

  QJsonArray result;
  for (const auto &id : merged) {
    const auto entry = oursMap.contains(id) ? oursMap.value(id)
                                            : theirsMap.value(id);
    if (!entry.isEmpty()) {
      result.append(entry);
    }
  }
  return result;
}

QJsonObject mergeSnapshotThreeWay(const QJsonObject &base,
                                  const QJsonObject &ours,
                                  const QJsonObject &theirs,
                                  const QString &path,
                                  QVector<RevisionMergeConflict> &conflicts,
                                  QVector<QPair<QString, QString>> &autoResolvedOut) {
  QJsonObject merged;
  QSet<QString> keys;
  for (const auto &key : base.keys()) {
    keys.insert(key);
  }
  for (const auto &key : ours.keys()) {
    keys.insert(key);
  }
  for (const auto &key : theirs.keys()) {
    keys.insert(key);
  }
  QStringList sortedKeys = keys.values();
  std::sort(sortedKeys.begin(), sortedKeys.end());

  const auto byId = [](const QJsonObject &object, const QString &key) {
    QJsonObject map;
    const QJsonValue value = object.value(key);
    if (!value.isArray()) {
      return map;
    }
    for (const auto &entry : value.toArray()) {
      const QString id = jsonIdentityOf(entry);
      if (!id.isEmpty() && !map.contains(id)) {
        map.insert(id, entry.toObject());
      }
    }
    return map;
  };

  for (const QString &key : sortedKeys) {
    const QString childPath =
        path.isEmpty() ? key : path + QLatin1Char('.') + key;
    const QJsonValue baseValue = base.value(key);
    const QJsonValue oursValue = ours.value(key);
    const QJsonValue theirsValue = theirs.value(key);

    if (baseValue.isArray() || oursValue.isArray() || theirsValue.isArray()) {
      merged.insert(
          key, mergeIdentityOrder(baseValue.isArray() ? baseValue.toArray() : QJsonArray(),
                                  oursValue.isArray() ? oursValue.toArray() : QJsonArray(),
                                  theirsValue.isArray() ? theirsValue.toArray() : QJsonArray(),
                                  childPath, conflicts));
      continue;
    }

    if (baseValue.isObject() || oursValue.isObject() ||
        theirsValue.isObject()) {
      const QJsonObject mergedObject =
          mergeById(baseValue.isObject() ? baseValue.toObject() : QJsonObject(),
                    oursValue.isObject() ? oursValue.toObject() : QJsonObject(),
                    theirsValue.isObject() ? theirsValue.toObject() : QJsonObject(),
                    childPath, conflicts, autoResolvedOut);
      if (!mergedObject.isEmpty()) {
        merged.insert(key, mergedObject);
      }
      continue;
    }

    if (qJsonValueEquals(oursValue, theirsValue)) {
      if (!oursValue.isUndefined() && !oursValue.isNull()) {
        merged.insert(key, oursValue);
      }
      continue;
    }
    if (qJsonValueEquals(oursValue, baseValue)) {
      if (!theirsValue.isUndefined() && !theirsValue.isNull()) {
        merged.insert(key, theirsValue);
      }
      continue;
    }
    if (qJsonValueEquals(theirsValue, baseValue)) {
      if (!oursValue.isUndefined() && !oursValue.isNull()) {
        merged.insert(key, oursValue);
      }
      continue;
    }
    QJsonValue autoResolved;
    QString autoReason;
    if (autoResolveConflictValue(baseValue, oursValue, theirsValue,
                                 autoResolved, autoReason)) {
      if (!autoResolved.isUndefined() && !autoResolved.isNull()) {
        merged.insert(key, autoResolved);
      }
      if (autoReason != QStringLiteral("ours-only") &&
          autoReason != QStringLiteral("theirs-only") &&
          autoReason != QStringLiteral("identical")) {
        autoResolvedOut.push_back({childPath, autoReason});
      }
      continue;
    }
    RevisionMergeConflict conflict;
    conflict.kind = RevisionMergeConflict::Kind::Content;
    conflict.path = childPath;
    conflict.baseValue = baseValue;
    conflict.oursValue = oursValue;
    conflict.theirsValue = theirsValue;
    conflicts.push_back(conflict);
    if (!oursValue.isUndefined() && !oursValue.isNull()) {
      merged.insert(key, oursValue);
    }
  }
  return merged;
}

QJsonObject projectSnapshotAsJson(const ArtifactProjectPtr &project) {
  if (!project) {
    return {};
  }
  QJsonObject root = project->toJson();
  stripVolatileFields(root);
  return root;
}

} // namespace

class ArtifactRevisionService::Impl {
public:
  QString activeProjectKey_;
  QString activeProjectPath_;
  QString activeProjectName_;
  QString storageRoot_;
  QString revisionsRoot_;
  QString ledgerPath_;
  QString headPath_;
  QString branchesPath_;
  QVector<RevisionBranchRef> branches_;
  QString currentBranchId_;
  QVector<ProjectRevisionRecord> revisions_;
  QString headRevisionId_;
  bool autoCommitEnabled_ = true;
  bool autoCommitSuspended_ = false;
  bool dirtySinceLastCommit_ = false;
  int autoCommitDelayMs_ = 1500;
  int autoCommitGeneration_ = 0;
  QByteArray lastCommittedSnapshotHash_;

  QString baseStorageRoot() const {
    QString root =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) {
      root = QDir::homePath();
    }
    QDir dir(root);
    dir.mkpath(QStringLiteral("ArtifactVCS"));
    return dir.filePath(QStringLiteral("ArtifactVCS"));
  }

  QString computeStorageRoot(const QString &projectKey) const {
    QDir dir(baseStorageRoot());
    dir.mkpath(projectKey);
    return dir.filePath(projectKey);
  }

  QString revisionsDirFor(const QString &projectKey) const {
    QDir dir(computeStorageRoot(projectKey));
    dir.mkpath(QStringLiteral("revisions"));
    return dir.filePath(QStringLiteral("revisions"));
  }

  QString ledgerFileFor(const QString &projectKey) const {
    QDir dir(computeStorageRoot(projectKey));
    return dir.filePath(QStringLiteral("ledger.jsonl"));
  }

  QString legacyLedgerFileFor(const QString &projectKey) const {
    QDir dir(computeStorageRoot(projectKey));
    return dir.filePath(QStringLiteral("ledger.json"));
  }

  // The head pointer changes on every commit and on restore, so it lives in a
  // small separate file that is rewritten wholesale. The revision list itself
  // is append-only, which keeps commit cost independent of history size.
  QString headFileFor(const QString &projectKey) const {
    QDir dir(computeStorageRoot(projectKey));
    return dir.filePath(QStringLiteral("head.json"));
  }

  // Branch refs are rewritten wholesale but stay small and change far less
  // often than commits, so they do not need an append-only format.
  QString branchesFileFor(const QString &projectKey) const {
    QDir dir(computeStorageRoot(projectKey));
    return dir.filePath(QStringLiteral("branches.json"));
  }

  QString snapshotFileFor(const QString &revisionId) const {
    QDir dir(revisionsRoot_);
    return dir.filePath(revisionId + QStringLiteral(".json"));
  }

  void ensureContext() {
    auto *manager = &ArtifactProjectManager::getInstance();
    const auto project = manager->getCurrentProjectSharedPtr();
    const QString projectPath =
        !manager->currentProjectRootPath().trimmed().isEmpty()
            ? manager->currentProjectRootPath()
            : manager->currentProjectPath();
    const QString projectName = project ? project->settings().projectName() : QString();
    const QString key = projectContextKey(projectPath, projectName);
    if (key == activeProjectKey_) {
      return;
    }

    activeProjectKey_ = key;
    activeProjectPath_ = projectPath;
    activeProjectName_ = projectName;
    storageRoot_ = computeStorageRoot(key);
    revisionsRoot_ = revisionsDirFor(key);
    ledgerPath_ = ledgerFileFor(key);
    headPath_ = headFileFor(key);
    branchesPath_ = branchesFileFor(key);
    revisions_.clear();
    branches_.clear();
    currentBranchId_.clear();
    headRevisionId_.clear();
    lastCommittedSnapshotHash_.clear();
    loadLedger();
    loadBranches();
    ensureDefaultBranch();
  }

  static QJsonObject branchToJson(const RevisionBranchRef &branch) {
    QJsonObject obj;
    obj[QStringLiteral("id")] = branch.id;
    obj[QStringLiteral("displayName")] = branch.displayName;
    obj[QStringLiteral("headRevisionId")] = branch.headRevisionId;
    obj[QStringLiteral("baseRevisionId")] = branch.baseRevisionId;
    obj[QStringLiteral("createdAt")] =
        branch.createdAt.toUTC().toString(Qt::ISODateWithMs);
    obj[QStringLiteral("updatedAt")] =
        branch.updatedAt.toUTC().toString(Qt::ISODateWithMs);
    return obj;
  }

  void loadBranches() {
    QFile file(branchesPath_);
    if (file.size() <= 0 || !file.open(QIODevice::ReadOnly)) {
      return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
      return;
    }
    const QJsonObject root = doc.object();
    currentBranchId_ =
        root.value(QStringLiteral("currentBranchId")).toString();
    const QJsonArray branches = root.value(QStringLiteral("branches")).toArray();
    branches_.clear();
    branches_.reserve(branches.size());
    for (const auto &value : branches) {
      if (!value.isObject()) {
        continue;
      }
      const QJsonObject obj = value.toObject();
      RevisionBranchRef branch;
      branch.id = obj.value(QStringLiteral("id")).toString();
      branch.displayName = obj.value(QStringLiteral("displayName")).toString();
      branch.headRevisionId = obj.value(QStringLiteral("headRevisionId")).toString();
      branch.baseRevisionId = obj.value(QStringLiteral("baseRevisionId")).toString();
      branch.createdAt = QDateTime::fromString(
          obj.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
      branch.updatedAt = QDateTime::fromString(
          obj.value(QStringLiteral("updatedAt")).toString(), Qt::ISODateWithMs);
      if (!branch.id.isEmpty()) {
        branches_.push_back(branch);
      }
    }
  }

  bool saveBranches() const {
    QDir dir(storageRoot_);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
      return false;
    }
    QJsonArray branchesArray;
    for (const auto &branch : branches_) {
      branchesArray.push_back(branchToJson(branch));
    }
    QJsonObject root;
    root[QStringLiteral("version")] = QStringLiteral("1");
    root[QStringLiteral("currentBranchId")] = currentBranchId_;
    root[QStringLiteral("branches")] = branchesArray;
    QSaveFile file(branchesPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return false;
    }
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (file.write(payload) != payload.size()) {
      file.cancelWriting();
      return false;
    }
    return file.commit();
  }

  const ProjectRevisionRecord *revisionById(const QString &revisionId) const {
    for (const auto &record : revisions_) {
      if (record.id == revisionId) {
        return &record;
      }
    }
    return nullptr;
  }

  int indexOfBranch(const QString &branchId) const {
    for (int i = 0; i < branches_.size(); ++i) {
      if (branches_.at(i).id == branchId) {
        return i;
      }
    }
    return -1;
  }

  // Projects created before branch refs existed must still end up with one,
  // otherwise every branch API call would fail on an empty list.
  void ensureDefaultBranch() {
    if (!branches_.isEmpty()) {
      if (indexOfBranch(currentBranchId_) < 0) {
        currentBranchId_ = branches_.first().id;
      }
      return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    RevisionBranchRef main;
    main.id = QStringLiteral("branch-main");
    main.displayName = QStringLiteral("main");
    main.headRevisionId = headRevisionId_;
    main.createdAt = now;
    main.updatedAt = now;
    branches_.push_back(main);
    currentBranchId_ = main.id;
    saveBranches();
  }

  static ProjectRevisionRecord recordFromJson(const QJsonObject &obj) {
    ProjectRevisionRecord record;
    record.id = obj.value(QStringLiteral("id")).toString();
    record.parentId = obj.value(QStringLiteral("parentId")).toString();
    const QJsonArray parentIds =
        obj.value(QStringLiteral("parentIds")).toArray();
    for (const auto &parentValue : parentIds) {
      if (parentValue.isString()) {
        record.parentIds.push_back(parentValue.toString());
      }
    }
    if (record.parentIds.isEmpty() && !record.parentId.isEmpty()) {
      // Records written before multi-parent commits only carry parentId.
      record.parentIds.push_back(record.parentId);
    }
    record.message = obj.value(QStringLiteral("message")).toString();
    record.author = obj.value(QStringLiteral("author")).toString();
    record.projectKey = obj.value(QStringLiteral("projectKey")).toString();
    record.projectName = obj.value(QStringLiteral("projectName")).toString();
    record.snapshotFile = obj.value(QStringLiteral("snapshotFile")).toString();
    record.timestampUtc =
        QDateTime::fromString(obj.value(QStringLiteral("timestampUtc")).toString(),
                              Qt::ISODateWithMs);
    if (!record.timestampUtc.isValid()) {
      record.timestampUtc =
          QDateTime::fromString(obj.value(QStringLiteral("timestampUtc")).toString(),
                                Qt::ISODate);
    }
    record.snapshotBytes = static_cast<qint64>(
        obj.value(QStringLiteral("snapshotBytes")).toDouble());
    const QJsonArray tags = obj.value(QStringLiteral("tags")).toArray();
    record.tags.reserve(tags.size());
    for (const auto &tagValue : tags) {
      if (tagValue.isString()) {
        record.tags.push_back(tagValue.toString());
      }
    }
    return record;
  }

  static QJsonObject recordToJson(const ProjectRevisionRecord &record) {
    QJsonObject obj;
    obj[QStringLiteral("id")] = record.id;
    obj[QStringLiteral("parentId")] = record.parentId;
    QJsonArray parentIds;
    for (const auto &parent : record.parentIds) {
      parentIds.push_back(parent);
    }
    obj[QStringLiteral("parentIds")] = parentIds;
    obj[QStringLiteral("message")] = record.message;
    obj[QStringLiteral("author")] = record.author;
    obj[QStringLiteral("projectKey")] = record.projectKey;
    obj[QStringLiteral("projectName")] = record.projectName;
    obj[QStringLiteral("snapshotFile")] = record.snapshotFile;
    obj[QStringLiteral("timestampUtc")] =
        record.timestampUtc.toUTC().toString(Qt::ISODateWithMs);
    obj[QStringLiteral("snapshotBytes")] =
        static_cast<double>(record.snapshotBytes);
    QJsonArray tagsArray;
    for (const auto &tag : record.tags) {
      tagsArray.push_back(tag);
    }
    obj[QStringLiteral("tags")] = tagsArray;
    return obj;
  }

  void loadHead() {
    QFile file(headPath_);
    if (file.size() <= 0 || !file.open(QIODevice::ReadOnly)) {
      return;
    }
    const QByteArray bytes = file.readAll();
    file.close();
    const QJsonDocument doc = QJsonDocument::fromJson(bytes);
    if (!doc.isObject()) {
      return;
    }
    const QString head = doc.object().value(QStringLiteral("headRevisionId")).toString();
    if (!head.isEmpty()) {
      headRevisionId_ = head;
    }
  }

  bool saveHead() const {
    QDir dir(storageRoot_);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
      return false;
    }
    QJsonObject root;
    root[QStringLiteral("version")] = QStringLiteral("1");
    root[QStringLiteral("projectKey")] = activeProjectKey_;
    root[QStringLiteral("projectName")] = activeProjectName_;
    root[QStringLiteral("headRevisionId")] = headRevisionId_;
    QSaveFile file(headPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return false;
    }
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (file.write(payload) != payload.size()) {
      file.cancelWriting();
      return false;
    }
    return file.commit();
  }

  void loadLegacyLedger(const QString &legacyPath) {
    QFile file(legacyPath);
    if (file.size() <= 0 || file.size() > kMaxRevisionLedgerBytes ||
        !file.open(QIODevice::ReadOnly)) {
      return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
      return;
    }
    const QJsonObject root = doc.object();
    if (headRevisionId_.isEmpty()) {
      headRevisionId_ = root.value(QStringLiteral("headRevisionId")).toString();
    }
    const QJsonArray revisions = root.value(QStringLiteral("revisions")).toArray();
    if (revisions.size() > kMaxRevisionRecords) {
      return;
    }
    revisions_.reserve(revisions_.size() + revisions.size());
    for (const auto &value : revisions) {
      if (!value.isObject()) {
        continue;
      }
      revisions_.push_back(recordFromJson(value.toObject()));
    }
  }

  void loadLedger() {
    loadHead();

    // Append-only ledger: one revision record per line.
    bool loadedAny = false;
    {
      QFile file(ledgerPath_);
      if (file.size() > 0 && file.size() <= kMaxRevisionLedgerBytes &&
          file.open(QIODevice::ReadOnly)) {
        QByteArray payload = file.readAll();
        file.close();
        // A truncated final line is expected after a crash mid-append; every
        // complete line before it is still valid, so only that line is dropped.
        qsizetype lastNewline = payload.lastIndexOf('\n');
        if (lastNewline >= 0) {
          payload.truncate(lastNewline + 1);
        }
        const QList<QByteArray> lines = payload.split('\n');
        int accepted = 0;
        for (const QByteArray &line : lines) {
          if (line.trimmed().isEmpty()) {
            continue;
          }
          const QJsonDocument doc = QJsonDocument::fromJson(line);
          if (!doc.isObject()) {
            continue;
          }
          const ProjectRevisionRecord record = recordFromJson(doc.object());
          if (record.id.isEmpty()) {
            continue;
          }
          revisions_.push_back(record);
          ++accepted;
          if (accepted >= kMaxRevisionRecords) {
            break;
          }
        }
        loadedAny = accepted > 0;
      }
    }

    if (!loadedAny) {
      // First run after the format change: adopt the legacy single-document
      // ledger so existing history is not lost, then rewrite it as lines.
      const QString legacyPath = legacyLedgerFileFor(activeProjectKey_);
      if (QFile::exists(legacyPath)) {
        loadLegacyLedger(legacyPath);
        if (!revisions_.isEmpty()) {
          appendRevisions(revisions_, /*rewriteExisting=*/true);
          saveHead();
        }
      }
    }

    if (!revisions_.isEmpty()) {
      const auto &latest = revisions_.last();
      QFile file(snapshotFileFor(latest.snapshotFile));
      if (file.size() > 0 && file.size() <= kMaxRevisionSnapshotBytes &&
          file.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = file.readAll();
        file.close();
        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        if (doc.isObject()) {
          lastCommittedSnapshotHash_ =
              QCryptographicHash::hash(
                  QJsonDocument(doc.object()).toJson(QJsonDocument::Compact),
                  QCryptographicHash::Sha256)
                  .toHex();
        } else {
          lastCommittedSnapshotHash_ = QCryptographicHash::hash(
                                            bytes, QCryptographicHash::Sha256)
                                            .toHex();
        }
      }
    }
  }

  bool appendRevisions(const QVector<ProjectRevisionRecord> &records,
                       bool rewriteExisting) const {
    if (records.isEmpty()) {
      return true;
    }
    QDir dir(storageRoot_);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
      return false;
    }
    QByteArray payload;
    for (const auto &record : records) {
      payload += QJsonDocument(recordToJson(record)).toJson(QJsonDocument::Compact);
      payload += '\n';
    }
    if (rewriteExisting) {
      QSaveFile file(ledgerPath_);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
      }
      if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return false;
      }
      return file.commit();
    }

    QFile file(ledgerPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
      return false;
    }
    // A previous crash may have left a partially written line. Appending
    // straight onto it would splice the new record into that broken line and
    // lose both, so terminate the partial line first.
    if (file.size() > 0) {
      if (!file.seek(file.size() - 1)) {
        file.close();
        return false;
      }
      const QByteArray lastByte = file.read(1);
      if (lastByte != QByteArray("\n")) {
        if (file.write("\n") != 1) {
          file.close();
          return false;
        }
      }
    }
    const bool ok = file.write(payload) == payload.size();
    file.close();
    return ok;
  }

  ProjectRevisionRecord makeRecord(const QJsonObject &snapshot,
                                   const QString &message,
                                   const QString &author,
                                   const QStringList &tags) const {
    ProjectRevisionRecord record;
    record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.parentId = headRevisionId_;
    if (!headRevisionId_.isEmpty()) {
      record.parentIds.push_back(headRevisionId_);
    }
    record.message = message.isEmpty() ? QStringLiteral("Snapshot") : message;
    record.author = author;
    record.projectKey = activeProjectKey_;
    record.projectName = activeProjectName_;
    record.timestampUtc = QDateTime::currentDateTimeUtc();
    record.tags = tags;
    record.snapshotFile = record.id + QStringLiteral(".json");
    record.snapshotBytes = QJsonDocument(snapshot).toJson(QJsonDocument::Compact).size();
    return record;
  }

  bool writeSnapshot(const QJsonObject &snapshot, const QString &snapshotFile) const {
    QDir dir(revisionsRoot_);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
      return false;
    }
    QSaveFile file(dir.filePath(snapshotFile));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return false;
    }
    const QByteArray payload = QJsonDocument(snapshot).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
      file.cancelWriting();
      return false;
    }
    return file.commit();
  }

  std::optional<QJsonObject> loadSnapshotById(const QString &revisionId) const {
    const auto record = std::find_if(revisions_.cbegin(), revisions_.cend(),
                                     [&revisionId](const ProjectRevisionRecord &entry) {
                                       return entry.id == revisionId;
                                     });
    if (record == revisions_.cend()) {
      return std::nullopt;
    }
    QFile file(snapshotFileFor(record->snapshotFile));
    if (file.size() <= 0 || file.size() > kMaxRevisionSnapshotBytes ||
        !file.open(QIODevice::ReadOnly)) {
      return std::nullopt;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
      return std::nullopt;
    }
    return doc.object();
  }
};

ArtifactRevisionService::ArtifactRevisionService(QObject *parent)
    : QObject(parent), impl_(std::make_unique<Impl>()) {}

ArtifactRevisionService::~ArtifactRevisionService() = default;

ArtifactRevisionService *ArtifactRevisionService::instance() {
  static ArtifactRevisionService service;
  return &service;
}

void ArtifactRevisionService::setAutoCommitEnabled(bool enabled) {
  impl_->autoCommitEnabled_ = enabled;
}

bool ArtifactRevisionService::autoCommitEnabled() const {
  return impl_->autoCommitEnabled_;
}

void ArtifactRevisionService::setAutoCommitDelayMs(int delayMs) {
  impl_->autoCommitDelayMs_ = std::max(0, delayMs);
}

int ArtifactRevisionService::autoCommitDelayMs() const {
  return impl_->autoCommitDelayMs_;
}

void ArtifactRevisionService::suspendAutoCommit(bool suspend) {
  if (suspend && !impl_->autoCommitSuspended_) {
    ++impl_->autoCommitGeneration_;
    impl_->dirtySinceLastCommit_ = false;
  }
  impl_->autoCommitSuspended_ = suspend;
}

bool ArtifactRevisionService::autoCommitSuspended() const {
  return impl_->autoCommitSuspended_;
}

QString ArtifactRevisionService::projectKey() const {
  impl_->ensureContext();
  return impl_->activeProjectKey_;
}

QString ArtifactRevisionService::storageRoot() const {
  impl_->ensureContext();
  return impl_->storageRoot_;
}

QString ArtifactRevisionService::revisionsRoot() const {
  impl_->ensureContext();
  return impl_->revisionsRoot_;
}

QVector<ProjectRevisionRecord> ArtifactRevisionService::revisions() const {
  impl_->ensureContext();
  return impl_->revisions_;
}

QStringList ArtifactRevisionService::revisionIds() const {
  impl_->ensureContext();
  QStringList ids;
  ids.reserve(impl_->revisions_.size());
  for (const auto &record : impl_->revisions_) {
    ids.push_back(record.id);
  }
  return ids;
}

QString ArtifactRevisionService::headRevisionId() const {
  impl_->ensureContext();
  return impl_->headRevisionId_;
}

QVector<RevisionBranchRef> ArtifactRevisionService::branches() const {
  impl_->ensureContext();
  return impl_->branches_;
}

QString ArtifactRevisionService::currentBranchId() const {
  impl_->ensureContext();
  return impl_->currentBranchId_;
}

bool ArtifactRevisionService::setCurrentBranchId(const QString &branchId) {
  impl_->ensureContext();
  if (impl_->indexOfBranch(branchId) < 0) {
    return false;
  }
  impl_->currentBranchId_ = branchId;
  return impl_->saveBranches();
}

bool ArtifactRevisionService::createBranch(const QString &displayName,
                                           const QString &fromRevisionId,
                                           const QString &branchId) {
  impl_->ensureContext();
  const QString name = displayName.trimmed();
  if (name.isEmpty()) {
    return false;
  }
  const QString startRevision =
      fromRevisionId.trimmed().isEmpty() ? impl_->headRevisionId_
                                         : fromRevisionId.trimmed();
  if (!startRevision.isEmpty() && !impl_->revisionById(startRevision)) {
    return false;
  }
  QString id = branchId.trimmed();
  if (id.isEmpty()) {
    id = QStringLiteral("branch-%1")
             .arg(QString::fromLatin1(
                 QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8()
                     .toHex()
                     .left(8)));
  } else if (impl_->indexOfBranch(id) >= 0) {
    return false;
  }

  const QDateTime now = QDateTime::currentDateTimeUtc();
  RevisionBranchRef branch;
  branch.id = id;
  branch.displayName = name;
  branch.headRevisionId = startRevision;
  branch.baseRevisionId = startRevision;
  branch.createdAt = now;
  branch.updatedAt = now;
  impl_->branches_.push_back(branch);
  return impl_->saveBranches();
}

bool ArtifactRevisionService::renameBranch(const QString &branchId,
                                           const QString &displayName) {
  impl_->ensureContext();
  const int index = impl_->indexOfBranch(branchId);
  if (index < 0 || displayName.trimmed().isEmpty()) {
    return false;
  }
  impl_->branches_[index].displayName = displayName.trimmed();
  impl_->branches_[index].updatedAt = QDateTime::currentDateTimeUtc();
  return impl_->saveBranches();
}

bool ArtifactRevisionService::moveBranchHead(const QString &branchId,
                                             const QString &revisionId) {
  impl_->ensureContext();
  const int index = impl_->indexOfBranch(branchId);
  if (index < 0 || !impl_->revisionById(revisionId)) {
    return false;
  }
  impl_->branches_[index].headRevisionId = revisionId;
  impl_->branches_[index].updatedAt = QDateTime::currentDateTimeUtc();
  return impl_->saveBranches();
}

bool ArtifactRevisionService::deleteBranch(const QString &branchId) {
  impl_->ensureContext();
  const int index = impl_->indexOfBranch(branchId);
  if (index < 0) {
    return false;
  }
  // Keep at least one branch so the project always has a valid current ref.
  if (impl_->branches_.size() <= 1) {
    return false;
  }
  impl_->branches_.remove(index);
  if (impl_->currentBranchId_ == branchId) {
    impl_->currentBranchId_ = impl_->branches_.first().id;
  }
  return impl_->saveBranches();
}

QStringList ArtifactRevisionService::ancestorRevisions(
    const QString &revisionId) const {
  impl_->ensureContext();
  QStringList result;
  QStringList pending;
  pending.push_back(revisionId);
  QSet<QString> visited;
  while (!pending.isEmpty()) {
    const QString current = pending.takeFirst();
    if (current.isEmpty() || visited.contains(current)) {
      continue;
    }
    visited.insert(current);
    const ProjectRevisionRecord *record = impl_->revisionById(current);
    if (!record) {
      continue;
    }
    result.push_back(current);
    // Multi-parent commits make this a walk of a DAG rather than a chain.
    if (!record->parentIds.isEmpty()) {
      pending += record->parentIds;
    } else if (!record->parentId.isEmpty()) {
      pending.push_back(record->parentId);
    }
  }
  return result;
}

QStringList ArtifactRevisionService::commonAncestorRevisions(
    const QString &leftRevisionId, const QString &rightRevisionId) const {
  const QStringList leftAncestors = ancestorRevisions(leftRevisionId);
  const QSet<QString> leftSet = QSet<QString>(leftAncestors.cbegin(),
                                               leftAncestors.cend());
  QStringList common;
  const QStringList rightAncestors = ancestorRevisions(rightRevisionId);
  for (const auto &candidate : rightAncestors) {
    if (leftSet.contains(candidate)) {
      common.push_back(candidate);
    }
  }
  return common;
}

RevisionMergeResult ArtifactRevisionService::mergeRevisions(
    const QString &oursRevisionId, const QString &theirsRevisionId,
    const QStringList &oursPreferredPaths) {
  impl_->ensureContext();
  RevisionMergeResult result;

  const auto oursSnapshot = impl_->loadSnapshotById(oursRevisionId);
  const auto theirsSnapshot = impl_->loadSnapshotById(theirsRevisionId);
  if (!oursSnapshot.has_value() || !theirsSnapshot.has_value()) {
    return result;
  }

  // The merge base is the nearest common ancestor. ancestorRevisions() walks
  // breadth-first, so the closest ancestor is the one whose distance to the
  // left revision is smallest; scanning the list for the first candidate that
  // is also reachable keeps the merge shallow, which is what a linear snapshot
  // history produced by this service expects.
  const QStringList commonAncestors =
      commonAncestorRevisions(oursRevisionId, theirsRevisionId);
  QJsonObject base;
  QString baseRevisionId;
  for (const auto &candidate : commonAncestors) {
    const auto baseSnapshot = impl_->loadSnapshotById(candidate);
    if (baseSnapshot.has_value()) {
      base = baseSnapshot.value();
      baseRevisionId = candidate;
      break;
    }
  }

  QVector<RevisionMergeConflict> conflicts;
  QVector<QPair<QString, QString>> autoResolvedOut;
  QJsonObject merged = mergeSnapshotThreeWay(
      base, oursSnapshot.value(), theirsSnapshot.value(), QString(),
      conflicts, autoResolvedOut);

  if (!oursPreferredPaths.isEmpty()) {
    // The caller resolved some conflicts by taking theirs. Re-apply those
    // paths on top of the ours-first merge result.
    const QSet<QString> preferred(oursPreferredPaths.cbegin(),
                                  oursPreferredPaths.cend());
    for (const auto &conflict : std::as_const(conflicts)) {
      if (!preferred.contains(conflict.path)) {
        continue;
      }
      const QStringList parts = conflict.path.split(QLatin1Char('.'));
      setJsonValueAtPath(merged, parts, 0, conflict.theirsValue);
    }
  }

  result.ok = true;
  result.mergeBaseRevisionId = baseRevisionId;
  result.mergedSnapshot = merged;
  result.conflicts = conflicts;
  result.autoResolved = autoResolvedOut;
  return result;
}

bool ArtifactRevisionService::mergeRevisionIntoProject(
    const QString &oursRevisionId, const QString &theirsRevisionId,
    const QStringList &resolvedPaths, const QString &message) {
  impl_->ensureContext();
  const RevisionMergeResult result =
      mergeRevisions(oursRevisionId, theirsRevisionId, resolvedPaths);
  if (!result.ok || result.mergedSnapshot.isEmpty()) {
    return false;
  }

  const QString commitMessage =
      message.isEmpty()
          ? QStringLiteral("Merge %1 into %2")
                .arg(theirsRevisionId.left(8), oursRevisionId.left(8))
          : message;
  if (!commitMerge(result.mergedSnapshot,
                   {oursRevisionId, theirsRevisionId}, commitMessage)) {
    return false;
  }

  // Load the merged state so the editor shows the result immediately.
  auto *manager = &ArtifactProjectManager::getInstance();
  const QString previousPath = manager->currentProjectPath();
  const QString previousRoot = manager->currentProjectRootPath();
  QTemporaryFile tempFile;
  tempFile.setAutoRemove(true);
  if (!tempFile.open()) {
    return false;
  }
  tempFile.write(QJsonDocument(result.mergedSnapshot).toJson(
      QJsonDocument::Indented));
  tempFile.flush();

  ++impl_->autoCommitGeneration_;
  impl_->dirtySinceLastCommit_ = false;
  impl_->autoCommitSuspended_ = true;
  manager->loadFromFile(tempFile.fileName());
  manager->setCurrentProjectPath(previousPath);
  manager->setCurrentProjectRootPath(previousRoot);
  impl_->autoCommitSuspended_ = false;
  impl_->dirtySinceLastCommit_ = false;
  ++impl_->autoCommitGeneration_;
  impl_->lastCommittedSnapshotHash_.clear();
  return manager->getCurrentProjectSharedPtr() != nullptr;
}

bool ArtifactRevisionService::commitMerge(const QJsonObject &snapshot,
                                          const QStringList &parentRevisionIds,
                                          const QString &message,
                                          const QString &author) {
  impl_->ensureContext();
  QStringList parents;
  for (const auto &parent : parentRevisionIds) {
    const QString trimmed = parent.trimmed();
    if (trimmed.isEmpty() || parents.contains(trimmed)) {
      continue;
    }
    if (!impl_->revisionById(trimmed)) {
      return false;
    }
    parents.push_back(trimmed);
  }
  if (parents.size() < 2) {
    // A merge needs at least two distinct parents to be meaningful.
    return false;
  }
  if (snapshot.isEmpty()) {
    return false;
  }

  const QByteArray snapshotBytes =
      QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
  const QByteArray snapshotHash =
      QCryptographicHash::hash(snapshotBytes, QCryptographicHash::Sha256).toHex();
  if (snapshotHash == impl_->lastCommittedSnapshotHash_ &&
      !impl_->revisions_.isEmpty()) {
    return true;
  }

  const QString effectiveAuthor =
      author.isEmpty() ? impl_->activeProjectName_ : author;
  ProjectRevisionRecord record;
  record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  record.parentId = parents.first();
  record.parentIds = parents;
  record.message = message.isEmpty() ? QStringLiteral("Merge") : message;
  record.author = effectiveAuthor;
  record.projectKey = impl_->activeProjectKey_;
  record.projectName = impl_->activeProjectName_;
  record.timestampUtc = QDateTime::currentDateTimeUtc();
  record.tags = {QStringLiteral("merge")};
  record.snapshotFile = record.id + QStringLiteral(".json");
  record.snapshotBytes = snapshotBytes.size();

  if (!impl_->writeSnapshot(snapshot, record.snapshotFile)) {
    return false;
  }
  impl_->revisions_.push_back(record);
  impl_->headRevisionId_ = record.id;
  impl_->lastCommittedSnapshotHash_ = snapshotHash;
  impl_->dirtySinceLastCommit_ = false;

  // The merge lands on the branch of the first parent, matching the fast
  // forward behaviour of other version control systems.
  const int branchIndex = impl_->indexOfBranch(impl_->currentBranchId_);
  if (branchIndex >= 0) {
    impl_->branches_[branchIndex].headRevisionId = record.id;
    impl_->branches_[branchIndex].updatedAt = record.timestampUtc;
    impl_->saveBranches();
  }

  if (!impl_->appendRevisions({record}, /*rewriteExisting=*/false)) {
    return false;
  }
  return impl_->saveHead();
}

bool ArtifactRevisionService::commitCurrentProject(const QString &message,
                                                   const QString &author,
                                                   const QStringList &tags) {
  impl_->ensureContext();
  auto *manager = &ArtifactProjectManager::getInstance();
  const auto project = manager->getCurrentProjectSharedPtr();
  if (!project || project->isNull()) {
    return false;
  }

  QJsonObject snapshot = projectSnapshotAsJson(project);
  if (snapshot.isEmpty()) {
    return false;
  }
  const QByteArray snapshotBytes =
      QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
  const QByteArray snapshotHash =
      QCryptographicHash::hash(snapshotBytes, QCryptographicHash::Sha256).toHex();
  if (snapshotHash == impl_->lastCommittedSnapshotHash_ && !impl_->revisions_.isEmpty()) {
    return true;
  }

  const QString effectiveAuthor =
      author.isEmpty() ? project->settings().author().toQString() : author;
  const ProjectRevisionRecord record =
      impl_->makeRecord(snapshot, message, effectiveAuthor, tags);
  if (!impl_->writeSnapshot(snapshot, record.snapshotFile)) {
    return false;
  }

  impl_->revisions_.push_back(record);
  impl_->headRevisionId_ = record.id;
  impl_->lastCommittedSnapshotHash_ = snapshotHash;
  impl_->dirtySinceLastCommit_ = false;
  const int activeBranch = impl_->indexOfBranch(impl_->currentBranchId_);
  if (activeBranch >= 0) {
    impl_->branches_[activeBranch].headRevisionId = record.id;
    impl_->branches_[activeBranch].updatedAt = record.timestampUtc;
    impl_->saveBranches();
  }

  if (auto *rq = ArtifactRenderQueueService::instance()) {
    ArtifactCore::RecoveryPoint rp;
    rp.id = record.id;
    rp.timestampMs = record.timestampUtc.toMSecsSinceEpoch();
    rp.projectId = impl_->activeProjectKey_;
    rp.projectName = record.projectName;
    rp.snapshotPath = record.snapshotFile;
    rp.isAutosave = message.contains(QStringLiteral("Auto"), Qt::CaseInsensitive);
    rq->sessionLedger().addRecoveryPoint(rp);
  }

  // Only the newly created record is appended, so the write cost does not grow
  // with the number of revisions already stored.
  if (!impl_->appendRevisions({record}, /*rewriteExisting=*/false)) {
    return false;
  }
  return impl_->saveHead();
}

bool ArtifactRevisionService::restoreRevision(const QString &revisionId) {
  impl_->ensureContext();
  if (revisionId.trimmed().isEmpty()) {
    return false;
  }
  const auto snapshot = impl_->loadSnapshotById(revisionId);
  if (!snapshot.has_value()) {
    return false;
  }

  auto *manager = &ArtifactProjectManager::getInstance();
  const QString previousPath = manager->currentProjectPath();
  const QString previousRoot = manager->currentProjectRootPath();
  QTemporaryFile tempFile;
  tempFile.setAutoRemove(true);
  if (!tempFile.open()) {
    return false;
  }
  tempFile.write(QJsonDocument(snapshot.value()).toJson(QJsonDocument::Indented));
  tempFile.flush();

  ++impl_->autoCommitGeneration_;
  impl_->dirtySinceLastCommit_ = false;
  impl_->autoCommitSuspended_ = true;
  manager->loadFromFile(tempFile.fileName());
  manager->setCurrentProjectPath(previousPath);
  manager->setCurrentProjectRootPath(previousRoot);
  impl_->autoCommitSuspended_ = false;

  impl_->headRevisionId_ = revisionId;
  // A restore moves the active branch too, otherwise the ref would still
  // claim a head the user is no longer on.
  const int activeBranch = impl_->indexOfBranch(impl_->currentBranchId_);
  if (activeBranch >= 0) {
    impl_->branches_[activeBranch].headRevisionId = revisionId;
    impl_->branches_[activeBranch].updatedAt =
        QDateTime::currentDateTimeUtc();
    impl_->saveBranches();
  }
  impl_->dirtySinceLastCommit_ = false;
  ++impl_->autoCommitGeneration_;
  impl_->lastCommittedSnapshotHash_.clear();
  impl_->saveHead();
  return manager->getCurrentProjectSharedPtr() != nullptr;
}

QJsonObject ArtifactRevisionService::revisionSnapshot(const QString &revisionId) const {
  impl_->ensureContext();
  const auto snapshot = impl_->loadSnapshotById(revisionId);
  return snapshot.has_value() ? snapshot.value() : QJsonObject{};
}

QJsonObject ArtifactRevisionService::diffRevisions(const QString &leftRevisionId,
                                                   const QString &rightRevisionId) const {
  impl_->ensureContext();
  QJsonArray changes;
  const auto left = impl_->loadSnapshotById(leftRevisionId);
  const auto right = impl_->loadSnapshotById(rightRevisionId);
  if (!left.has_value() || !right.has_value()) {
    QJsonObject result;
    result[QStringLiteral("changes")] = changes;
    result[QStringLiteral("changeCount")] = 0;
    return result;
  }

  appendDiffRecursive(QString(), left.value(), right.value(), changes);
  QJsonObject result;
  result[QStringLiteral("changes")] = changes;
  result[QStringLiteral("changeCount")] = changes.size();
  result[QStringLiteral("leftRevisionId")] = leftRevisionId;
  result[QStringLiteral("rightRevisionId")] = rightRevisionId;
  return result;
}

void ArtifactRevisionService::noteProjectChanged() {
  impl_->ensureContext();
  if (!impl_->autoCommitEnabled_ || impl_->autoCommitSuspended_) {
    impl_->dirtySinceLastCommit_ = true;
    return;
  }

  impl_->dirtySinceLastCommit_ = true;
  const int generation = ++impl_->autoCommitGeneration_;
  QTimer::singleShot(impl_->autoCommitDelayMs_, this, [this, generation]() {
    if (!impl_ || generation != impl_->autoCommitGeneration_ ||
        impl_->autoCommitSuspended_ || !impl_->dirtySinceLastCommit_) {
      return;
    }
    commitCurrentProject(QStringLiteral("Auto snapshot"));
  });
}

} // namespace Artifact
