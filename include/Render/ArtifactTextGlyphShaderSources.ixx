module;
#include <QByteArray>
#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
export module Artifact.Render.TextGlyphShaderSources;

export namespace Artifact {

/// Shader source provider independent of ShaderManager and renderer-wide PSOs.
QByteArray artifactTextGlyphPixelShaderSource();
QByteArray artifactTextGlyphVertexShaderSource();
QByteArray artifactTextGlyphTransformVertexShaderSource();

bool createArtifactTextGlyphShaders(
    Diligent::IRenderDevice* device,
    Diligent::RefCntAutoPtr<Diligent::IShader>& pixelShader,
    Diligent::RefCntAutoPtr<Diligent::IShader>& vertexShader,
    Diligent::RefCntAutoPtr<Diligent::IShader>& transformVertexShader);

bool createArtifactTextGlyphPipelines(
    Diligent::IRenderDevice* device,
    Diligent::TEXTURE_FORMAT targetFormat,
    Diligent::IShader* vertexShader,
    Diligent::IShader* pixelShader,
    Diligent::IShader* transformVertexShader,
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>& glyphPipeline,
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& glyphBinding,
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>& transformedPipeline,
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& transformedBinding,
    Diligent::RefCntAutoPtr<Diligent::ISampler>& atlasSampler);

}
