module;

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cstddef>
#include <memory>
#include <utility>

export module Artifact.Widgets.CompositionTrackerSessionUndoCommands;

import Artifact.Service.Project;
import Artifact.Widgets.PointTrackerGizmo;
import Tracking.MotionTracker;
import Undo.UndoManager;
import Utils.Id;

namespace Artifact {
export LayerID projectTrackerSourceLayerId(const CompositionID& compositionId,
                                    int trackerId) {
  if (compositionId.isNil()) return LayerID();
  auto* service = ArtifactProjectService::instance();
  const auto project = service ? service->getCurrentProjectSharedPtr() : nullptr;
  if (!project) return LayerID();
  const QJsonObject extensionData = project->extensionData();
  const QJsonObject bySession = extensionData
      .value(QStringLiteral("trackerSourceLayersBySession"))
      .toObject()
      .value(compositionId.toString())
      .toObject();
  if (trackerId > 0) {
    const QString sessionLayerId = bySession
        .value(QString::number(trackerId))
        .toString()
        .trimmed();
    if (!sessionLayerId.isEmpty()) return LayerID(sessionLayerId);
  }
  const QJsonObject sourceLayers = extensionData
      .value(QStringLiteral("trackerSourceLayersByComposition"))
      .toObject();
  const QString layerId = sourceLayers
      .value(compositionId.toString())
      .toString()
      .trimmed();
  return layerId.isEmpty() ? LayerID() : LayerID(layerId);
}

export void persistProjectTrackerSourceLayerId(const CompositionID& compositionId,
                                        int trackerId,
                                        const LayerID& layerId) {
  if (compositionId.isNil() || layerId.isNil()) return;
  auto* service = ArtifactProjectService::instance();
  const auto project = service ? service->getCurrentProjectSharedPtr() : nullptr;
  if (!project) return;
  QJsonObject extensionData = project->extensionData();
  QJsonObject sourceLayers = extensionData
      .value(QStringLiteral("trackerSourceLayersByComposition"))
      .toObject();
  const QString compositionKey = compositionId.toString();
  const QString layerKey = layerId.toString();
  bool changed = false;
  if (sourceLayers.value(compositionKey).toString() != layerKey) {
    sourceLayers.insert(compositionKey, layerKey);
    extensionData.insert(QStringLiteral("trackerSourceLayersByComposition"),
                         sourceLayers);
    changed = true;
  }
  if (trackerId > 0) {
    QJsonObject allSessionSources = extensionData
        .value(QStringLiteral("trackerSourceLayersBySession"))
        .toObject();
    QJsonObject sessionSources = allSessionSources
        .value(compositionKey)
        .toObject();
    const QString trackerKey = QString::number(trackerId);
    if (sessionSources.value(trackerKey).toString() != layerKey) {
      sessionSources.insert(trackerKey, layerKey);
      allSessionSources.insert(compositionKey, sessionSources);
      extensionData.insert(QStringLiteral("trackerSourceLayersBySession"),
                           allSessionSources);
      changed = true;
    }
  }
  if (!changed) return;
  project->setExtensionData(extensionData);
  project->setDirty(true);
}

export QJsonObject projectTrackerSessionManifest(const CompositionID& compositionId) {
  if (compositionId.isNil()) return {};
  auto* service = ArtifactProjectService::instance();
  const auto project = service ? service->getCurrentProjectSharedPtr() : nullptr;
  if (!project) return {};
  const QJsonObject allSessions = project->extensionData()
      .value(QStringLiteral("trackerSessionsByComposition"))
      .toObject();
  return allSessions.value(compositionId.toString()).toObject();
}

void persistProjectTrackerSessionManifest(const CompositionID& compositionId,
                                          const QJsonObject& manifest) {
  if (compositionId.isNil()) return;
  auto* service = ArtifactProjectService::instance();
  const auto project = service ? service->getCurrentProjectSharedPtr() : nullptr;
  if (!project) return;
  QJsonObject extensionData = project->extensionData();
  QJsonObject allSessions = extensionData
      .value(QStringLiteral("trackerSessionsByComposition"))
      .toObject();
  const QString compositionKey = compositionId.toString();
  if (allSessions.value(compositionKey).toObject() == manifest) return;
  allSessions.insert(compositionKey, manifest);
  extensionData.insert(QStringLiteral("trackerSessionsByComposition"),
                       allSessions);
  project->setExtensionData(extensionData);
  project->setDirty(true);
}

export void registerProjectTrackerSession(const CompositionID& compositionId,
                                   int trackerId, bool activate = true) {
  if (compositionId.isNil() || trackerId <= 0) return;
  QJsonObject manifest = projectTrackerSessionManifest(compositionId);
  QJsonArray trackerIds = manifest.value(QStringLiteral("trackerIds")).toArray();
  const QJsonValue trackerIdValue(trackerId);
  if (!trackerIds.contains(trackerIdValue)) trackerIds.append(trackerId);
  manifest.insert(QStringLiteral("trackerIds"), trackerIds);
  if (activate || manifest.value(QStringLiteral("activeTrackerId")).toInt() <= 0) {
    manifest.insert(QStringLiteral("activeTrackerId"), trackerId);
  }
  persistProjectTrackerSessionManifest(compositionId, manifest);
}

export void setProjectActiveTrackerSession(const CompositionID& compositionId,
                                    int trackerId) {
  if (compositionId.isNil() || trackerId <= 0) return;
  QJsonObject manifest = projectTrackerSessionManifest(compositionId);
  manifest.insert(QStringLiteral("activeTrackerId"), trackerId);
  persistProjectTrackerSessionManifest(compositionId, manifest);
}

export void removeProjectTrackerSession(const CompositionID& compositionId,
                                 int trackerId) {
  if (compositionId.isNil() || trackerId <= 0) return;
  QJsonObject manifest = projectTrackerSessionManifest(compositionId);
  const QJsonArray existing = manifest.value(QStringLiteral("trackerIds")).toArray();
  QJsonArray remaining;
  for (const QJsonValue& value : existing) {
    if (value.toInt() != trackerId) remaining.append(value);
  }
  manifest.insert(QStringLiteral("trackerIds"), remaining);
  if (manifest.value(QStringLiteral("activeTrackerId")).toInt() == trackerId) {
    manifest.insert(QStringLiteral("activeTrackerId"),
                    remaining.isEmpty() ? 0 : remaining.first().toInt());
  }
  QJsonObject geometryByTracker =
      manifest.value(QStringLiteral("geometryByTracker")).toObject();
  geometryByTracker.remove(QString::number(trackerId));
  manifest.insert(QStringLiteral("geometryByTracker"), geometryByTracker);
  persistProjectTrackerSessionManifest(compositionId, manifest);

  auto* service = ArtifactProjectService::instance();
  const auto project = service ? service->getCurrentProjectSharedPtr() : nullptr;
  if (project) {
    QJsonObject extensionData = project->extensionData();
    QJsonObject allSessionSources = extensionData
        .value(QStringLiteral("trackerSourceLayersBySession"))
        .toObject();
    QJsonObject sessionSources = allSessionSources
        .value(compositionId.toString())
        .toObject();
    const QString trackerKey = QString::number(trackerId);
    if (sessionSources.contains(trackerKey)) {
      sessionSources.remove(trackerKey);
      allSessionSources.insert(compositionId.toString(), sessionSources);
      extensionData.insert(QStringLiteral("trackerSourceLayersBySession"),
                           allSessionSources);
      project->setExtensionData(extensionData);
      project->setDirty(true);
    }
  }
}

export void persistProjectTrackerGeometry(
    const CompositionID& compositionId, int trackerId,
    const ArtifactPointTrackerGizmo::TrackerBoxState& state) {
  if (compositionId.isNil() || trackerId <= 0) return;
  QJsonObject manifest = projectTrackerSessionManifest(compositionId);
  QJsonObject geometryByTracker =
      manifest.value(QStringLiteral("geometryByTracker")).toObject();
  QJsonObject geometry;
  geometry.insert(QStringLiteral("centerX"), state.innerCenter.x());
  geometry.insert(QStringLiteral("centerY"), state.innerCenter.y());
  geometry.insert(QStringLiteral("innerHalfW"), state.innerHalfW);
  geometry.insert(QStringLiteral("innerHalfH"), state.innerHalfH);
  geometry.insert(QStringLiteral("outerHalfW"), state.outerHalfW);
  geometry.insert(QStringLiteral("outerHalfH"), state.outerHalfH);
  const QString trackerKey = QString::number(trackerId);
  if (geometryByTracker.value(trackerKey).toObject() == geometry) return;
  geometryByTracker.insert(trackerKey, geometry);
  manifest.insert(QStringLiteral("geometryByTracker"), geometryByTracker);
  persistProjectTrackerSessionManifest(compositionId, manifest);
}

void persistProjectTrackerSessionSourceLayerId(
    const CompositionID& compositionId, int trackerId, const LayerID& layerId) {
  if (compositionId.isNil() || trackerId <= 0) return;
  auto* service = ArtifactProjectService::instance();
  const auto project = service ? service->getCurrentProjectSharedPtr() : nullptr;
  if (!project) return;
  QJsonObject extensionData = project->extensionData();
  QJsonObject allSessionSources = extensionData
      .value(QStringLiteral("trackerSourceLayersBySession"))
      .toObject();
  QJsonObject sessionSources = allSessionSources
      .value(compositionId.toString())
      .toObject();
  const QString trackerKey = QString::number(trackerId);
  const QString layerKey = layerId.isNil() ? QString() : layerId.toString();
  if (sessionSources.value(trackerKey).toString() == layerKey) return;
  if (layerKey.isEmpty()) sessionSources.remove(trackerKey);
  else sessionSources.insert(trackerKey, layerKey);
  allSessionSources.insert(compositionId.toString(), sessionSources);
  extensionData.insert(QStringLiteral("trackerSourceLayersBySession"),
                       allSessionSources);
  project->setExtensionData(extensionData);
  project->setDirty(true);
}

export class TrackerSessionUndoCommand final : public UndoCommand {
public:
  TrackerSessionUndoCommand(CompositionID compositionId, int trackerId,
                            QString trackerJson, QJsonObject geometry,
                            LayerID sourceLayerId, bool wasCreated,
                            int previousActiveTrackerId = 0)
      : compositionId_(std::move(compositionId)), trackerId_(trackerId),
        trackerJson_(std::move(trackerJson)), geometry_(std::move(geometry)),
        sourceLayerId_(std::move(sourceLayerId)), wasCreated_(wasCreated),
        previousActiveTrackerId_(previousActiveTrackerId), firstRedo_(true) {}

