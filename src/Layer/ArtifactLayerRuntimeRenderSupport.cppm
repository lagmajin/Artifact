module;

#include <QJsonObject>
#include <QJsonValue>
#include <QRectF>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cstdint>
#include <limits>

module Artifact.Layer.RuntimeRenderSupport;

import Artifact.Render.IRenderer;
import Color.Float;
import Geometry.Fracture;
import Graphics.ParticleData;
import Physics.Fluid;

namespace Artifact {

QJsonObject componentSnapshotVectorToJson(const QVector3D& value) {
  return {{QStringLiteral("x"), value.x()},
          {QStringLiteral("y"), value.y()},
          {QStringLiteral("z"), value.z()}};
}

QVector3D componentSnapshotVectorFromJson(const QJsonValue& value) {
  const QJsonObject object = value.toObject();
  return {static_cast<float>(object.value(QStringLiteral("x")).toDouble()),
          static_cast<float>(object.value(QStringLiteral("y")).toDouble()),
          static_cast<float>(object.value(QStringLiteral("z")).toDouble())};
}

QString componentSnapshotFrameToJson(int64_t frame) {
  return QString::number(frame);
}

int64_t componentSnapshotFrameFromJson(const QJsonObject& object,
                                       const QString& key) {
  bool ok = false;
  const qlonglong value = object.value(key).toString().toLongLong(&ok);
  return ok ? static_cast<int64_t>(value)
            : std::numeric_limits<int64_t>::min();
}

void submitFractureRenderElement(ArtifactIRenderer *renderer,
                                 const FractureRenderElement &element) {
  if (!renderer || element.empty()) {
    return;
  }
  if (!element.debris.particles.empty()) {
    renderer->drawParticles(element.debris);
  }
  for (const auto &shard : element.shards) {
    renderer->drawSolidPolygonLocal(shard.polygon, shard.color);
  }
}

ArtifactCore::ParticleRenderData makeLiquid2DRenderData(
    const ArtifactCore::LiquidSnapshot2D& snapshot, const QRectF& bounds,
    float particleSpacing, int64_t frameNumber) {
  ArtifactCore::ParticleRenderData result;
  result.frameNumber = frameNumber;
  result.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
  result.options.billboard =
      ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    return result;
  }

  const float velocityScale = static_cast<float>(
      std::min(bounds.width(), bounds.height()));
  const float particleSize = std::max(
      2.0f, velocityScale * std::clamp(particleSpacing, 0.025f, 0.2f) *
                1.35f);
  result.particles.reserve(snapshot.particles.size());
  for (const auto& source : snapshot.particles) {
    ArtifactCore::ParticleVertex particle{};
    particle.px = static_cast<float>(bounds.left() +
                                      source.x * bounds.width());
    particle.py = static_cast<float>(bounds.top() +
                                      source.y * bounds.height());
    particle.pz = 0.0f;
    particle.vx = source.vx * velocityScale;
    particle.vy = source.vy * velocityScale;
    particle.vz = 0.0f;
    particle.r = 0.12f;
    particle.g = 0.52f;
    particle.b = 0.95f;
    particle.a = 0.82f;
    particle.size = particleSize;
    particle.stretch = 1.0f;
    particle.rotation = 0.0f;
    particle.age = 0.0f;
    particle.lifetime = 1.0f;
    result.particles.push_back(particle);
  }
  return result;
}

}
