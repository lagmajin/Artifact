module;
#include <algorithm>
#include <functional>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QSet>
#include <QUuid>

module Artifact.Project.RevisionService;

import std;
import Artifact.Project;
import Artifact.Render.Queue.Service;
import Core.Diagnostics.SessionLedger;

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
    const auto leftArr = lhs.toArray();
    const auto rightArr = rhs.toArray();
    const int maxCount = std::max(leftArr.size(), rightArr.size());
    for (int i = 0; i < maxCount; ++i) {
      const QString childPath = QStringLiteral("%1[%2]").arg(path, QString::number(i));
      const QJsonValue leftValue = i < leftArr.size() ? leftArr.at(i) : QJsonValue();
      const QJsonValue rightValue = i < rightArr.size() ? rightArr.at(i) : QJsonValue();
      appendDiffRecursive(childPath, leftValue, rightValue, out);
    }
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

QJsonObject projectSnapshotAsJson(const std::shared_ptr<ArtifactProject> &project) {
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
    return dir.filePath(QStringLiteral("ledger.json"));
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
    revisions_.clear();
    headRevisionId_.clear();
    lastCommittedSnapshotHash_.clear();
    loadLedger();
  }

  void loadLedger() {
    QFile file(ledgerPath_);
    if (!file.open(QIODevice::ReadOnly)) {
      return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
      return;
    }

    const QJsonObject root = doc.object();
    headRevisionId_ = root.value(QStringLiteral("headRevisionId")).toString();
    const QJsonArray revisions = root.value(QStringLiteral("revisions")).toArray();
    revisions_.clear();
    revisions_.reserve(revisions.size());
    for (const auto &value : revisions) {
      if (!value.isObject()) {
        continue;
      }
      const QJsonObject obj = value.toObject();
      ProjectRevisionRecord record;
      record.id = obj.value(QStringLiteral("id")).toString();
      record.parentId = obj.value(QStringLiteral("parentId")).toString();
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
      revisions_.push_back(std::move(record));
    }
    if (!revisions_.isEmpty()) {
      const auto &latest = revisions_.last();
      QFile file(snapshotFileFor(latest.snapshotFile));
      if (file.open(QIODevice::ReadOnly)) {
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

  bool saveLedger() const {
    QDir dir(storageRoot_);
    if (!dir.exists()) {
      if (!dir.mkpath(QStringLiteral("."))) {
        return false;
      }
    }

    QJsonArray revisionsArray;
    for (const auto &record : revisions_) {
      QJsonObject obj;
      obj[QStringLiteral("id")] = record.id;
      obj[QStringLiteral("parentId")] = record.parentId;
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
      revisionsArray.push_back(obj);
    }

    QJsonObject root;
    root[QStringLiteral("version")] = QStringLiteral("1");
    root[QStringLiteral("projectKey")] = activeProjectKey_;
    root[QStringLiteral("projectName")] = activeProjectName_;
    root[QStringLiteral("headRevisionId")] = headRevisionId_;
    root[QStringLiteral("revisions")] = revisionsArray;

    QFile file(ledgerPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
  }

  ProjectRevisionRecord makeRecord(const QJsonObject &snapshot,
                                   const QString &message,
                                   const QString &author,
                                   const QStringList &tags) const {
    ProjectRevisionRecord record;
    record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.parentId = headRevisionId_;
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
    QFile file(dir.filePath(snapshotFile));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return false;
    }
    file.write(QJsonDocument(snapshot).toJson(QJsonDocument::Indented));
    file.close();
    return true;
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
    if (!file.open(QIODevice::ReadOnly)) {
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

  return impl_->saveLedger();
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
  impl_->dirtySinceLastCommit_ = false;
  ++impl_->autoCommitGeneration_;
  impl_->lastCommittedSnapshotHash_.clear();
  impl_->saveLedger();
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