  void undo() override { lastOperationSucceeded_ = setExists(!wasCreated_); }

  void redo() override {
    if (firstRedo_) {
      firstRedo_ = false;
      lastOperationSucceeded_ = true;
      return;
    }
    lastOperationSucceeded_ = setExists(wasCreated_);
  }

  bool lastOperationSucceeded() const override {
    return lastOperationSucceeded_;
  }

  QString label() const override {
    return wasCreated_ ? QStringLiteral("Create Motion Tracker")
                       : QStringLiteral("Delete Motion Tracker");
  }

  size_t estimatedMemoryBytes() const override {
    return sizeof(*this) +
           static_cast<size_t>(trackerJson_.size()) * sizeof(QChar) +
           static_cast<size_t>(geometry_.size()) * sizeof(QJsonValue);
  }

private:
  bool setExists(bool shouldExist) {
    if (compositionId_.isNil() || trackerId_ <= 0 || trackerJson_.isEmpty()) {
      return false;
    }
    auto* projectService = ArtifactProjectService::instance();
    const auto project = projectService
        ? projectService->getCurrentProjectSharedPtr()
        : nullptr;
    if (!project || !project->hasComposition(compositionId_)) return false;
    auto& manager = ArtifactCore::TrackerManager::instance();
    auto* tracker = manager.tracker(trackerId_);
    if (shouldExist) {
      if (!tracker) {
        auto restored = std::make_unique<ArtifactCore::MotionTracker>(trackerId_);
        if (!restored->fromJson(trackerJson_) ||
            restored->id() != trackerId_) {
          return false;
        }
        tracker = manager.adoptTracker(std::move(restored));
        if (!tracker) return false;
      } else if (!tracker->fromJson(trackerJson_)) {
        return false;
      }
      registerProjectTrackerSession(compositionId_, trackerId_, true);
      if (!geometry_.isEmpty()) {
        QJsonObject manifest =
            projectTrackerSessionManifest(compositionId_);
        QJsonObject geometryByTracker = manifest
            .value(QStringLiteral("geometryByTracker"))
            .toObject();
        geometryByTracker.insert(QString::number(trackerId_), geometry_);
        manifest.insert(QStringLiteral("geometryByTracker"),
                        geometryByTracker);
        persistProjectTrackerSessionManifest(compositionId_, manifest);
      }
      persistProjectTrackerSessionSourceLayerId(
          compositionId_, trackerId_, sourceLayerId_);
      return true;
    }
    if (tracker) manager.removeTracker(trackerId_);
    removeProjectTrackerSession(compositionId_, trackerId_);
    if (wasCreated_ && previousActiveTrackerId_ > 0 &&
        manager.tracker(previousActiveTrackerId_)) {
      setProjectActiveTrackerSession(compositionId_,
                                     previousActiveTrackerId_);
    }
    return true;
  }

  CompositionID compositionId_;
  int trackerId_ = 0;
  QString trackerJson_;
  QJsonObject geometry_;
  LayerID sourceLayerId_;
  bool wasCreated_ = false;
  int previousActiveTrackerId_ = 0;
  bool firstRedo_ = true;
  bool lastOperationSucceeded_ = true;
};


} // namespace Artifact
