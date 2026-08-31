module;

#include <QJsonObject>
#include <QJsonValue>
#include <QRectF>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <limits>
#include <vector>

export module Artifact.Layer.RuntimeRenderSupport;

import Artifact.Render.IRenderer;
import Color.Float;
import Geometry.Fracture;
import Graphics.ParticleData;
import Physics.Fluid;

export namespace Artifact {

struct FractureShardRenderPrimitive {
  std::vector<Detail::float2> polygon;
  ArtifactCore::FloatColor color;
};

struct FractureRenderElement {
  ArtifactCore::ParticleRenderData debris;
  std::vector<FractureShardRenderPrimitive> shards;

  bool empty() const {
    return debris.particles.empty() && shards.empty();
  }
};

struct LayerComponentRuntimeSnapshotData {
  ArtifactCore::FractureState fractureState;
  std::vector<ArtifactCore::ParticleVertex> componentParticles;
  std::int64_t fractureMotionLastFrame = std::numeric_limits<std::int64_t>::min();
  std::int64_t componentParticlesLastFrame = std::numeric_limits<std::int64_t>::min();
  std::int64_t lastCollisionImpactFrame = std::numeric_limits<std::int64_t>::min();
};

QJsonObject componentSnapshotVectorToJson(const QVector3D& value);
QVector3D componentSnapshotVectorFromJson(const QJsonValue& value);
QString componentSnapshotFrameToJson(std::int64_t frame);
std::int64_t componentSnapshotFrameFromJson(const QJsonObject& object,
                                            const QString& key);

void submitFractureRenderElement(ArtifactIRenderer* renderer,
                                 const FractureRenderElement& element);
ArtifactCore::ParticleRenderData makeLiquid2DRenderData(
    const ArtifactCore::LiquidSnapshot2D& snapshot,
    const QRectF& bounds,
    float particleSpacing,
    std::int64_t frameNumber);

}
