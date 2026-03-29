module;
#include <RenderDevice.h>
#include <Shader.h>
#include <PipelineState.h>
#include <Sampler.h>
#include <RefCntAutoPtr.hpp>
#include <BasicMath.hpp>
#include <QByteArray>
#include <cmath>

module Artifact.Render.ShaderManager;

import Graphics.Shader.Set;
import Graphics;
import Graphics.Shader.Compile.Task;
import Graphics.Shader.HLSL.Basics.Vertex;
import Render.Shader.ThickLine;
import Render.Shader.ViewerHelpers;

namespace Artifact {

using namespace Diligent;
using namespace ArtifactCore;

class ShaderManager::Impl {
public:
    RefCntAutoPtr<IRenderDevice> device_;
    TEXTURE_FORMAT rtvFormat_ = TEX_FORMAT_RGBA8_UNORM_SRGB;

    RenderShaderPair lineShaders_;
    RenderShaderPair outlineShaders_;
    RenderShaderPair solidShaders_;
    RenderShaderPair solidRectTransformShaders_;
    RenderShaderPair spriteShaders_;
    RenderShaderPair thickLineShaders_;
    RenderShaderPair dotLineShaders_;
    RenderShaderPair solidTriangleShaders_;
    RenderShaderPair checkerboardShaders_;
    RenderShaderPair gridShaders_;
    RenderShaderPair spriteTransformShaders_;
    RenderShaderPair maskedSpriteShaders_;
    RenderShaderPair gizmo3DShaders_;

    PSOAndSRB linePsoAndSrb_;
    PSOAndSRB outlinePsoAndSrb_;
    PSOAndSRB solidRectPsoAndSrb_;
    PSOAndSRB solidRectTransformPsoAndSrb_;
    PSOAndSRB spritePsoAndSrb_;
    PSOAndSRB thickLinePsoAndSrb_;
    PSOAndSRB dotLinePsoAndSrb_;
    PSOAndSRB solidTrianglePsoAndSrb_;
    PSOAndSRB checkerboardPsoAndSrb_;
    PSOAndSRB gridPsoAndSrb_;
    PSOAndSRB spriteTransformPsoAndSrb_;
    PSOAndSRB maskedSpritePsoAndSrb_;
    PSOAndSRB gizmo3DPsoAndSrb_;

    RefCntAutoPtr<ISampler> spriteSampler_;

    bool initialized_ = false;

    Impl() = default;

    Impl(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT rtvFormat)
        : device_(device), rtvFormat_(rtvFormat), initialized_(true)
    {
    }

