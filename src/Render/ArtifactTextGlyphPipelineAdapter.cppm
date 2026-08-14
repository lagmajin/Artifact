module;
module Artifact.Render.TextGlyphPipelineAdapter;

import Artifact.Render.TextGlyphSubmitter.Contract;
import Artifact.Render.ShaderManager;

namespace Artifact {

ArtifactTextGlyphPipelineProvider makeArtifactTextGlyphPipelineProvider(
    ShaderManager& shaderManager) {
    const auto glyph = shaderManager.glyphQuadPsoAndSrb();
    const auto transformed = shaderManager.glyphQuadTransformPsoAndSrb();
    return ArtifactTextGlyphPipelineProvider{
        glyph.pPSO,
        glyph.pSRB,
        transformed.pPSO,
        transformed.pSRB,
        shaderManager.glyphAtlasSampler().RawPtr()};
}

}
