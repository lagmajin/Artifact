module;
#include <vector>
#include <cstring>
#include <memory>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
module Artifact.Render.FinalPostProcess;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Compute.LUT3DComputer;
import Graphics.Shader.Compute.HLSL.LUT3D;

namespace Artifact {

using namespace ArtifactCore;
using namespace Diligent;

class ArtifactFinalPostProcess::Impl {
public:
    Impl(GpuContext& ctx)
        : context_(ctx)
        , lutComputer_(ctx)
    {
        lutComputer_.initialize();
    }

    void updateFromColorLUT(const float* data, int size)
    {
        if (!data || size < 2 || size > 256) {
            hasLUT_ = false;
            return;
        }

        // Acquire device context for upload
        IDeviceContext* pCtx = context_.DeviceContext();
        if (!pCtx) {
            hasLUT_ = false;
            return;
        }

        lutComputer_.uploadLUT(pCtx, data, size);
        hasLUT_ = true;
    }

    void clearLUT()
    {
        hasLUT_ = false;
    }

    bool apply(IDeviceContext* pContext,
               ITextureView* srcSRV,
               ITextureView* dstUAV,
               int width, int height)
    {
        if (!hasLUT_ || !pContext || !srcSRV || !dstUAV) {
            return false;
        }

        lutComputer_.apply(pContext, srcSRV, dstUAV);
        return true;
    }

    bool hasActiveLUT() const { return hasLUT_; }

private:
    GpuContext& context_;
    LUT3DGPUComputer lutComputer_;
    bool hasLUT_ = false;
};

ArtifactFinalPostProcess::ArtifactFinalPostProcess(GpuContext& ctx)
    : impl_(std::make_unique<Impl>(ctx)) {}

ArtifactFinalPostProcess::~ArtifactFinalPostProcess() = default;

void ArtifactFinalPostProcess::updateFromEffectStack(
    const ArtifactCore::CompositionFinalEffectStack& stack)
{
    // Find first enabled LUT effect in the stack
    for (const auto& effect : stack.getEffects()) {
        if (!effect || !effect->isEnabled()) continue;
        if (effect->getType() != ArtifactCore::FinalEffectType::LUT) continue;

        // LUT is stored as a pre-loaded ColorLUT via the composition settings.
        // For GPU path, the caller should call updateFromColorLUT() directly
        // with the actual LUT data from the ColorScienceManager.
        // This method just validates that a LUT effect is configured.
        return;
    }
    impl_->clearLUT();
}

bool ArtifactFinalPostProcess::apply(IDeviceContext* pContext,
                                      ITextureView* srcSRV,
                                      ITextureView* dstUAV,
                                      int width, int height)
{
    return impl_->apply(pContext, srcSRV, dstUAV, width, height);
}

bool ArtifactFinalPostProcess::hasActiveLUT() const
{
    return impl_->hasActiveLUT();
}

}
