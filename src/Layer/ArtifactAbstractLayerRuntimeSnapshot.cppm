#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <utility>

import Artifact.Layer.Abstract;
import Artifact.Layer.RuntimeRenderSupport;
import Container.NamedVector;
import Geometry.Fracture;
import Graphics.ParticleData;
import Memory.SharedPtr;

// Qt preprocessor macros are not carried by an imported IFC.
#define QStringLiteral(text) QString::fromUtf8(text)

namespace Artifact {

using namespace ArtifactCore;

QJsonObject ArtifactAbstractLayer::serializeComponentRuntimeSnapshot(
    const LayerComponentRuntimeSnapshot& snapshot) const {
  if (!snapshot.isValid()) {
    return {};
  }
  const auto data = staticPointerCast<
      const LayerComponentRuntimeSnapshotData>(snapshot.storage);
  if (!data) {
    return {};
  }

  QJsonObject fracture;
  fracture.insert(QStringLiteral("kind"),
                  static_cast<int>(data->fractureState.kind));
  fracture.insert(QStringLiteral("damage"), data->fractureState.damage);
  fracture.insert(QStringLiteral("lastImpact"), data->fractureState.lastImpact);
  fracture.insert(QStringLiteral("crackProgress"),
                  data->fractureState.crackProgress);
  QJsonArray shards;
  for (const auto& shard : data->fractureState.shards) {
    QJsonObject object;
    object.insert(QStringLiteral("position"),
                  componentSnapshotVectorToJson(shard.position));
    object.insert(QStringLiteral("velocity"),
                  componentSnapshotVectorToJson(shard.velocity));
    object.insert(QStringLiteral("angularVelocity"),
                  componentSnapshotVectorToJson(shard.angularVelocity));
    object.insert(QStringLiteral("rotation"), shard.rotation);
    object.insert(QStringLiteral("scale"), shard.scale);
    object.insert(QStringLiteral("opacity"), shard.opacity);
    object.insert(QStringLiteral("age"), shard.age);
    object.insert(QStringLiteral("lifetime"), shard.lifetime);
    object.insert(QStringLiteral("active"), shard.active);
    object.insert(QStringLiteral("debris"), shard.debris);
    shards.append(object);
  }
  fracture.insert(QStringLiteral("shards"), shards);

  QJsonArray particles;
  for (const auto& particle : data->componentParticles) {
    QJsonObject object;
    object.insert(QStringLiteral("px"), particle.px);
    object.insert(QStringLiteral("py"), particle.py);
    object.insert(QStringLiteral("pz"), particle.pz);
    object.insert(QStringLiteral("vx"), particle.vx);
    object.insert(QStringLiteral("vy"), particle.vy);
    object.insert(QStringLiteral("vz"), particle.vz);
    object.insert(QStringLiteral("r"), particle.r);
    object.insert(QStringLiteral("g"), particle.g);
    object.insert(QStringLiteral("b"), particle.b);
    object.insert(QStringLiteral("a"), particle.a);
    object.insert(QStringLiteral("size"), particle.size);
    object.insert(QStringLiteral("stretch"), particle.stretch);
    object.insert(QStringLiteral("rotation"), particle.rotation);
    object.insert(QStringLiteral("age"), particle.age);
    object.insert(QStringLiteral("lifetime"), particle.lifetime);
    object.insert(QStringLiteral("spriteFrame"), particle.spriteFrame);
    object.insert(QStringLiteral("spriteRows"), particle.spriteRows);
    object.insert(QStringLiteral("spriteCols"), particle.spriteCols);
    particles.append(object);
  }

  QJsonObject object;
  object.insert(QStringLiteral("version"), 1);
  object.insert(QStringLiteral("fracture"), fracture);
  object.insert(QStringLiteral("particles"), particles);
  object.insert(QStringLiteral("fractureMotionLastFrame"),
                componentSnapshotFrameToJson(data->fractureMotionLastFrame));
  object.insert(QStringLiteral("componentParticlesLastFrame"),
                componentSnapshotFrameToJson(
                    data->componentParticlesLastFrame));
  object.insert(QStringLiteral("lastCollisionImpactFrame"),
                componentSnapshotFrameToJson(
                    data->lastCollisionImpactFrame));
  return object;
}

LayerComponentRuntimeSnapshot
ArtifactAbstractLayer::deserializeComponentRuntimeSnapshot(
    const QJsonObject& object) const {
  if (object.value(QStringLiteral("version")).toInt() != 1) {
    return {};
  }

  auto data = makeShared<LayerComponentRuntimeSnapshotData>();
  const QJsonObject fracture =
      object.value(QStringLiteral("fracture")).toObject();
  const int kind = fracture.value(QStringLiteral("kind")).toInt();
  if (kind < static_cast<int>(FractureStateKind::Intact) ||
      kind > static_cast<int>(FractureStateKind::Shattered)) {
    return {};
  }
  data->fractureState.kind = static_cast<FractureStateKind>(kind);
  data->fractureState.damage = static_cast<float>(
      fracture.value(QStringLiteral("damage")).toDouble());
  data->fractureState.lastImpact = static_cast<float>(
      fracture.value(QStringLiteral("lastImpact")).toDouble());
  data->fractureState.crackProgress = static_cast<float>(
      fracture.value(QStringLiteral("crackProgress")).toDouble());
  const QJsonArray shards = fracture.value(QStringLiteral("shards")).toArray();
  constexpr qsizetype kMaxPersistedShards = 65536;
  if (shards.size() > kMaxPersistedShards) {
    return {};
  }
  data->fractureState.shards.reserve(
      static_cast<std::size_t>(shards.size()));
  for (const auto& value : shards) {
    if (!value.isObject()) {
      return {};
    }
    const QJsonObject object = value.toObject();
    FractureShardMotion shard;
    shard.position = componentSnapshotVectorFromJson(
        object.value(QStringLiteral("position")));
    shard.velocity = componentSnapshotVectorFromJson(
        object.value(QStringLiteral("velocity")));
    shard.angularVelocity = componentSnapshotVectorFromJson(
        object.value(QStringLiteral("angularVelocity")));
    shard.rotation = static_cast<float>(
        object.value(QStringLiteral("rotation")).toDouble());
    shard.scale = static_cast<float>(
        object.value(QStringLiteral("scale")).toDouble(1.0));
    shard.opacity = static_cast<float>(
        object.value(QStringLiteral("opacity")).toDouble(1.0));
    shard.age = static_cast<float>(
        object.value(QStringLiteral("age")).toDouble());
    shard.lifetime = static_cast<float>(
        object.value(QStringLiteral("lifetime")).toDouble(1.0));
    shard.active = object.value(QStringLiteral("active")).toBool(true);
    shard.debris = object.value(QStringLiteral("debris")).toBool(false);
    data->fractureState.shards.push_back(shard);
  }

  const QJsonArray particles =
      object.value(QStringLiteral("particles")).toArray();
  constexpr qsizetype kMaxPersistedParticles = 1000000;
  if (particles.size() > kMaxPersistedParticles) {
    return {};
  }
  data->componentParticles.reserve(
      static_cast<std::size_t>(particles.size()));
  for (const auto& value : particles) {
    if (!value.isObject()) {
      return {};
    }
    const QJsonObject object = value.toObject();
    ArtifactCore::ParticleVertex particle{};
    particle.px = static_cast<float>(object.value(QStringLiteral("px")).toDouble());
    particle.py = static_cast<float>(object.value(QStringLiteral("py")).toDouble());
    particle.pz = static_cast<float>(object.value(QStringLiteral("pz")).toDouble());
    particle.vx = static_cast<float>(object.value(QStringLiteral("vx")).toDouble());
    particle.vy = static_cast<float>(object.value(QStringLiteral("vy")).toDouble());
    particle.vz = static_cast<float>(object.value(QStringLiteral("vz")).toDouble());
    particle.r = static_cast<float>(object.value(QStringLiteral("r")).toDouble());
    particle.g = static_cast<float>(object.value(QStringLiteral("g")).toDouble());
    particle.b = static_cast<float>(object.value(QStringLiteral("b")).toDouble());
    particle.a = static_cast<float>(object.value(QStringLiteral("a")).toDouble());
    particle.size = static_cast<float>(object.value(QStringLiteral("size")).toDouble(1.0));
    particle.stretch = static_cast<float>(object.value(QStringLiteral("stretch")).toDouble(1.0));
    particle.rotation = static_cast<float>(object.value(QStringLiteral("rotation")).toDouble());
    particle.age = static_cast<float>(object.value(QStringLiteral("age")).toDouble());
    particle.lifetime = static_cast<float>(object.value(QStringLiteral("lifetime")).toDouble(1.0));
    particle.spriteFrame = object.value(QStringLiteral("spriteFrame")).toInt();
    particle.spriteRows = object.value(QStringLiteral("spriteRows")).toInt(1);
    particle.spriteCols = object.value(QStringLiteral("spriteCols")).toInt(1);
    data->componentParticles.push_back(particle);
  }

  data->fractureMotionLastFrame = componentSnapshotFrameFromJson(
      object, QStringLiteral("fractureMotionLastFrame"));
  data->componentParticlesLastFrame = componentSnapshotFrameFromJson(
      object, QStringLiteral("componentParticlesLastFrame"));
  data->lastCollisionImpactFrame = componentSnapshotFrameFromJson(
      object, QStringLiteral("lastCollisionImpactFrame"));
  const std::size_t estimatedBytes =
      sizeof(LayerComponentRuntimeSnapshotData) +
      data->componentParticles.size() * sizeof(ArtifactCore::ParticleVertex) +
      data->fractureState.shards.size() * sizeof(FractureShardMotion);
  return {std::move(data), estimatedBytes};
}

} // namespace Artifact

#undef QStringLiteral
