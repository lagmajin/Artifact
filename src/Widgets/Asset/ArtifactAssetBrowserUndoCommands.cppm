module;

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPair>
#include <QSet>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>

#include <functional>
#include <cstddef>
#include <utility>

export module Artifact.Widgets.AssetBrowserUndoCommands;

import Artifact.Layer.Abstract;
import Artifact.Project;
import Artifact.Service.Project;
import Undo.UndoManager;

namespace Artifact {
using namespace ArtifactCore;

namespace {

bool copyAssetTree(const QString& sourcePath, const QString& destinationPath) {
  const QFileInfo sourceInfo(sourcePath);
  if (sourceInfo.isFile()) return QFile::copy(sourcePath, destinationPath);
  if (!sourceInfo.isDir() || !QDir().mkpath(destinationPath)) return false;
  const QDir sourceDir(sourcePath);
  for (const QFileInfo& entry : sourceDir.entryInfoList(QDir::NoDotAndDotDot |
                                                        QDir::AllEntries)) {
    if (!copyAssetTree(entry.absoluteFilePath(),
                       QDir(destinationPath).filePath(entry.fileName()))) {
      return false;
    }
  }
  return true;
}

void collectAssetFiles(const QString& path, QStringList& files) {
  const QFileInfo info(path);
  if (info.isFile()) {
    files.push_back(info.absoluteFilePath());
    return;
  }
  if (!info.isDir()) return;
  const QDir dir(path);
  for (const QFileInfo& entry : dir.entryInfoList(QDir::NoDotAndDotDot |
                                                  QDir::AllEntries)) {
    collectAssetFiles(entry.absoluteFilePath(), files);
  }
}

void collectAssetRegistrationMetadata(
    const ArtifactProjectPtr& project,
    const QStringList& files,
    QHash<QString, QPair<QStringList, double>>& metadata) {
  if (!project) return;
  QSet<QString> targets;
  for (const QString& file : files) {
    targets.insert(QFileInfo(file).absoluteFilePath());
  }
  std::function<void(ProjectItem*)> visit = [&](ProjectItem* item) {
    if (!item) return;
    if (item->type() == eProjectItemType::Footage) {
      auto* footage = static_cast<FootageItem*>(item);
      const QString path = QFileInfo(footage->filePath).absoluteFilePath();
      if (targets.contains(path)) {
        metadata.insert(path, qMakePair(footage->sequencePaths,
                                        footage->frameRate));
      }
    }
    for (ProjectItem* child : item->children) visit(child);
  };
  for (ProjectItem* root : project->projectItems()) visit(root);
}

} // namespace

} // namespace Artifact

export namespace Artifact {
using namespace ArtifactCore;

class AssetRegistrationCommand final : public UndoCommand {
public:
  AssetRegistrationCommand(ArtifactProjectPtr project,
                           const QString& path)
      : project_(std::move(project)), path_(path) {}

  void undo() override {
    lastOperationSucceeded_ = project_ && project_->removeAssetByPath(path_);
  }

  void redo() override {
    if (firstRedo_) {
      firstRedo_ = false;
      lastOperationSucceeded_ = true;
      return;
    }
    lastOperationSucceeded_ = static_cast<bool>(project_);
    if (project_) project_->addAssetFromPath(path_);
  }

  bool lastOperationSucceeded() const override {
    return lastOperationSucceeded_;
  }

  size_t estimatedMemoryBytes() const override {
    return sizeof(*this) + static_cast<size_t>(path_.size()) * sizeof(QChar);
  }

  QString label() const override {
    return QStringLiteral("Add Asset to Project");
  }

private:
  ArtifactProjectPtr project_;
  QString path_;
  bool firstRedo_ = true;
  bool lastOperationSucceeded_ = true;
};

class RelinkAssetCommand final : public UndoCommand {
public:
  RelinkAssetCommand(QString oldPath, QString newPath)
      : oldPath_(std::move(oldPath)), newPath_(std::move(newPath)) {}

  void undo() override {
    lastOperationSucceeded_ = false;
    if (auto* service = ArtifactProjectService::instance()) {
      lastOperationSucceeded_ = service->relinkFootageByPath(newPath_, oldPath_);
    }
  }

