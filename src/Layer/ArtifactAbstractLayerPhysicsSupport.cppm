#include <cstddef>

import Artifact.Layer.Abstract;
import Artifact.Layer.PhysicsBridge;

namespace Artifact {
using namespace ArtifactCore;
namespace LayerPhysics = LayerPhysicsBridge;

bool ArtifactAbstractLayer::hasSoftBodyPhysics() const {
  return static_cast<bool>(LayerPhysics::softBody(id()));
}

SoftBodyDeformationMesh ArtifactAbstractLayer::softBodyDeformationMesh() const {
  SoftBodyDeformationMesh mesh;
  const auto solver = LayerPhysics::softBody(id());
  if (!solver) return mesh;
  const auto vertices = solver->getUVVertices();
  const auto indices = solver->getGridTriangleIndices();
  mesh.vertices.reserve(vertices.size() * 4U);
  for (const auto& vertex : vertices) {
    mesh.vertices.add(vertex.x);
    mesh.vertices.add(vertex.y);
    mesh.vertices.add(vertex.u);
    mesh.vertices.add(vertex.v);
  }
  mesh.indices.reserve(indices.size());
  for (const auto index : indices) mesh.indices.add(index);
  return mesh;
}

bool ArtifactAbstractLayer::hasCloth3DPhysics() const {
  return static_cast<bool>(LayerPhysics::cloth3D(id()));
}

ClothDeformationMesh3D ArtifactAbstractLayer::cloth3DDeformationMesh() const {
  ClothDeformationMesh3D mesh;
  const auto solver = LayerPhysics::cloth3D(id());
  if (!solver || solver->gridColumns() < 2 || solver->gridRows() < 2 ||
      solver->pointCount() != static_cast<std::size_t>(
                                  solver->gridColumns() * solver->gridRows())) {
    return mesh;
  }
  const int columns = solver->gridColumns();
  const int rows = solver->gridRows();
  mesh.positions.reserve(solver->pointCount() * 3U);
  mesh.uvs.reserve(solver->pointCount() * 2U);
  const float invColumns = 1.0f / static_cast<float>(columns - 1);
  const float invRows = 1.0f / static_cast<float>(rows - 1);
  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < columns; ++x) {
      const auto& point = solver->point(static_cast<std::size_t>(y * columns + x));
      mesh.positions.push_back(point.x);
      mesh.positions.push_back(point.y);
      mesh.positions.push_back(point.z);
      mesh.uvs.push_back(static_cast<float>(x) * invColumns);
      mesh.uvs.push_back(static_cast<float>(y) * invRows);
    }
  }
  const auto indices = solver->getGridTriangleIndices();
  mesh.indices.reserve(indices.size());
  for (const auto index : indices) mesh.indices.add(index);
  return mesh;
}

} // namespace Artifact
