# Artifact — Notes Index

This repository contains developer notes and feature discussion summaries.

- Notes directory: `docs/Notes/` (contains meeting notes, summaries, and related docs)
- Feature discussion summary: `docs/Notes/feature-discussion-summary.md`

Open the `docs/Notes/` directory to see all saved notes and summaries.

## Implemented List (Particle Test Path)

- Added CPU software frame pipeline in `Artifact.Generator.Particle`:
  - `ParticleSystem::updateAndRenderSoftwareFrame(float deltaTime, int width, int height, const QColor& clearColor)`
  - Performs simulation update and returns the rendered frame as `QImage`.
- Added software-render camera API:
  - `ParticleSystem::setCameraPosition(const QVector3D& position)`
  - `ParticleSystem::cameraPosition() const`
- Renderer behavior:
  - Pure CPU path (no GPU dependency), perspective projection, depth sort (far-to-near), and particle blending into `QImage`.
- Existing particle-system enhancements retained:
  - Fixed-step deterministic simulation controls (`deterministic`, `randomSeed`, `fixedTimeStep`, `maxSubSteps`)
  - Broad-phase self-collision grid (`enableSelfCollision`, `selfCollisionRadius`, `selfCollisionResponse`)
  - TBB-based parallel particle update for large particle counts.
