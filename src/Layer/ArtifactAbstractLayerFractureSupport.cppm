#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <QVariant>

import Artifact.Layer.Abstract;
import Geometry.Fracture;
import Property.Abstract;

namespace Artifact {
using namespace ArtifactCore;

void applyFragmentFieldsAndFloorCollision(
    const ArtifactAbstractLayer* layer, FractureShardMotion& shard,
    const QMatrix4x4& baseTransform, const float deltaSeconds) {
  if (!layer || !shard.active || deltaSeconds <= 0.0f) return;
  for (const auto& field : layer->layerFields()) {
    if (!field.enabled || field.strength == 0.0f) continue;
    const float strength = field.strength * (field.invert ? -1.0f : 1.0f);
    const float centerX = static_cast<float>(field.settings.value(
        QStringLiteral("centerX")).toDouble(0.0));
    const float centerY = static_cast<float>(field.settings.value(
        QStringLiteral("centerY")).toDouble(0.0));
    QVector3D fromCenter(shard.position.x() - centerX,
                         shard.position.y() - centerY, 0.0f);
    QVector3D acceleration(0.0f, 0.0f, 0.0f);
    if (field.typeId == QStringLiteral("artifact.field.radial") ||
        field.typeId == QStringLiteral("artifact.field.sphere")) {
      const float radius = std::max(1.0f, static_cast<float>(field.settings
          .value(QStringLiteral("outerRadius")).toDouble(
              field.settings.value(QStringLiteral("radius")).toDouble(160.0))));
      const float distance = fromCenter.length();
      if (distance <= radius && distance > 0.0001f) {
        fromCenter.normalize();
        acceleration = fromCenter * strength * (1.0f - distance / radius) * 600.0f;
      }
    } else if (field.typeId == QStringLiteral("artifact.field.linear")) {
      const float angle = static_cast<float>(field.settings.value(
          QStringLiteral("angle")).toDouble(0.0)) * 3.1415926535f / 180.0f;
      acceleration = QVector3D(std::cos(angle), std::sin(angle), 0.0f) * strength * 600.0f;
    } else if (field.typeId == QStringLiteral("artifact.field.box")) {
      const float halfX = std::max(1.0f, static_cast<float>(field.settings
          .value(QStringLiteral("halfX")).toDouble(120.0)));
      const float halfY = std::max(1.0f, static_cast<float>(field.settings
          .value(QStringLiteral("halfY")).toDouble(120.0)));
      if (std::abs(fromCenter.x()) <= halfX && std::abs(fromCenter.y()) <= halfY) {
        acceleration.setY(strength * 600.0f);
      }
    } else if (field.typeId == QStringLiteral("artifact.field.noise")) {
      const float phase = shard.position.x() * 0.013f + shard.position.y() * 0.017f;
      acceleration = QVector3D(std::sin(phase), std::cos(phase), 0.0f) * strength * 300.0f;
    }
    shard.velocity += acceleration * deltaSeconds;
  }
  const auto windEnabled = layer->getProperty(QStringLiteral("physics.wind.enabled"));
  if (windEnabled && windEnabled->getValue().toBool()) {
    const auto windX = layer->getProperty(QStringLiteral("physics.wind.x"));
    const auto windY = layer->getProperty(QStringLiteral("physics.wind.y"));
    const auto windStrength = layer->getProperty(QStringLiteral("physics.wind.strength"));
    const auto windTorque = layer->getProperty(QStringLiteral("physics.wind.torque"));
    QVector2D direction(windX ? windX->getValue().toFloat() : 1.0f,
                        windY ? windY->getValue().toFloat() : 0.0f);
    if (direction.lengthSquared() > 0.000001f) {
      direction.normalize();
      float fieldWeight = 1.0f;
      const QVector3D canvasPosition = baseTransform.map(shard.position);
      const auto channels = layer->compositionFieldChannelsAtCanvasPoint(
          QPointF(canvasPosition.x(), canvasPosition.y()));
      if (channels.affected) fieldWeight = channels.weight;
      const float windValue = (windStrength ? windStrength->getValue().toFloat() : 0.0f) * fieldWeight;
      shard.velocity += QVector3D(direction.x(), direction.y(), 0.0f) * windValue * deltaSeconds;
      shard.angularVelocity.setZ(shard.angularVelocity.z() +
          (windTorque ? windTorque->getValue().toFloat() : 0.0f) * windValue * deltaSeconds);
    }
  }
  const auto collisionEnabled = layer->getProperty(
      QStringLiteral("component.collision.enabled"));
  if (!collisionEnabled || !collisionEnabled->getValue().toBool()) return;
  const QSizeF compositionSize = layer->compositionSizeHint();
  if (!compositionSize.isValid()) return;
  const auto floorProperty = layer->getProperty(QStringLiteral("component.collision.floorY"));
  const float configuredFloor = floorProperty ? floorProperty->getValue().toFloat() : 0.0f;
  const float floorY = configuredFloor > 0.0f ? configuredFloor : static_cast<float>(compositionSize.height());
  const float shardRadius = std::max(2.0f, 5.0f * shard.scale);
  const auto restitutionProperty = layer->getProperty(QStringLiteral("physics.restitution"));
  const float restitution = std::clamp(restitutionProperty ? restitutionProperty->getValue().toFloat() : 0.25f, 0.0f, 1.0f);
  QVector3D worldPosition = baseTransform.map(shard.position);
  if (worldPosition.y() + shardRadius > floorY) {
    shard.position.setY(shard.position.y() + floorY - shardRadius - worldPosition.y());
    if (shard.velocity.y() > 0.0f) {
      shard.velocity.setY(-shard.velocity.y() * restitution);
      shard.velocity.setX(shard.velocity.x() * 0.92f);
    }
  }
  const auto boundsProperty = layer->getProperty(QStringLiteral("component.collision.compositionBounds"));
  if (!boundsProperty || !boundsProperty->getValue().toBool()) return;
  worldPosition = baseTransform.map(shard.position);
  const float compositionWidth = static_cast<float>(compositionSize.width());
  const float compositionHeight = static_cast<float>(compositionSize.height());
  if (worldPosition.x() - shardRadius < 0.0f || worldPosition.x() + shardRadius > compositionWidth) {
    const float targetX = std::clamp(worldPosition.x(), shardRadius, compositionWidth - shardRadius);
    shard.position.setX(shard.position.x() + targetX - worldPosition.x());
    shard.velocity.setX(-shard.velocity.x() * restitution);
  }
  worldPosition = baseTransform.map(shard.position);
  if (worldPosition.y() - shardRadius < 0.0f || worldPosition.y() + shardRadius > compositionHeight) {
    const float targetY = std::clamp(worldPosition.y(), shardRadius, compositionHeight - shardRadius);
    shard.position.setY(shard.position.y() + targetY - worldPosition.y());
    shard.velocity.setY(-shard.velocity.y() * restitution);
  }
}

void syncFragmentDataset(const ArtifactAbstractLayer* layer,
                         const FractureState& fractureState,
                         const FractureResult* prefractureResult,
                         LayerEvaluationState& evaluationState) {
  auto& fragments = evaluationState.fragments;
  auto& fragmentGeometry = evaluationState.fragmentGeometry;
  fragments.clear();
  fragmentGeometry.clear();
  if (!layer) return;
  fragments.reserve(fractureState.shards.size());
  fragmentGeometry.reserve(fractureState.shards.size());
  const QString ownerLayerId = layer->id().toString();
  for (std::size_t index = 0; index < fractureState.shards.size(); ++index) {
    const auto& shard = fractureState.shards[index];
    QMatrix4x4 fragmentTransform;
    fragmentTransform.setToIdentity();
    fragmentTransform.translate(shard.position);
    fragmentTransform.rotate(shard.rotation, 0.0f, 0.0f, 1.0f);
    fragmentTransform.scale(shard.scale);
    LayerFragmentState fragment;
    fragment.entityId = SimulationEntityId{
        ownerLayerId, QStringLiteral("component.fracture"),
        static_cast<std::uint64_t>(index), 0};
    fragment.sourceEntityId = SimulationEntityId{
        ownerLayerId, QStringLiteral("layer.source"), 0, 0};
    fragment.geometryHandle = QStringLiteral("fracture.shard.%1").arg(index);
    fragment.transform = fragmentTransform;
    fragment.linearVelocity = shard.velocity;
    fragment.angularVelocity = shard.angularVelocity;
    const FractureShard* sourceShard = nullptr;
    if (prefractureResult && prefractureResult->valid && index < prefractureResult->shards.size()) {
      sourceShard = &prefractureResult->shards[index];
      fragment.mass = sourceShard->mass;
    }
    fragment.opacity = shard.opacity;
    fragment.age = shard.age;
    fragment.lifetime = shard.lifetime;
    fragment.active = shard.active;
    fragment.debris = shard.debris;
    LayerFragmentGeometry geometry;
    geometry.geometryHandle = fragment.geometryHandle;
    geometry.materialHandle = QStringLiteral("layer.source:%1").arg(ownerLayerId);
    if (sourceShard && sourceShard->polygon.size() >= 3) {
      const QVector3D centroid = sourceShard->sourceCentroid;
      const QRectF bounds = layer->localBounds();
      const float width = std::max(1.0f, static_cast<float>(bounds.width()));
      const float height = std::max(1.0f, static_cast<float>(bounds.height()));
      geometry.localPolygon.reserve(static_cast<std::size_t>(sourceShard->polygon.size()));
      geometry.localUV.reserve(static_cast<std::size_t>(sourceShard->polygon.size()));
      for (const QPointF& point : sourceShard->polygon) {
        geometry.localPolygon.emplace_back(static_cast<float>(point.x()) - centroid.x(), static_cast<float>(point.y()) - centroid.y());
        geometry.localUV.emplace_back((static_cast<float>(point.x()) - static_cast<float>(bounds.left())) / width,
                                      (static_cast<float>(point.y()) - static_cast<float>(bounds.top())) / height);
      }
    } else {
      const float seed = static_cast<float>(index) * 1.61803398875f;
      const float skew = std::sin(seed) * 0.22f;
      const float pinch = 0.16f + std::cos(seed * 0.73f) * 0.07f;
      geometry.localPolygon.assign({
          QVector2D(-4.5f - skew * 10.0f, -10.0f - pinch * 2.5f),
          QVector2D(3.5f - skew * 8.0f, -9.2f - pinch * 1.5f),
          QVector2D(0.5f + skew * 1.5f, 0.8f + pinch * 0.8f),
          QVector2D(5.8f + skew * 11.0f, 9.5f - pinch * 0.5f),
          QVector2D(-6.2f + skew * 9.0f, 8.2f + pinch * 2.0f)});
      geometry.localUV.reserve(geometry.localPolygon.size());
      const QRectF bounds = layer->localBounds();
      const float width = std::max(1.0f, static_cast<float>(bounds.width()));
      const float height = std::max(1.0f, static_cast<float>(bounds.height()));
      for (const QVector2D& point : geometry.localPolygon) {
        geometry.localUV.emplace_back(std::clamp((shard.position.x() + point.x() - static_cast<float>(bounds.left())) / width, 0.0f, 1.0f),
                                      std::clamp((shard.position.y() + point.y() - static_cast<float>(bounds.top())) / height, 0.0f, 1.0f));
      }
    }
    fragments.push_back(std::move(fragment));
    fragmentGeometry.push_back(std::move(geometry));
  }
}

} // namespace Artifact
