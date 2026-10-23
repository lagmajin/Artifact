module;
module Artifact.Render.ThreeDPrimitivePipelineAdapter;

import Artifact.Render.ThreeDPrimitiveSubmitter.Contract;
import Artifact.Render.ShaderManager;

namespace Artifact {

Artifact3DPrimitivePipelineProvider
makeArtifact3DPrimitivePipelineProvider(ShaderManager& /*shaderManager*/) {
    // Phase 1 (L1) of the 3D shader variant plan intentionally returns
    // an empty provider. ShaderManager does not yet expose the
    // Unlit / FlatLit / Wire PSO getters; adding them is the next
    // commit in this plan. Callers must check isValid() before
    // switching the 3D layer draw path off the legacy line renderer.
    return Artifact3DPrimitivePipelineProvider{};
}

} // namespace Artifact