    void createShaders();
    void createPSOs();
    void destroy();
};

void ShaderManager::Impl::createShaders()
{
    if (!device_) {
        return;
    }

    ShaderCreateInfo lineVsInfo;
    lineVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    lineVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    lineVsInfo.EntryPoint = "main";
    lineVsInfo.Desc.Name = "LayerEditorVertexShader";
    lineVsInfo.Source = g_thickLineVS.constData();
    lineVsInfo.SourceLength = g_thickLineVS.length();

    ShaderCreateInfo linePsInfo;
    linePsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    linePsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    linePsInfo.Desc.Name = "LayerEditorVertexMyPixelShader";
    linePsInfo.Source = g_thickLinePS.constData();
    linePsInfo.SourceLength = g_thickLinePS.length();

    ShaderCreateInfo drawOutlineRectVsInfo;
    drawOutlineRectVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    drawOutlineRectVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    drawOutlineRectVsInfo.Desc.Name = "LayerEditorOutlineVertexShader";
    drawOutlineRectVsInfo.Source = ArtifactCore::drawOutlineRectVSSource.constData();
    drawOutlineRectVsInfo.SourceLength = ArtifactCore::drawOutlineRectVSSource.length();

    ShaderCreateInfo drawOutlineRectPsInfo;
    drawOutlineRectPsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    drawOutlineRectPsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    drawOutlineRectPsInfo.Desc.Name = "LayerEditorOutlinePixelShader";
    drawOutlineRectPsInfo.Source = drawOutlineRectPSSource.constData();
    drawOutlineRectPsInfo.SourceLength = drawOutlineRectPSSource.length();

    ShaderCreateInfo solidRectVsInfo;
    solidRectVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    solidRectVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    solidRectVsInfo.Desc.Name = "IRenderSolidRectVertexShader";
    solidRectVsInfo.Source = ArtifactCore::drawSolidRectVSSource.constData();
    solidRectVsInfo.SourceLength = ArtifactCore::drawSolidRectVSSource.length();

    ShaderCreateInfo solidPsInfo;
    solidPsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    solidPsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    solidPsInfo.Desc.Name = "IRenderSolidRectPixelShader";
    solidPsInfo.Source = g_qsSolidColorPSSource.constData();
    solidPsInfo.SourceLength = g_qsSolidColorPSSource.length();

    ShaderCreateInfo sprite2DVsInfo;
    sprite2DVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    sprite2DVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    sprite2DVsInfo.Desc.Name = "SpriteVertexShader";
    sprite2DVsInfo.Source = g_2DSpriteVS.constData();
    sprite2DVsInfo.SourceLength = g_2DSpriteVS.length();

    ShaderCreateInfo sprite2DPsInfo;
    sprite2DPsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    sprite2DPsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    sprite2DPsInfo.Desc.Name = "SpritePixelShader";
    sprite2DPsInfo.Source = g_qsBasicSprite2DImagePS.constData();
    sprite2DPsInfo.SourceLength = g_qsBasicSprite2DImagePS.length();

    ShaderCreateInfo solidRectVsInfo2;
    solidRectVsInfo2.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    solidRectVsInfo2.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    solidRectVsInfo2.Desc.Name = "SolidRectVertexShader";
    solidRectVsInfo2.Source = ArtifactCore::drawSolidRectVSSource.constData();
    solidRectVsInfo2.SourceLength = ArtifactCore::drawSolidRectVSSource.length();

    ShaderCreateInfo solidRectTransformVsInfo;
    solidRectTransformVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    solidRectTransformVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    solidRectTransformVsInfo.Desc.Name = "SolidRectTransformVertexShader";
    solidRectTransformVsInfo.Source = ArtifactCore::drawSolidRectTransformVSSource.constData();
    solidRectTransformVsInfo.SourceLength = ArtifactCore::drawSolidRectTransformVSSource.length();

    ShaderCreateInfo spriteTransformVsInfo;
    spriteTransformVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    spriteTransformVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    spriteTransformVsInfo.Desc.Name = "SpriteTransformVertexShader";
    spriteTransformVsInfo.Source = ArtifactCore::drawSpriteTransformVSSource.constData();
    spriteTransformVsInfo.SourceLength = ArtifactCore::drawSpriteTransformVSSource.length();

    const QByteArray maskedSpritePsSource = R"(
struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR0;
};

Texture2D g_scene : register(t0);
Texture2D g_mask  : register(t1);
SamplerState g_sampler : register(s0);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 scene = g_scene.Sample(g_sampler, input.TexCoord);
    float maskAlpha = g_mask.Sample(g_sampler, input.TexCoord).a;
    float4 color = scene * input.Color;
    color.a *= maskAlpha;
    return color;
}
)";
    ShaderCreateInfo maskedSpritePsInfo;
    maskedSpritePsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    maskedSpritePsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    maskedSpritePsInfo.Desc.Name = "MaskedSpritePixelShader";
    maskedSpritePsInfo.Source = maskedSpritePsSource.constData();
    maskedSpritePsInfo.SourceLength = maskedSpritePsSource.length();

    ShaderCreateInfo solidRectPsInfo2;
    solidRectPsInfo2.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    solidRectPsInfo2.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    solidRectPsInfo2.Desc.Name = "SolidRectPixelShader";
    solidRectPsInfo2.Source = g_qsSolidColorPSSource.constData();
    solidRectPsInfo2.SourceLength = g_qsSolidColorPSSource.length();

    ShaderCreateInfo checkerboardPsInfo;
    checkerboardPsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    checkerboardPsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    checkerboardPsInfo.Desc.Name = "CheckerboardPixelShader";
    checkerboardPsInfo.Source = g_checkerboardPS.constData();
    checkerboardPsInfo.SourceLength = g_checkerboardPS.length();

    ShaderCreateInfo gridPsInfo;
    gridPsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    gridPsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    gridPsInfo.Desc.Name = "GridPixelShader";
    gridPsInfo.Source = g_gridPS.constData();
    gridPsInfo.SourceLength = g_gridPS.length();

    device_->CreateShader(lineVsInfo, &lineShaders_.VS);
    device_->CreateShader(linePsInfo, &lineShaders_.PS);
    device_->CreateShader(sprite2DVsInfo, &spriteShaders_.VS);
    device_->CreateShader(sprite2DPsInfo, &spriteShaders_.PS);
    device_->CreateShader(solidRectVsInfo2, &solidShaders_.VS);
    device_->CreateShader(solidRectPsInfo2, &solidShaders_.PS);
    solidTriangleShaders_ = solidShaders_;
    device_->CreateShader(solidRectTransformVsInfo, &solidRectTransformShaders_.VS);
    device_->CreateShader(solidRectPsInfo2, &solidRectTransformShaders_.PS);
    device_->CreateShader(spriteTransformVsInfo, &spriteTransformShaders_.VS);
    spriteTransformShaders_.PS = spriteShaders_.PS;
    device_->CreateShader(spriteTransformVsInfo, &maskedSpriteShaders_.VS);
    device_->CreateShader(maskedSpritePsInfo, &maskedSpriteShaders_.PS);

    device_->CreateShader(checkerboardPsInfo, &checkerboardShaders_.PS);
    device_->CreateShader(gridPsInfo, &gridShaders_.PS);
    checkerboardShaders_.VS = solidShaders_.VS;
    gridShaders_.VS = solidShaders_.VS;

    {
        ShaderCreateInfo thickLineVsInfo;
        thickLineVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        thickLineVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
        thickLineVsInfo.Desc.Name = "ThickLineVertexShader";
        thickLineVsInfo.Source = g_thickLineVS.constData();
        thickLineVsInfo.SourceLength = g_thickLineVS.length();

        ShaderCreateInfo thickLinePsInfo;
        thickLinePsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        thickLinePsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
        thickLinePsInfo.Desc.Name = "ThickLinePixelShader";
        thickLinePsInfo.Source = g_thickLinePS.constData();
        thickLinePsInfo.SourceLength = g_thickLinePS.length();

        device_->CreateShader(thickLineVsInfo, &thickLineShaders_.VS);
        device_->CreateShader(thickLinePsInfo, &thickLineShaders_.PS);
    }

    {
        ShaderCreateInfo dotLineVsInfo;
        dotLineVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        dotLineVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
        dotLineVsInfo.Desc.Name = "DotLineVertexShader";
        dotLineVsInfo.Source = g_dotLineVS.constData();
        dotLineVsInfo.SourceLength = g_dotLineVS.length();

        ShaderCreateInfo dotLinePsInfo;
        dotLinePsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        dotLinePsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
        dotLinePsInfo.Desc.Name = "DotLinePixelShader";
        dotLinePsInfo.Source = g_dotLinePS.constData();
        dotLinePsInfo.SourceLength = g_dotLinePS.length();

        device_->CreateShader(dotLineVsInfo, &dotLineShaders_.VS);
        device_->CreateShader(dotLinePsInfo, &dotLineShaders_.PS);
    }

    {
        // Gizmo 3D Shader (Simple 3D transformation)
        const char* gizmo3DVS = R"(
            cbuffer TransformCB {
                float4x4 g_WorldViewProj;
            };
            struct VSInput {
                float3 Pos   : ATTRIB0;
                float4 Color : ATTRIB1;
            };
            struct PSInput {
                float4 Pos   : SV_POSITION;
                float4 Color : COLOR;
            };
            void main(in VSInput VSIn, out PSInput PSIn) {
                PSIn.Pos   = mul(float4(VSIn.Pos, 1.0), g_WorldViewProj);
                PSIn.Color = VSIn.Color;
            }
        )";
        ShaderCreateInfo vsInfo;
        vsInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        vsInfo.Desc.ShaderType = SHADER_TYPE_VERTEX;
        vsInfo.Desc.Name = "Gizmo3D VS";
        vsInfo.Source = gizmo3DVS;
        
        device_->CreateShader(vsInfo, &gizmo3DShaders_.VS);
        gizmo3DShaders_.PS = lineShaders_.PS; // Reuse simple color pixel shader
    }
}

