module;
export module Artifact.Render.ThreeDPrimitivePipelineAdapter;

import Artifact.Render.ThreeDPrimitiveSubmitter.Contract;
import Artifact.Render.ShaderManager;

export namespace Artifact {

/// Transitional adapter from the existing ShaderManager to the
/// 3D-primitive provider. Phase 1 (L1) ships the boundary with no
/// matching shaders in ShaderManager; the adapter therefore returns a
/// provider whose members are all nullptr. Phase 2 (L2) will add the
/// Unlit / FlatLit / Wire pipeline getters to ShaderManager and fill
/// this adapter in.
Artifact3DPrimitivePipelineProvider
makeArtifact3DPrimitivePipelineProvider(ShaderManager& shaderManager);

} // namespace Artifact
