module;
#include <memory>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <array>
export module Artifact.Render.FinalPostProcess;

import Core.Composition.FinalEffect;
import Graphics.GPUcomputeContext;
import Graphics.Compute.LUT3DComputer;
import Color.ColorSpace;

export namespace Artifact {

/// Post-composition final pass: LUT + tone mapping.
/// Wraps LUT3DGPUComputer and drives it from CompositionFinalEffectStack state.
class ArtifactFinalPostProcess {
public:
    ArtifactFinalPostProcess(ArtifactCore::GpuContext& ctx);
    ~ArtifactFinalPostProcess();

    /// Update internal LUT texture from a CompositionFinalEffect of type LUT.
    /// Call when the effect stack changes.
    void updateFromEffectStack(const ArtifactCore::CompositionFinalEffectStack& stack);

    /// Upload a CPU ColorLUT to GPU.
    /// data: RGB float values (size^3 * 3 floats), size: LUT dimension (e.g. 33).
    void updateFromColorLUT(const float* data, int size);

    /// Clear the active LUT.
    void clearLUT();

    // OCIO view transform
    void setViewTransformEnabled(bool enabled);
    bool isViewTransformEnabled() const;
    void setWorkingToDisplayMatrix(const float* matrix4x4); // 4x4 row-major matrix

    /// Apply post-processing to srcSRV, writing to dstUAV.
    /// If no LUT is active, this is a no-op (returns false).
    bool apply(Diligent::IDeviceContext* pContext,
               Diligent::ITextureView* srcSRV,
               Diligent::ITextureView* dstUAV,
               int width, int height);

    /// Whether a LUT is currently loaded.
    bool hasActiveLUT() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
