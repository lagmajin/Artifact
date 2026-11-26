module;

#define QT_NO_KEYWORDS

#include <cmath>
#include <limits>

#include <QMatrix4x4>
#include <QVector>
#include <QVector3D>

module Artifact.Widgets.CompositionRenderController;

import Animation.Transform3D;
import Artifact.Layer.Abstract;
import Artifact.Layers.Model3D;
import Artifact.Widgets.Gizmo3D;
import Math.Vec;
import Mesh;
import Time.Rational;

namespace Artifact {

Ray pickingLegacyRayFromWorldRay(const WorldRay &ray) noexcept {
  return {QVector3D(ray.origin.x, ray.origin.y, ray.origin.z),
          QVector3D(ray.direction.x, ray.direction.y, ray.direction.z)};
}

static bool intersectPickingRayTriangle(const Ray &ray, const QVector3D &a,
                                        const QVector3D &b, const QVector3D &c,
                                        float &distance) {
  constexpr float kParallelEpsilon = 1.0e-7f;
  const QVector3D edgeAB = b - a;
  const QVector3D edgeAC = c - a;
  const QVector3D p = QVector3D::crossProduct(ray.direction, edgeAC);
  const float determinant = QVector3D::dotProduct(edgeAB, p);
  if (std::abs(determinant) <= kParallelEpsilon) {
    return false;
  }
  const float inverseDeterminant = 1.0f / determinant;
  const QVector3D originToA = ray.origin - a;
  const float u = QVector3D::dotProduct(originToA, p) * inverseDeterminant;
  if (u < 0.0f || u > 1.0f) {
    return false;
  }
  const QVector3D q = QVector3D::crossProduct(originToA, edgeAB);
  const float v = QVector3D::dotProduct(ray.direction, q) * inverseDeterminant;
  if (v < 0.0f || u + v > 1.0f) {
    return false;
  }
  const float t = QVector3D::dotProduct(edgeAC, q) * inverseDeterminant;
  if (t <= kParallelEpsilon) {
    return false;
  }
  distance = t;
  return true;
}

bool intersectModelLayerPickingRay(const Artifact3DLayer &layer,
    const WorldRay &ray, ArtifactCore::Units::WorldLength &distance) {
  // The legacy model matrix below is built from this layer's transform only;
  // retain the existing Qt geometry boundary until its parent-space contract
  // is reconciled with global world-ray coordinates.
  const Ray legacyRay = pickingLegacyRayFromWorldRay(ray);
  const auto positions = layer.mesh().vertexAttributes().get<QVector3D>("position");
  if (!positions || positions->data().isEmpty()) {
    return false;
  }

  const double layerFps = layer.compositionFrameRate();
  const auto snapshot = layer.transform3D().snapshotAt(
      RationalTime(layer.currentFrame(),
                   std::isfinite(layerFps) && layerFps > 0.0 ? layerFps : 30.0));
  QMatrix4x4 modelMatrix;
  modelMatrix.translate(snapshot.positionX, snapshot.positionY, snapshot.positionZ);
  modelMatrix.rotate(snapshot.rotationX, 1.0f, 0.0f, 0.0f);
  modelMatrix.rotate(snapshot.rotationY, 0.0f, 1.0f, 0.0f);
  modelMatrix.rotate(snapshot.rotationZ, 0.0f, 0.0f, 1.0f);
  modelMatrix.scale(snapshot.scaleX, snapshot.scaleY, snapshot.scaleZ);
  modelMatrix.translate(-snapshot.anchorX, -snapshot.anchorY, -snapshot.anchorZ);

  bool hit = false;
  float nearestDistance = std::numeric_limits<float>::max();
  const auto &mesh = layer.mesh();
  for (int polygonIndex = 0; polygonIndex < mesh.polygonCount(); ++polygonIndex) {
    const QVector<int> polygon = mesh.getPolygonVertices(polygonIndex);
    if (polygon.size() < 3) {
      continue;
    }
    const int rootIndex = polygon.front();
    if (rootIndex < 0 || rootIndex >= positions->data().size()) {
      continue;
    }
    const QVector3D root = modelMatrix.map(positions->data().at(rootIndex));
    for (int vertexIndex = 1; vertexIndex + 1 < polygon.size(); ++vertexIndex) {
      const int bIndex = polygon.at(vertexIndex);
      const int cIndex = polygon.at(vertexIndex + 1);
      if (bIndex < 0 || cIndex < 0 || bIndex >= positions->data().size() ||
          cIndex >= positions->data().size()) {
        continue;
      }
      float triangleDistance = 0.0f;
      if (intersectPickingRayTriangle(
              legacyRay, root, modelMatrix.map(positions->data().at(bIndex)),
              modelMatrix.map(positions->data().at(cIndex)), triangleDistance) &&
          triangleDistance < nearestDistance) {
        nearestDistance = triangleDistance;
        hit = true;
      }
    }
  }
  if (hit) {
    distance = ArtifactCore::Units::WorldLength{nearestDistance};
  }
  return hit;
}

} // namespace Artifact
