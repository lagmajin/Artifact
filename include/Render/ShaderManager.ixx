module;
#include <RenderDevice.h>
#include <Shader.h>
#include <PipelineState.h>
#include <Sampler.h>
#include <RefCntAutoPtr.hpp>

export module Artifact.Render.ShaderManager;

import Graphics.Shader.Set;
import Graphics;

export namespace Artifact {

using namespace Diligent;
using namespace ArtifactCore;

class ShaderManager {
public:
    ShaderManager();
    explicit ShaderManager(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT rtvFormat);
    ~ShaderManager();

    void initialize(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT rtvFormat);
    void createShaders();
    void createPSOs();
    void destroy();

    RenderShaderPair lineShaders() const;
    RenderShaderPair outlineShaders() const;
    RenderShaderPair solidShaders() const;
    RenderShaderPair spriteShaders() const;
    RenderShaderPair thickLineShaders() const;
    RenderShaderPair dotLineShaders() const;
    RenderShaderPair solidTriangleShaders() const;
    RenderShaderPair checkerboardShaders() const;
    RenderShaderPair gridShaders() const;

    PSOAndSRB linePsoAndSrb() const;
    PSOAndSRB outlinePsoAndSrb() const;
    PSOAndSRB solidRectPsoAndSrb() const;
    PSOAndSRB spritePsoAndSrb() const;
    PSOAndSRB thickLinePsoAndSrb() const;
    PSOAndSRB dotLinePsoAndSrb() const;
    PSOAndSRB solidTrianglePsoAndSrb() const;
    PSOAndSRB checkerboardPsoAndSrb() const;
    PSOAndSRB gridPsoAndSrb() const;

    RefCntAutoPtr<ISampler> spriteSampler() const;

    bool isInitialized() const;

private:
    class Impl;
    Impl* impl_;
};

}