  void redo() override {
    if (firstRedo_) {
      firstRedo_ = false;
      lastOperationSucceeded_ = true;
      return;
    }
    lastOperationSucceeded_ = false;
    if (auto* service = ArtifactProjectService::instance()) {
      lastOperationSucceeded_ = service->relinkFootageByPath(oldPath_, newPath_);
    }
  }

  bool lastOperationSucceeded() const override {
    return lastOperationSucceeded_;
  }

  size_t estimatedMemoryBytes() const override {
    return sizeof(*this) + static_cast<size_t>(
        oldPath_.size() + newPath_.size()) * sizeof(QChar);
  }

  QString label() const override {
    return QStringLiteral("Relink Asset");
  }

private:
  QString oldPath_;
  QString newPath_;
  bool firstRedo_ = true;
  bool lastOperationSucceeded_ = true;
};

struct RelinkLayerSourceChange {
  ArtifactAbstractLayerWeak layer;
  QString propertyPath;
  QString oldPath;
  QString newPath;
};

class RelinkAssetBatchCommand final : public UndoCommand {
public:
  RelinkAssetBatchCommand(QVector<QPair<QString, QString>> changes,
                          QVector<RelinkLayerSourceChange> layerChanges)
      : changes_(std::move(changes)),
        layerChanges_(std::move(layerChanges)) {}

  void undo() override {
    lastOperationSucceeded_ = false;
    auto* service = ArtifactProjectService::instance();
    if (!service) return;
    QVector<ArtifactAbstractLayerPtr> layers;
    layers.reserve(layerChanges_.size());
    for (const auto& change : layerChanges_) {
      auto layer = change.layer.lock();
      if (!layer) return;
      layers.push_back(std::move(layer));
    }
    for (int i = layers.size() - 1; i >= 0; --i) {
      if (!layers[i]->setLayerPropertyValue(layerChanges_[i].propertyPath,
                                             layerChanges_[i].oldPath)) {
        for (int rollback = i + 1; rollback < layers.size(); ++rollback) {
          layers[rollback]->setLayerPropertyValue(
              layerChanges_[rollback].propertyPath,
              layerChanges_[rollback].newPath);
        }
        return;
      }
    }
    QVector<QPair<QString, QString>> appliedChanges;
    for (auto it = changes_.crbegin(); it != changes_.crend(); ++it) {
      if (!service->relinkFootageByPath(it->second, it->first)) {
        for (auto rollback = appliedChanges.crbegin();
             rollback != appliedChanges.crend(); ++rollback) {
          service->relinkFootageByPath(rollback->first, rollback->second);
        }
        for (int rollback = 0; rollback < layers.size(); ++rollback) {
          layers[rollback]->setLayerPropertyValue(
              layerChanges_[rollback].propertyPath,
              layerChanges_[rollback].newPath);
        }
        return;
      }
      appliedChanges.push_back(*it);
    }
    lastOperationSucceeded_ = true;
  }

  void redo() override {
    if (firstRedo_) {
      firstRedo_ = false;
      lastOperationSucceeded_ = true;
      return;
    }
    lastOperationSucceeded_ = false;
    auto* service = ArtifactProjectService::instance();
    if (!service) return;
    QVector<ArtifactAbstractLayerPtr> layers;
    layers.reserve(layerChanges_.size());
    for (const auto& change : layerChanges_) {
      auto layer = change.layer.lock();
      if (!layer) return;
      layers.push_back(std::move(layer));
    }
    QVector<QPair<QString, QString>> appliedChanges;
    for (const auto& change : changes_) {
      if (!service->relinkFootageByPath(change.first, change.second)) {
        for (auto rollback = appliedChanges.crbegin();
             rollback != appliedChanges.crend(); ++rollback) {
          service->relinkFootageByPath(rollback->second, rollback->first);
        }
        return;
      }
      appliedChanges.push_back(change);
    }
    for (int i = 0; i < layers.size(); ++i) {
      const auto& change = layerChanges_[i];
      if (!layers[i]->setLayerPropertyValue(change.propertyPath,
                                             change.newPath)) {
        for (int rollback = i - 1; rollback >= 0; --rollback) {
          layers[rollback]->setLayerPropertyValue(
              layerChanges_[rollback].propertyPath,
              layerChanges_[rollback].oldPath);
        }
        for (auto rollback = appliedChanges.crbegin();
             rollback != appliedChanges.crend(); ++rollback) {
          service->relinkFootageByPath(rollback->second, rollback->first);
        }
        return;
      }
    }
    lastOperationSucceeded_ = true;
  }

