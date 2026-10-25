# Explicit BMI references for ArtifactRender implementation units.
# Keep this list in sync with ARTIFACT_RENDER_IMPL when adding module-backed
# renderer implementations.

set(ARTIFACT_RENDER_IMPLEMENTATION_MODULE_REFERENCES
    "src/Render/ArtifactTextGlyphPipelineAdapter.cpp|Artifact.Render.TextGlyphPipelineAdapter|include/Render/ArtifactTextGlyphPipelineAdapter.ixx"
    "src/Render/Artifact3DPrimitivePipelineAdapter.cpp|Artifact.Render.ThreeDPrimitivePipelineAdapter|include/Render/Artifact3DPrimitivePipelineAdapter.ixx"
    "src/Render/Artifact3DPrimitiveSubmitter.cpp|Artifact.Render.ThreeDPrimitiveSubmitter.Contract|include/Render/Artifact3DPrimitiveSubmitter.ixx"
    "src/Render/DiligentDeviceManager.cpp|Artifact.Render.DiligentDeviceManager|include/Render/DiligentDeviceManager.ixx"
    "src/Render/ShaderManager.cpp|Artifact.Render.ShaderManager|include/Render/ShaderManager.ixx"
    "src/Render/RenderCommandBuffer.cpp|Artifact.Render.RenderCommandBuffer|include/Render/RenderCommandBuffer.ixx"
    "src/Render/PrimitiveRenderer2D.cpp|Artifact.Render.PrimitiveRenderer2D|include/Render/PrimitiveRenderer2D.ixx"
    "src/Render/PrimitiveRenderer3D.cpp|Artifact.Render.PrimitiveRenderer3D|include/Render/PrimitiveRenderer3D.ixx"
    "src/Render/DiligentImmediateSubmitter.cpp|Artifact.Render.DiligentImmediateSubmitter|include/Render/DiligentImmediateSubmitter.ixx"
    "src/Render/DiligentBindlessSubmitter.cpp|Artifact.Render.DiligentBindlessSubmitter|include/Render/DiligentBindlessSubmitter.ixx"
    "src/Render/ArtifactFinalPostProcess.cpp|Artifact.Render.FinalPostProcess|include/Render/ArtifactFinalPostProcess.ixx"
    "src/Render/ArtifactOffscreenRenderer2D.cpp|Artifact.Render.Offscreen|include/Render/ArtifactOffscreenRenderer2D.ixx"
    "src/Render/ArtifactIRenderer.cpp|Artifact.Render.IRenderer|include/Render/ArtifactIRenderer.ixx"
    "src/Render/ArtifactMotionBlurPass.cpp|Artifact.Render.MotionBlurPass|include/Render/ArtifactMotionBlurPass.ixx"
    "src/Render/ArtifactDepthOfFieldPass.cpp|Artifact.Render.DepthOfFieldPass|include/Render/ArtifactDepthOfFieldPass.ixx"
    "src/Render/GPUTextureCacheManager.cpp|Artifact.Render.GPUTextureCacheManager|include/Render/GPUTextureCacheManager.ixx"
    "src/LOD/ArtifactLODManager.cpp|Artifact.LOD.Manager|include/LOD/ArtifactLODManager.ixx"
)
