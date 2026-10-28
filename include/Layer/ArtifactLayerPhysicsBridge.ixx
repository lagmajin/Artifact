module;

#include <cstdint>

export module Artifact.Layer.PhysicsBridge;

import Memory.SharedPtr;
import Utils.Id;
import Physics2D;
import Physics.Mpm2D;
import Physics.SoftBody;
import Physics.Cloth3D;
import Core.Simulation.Pyro;

export namespace Artifact::LayerPhysicsBridge {

struct FractureLodScale {
  float shards = 1.0f;
  float debris = 1.0f;
};

ArtifactCore::SharedPtr<ArtifactCore::Physics2D>
rigidWorld(const ArtifactCore::LayerID& layerId);
ArtifactCore::SharedPtr<ArtifactCore::Physics2D>
ensureRigidWorld(const ArtifactCore::LayerID& layerId);
void removeRigidWorld(const ArtifactCore::LayerID& layerId);

ArtifactCore::SharedPtr<ArtifactCore::Physics2D>
compositionRigidWorld(const ArtifactCore::CompositionID& compositionId);
ArtifactCore::SharedPtr<ArtifactCore::Physics2D>
ensureCompositionRigidWorld(const ArtifactCore::CompositionID& compositionId);

ArtifactCore::SharedPtr<ArtifactCore::SoftBodySolver>
softBody(const ArtifactCore::LayerID& layerId);
ArtifactCore::SharedPtr<ArtifactCore::SoftBodySolver>
ensureSoftBody(const ArtifactCore::LayerID& layerId);
ArtifactCore::SharedPtr<ArtifactCore::SoftBodySolver> createSoftBodyGrid(
    const ArtifactCore::LayerID& layerId, float left, float top, float width,
    float height, int columns, int rows, float pointMass, float stiffness,
    bool pinTopRow);
void removeSoftBody(const ArtifactCore::LayerID& layerId);
void clearSoftBodyColliders(const ArtifactCore::LayerID& layerId);
void registerSoftBodyCollider(const ArtifactCore::LayerID& layerId,
                              const ArtifactCore::SoftBodyCollider& collider);
void setSoftBodyWind(const ArtifactCore::LayerID& layerId, float x, float y,
                     float strength);
void configureSoftBody(const ArtifactCore::LayerID& layerId, float gravityX,
                       float gravityY, float linearDamping);

ArtifactCore::SharedPtr<ArtifactCore::ClothSolver3D>
cloth3D(const ArtifactCore::LayerID& layerId);
ArtifactCore::SharedPtr<ArtifactCore::ClothSolver3D>
ensureCloth3D(const ArtifactCore::LayerID& layerId);
ArtifactCore::SharedPtr<ArtifactCore::ClothSolver3D> createCloth3DGrid(
    const ArtifactCore::LayerID& layerId, float left, float top, float width,
    float height, float depth, int columns, int rows, float pointMass,
    float stiffness, bool pinTopRow);
void removeCloth3D(const ArtifactCore::LayerID& layerId);
void setCloth3DWind(const ArtifactCore::LayerID& layerId, float x, float y,
                    float z, float strength);

ArtifactCore::SharedPtr<ArtifactCore::PyroSimulation>
pyro(const ArtifactCore::LayerID& layerId);
ArtifactCore::SharedPtr<ArtifactCore::PyroSimulation>
ensurePyro(const ArtifactCore::LayerID& layerId);
void removePyro(const ArtifactCore::LayerID& layerId);

bool createMaterialGrid(const ArtifactCore::LayerID& layerId, float left,
                        float top, float width, float height, int columns,
                        int rows, int preset);
ArtifactCore::SharedPtr<ArtifactCore::MpmSolver2D>
createGranularMaterialGrid(std::uint64_t simulationWorldId,
                           const ArtifactCore::LayerID& layerId, float left,
                           float top, float width, float height,
                           float domainLeft, float domainTop, float domainWidth,
                           float domainHeight, int columns, int rows,
                           float youngModulus, float poissonRatio,
                           float frictionAngleRadians, float density,
                           float gravityY, float damping,
                           float supportRadiusLimit);
ArtifactCore::SharedPtr<ArtifactCore::MpmSolver2D>
granularMaterialSolver(std::uint64_t simulationWorldId,
                       const ArtifactCore::LayerID& layerId);
void removeGranularMaterialSolver(std::uint64_t simulationWorldId,
                                  const ArtifactCore::LayerID& layerId);
void setGranularMaterialGravity(std::uint64_t simulationWorldId,
                                const ArtifactCore::LayerID& layerId,
                                float gravityX, float gravityY);
void setGranularMaterialSolverActive(std::uint64_t simulationWorldId,
                                     const ArtifactCore::LayerID& layerId,
                                     bool active);
float granularMaterialBacklogSeconds(
    std::uint64_t simulationWorldId,
    const ArtifactCore::LayerID& layerId);
void removeMaterialSolver(const ArtifactCore::LayerID& layerId);

FractureLodScale fractureLodScale();

}