  bool lastOperationSucceeded() const override {
    return lastOperationSucceeded_;
  }

  size_t estimatedMemoryBytes() const override {
    size_t bytes = sizeof(*this);
    for (const auto& change : changes_) {
      bytes += sizeof(change) + static_cast<size_t>(
          change.first.size() + change.second.size()) * sizeof(QChar);
    }
    for (const auto& change : layerChanges_) {
      bytes += sizeof(change) + static_cast<size_t>(
          change.propertyPath.size() + change.oldPath.size() +
          change.newPath.size()) * sizeof(QChar);
    }
    return bytes;
  }

  QString label() const override { return QStringLiteral("Relink Assets"); }

private:
  QVector<QPair<QString, QString>> changes_;
  QVector<RelinkLayerSourceChange> layerChanges_;
  bool firstRedo_ = true;
  bool lastOperationSucceeded_ = true;
};

class DeleteAssetFileCommand final : public UndoCommand {
public:
  DeleteAssetFileCommand(ArtifactProjectPtr project,
                         QString path)
      : project_(std::move(project)), path_(std::move(path)) {
    const QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                         QStringLiteral("/ArtifactStudio/AssetUndo");
    QDir().mkpath(root);
    backupPath_ = QDir(root).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) +
                                      QStringLiteral(".asset"));
  }

  ~DeleteAssetFileCommand() override {
    if (QFileInfo(backupPath_).isDir()) {
      QDir(backupPath_).removeRecursively();
    } else {
      QFile::remove(backupPath_);
    }
  }

  bool lastOperationSucceeded() const override {
    return lastOperationSucceeded_;
  }

  void undo() override {
    lastOperationSucceeded_ = false;
    if (!backupReady_ || !QFileInfo::exists(backupPath_) ||
        QFileInfo::exists(path_) || !copyAssetTree(backupPath_, path_)) {
      if (QFileInfo::exists(path_)) {
        QFileInfo restoredInfo(path_);
        if (restoredInfo.isDir()) {
          QDir(path_).removeRecursively();
        } else {
          QFile::remove(path_);
        }
      }
      return;
    }
    if (project_) {
      for (const QString& file : deletedFiles_) {
        const auto registration = registrationMetadata_.value(file);
        project_->addAssetFromPath(file, registration.first, registration.second);
      }
    }
    lastOperationSucceeded_ = true;
  }

  void redo() override {
    lastOperationSucceeded_ = false;
    if (!QFileInfo::exists(path_)) return;
    if (!backupReady_) {
      if (deletedFiles_.isEmpty()) collectAssetFiles(path_, deletedFiles_);
      if (registrationMetadata_.isEmpty()) {
        collectAssetRegistrationMetadata(project_, deletedFiles_, registrationMetadata_);
      }
      if (!copyAssetTree(path_, backupPath_)) {
        if (QFileInfo(backupPath_).isDir()) {
          QDir(backupPath_).removeRecursively();
        } else {
          QFile::remove(backupPath_);
        }
        return;
      }
      backupReady_ = true;
    }
    const bool removed = QFileInfo(path_).isDir()
                             ? QDir(path_).removeRecursively()
                             : QFile::remove(path_);
    if (!removed) return;
    if (project_) {
      for (const QString& file : deletedFiles_) project_->removeAssetByPath(file);
    }
    lastOperationSucceeded_ = true;
  }

  QString label() const override {
    return QStringLiteral("Delete Asset File");
  }

private:
  ArtifactProjectPtr project_;
  QString path_;
  QString backupPath_;
  QStringList deletedFiles_;
  QHash<QString, QPair<QStringList, double>> registrationMetadata_;
  bool backupReady_ = false;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
