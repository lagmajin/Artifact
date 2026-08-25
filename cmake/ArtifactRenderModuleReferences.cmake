# Explicit BMI references for ArtifactRender implementation units.
# Keep this list in sync with ARTIFACT_RENDER_IMPL when adding module-backed
# renderer implementations.

set(ARTIFACT_RENDER_IMPLEMENTATION_MODULE_REFERENCES
    "src/Render/ArtifactTextGlyphPipelineAdapter.cppm|Artifact.Render.TextGlyphPipelineAdapter|include/Render/ArtifactTextGlyphPipelineAdapter.ixx"
    "src/Render/DiligentDeviceManager.cppm|Artifact.Render.DiligentDeviceManager|include/Render/DiligentDeviceManager.ixx"
    "src/Render/ShaderManager.cppm|Artifact.Render.ShaderManager|include/Render/ShaderManager.ixx"
    "src/Render/RenderCommandBuffer.cppm|Artifact.Render.RenderCommandBuffer|include/Render/RenderCommandBuffer.ixx"
    "src/Render/PrimitiveRenderer2D.cppm|Artifact.Render.PrimitiveRenderer2D|include/Render/PrimitiveRenderer2D.ixx"
    "src/Render/PrimitiveRenderer3D.cppm|Artifact.Render.PrimitiveRenderer3D|include/Render/PrimitiveRenderer3D.ixx"
    "src/Render/DiligentImmediateSubmitter.cppm|Artifact.Render.DiligentImmediateSubmitter|include/Render/DiligentImmediateSubmitter.ixx"
    "src/Render/DiligentBindlessSubmitter.cppm|Artifact.Render.DiligentBindlessSubmitter|include/Render/DiligentBindlessSubmitter.ixx"
    "src/Render/ArtifactFinalPostProcess.cppm|Artifact.Render.FinalPostProcess|include/Render/ArtifactFinalPostProcess.ixx"
    "src/Render/ArtifactOffscreenRenderer2D.cppm|Artifact.Render.Offscreen|include/Render/ArtifactOffscreenRenderer2D.ixx"
    "src/Render/ArtifactIRenderer.cppm|Artifact.Render.IRenderer|include/Render/ArtifactIRenderer.ixx"
    "src/Render/ArtifactMotionBlurPass.cppm|Artifact.Render.MotionBlurPass|include/Render/ArtifactMotionBlurPass.ixx"
    "src/Render/ArtifactDepthOfFieldPass.cppm|Artifact.Render.DepthOfFieldPass|include/Render/ArtifactDepthOfFieldPass.ixx"
    "src/Render/GPUTextureCacheManager.cppm|Artifact.Render.GPUTextureCacheManager|include/Render/GPUTextureCacheManager.ixx"
    "src/LOD/ArtifactLODManager.cppm|Artifact.LOD.Manager|include/LOD/ArtifactLODManager.ixx"
)
