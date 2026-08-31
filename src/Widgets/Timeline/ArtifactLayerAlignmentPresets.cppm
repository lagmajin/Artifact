module;

#include <QPointF>
#include <QRectF>
#include <QVector>

#include <cstddef>
#include <iterator>
#include <vector>

export module Artifact.Widgets.LayerAlignmentPresets;

import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Frame.Position;
import Geometry.LayerAlignment;

export namespace Artifact {

struct LayerPlacementSnapshot {
  LayerID id;
  QRectF bounds;
  QPointF position;
};

std::vector<LayerPlacementSnapshot> captureLayerPlacements(
    const ArtifactCompositionPtr &composition, const QVector<LayerID> &ids) {
  std::vector<LayerPlacementSnapshot> snapshots;
  if (!composition || ids.isEmpty()) return snapshots;
  snapshots.reserve(static_cast<std::size_t>(ids.size()));
  for (const auto &id : ids) {
    auto layer = composition->layerById(id);
    if (!layer) continue;
    LayerPlacementSnapshot snapshot;
    snapshot.id = id;
    snapshot.bounds = layer->transformedBoundingBox();
    snapshot.position = QPointF(layer->transform3D().positionX(),
                                layer->transform3D().positionY());
    snapshots.push_back(snapshot);
  }
  return snapshots;
}

bool applyAlignPreset(const ArtifactCompositionPtr &composition,
                      const QVector<LayerID> &selectedIds, int presetIndex) {
  if (!composition || selectedIds.size() < 2) return false;
  auto snapshots = captureLayerPlacements(composition, selectedIds);
  if (snapshots.size() < 2) return false;

  std::vector<ArtifactCore::AlignmentObject> objects;
  objects.reserve(snapshots.size());
  for (std::size_t i = 0; i < snapshots.size(); ++i) {
    const auto &snapshot = snapshots[i];
    ArtifactCore::AlignmentObject object;
    object.id = static_cast<int>(i);
    object.bounds = snapshot.bounds;
    object.currentPosition = snapshot.position;
    objects.push_back(object);
  }

  const ArtifactCore::AlignType types[] = {
      ArtifactCore::AlignType::Left,
      ArtifactCore::AlignType::CenterHorizontal,
      ArtifactCore::AlignType::Right,
      ArtifactCore::AlignType::Top,
      ArtifactCore::AlignType::CenterVertical,
      ArtifactCore::AlignType::Bottom,
  };
  QRectF dummy;
  ArtifactCore::LayerAlignment::align(
      objects, types[presetIndex % static_cast<int>(std::size(types))],
      ArtifactCore::AlignmentTarget::Selection, dummy);

  const ArtifactCore::RationalTime time(0, 30000);
  for (std::size_t i = 0; i < snapshots.size() && i < objects.size(); ++i) {
    const std::size_t sourceIndex = static_cast<std::size_t>(objects[i].id);
    if (sourceIndex >= snapshots.size()) continue;
    auto layer = composition->layerById(snapshots[sourceIndex].id);
    if (!layer) continue;
    layer->transform3D().setPosition(time, objects[i].currentPosition.x(),
                                     objects[i].currentPosition.y());
    layer->changed();
  }
  return true;
}

bool applyDistributePreset(const ArtifactCompositionPtr &composition,
                           const QVector<LayerID> &selectedIds,
                           int presetIndex) {
  if (!composition || selectedIds.size() < 3) return false;
  auto snapshots = captureLayerPlacements(composition, selectedIds);
  if (snapshots.size() < 3) return false;

  std::vector<ArtifactCore::AlignmentObject> objects;
  objects.reserve(snapshots.size());
  for (std::size_t i = 0; i < snapshots.size(); ++i) {
    const auto &snapshot = snapshots[i];
    ArtifactCore::AlignmentObject object;
    object.id = static_cast<int>(i);
    object.bounds = snapshot.bounds;
    object.currentPosition = snapshot.position;
    objects.push_back(object);
  }

  const ArtifactCore::DistributeType types[] = {
      ArtifactCore::DistributeType::Left,
      ArtifactCore::DistributeType::CenterHorizontal,
      ArtifactCore::DistributeType::Right,
      ArtifactCore::DistributeType::Top,
      ArtifactCore::DistributeType::CenterVertical,
      ArtifactCore::DistributeType::Bottom,
  };
  ArtifactCore::LayerAlignment::distribute(
      objects, types[presetIndex % static_cast<int>(std::size(types))]);

  const ArtifactCore::RationalTime time(0, 30000);
  for (std::size_t i = 0; i < snapshots.size() && i < objects.size(); ++i) {
    const std::size_t sourceIndex = static_cast<std::size_t>(objects[i].id);
    if (sourceIndex >= snapshots.size()) continue;
    auto layer = composition->layerById(snapshots[sourceIndex].id);
    if (!layer) continue;
    layer->transform3D().setPosition(time, objects[i].currentPosition.x(),
                                     objects[i].currentPosition.y());
    layer->changed();
  }
  return true;
}

} // namespace Artifact
