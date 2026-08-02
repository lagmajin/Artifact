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

    void setLUTInputDomain(float minValue, float maxValue)
    {
        lutComputer_.setInputDomain(minValue, maxValue);
    }

    bool apply(IDeviceContext* pContext,
               ITextureView* srcSRV,
               ITextureView* dstUAV,
               int width, int height)
    {
        // The OCIO working-to-display transform is represented by the active
        // baked LUT in this path. A direct GPUProcessor shader path remains a
        // separate backend.
        if (viewTransformEnabled_ && srcSRV && dstUAV) {
            if (!hasLUT_) return true;
        }

        if (!hasLUT_ || !pContext || !srcSRV || !dstUAV) {
            return false;
        }

        lutComputer_.apply(pContext, srcSRV, dstUAV);
        return true;
    }

    bool hasActiveLUT() const { return hasLUT_; }

    void setViewTransformEnabled(bool enabled) { viewTransformEnabled_ = enabled; }
    bool isViewTransformEnabled() const { return viewTransformEnabled_; }
    void setWorkingToDisplayMatrix(const float* m) {
        if (m) std::memcpy(workingToDisplayMatrix_.data(), m, 16 * sizeof(float));
    }

private:
    GpuContext& context_;
    LUT3DGPUComputer lutComputer_;
    bool hasLUT_ = false;
    bool viewTransformEnabled_ = false;
    std::array<float, 16> workingToDisplayMatrix_ = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    ColorSpace workingSpace_ = ColorSpace::Linear;
    ColorSpace displaySpace_ = ColorSpace::sRGB;
};

ArtifactFinalPostProcess::ArtifactFinalPostProcess(GpuContext& ctx)
    : impl_(std::make_unique<Impl>(ctx)) {}

ArtifactFinalPostProcess::~ArtifactFinalPostProcess() = default;

void ArtifactFinalPostProcess::updateFromColorLUT(const float* data, int size)
{
    impl_->updateFromColorLUT(data, size);
}

void ArtifactFinalPostProcess::setLUTInputDomain(float minValue,
                                                  float maxValue)
{
    impl_->setLUTInputDomain(minValue, maxValue);
}

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

void ArtifactFinalPostProcess::setViewTransformEnabled(bool enabled)
{
    impl_->setViewTransformEnabled(enabled);
}

bool ArtifactFinalPostProcess::isViewTransformEnabled() const
{
    return impl_->isViewTransformEnabled();
}

void ArtifactFinalPostProcess::setWorkingToDisplayMatrix(const float* matrix4x4)
{
    impl_->setWorkingToDisplayMatrix(matrix4x4);
}

}
