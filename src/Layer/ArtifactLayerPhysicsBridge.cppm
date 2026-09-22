module Artifact.Layer.PhysicsBridge;

import Physics.System;

namespace Artifact::LayerPhysicsBridge {

using namespace ArtifactCore;

SharedPtr<Physics2D> rigidWorld(const LayerID& layerId) {
  return PhysicsSystem::instance().getRigidWorld(layerId);
}

SharedPtr<Physics2D> ensureRigidWorld(const LayerID& layerId) {
  auto world = rigidWorld(layerId);
  return world ? world : PhysicsSystem::instance().createRigidWorld(layerId);
}

void removeRigidWorld(const LayerID& layerId) {
  PhysicsSystem::instance().unregisterRigidWorld(layerId);
}

SharedPtr<Physics2D> compositionRigidWorld(const CompositionID& compositionId) {
  return PhysicsSystem::instance().getCompositionRigidWorld(compositionId);
}

SharedPtr<Physics2D> ensureCompositionRigidWorld(
    const CompositionID& compositionId) {
  auto world = compositionRigidWorld(compositionId);
  return world ? world
               : PhysicsSystem::instance().createCompositionRigidWorld(
                     compositionId);
}

SharedPtr<SoftBodySolver> softBody(const LayerID& layerId) {
  return PhysicsSystem::instance().getSoftBody(layerId);
}

SharedPtr<SoftBodySolver> ensureSoftBody(const LayerID& layerId) {
  auto solver = softBody(layerId);
  return solver ? solver : PhysicsSystem::instance().createSoftBody(layerId);
}

SharedPtr<SoftBodySolver> createSoftBodyGrid(
    const LayerID& layerId, float left, float top, float width, float height,
    int columns, int rows, float pointMass, float stiffness, bool pinTopRow) {
  return PhysicsSystem::instance().createSoftBodyGrid(
      layerId, left, top, width, height, columns, rows, pointMass, stiffness,
      pinTopRow);
}

void removeSoftBody(const LayerID& layerId) {
  PhysicsSystem::instance().unregisterSoftBody(layerId);
}

void clearSoftBodyColliders(const LayerID& layerId) {
  PhysicsSystem::instance().clearSoftBodyColliders(layerId);
}

void registerSoftBodyCollider(const LayerID& layerId,
                              const SoftBodyCollider& collider) {
  PhysicsSystem::instance().registerSoftBodyCollider(layerId, collider);
}

void setSoftBodyWind(const LayerID& layerId, float x, float y,
                     float strength) {
  PhysicsSystem::instance().setSoftBodyWind(layerId, x, y, strength);
}

void configureSoftBody(const LayerID& layerId, float gravityX, float gravityY,
                       float linearDamping) {
  const auto solver = PhysicsSystem::instance().getSoftBody(layerId);
  if (!solver) return;
  solver->setGravity(gravityX, gravityY);
  solver->setLinearDamping(linearDamping);
}

SharedPtr<ClothSolver3D> cloth3D(const LayerID& layerId) {
  return PhysicsSystem::instance().getCloth3D(layerId);
}

SharedPtr<ClothSolver3D> ensureCloth3D(const LayerID& layerId) {
  auto solver = cloth3D(layerId);
  return solver ? solver : PhysicsSystem::instance().createCloth3D(layerId);
}

SharedPtr<ClothSolver3D> createCloth3DGrid(
    const LayerID& layerId, float left, float top, float width, float height,
    float depth, int columns, int rows, float pointMass, float stiffness,
    bool pinTopRow) {
  return PhysicsSystem::instance().createCloth3DGrid(
      layerId, left, top, width, height, depth, columns, rows, pointMass,
      stiffness, pinTopRow);
}

void removeCloth3D(const LayerID& layerId) {
  PhysicsSystem::instance().unregisterCloth3D(layerId);
}

void setCloth3DWind(const LayerID& layerId, float x, float y, float z,
                    float strength) {
  PhysicsSystem::instance().setCloth3DWind(layerId, x, y, z, strength);
}

void createMaterialGrid(const LayerID& layerId, float left, float top,
                        float width, float height, int columns, int rows,
                        int preset) {
  PhysicsSystem::instance().createMaterialGrid(
      layerId, left, top, width, height, columns, rows,
      static_cast<MpmMaterialPreset>(preset));
}

void removeMaterialSolver(const LayerID& layerId) {
  PhysicsSystem::instance().unregisterMaterialSolver(layerId);
}

FractureLodScale fractureLodScale() {
  const auto& settings = PhysicsSystem::instance().physicsLODSettings();
  return {settings.fractureShardScale, settings.fractureDebrisScale};
}

}