void ShaderManager::Impl::createPSOs()
{
    if (!device_) {
        return;
    }

    GraphicsPipelineStateCreateInfo drawLinePSOCreateInfo;
    drawLinePSOCreateInfo.PSODesc.Name = "DrawLine PSO";
    drawLinePSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    LayoutElement lineLayoutElems[] = {
        LayoutElement{0, 0, 2, VT_FLOAT32, false},
        LayoutElement{1, 0, 4, VT_FLOAT32, false}
    };

    drawLinePSOCreateInfo.pVS = lineShaders_.VS;
    drawLinePSOCreateInfo.pPS = lineShaders_.PS;

    auto& LGP = drawLinePSOCreateInfo.GraphicsPipeline;
    LGP.NumRenderTargets = 1;
    LGP.RTVFormats[0] = rtvFormat_;
    LGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_LINE_LIST;
    LGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
    LGP.DepthStencilDesc.DepthEnable = False;
    LGP.InputLayout.LayoutElements = lineLayoutElems;
    LGP.InputLayout.NumElements = 2;

    ShaderResourceVariableDesc Vars_Line[] = {
        { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
    };
    drawLinePSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars_Line;
    drawLinePSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars_Line);

    device_->CreateGraphicsPipelineState(drawLinePSOCreateInfo, &linePsoAndSrb_.pPSO);
    if (linePsoAndSrb_.pPSO)
    {
        linePsoAndSrb_.pPSO->CreateShaderResourceBinding(&linePsoAndSrb_.pSRB, true);
    }

    GraphicsPipelineStateCreateInfo drawSolidRectPSOCreateInfo;
    drawSolidRectPSOCreateInfo.PSODesc.Name = "DrawSolidRect PSO";
    drawSolidRectPSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    LayoutElement solidRectLayoutElems[] = {
        LayoutElement{0, 0, 2, VT_FLOAT32, false},
        LayoutElement{1, 0, 4, VT_FLOAT32, false}
    };

    auto& GP = drawSolidRectPSOCreateInfo.GraphicsPipeline;
    GP.NumRenderTargets = 1;
    GP.RTVFormats[0] = rtvFormat_;
    GP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    GP.RasterizerDesc.CullMode = CULL_MODE_NONE;
    GP.DepthStencilDesc.DepthEnable = False;
    auto& solidRTBlend = GP.BlendDesc.RenderTargets[0];
    solidRTBlend.BlendEnable = True;
    solidRTBlend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    solidRTBlend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    solidRTBlend.BlendOp = BLEND_OPERATION_ADD;
    solidRTBlend.SrcBlendAlpha = BLEND_FACTOR_ONE;
    solidRTBlend.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    solidRTBlend.BlendOpAlpha = BLEND_OPERATION_ADD;
    solidRTBlend.RenderTargetWriteMask = COLOR_MASK_ALL;
    LayoutElement LayoutElems2[] =
    {
        LayoutElement{0, 0, 2, VT_FLOAT32, false},
        LayoutElement{1, 0, 4, VT_FLOAT32, false}
    };
    GP.InputLayout.LayoutElements = LayoutElems2;
    GP.InputLayout.NumElements = 2;
    ShaderResourceVariableDesc Vars2[] =
    {
        { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL,  "ColorBuffer", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };
    drawSolidRectPSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars2;
    drawSolidRectPSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars2);

    drawSolidRectPSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = solidRectLayoutElems;
    drawSolidRectPSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = 2;

    drawSolidRectPSOCreateInfo.pVS = solidShaders_.VS;
    drawSolidRectPSOCreateInfo.pPS = solidShaders_.PS;

    device_->CreateGraphicsPipelineState(drawSolidRectPSOCreateInfo, &solidRectPsoAndSrb_.pPSO);
    if (solidRectPsoAndSrb_.pPSO)
    {
        solidRectPsoAndSrb_.pPSO->CreateShaderResourceBinding(&solidRectPsoAndSrb_.pSRB, true);
    }

    GraphicsPipelineStateCreateInfo drawSolidRectTransformPSOCreateInfo = drawSolidRectPSOCreateInfo;
    drawSolidRectTransformPSOCreateInfo.PSODesc.Name = "DrawSolidRectTransform PSO";
    drawSolidRectTransformPSOCreateInfo.pVS = solidRectTransformShaders_.VS;
    drawSolidRectTransformPSOCreateInfo.pPS = solidRectTransformShaders_.PS;
    drawSolidRectTransformPSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    device_->CreateGraphicsPipelineState(drawSolidRectTransformPSOCreateInfo, &solidRectTransformPsoAndSrb_.pPSO);
    if (solidRectTransformPsoAndSrb_.pPSO)
    {
        solidRectTransformPsoAndSrb_.pPSO->CreateShaderResourceBinding(&solidRectTransformPsoAndSrb_.pSRB, true);
    }

    GraphicsPipelineStateCreateInfo spritePSOCreateInfo;
    spritePSOCreateInfo.PSODesc.Name = "DrawSprite PSO";
    spritePSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    LayoutElement spriteLayoutElems[] = {
        LayoutElement{0, 0, 2, VT_FLOAT32, false},
        LayoutElement{1, 0, 2, VT_FLOAT32, false},
        LayoutElement{2, 0, 4, VT_FLOAT32, false}
    };
    auto& GP2 = spritePSOCreateInfo.GraphicsPipeline;
    GP2.NumRenderTargets = 1;
    GP2.RTVFormats[0] = rtvFormat_;
    GP2.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    GP2.RasterizerDesc.CullMode = CULL_MODE_NONE;
    GP2.DepthStencilDesc.DepthEnable = False;
    auto& spriteRTBlend = GP2.BlendDesc.RenderTargets[0];
    spriteRTBlend.BlendEnable = True;
    spriteRTBlend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    spriteRTBlend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    spriteRTBlend.BlendOp = BLEND_OPERATION_ADD;
    spriteRTBlend.SrcBlendAlpha = BLEND_FACTOR_ONE;
    spriteRTBlend.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    spriteRTBlend.BlendOpAlpha = BLEND_OPERATION_ADD;
    spriteRTBlend.RenderTargetWriteMask = COLOR_MASK_ALL;
    GP2.InputLayout.LayoutElements = spriteLayoutElems;
    GP2.InputLayout.NumElements = 3;
    ShaderResourceVariableDesc spriteVars[] = {
        { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL, "g_texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL, "g_sampler", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };
    spritePSOCreateInfo.PSODesc.ResourceLayout.Variables = spriteVars;
    spritePSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(spriteVars);
    spritePSOCreateInfo.pVS = spriteShaders_.VS;
    spritePSOCreateInfo.pPS = spriteShaders_.PS;

    device_->CreateGraphicsPipelineState(spritePSOCreateInfo, &spritePsoAndSrb_.pPSO);
    if (spritePsoAndSrb_.pPSO)
    {
        spritePsoAndSrb_.pPSO->CreateShaderResourceBinding(&spritePsoAndSrb_.pSRB, true);
    }

    GraphicsPipelineStateCreateInfo drawSpriteTransformPSOCreateInfo = spritePSOCreateInfo;
    drawSpriteTransformPSOCreateInfo.PSODesc.Name = "DrawSpriteTransform PSO";
    drawSpriteTransformPSOCreateInfo.pVS = spriteTransformShaders_.VS;
    drawSpriteTransformPSOCreateInfo.pPS = spriteTransformShaders_.PS;
    drawSpriteTransformPSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    device_->CreateGraphicsPipelineState(drawSpriteTransformPSOCreateInfo, &spriteTransformPsoAndSrb_.pPSO);
    if (spriteTransformPsoAndSrb_.pPSO)
    {
        spriteTransformPsoAndSrb_.pPSO->CreateShaderResourceBinding(&spriteTransformPsoAndSrb_.pSRB, true);
    }

    GraphicsPipelineStateCreateInfo maskedSpritePSOCreateInfo = spritePSOCreateInfo;
    maskedSpritePSOCreateInfo.PSODesc.Name = "DrawMaskedSprite PSO";
    maskedSpritePSOCreateInfo.pVS = maskedSpriteShaders_.VS;
    maskedSpritePSOCreateInfo.pPS = maskedSpriteShaders_.PS;
    ShaderResourceVariableDesc maskedSpriteVars[] = {
        { SHADER_TYPE_PIXEL, "g_scene", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL, "g_mask", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL, "g_sampler", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };
    maskedSpritePSOCreateInfo.PSODesc.ResourceLayout.Variables = maskedSpriteVars;
    maskedSpritePSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(maskedSpriteVars);
    device_->CreateGraphicsPipelineState(maskedSpritePSOCreateInfo, &maskedSpritePsoAndSrb_.pPSO);
    if (maskedSpritePsoAndSrb_.pPSO)
    {
        maskedSpritePsoAndSrb_.pPSO->CreateShaderResourceBinding(&maskedSpritePsoAndSrb_.pSRB, true);
    }

    GraphicsPipelineStateCreateInfo drawSolidTrianglePSOCreateInfo = drawSolidRectPSOCreateInfo;
    drawSolidTrianglePSOCreateInfo.PSODesc.Name = "DrawSolidTriangle PSO";
    drawSolidTrianglePSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    device_->CreateGraphicsPipelineState(drawSolidTrianglePSOCreateInfo, &solidTrianglePsoAndSrb_.pPSO);
    if (solidTrianglePsoAndSrb_.pPSO)
    {
        solidTrianglePsoAndSrb_.pPSO->CreateShaderResourceBinding(&solidTrianglePsoAndSrb_.pSRB, true);
    }

    SamplerDesc spriteSamplerDesc;
    spriteSamplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    spriteSamplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    spriteSamplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    spriteSamplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
    spriteSamplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
    spriteSamplerDesc.AddressW = TEXTURE_ADDRESS_CLAMP;
    spriteSamplerDesc.ComparisonFunc = COMPARISON_FUNC_ALWAYS;
    spriteSamplerDesc.MaxAnisotropy = 1;
    spriteSamplerDesc.MipLODBias = 0.0f;
    spriteSamplerDesc.MinLOD = 0.0f;
    spriteSamplerDesc.MaxLOD = FLT_MAX;
    device_->CreateSampler(spriteSamplerDesc, &spriteSampler_);

    {
        GraphicsPipelineStateCreateInfo thickLinePSOCreateInfo;
        thickLinePSOCreateInfo.PSODesc.Name = "DrawThickLine PSO";
        thickLinePSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        LayoutElement thickLineLayoutElems[] = {
            LayoutElement{0, 0, 2, VT_FLOAT32, false},
            LayoutElement{1, 0, 4, VT_FLOAT32, false}
        };

        auto& TLGP = thickLinePSOCreateInfo.GraphicsPipeline;
        TLGP.NumRenderTargets = 1;
        TLGP.RTVFormats[0] = rtvFormat_;
        TLGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        TLGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
        TLGP.DepthStencilDesc.DepthEnable = False;
        TLGP.InputLayout.LayoutElements = thickLineLayoutElems;
        TLGP.InputLayout.NumElements = 2;

        ShaderResourceVariableDesc thickLineVars[] = {
            { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
        };
        thickLinePSOCreateInfo.PSODesc.ResourceLayout.Variables = thickLineVars;
        thickLinePSOCreateInfo.PSODesc.ResourceLayout.NumVariables = 1;

        thickLinePSOCreateInfo.pVS = thickLineShaders_.VS;
        thickLinePSOCreateInfo.pPS = thickLineShaders_.PS;

        device_->CreateGraphicsPipelineState(thickLinePSOCreateInfo, &thickLinePsoAndSrb_.pPSO);
        if (thickLinePsoAndSrb_.pPSO)
        {
            thickLinePsoAndSrb_.pPSO->CreateShaderResourceBinding(&thickLinePsoAndSrb_.pSRB, true);
        }
    }

    {
        GraphicsPipelineStateCreateInfo dotLinePSOCreateInfo;
        dotLinePSOCreateInfo.PSODesc.Name = "DrawDotLine PSO";
        dotLinePSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        LayoutElement dotLineLayoutElems[] = {
            LayoutElement{0, 0, 2, VT_FLOAT32, false},
            LayoutElement{1, 0, 4, VT_FLOAT32, false},
            LayoutElement{2, 0, 1, VT_FLOAT32, false}
        };

        auto& DLGP = dotLinePSOCreateInfo.GraphicsPipeline;
        DLGP.NumRenderTargets = 1;
        DLGP.RTVFormats[0] = rtvFormat_;
        DLGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        DLGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
        DLGP.DepthStencilDesc.DepthEnable = False;
        DLGP.InputLayout.LayoutElements = dotLineLayoutElems;
        DLGP.InputLayout.NumElements = 3;

        ShaderResourceVariableDesc dotLineVars[] = {
            { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_PIXEL,  "DotLineCB",   SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
        };
        dotLinePSOCreateInfo.PSODesc.ResourceLayout.Variables = dotLineVars;
        dotLinePSOCreateInfo.PSODesc.ResourceLayout.NumVariables = 2;

        dotLinePSOCreateInfo.pVS = dotLineShaders_.VS;
        dotLinePSOCreateInfo.pPS = dotLineShaders_.PS;

        device_->CreateGraphicsPipelineState(dotLinePSOCreateInfo, &dotLinePsoAndSrb_.pPSO);
        if (dotLinePsoAndSrb_.pPSO)
        {
            dotLinePsoAndSrb_.pPSO->CreateShaderResourceBinding(&dotLinePsoAndSrb_.pSRB, true);
        }
    }

    {
        GraphicsPipelineStateCreateInfo PSOCreateInfo = drawSolidRectPSOCreateInfo;
        PSOCreateInfo.PSODesc.Name = "Checkerboard PSO";
        PSOCreateInfo.pPS = checkerboardShaders_.PS;
        ShaderResourceVariableDesc Vars[] = {
            { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_PIXEL,  "ViewerHelperCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
        };
        PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
        PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);
        device_->CreateGraphicsPipelineState(PSOCreateInfo, &checkerboardPsoAndSrb_.pPSO);
        if (checkerboardPsoAndSrb_.pPSO)
            checkerboardPsoAndSrb_.pPSO->CreateShaderResourceBinding(&checkerboardPsoAndSrb_.pSRB, true);
    }

    {
        GraphicsPipelineStateCreateInfo PSOCreateInfo = drawSolidRectPSOCreateInfo;
        PSOCreateInfo.PSODesc.Name = "Grid PSO";
        PSOCreateInfo.pPS = gridShaders_.PS;
        ShaderResourceVariableDesc Vars[] = {
            { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_PIXEL,  "ViewerHelperCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
        };
        PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
        PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);
        device_->CreateGraphicsPipelineState(PSOCreateInfo, &gridPsoAndSrb_.pPSO);
        if (gridPsoAndSrb_.pPSO)
            gridPsoAndSrb_.pPSO->CreateShaderResourceBinding(&gridPsoAndSrb_.pSRB, true);
    }

    {
        // Gizmo 3D PSO
        GraphicsPipelineStateCreateInfo PSOCreateInfo;
        PSOCreateInfo.PSODesc.Name = "Gizmo3D PSO";
        PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        LayoutElement layoutElems[] = {
            LayoutElement{0, 0, 3, VT_FLOAT32, false}, // Pos (float3)
            LayoutElement{1, 0, 4, VT_FLOAT32, false}  // Color (float4)
        };

        auto& GP = PSOCreateInfo.GraphicsPipeline;
        GP.NumRenderTargets = 1;
        GP.RTVFormats[0] = rtvFormat_;
        GP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_LINE_LIST;
        GP.RasterizerDesc.CullMode = CULL_MODE_NONE;
        
        // Depth: Always show on top
        GP.DepthStencilDesc.DepthEnable = True;
        GP.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_ALWAYS;
        
        GP.BlendDesc.RenderTargets[0].BlendEnable = True;
        GP.BlendDesc.RenderTargets[0].SrcBlend = BLEND_FACTOR_SRC_ALPHA;
        GP.BlendDesc.RenderTargets[0].DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;

        GP.InputLayout.LayoutElements = layoutElems;
        GP.InputLayout.NumElements = 2;

        ShaderResourceVariableDesc vars[] = {
            { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
        };
        PSOCreateInfo.PSODesc.ResourceLayout.Variables = vars;
        PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = 1;

        PSOCreateInfo.pVS = gizmo3DShaders_.VS;
        PSOCreateInfo.pPS = gizmo3DShaders_.PS;

        device_->CreateGraphicsPipelineState(PSOCreateInfo, &gizmo3DPsoAndSrb_.pPSO);
        if (gizmo3DPsoAndSrb_.pPSO)
            gizmo3DPsoAndSrb_.pPSO->CreateShaderResourceBinding(&gizmo3DPsoAndSrb_.pSRB, true);
    }
}

void ShaderManager::Impl::destroy()
{
    linePsoAndSrb_.pPSO.Release();
    linePsoAndSrb_.pSRB.Release();
    outlinePsoAndSrb_.pPSO.Release();
    outlinePsoAndSrb_.pSRB.Release();
    solidRectPsoAndSrb_.pPSO.Release();
    solidRectPsoAndSrb_.pSRB.Release();
    solidRectTransformPsoAndSrb_.pPSO.Release();
    solidRectTransformPsoAndSrb_.pSRB.Release();
    spritePsoAndSrb_.pPSO.Release();
    spritePsoAndSrb_.pSRB.Release();
    maskedSpritePsoAndSrb_.pPSO.Release();
    maskedSpritePsoAndSrb_.pSRB.Release();
    thickLinePsoAndSrb_.pPSO.Release();
    thickLinePsoAndSrb_.pSRB.Release();
    dotLinePsoAndSrb_.pPSO.Release();
    dotLinePsoAndSrb_.pSRB.Release();
    solidTrianglePsoAndSrb_.pPSO.Release();
    solidTrianglePsoAndSrb_.pSRB.Release();
    checkerboardPsoAndSrb_.pPSO.Release();
    checkerboardPsoAndSrb_.pSRB.Release();
    gridPsoAndSrb_.pPSO.Release();
    gridPsoAndSrb_.pSRB.Release();
    gizmo3DPsoAndSrb_.pPSO.Release();
    gizmo3DPsoAndSrb_.pSRB.Release();

    lineShaders_.VS.Release();
    lineShaders_.PS.Release();
    outlineShaders_.VS.Release();
    outlineShaders_.PS.Release();
    solidShaders_.VS.Release();
    solidShaders_.PS.Release();
    solidRectTransformShaders_.VS.Release();
    solidRectTransformShaders_.PS.Release();
    spriteShaders_.VS.Release();
    spriteShaders_.PS.Release();
    maskedSpriteShaders_.VS.Release();
    maskedSpriteShaders_.PS.Release();
    thickLineShaders_.VS.Release();
    thickLineShaders_.PS.Release();
    dotLineShaders_.VS.Release();
    dotLineShaders_.PS.Release();
    checkerboardShaders_.VS.Release();
    checkerboardShaders_.PS.Release();
    gridShaders_.VS.Release();
    gridShaders_.PS.Release();
    gizmo3DShaders_.VS.Release();
    gizmo3DShaders_.PS.Release();

    spriteSampler_.Release();

    initialized_ = false;
}

ShaderManager::ShaderManager()
    : impl_(new Impl())
{
}

ShaderManager::ShaderManager(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT rtvFormat)
    : impl_(new Impl(device, rtvFormat))
{
}

ShaderManager::~ShaderManager()
{
    delete impl_;
}

void ShaderManager::initialize(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT rtvFormat)
{
    impl_->device_ = device;
    impl_->rtvFormat_ = rtvFormat;
    impl_->initialized_ = true;
}

void ShaderManager::createShaders()
{
    impl_->createShaders();
}

void ShaderManager::createPSOs()
{
    impl_->createPSOs();
}

void ShaderManager::destroy()
{
    impl_->destroy();
}

RenderShaderPair ShaderManager::lineShaders() const
{
    return impl_->lineShaders_;
}

RenderShaderPair ShaderManager::outlineShaders() const
{
    return impl_->outlineShaders_;
}

RenderShaderPair ShaderManager::solidShaders() const
{
    return impl_->solidShaders_;
}

RenderShaderPair ShaderManager::spriteShaders() const
{
    return impl_->spriteShaders_;
}

RenderShaderPair ShaderManager::thickLineShaders() const
{
    return impl_->thickLineShaders_;
}

RenderShaderPair ShaderManager::dotLineShaders() const
{
    return impl_->dotLineShaders_;
}

RenderShaderPair ShaderManager::solidTriangleShaders() const
{
    return impl_->solidTriangleShaders_;
}

RenderShaderPair ShaderManager::checkerboardShaders() const
{
    return impl_->checkerboardShaders_;
}

RenderShaderPair ShaderManager::gridShaders() const
{
    return impl_->gridShaders_;
}

RenderShaderPair ShaderManager::maskedSpriteShaders() const
{
    return impl_->maskedSpriteShaders_;
}

PSOAndSRB ShaderManager::linePsoAndSrb() const
{
    return impl_->linePsoAndSrb_;
}

PSOAndSRB ShaderManager::outlinePsoAndSrb() const
{
    return impl_->outlinePsoAndSrb_;
}

PSOAndSRB ShaderManager::solidRectPsoAndSrb() const
{
    return impl_->solidRectPsoAndSrb_;
}

PSOAndSRB ShaderManager::solidRectTransformPsoAndSrb() const
{
    return impl_->solidRectTransformPsoAndSrb_;
}

PSOAndSRB ShaderManager::spritePsoAndSrb() const
{
    return impl_->spritePsoAndSrb_;
}

PSOAndSRB ShaderManager::thickLinePsoAndSrb() const
{
    return impl_->thickLinePsoAndSrb_;
}

PSOAndSRB ShaderManager::dotLinePsoAndSrb() const
{
    return impl_->dotLinePsoAndSrb_;
}

PSOAndSRB ShaderManager::solidTrianglePsoAndSrb() const
{
    return impl_->solidTrianglePsoAndSrb_;
}

PSOAndSRB ShaderManager::checkerboardPsoAndSrb() const
{
    return impl_->checkerboardPsoAndSrb_;
}

PSOAndSRB ShaderManager::gridPsoAndSrb() const
{
    return impl_->gridPsoAndSrb_;
}

PSOAndSRB ShaderManager::spriteTransformPsoAndSrb() const
{
    return impl_->spriteTransformPsoAndSrb_;
}

PSOAndSRB ShaderManager::maskedSpritePsoAndSrb() const
{
    return impl_->maskedSpritePsoAndSrb_;
}

PSOAndSRB ShaderManager::gizmo3DPsoAndSrb() const
{
    return impl_->gizmo3DPsoAndSrb_;
}

RefCntAutoPtr<ISampler> ShaderManager::spriteSampler() const
{
    return impl_->spriteSampler_;
}

bool ShaderManager::isInitialized() const
{
    return impl_->initialized_;
}

}
