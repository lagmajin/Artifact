module;
#include <QByteArray>
#include <Shader.h>
#include <RenderDevice.h>
#include <RefCntAutoPtr.hpp>
module Artifact.Render.TextGlyphShaderSources;

namespace Artifact {

QByteArray artifactTextGlyphPixelShaderSource() {
    return QByteArrayLiteral(R"(
struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR0;
};
Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);
float4 main(PS_INPUT input) : SV_TARGET {
    const float4 sampled = g_texture.Sample(g_sampler, input.TexCoord);
    if (input.Color.a < 0.0f)
        return float4(sampled.rgb, sampled.a * -input.Color.a);
    const float alpha = sampled.a;
    return float4(input.Color.rgb * alpha, input.Color.a * alpha);
}
)");
}

QByteArray artifactTextGlyphVertexShaderSource() {
    return QByteArrayLiteral(R"(
cbuffer TransformCB { float2 Offset; float2 Scale; float2 ScreenSize; };
struct VS_INPUT {
    float2 Pos : ATTRIB0;
    float2 TexCoord : ATTRIB1;
    float4 Color : ATTRIB2;
};
struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR0;
};
void main(VS_INPUT input, out PS_INPUT output) {
    float2 pixel = Offset + input.Pos * Scale;
    float2 ndc = float2(pixel.x / max(ScreenSize.x, 1.0f) * 2.0f - 1.0f,
                        1.0f - pixel.y / max(ScreenSize.y, 1.0f) * 2.0f);
    output.Position = float4(ndc, 0.0f, 1.0f);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
}
)");
}

QByteArray artifactTextGlyphTransformVertexShaderSource() {
    return QByteArrayLiteral(R"(
cbuffer TransformCB { float4x4 g_Transform; };
struct VS_INPUT {
    float2 Pos : ATTRIB0;
    float2 TexCoord : ATTRIB1;
    float4 Color : ATTRIB2;
};
struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR0;
};
void main(VS_INPUT input, out PS_INPUT output) {
    output.Position = mul(float4(input.Pos, 0.0f, 1.0f), g_Transform);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
}
)");
}

bool createArtifactTextGlyphShaders(
    Diligent::IRenderDevice* device,
    Diligent::RefCntAutoPtr<Diligent::IShader>& pixelShader,
    Diligent::RefCntAutoPtr<Diligent::IShader>& vertexShader,
    Diligent::RefCntAutoPtr<Diligent::IShader>& transformVertexShader) {
    pixelShader = nullptr;
    vertexShader = nullptr;
    transformVertexShader = nullptr;
    if (!device) return false;

    const QByteArray pixelSource = artifactTextGlyphPixelShaderSource();
    Diligent::ShaderCreateInfo pixelInfo;
    pixelInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    pixelInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    pixelInfo.Desc.Name = "ArtifactTextGlyphPixelShader";
    pixelInfo.Source = pixelSource.constData();
    pixelInfo.SourceLength = static_cast<Diligent::Uint32>(pixelSource.size());
    device->CreateShader(pixelInfo, &pixelShader);

    const QByteArray vertexSource = artifactTextGlyphVertexShaderSource();
    Diligent::ShaderCreateInfo vertexInfo;
    vertexInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    vertexInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    vertexInfo.Desc.Name = "ArtifactTextGlyphVertexShader";
    vertexInfo.Source = vertexSource.constData();
    vertexInfo.SourceLength = static_cast<Diligent::Uint32>(vertexSource.size());
    device->CreateShader(vertexInfo, &vertexShader);

    const QByteArray transformSource = artifactTextGlyphTransformVertexShaderSource();
    Diligent::ShaderCreateInfo transformInfo;
    transformInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    transformInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    transformInfo.Desc.Name = "ArtifactTextGlyphTransformVertexShader";
    transformInfo.Source = transformSource.constData();
    transformInfo.SourceLength = static_cast<Diligent::Uint32>(transformSource.size());
    device->CreateShader(transformInfo, &transformVertexShader);
    return pixelShader != nullptr && vertexShader != nullptr && transformVertexShader != nullptr;
}

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
    Diligent::RefCntAutoPtr<Diligent::ISampler>& atlasSampler) {
    glyphPipeline = nullptr;
    glyphBinding = nullptr;
    transformedPipeline = nullptr;
    transformedBinding = nullptr;
    atlasSampler = nullptr;
    if (!device || !vertexShader || !pixelShader || !transformVertexShader) return false;

    static const Diligent::LayoutElement layout[] = {
        {0, 0, 2, Diligent::VT_FLOAT32, false},
        {1, 0, 2, Diligent::VT_FLOAT32, false},
        {2, 0, 4, Diligent::VT_FLOAT32, false}};
    static const Diligent::LayoutElement transformLayout[] = {
        {0, 0, 2, Diligent::VT_FLOAT32, false, 0, 8 * sizeof(float)},
        {1, 0, 2, Diligent::VT_FLOAT32, false, 2 * sizeof(float), 8 * sizeof(float)},
        {2, 0, 4, Diligent::VT_FLOAT32, false, 4 * sizeof(float), 8 * sizeof(float)}};
    static const Diligent::ShaderResourceVariableDesc variables[] = {
        {Diligent::SHADER_TYPE_VERTEX, "TransformCB", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_PIXEL, "g_texture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_PIXEL, "g_sampler", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};

    Diligent::GraphicsPipelineStateCreateInfo info;
    info.PSODesc.Name = "ArtifactTextGlyph PSO";
    info.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    info.pVS = vertexShader;
    info.pPS = pixelShader;
    auto& gp = info.GraphicsPipeline;
    gp.NumRenderTargets = 1;
    gp.RTVFormats[0] = targetFormat;
    gp.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    gp.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
    gp.DepthStencilDesc.DepthEnable = false;
    auto& blend = gp.BlendDesc.RenderTargets[0];
    blend.BlendEnable = true;
    blend.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
    blend.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    blend.BlendOp = Diligent::BLEND_OPERATION_ADD;
    blend.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
    blend.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    gp.InputLayout.LayoutElements = layout;
    gp.InputLayout.NumElements = 3;
    info.PSODesc.ResourceLayout.Variables = variables;
    info.PSODesc.ResourceLayout.NumVariables = 3;
    device->CreateGraphicsPipelineState(info, &glyphPipeline);
    if (glyphPipeline) glyphPipeline->CreateShaderResourceBinding(&glyphBinding, true);

    info.PSODesc.Name = "ArtifactTextGlyph Transform PSO";
    info.pVS = transformVertexShader;
    gp.InputLayout.LayoutElements = transformLayout;
    device->CreateGraphicsPipelineState(info, &transformedPipeline);
    if (transformedPipeline) transformedPipeline->CreateShaderResourceBinding(&transformedBinding, true);

    Diligent::SamplerDesc samplerDesc;
    samplerDesc.MinFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = Diligent::FILTER_TYPE_LINEAR;
    device->CreateSampler(samplerDesc, &atlasSampler);
    return glyphPipeline && glyphBinding && transformedPipeline && transformedBinding && atlasSampler;
}

}
