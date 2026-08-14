module;
export module Artifact.Render.TextGlyphPipelineAdapter;

import Artifact.Render.TextGlyphSubmitter.Contract;
import Artifact.Render.ShaderManager;

export namespace Artifact {

/// Transitional adapter from the existing ShaderManager to the glyph-only
/// provider. The glyph submitter remains independent of ShaderManager.
ArtifactTextGlyphPipelineProvider makeArtifactTextGlyphPipelineProvider(
    ShaderManager& shaderManager);

}
