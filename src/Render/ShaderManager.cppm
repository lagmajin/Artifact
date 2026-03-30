module;
#include <RenderDevice.h>
#include <Shader.h>
#include <PipelineState.h>
#include <Sampler.h>
#include <RefCntAutoPtr.hpp>
#include <tbb/task_group.h>
#include <BasicMath.hpp>
#include <QByteArray>
#include <QDebug>
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

    void createLineFamilyPSOs();
    void createSpriteFamilyPSOs();
    void createUtilityFamilyPSOs();
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

void ShaderManager::Impl::createLineFamilyPSOs()
{
    if (!device_) {
        return;
    }

    static const LayoutElement lineLayoutElems[] = {
        LayoutElement{0, 0, 2, VT_FLOAT32, false},
        LayoutElement{1, 0, 4, VT_FLOAT32, false}
    };
    static const ShaderResourceVariableDesc lineVars[] = {
        { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
    };

    GraphicsPipelineStateCreateInfo lineInfo;
    lineInfo.PSODesc.Name = "DrawLine PSO";
    lineInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    lineInfo.pVS = lineShaders_.VS;
    lineInfo.pPS = lineShaders_.PS;
    auto& lineGP = lineInfo.GraphicsPipeline;
    lineGP.NumRenderTargets = 1;
    lineGP.RTVFormats[0] = rtvFormat_;
    lineGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_LINE_LIST;
    lineGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
    lineGP.DepthStencilDesc.DepthEnable = False;
    lineGP.InputLayout.LayoutElements = lineLayoutElems;
    lineGP.InputLayout.NumElements = _countof(lineLayoutElems);
    lineInfo.PSODesc.ResourceLayout.Variables = lineVars;
    lineInfo.PSODesc.ResourceLayout.NumVariables = _countof(lineVars);
    device_->CreateGraphicsPipelineState(lineInfo, &linePsoAndSrb_.pPSO);
    if (linePsoAndSrb_.pPSO) {
        linePsoAndSrb_.pPSO->CreateShaderResourceBinding(&linePsoAndSrb_.pSRB, true);
    }

    static const LayoutElement solidLayoutElems[] = {
        LayoutElement{0, 0, 2, VT_FLOAT32, false},
        LayoutElement{1, 0, 4, VT_FLOAT32, false}
    };
    static const ShaderResourceVariableDesc solidVars[] = {
        { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL,  "ColorBuffer", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };

    GraphicsPipelineStateCreateInfo solidInfo;
    solidInfo.PSODesc.Name = "DrawSolidRect PSO";
    solidInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    solidInfo.pVS = solidShaders_.VS;
    solidInfo.pPS = solidShaders_.PS;
    auto& solidGP = solidInfo.GraphicsPipeline;
    solidGP.NumRenderTargets = 1;
    solidGP.RTVFormats[0] = rtvFormat_;
    solidGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    solidGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
    solidGP.DepthStencilDesc.DepthEnable = False;
    auto& solidBlend = solidGP.BlendDesc.RenderTargets[0];
    solidBlend.BlendEnable = True;
    solidBlend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    solidBlend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    solidBlend.BlendOp = BLEND_OPERATION_ADD;
    solidBlend.SrcBlendAlpha = BLEND_FACTOR_ONE;
    solidBlend.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    solidBlend.BlendOpAlpha = BLEND_OPERATION_ADD;
    solidBlend.RenderTargetWriteMask = COLOR_MASK_ALL;
    solidGP.InputLayout.LayoutElements = solidLayoutElems;
    solidGP.InputLayout.NumElements = _countof(solidLayoutElems);
    solidInfo.PSODesc.ResourceLayout.Variables = solidVars;
    solidInfo.PSODesc.ResourceLayout.NumVariables = _countof(solidVars);
    device_->CreateGraphicsPipelineState(solidInfo, &solidRectPsoAndSrb_.pPSO);
    if (solidRectPsoAndSrb_.pPSO) {
        solidRectPsoAndSrb_.pPSO->CreateShaderResourceBinding(&solidRectPsoAndSrb_.pSRB, true);
    }

    GraphicsPipelineStateCreateInfo transformInfo = solidInfo;
    transformInfo.PSODesc.Name = "DrawSolidRectTransform PSO";
    transformInfo.pVS = solidRectTransformShaders_.VS;
    transformInfo.pPS = solidRectTransformShaders_.PS;
    transformInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    device_->CreateGraphicsPipelineState(transformInfo, &solidRectTransformPsoAndSrb_.pPSO);
    if (solidRectTransformPsoAndSrb_.pPSO) {
        solidRectTransformPsoAndSrb_.pPSO->CreateShaderResourceBinding(&solidRectTransformPsoAndSrb_.pSRB, true);
    }

    GraphicsPipelineStateCreateInfo triangleInfo = solidInfo;
    triangleInfo.PSODesc.Name = "DrawSolidTriangle PSO";
    triangleInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    device_->CreateGraphicsPipelineState(triangleInfo, &solidTrianglePsoAndSrb_.pPSO);
    if (solidTrianglePsoAndSrb_.pPSO) {
        solidTrianglePsoAndSrb_.pPSO->CreateShaderResourceBinding(&solidTrianglePsoAndSrb_.pSRB, true);
    }
}

void ShaderManager::Impl::createSpriteFamilyPSOs()
{
    if (!device_) {
        return;
    }

    static const LayoutElement spriteLayoutElems[] = {
        LayoutElement{0, 0, 2, VT_FLOAT32, false},
        LayoutElement{1, 0, 2, VT_FLOAT32, false},
        LayoutElement{2, 0, 4, VT_FLOAT32, false}
    };
    static const ShaderResourceVariableDesc spriteVars[] = {
        { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL, "g_texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL, "g_sampler", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };

    GraphicsPipelineStateCreateInfo spriteInfo;
    spriteInfo.PSODesc.Name = "DrawSprite PSO";
    spriteInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    spriteInfo.pVS = spriteShaders_.VS;
    spriteInfo.pPS = spriteShaders_.PS;
    auto& spriteGP = spriteInfo.GraphicsPipeline;
    spriteGP.NumRenderTargets = 1;
    spriteGP.RTVFormats[0] = rtvFormat_;
    spriteGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    spriteGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
    spriteGP.DepthStencilDesc.DepthEnable = False;
    auto& spriteBlend = spriteGP.BlendDesc.RenderTargets[0];
    spriteBlend.BlendEnable = True;
    spriteBlend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    spriteBlend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    spriteBlend.BlendOp = BLEND_OPERATION_ADD;
    spriteBlend.SrcBlendAlpha = BLEND_FACTOR_ONE;
    spriteBlend.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    spriteBlend.BlendOpAlpha = BLEND_OPERATION_ADD;
    spriteBlend.RenderTargetWriteMask = COLOR_MASK_ALL;
    spriteGP.InputLayout.LayoutElements = spriteLayoutElems;
    spriteGP.InputLayout.NumElements = _countof(spriteLayoutElems);
    spriteInfo.PSODesc.ResourceLayout.Variables = spriteVars;
    spriteInfo.PSODesc.ResourceLayout.NumVariables = _countof(spriteVars);
    const bool vrsEnabled = device_->GetDeviceInfo().Features.VariableRateShading != DEVICE_FEATURE_STATE_DISABLED;
    if (vrsEnabled && (device_->GetAdapterInfo().ShadingRate.CapFlags & SHADING_RATE_CAP_FLAG_PER_PRIMITIVE)) {
        spriteInfo.GraphicsPipeline.ShadingRateFlags = PIPELINE_SHADING_RATE_FLAG_PER_PRIMITIVE;
    } else {
        spriteInfo.GraphicsPipeline.ShadingRateFlags = PIPELINE_SHADING_RATE_FLAG_NONE;
        if (device_->GetAdapterInfo().ShadingRate.CapFlags & SHADING_RATE_CAP_FLAG_PER_PRIMITIVE) {
            qWarning() << "[ShaderManager] Per-primitive shading rate is supported by the adapter,"
                       << "but VariableRateShading is disabled for this device. PSOs will be created without ShadingRateFlags.";
        }
    }
    device_->CreateGraphicsPipelineState(spriteInfo, &spritePsoAndSrb_.pPSO);
    if (spritePsoAndSrb_.pPSO) {
        spritePsoAndSrb_.pPSO->CreateShaderResourceBinding(&spritePsoAndSrb_.pSRB, true);
    } else {
        qWarning() << "[ShaderManager] Failed to create PSO:" << "DrawSprite PSO";
    }

    GraphicsPipelineStateCreateInfo spriteTransformInfo = spriteInfo;
    spriteTransformInfo.PSODesc.Name = "DrawSpriteTransform PSO";
    spriteTransformInfo.pVS = spriteTransformShaders_.VS;
    spriteTransformInfo.pPS = spriteTransformShaders_.PS;
    spriteTransformInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    device_->CreateGraphicsPipelineState(spriteTransformInfo, &spriteTransformPsoAndSrb_.pPSO);
    if (spriteTransformPsoAndSrb_.pPSO) {
        spriteTransformPsoAndSrb_.pPSO->CreateShaderResourceBinding(&spriteTransformPsoAndSrb_.pSRB, true);
    } else {
        qWarning() << "[ShaderManager] Failed to create PSO:" << "DrawSpriteTransform PSO";
    }

    static const ShaderResourceVariableDesc maskedSpriteVars[] = {
        { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL, "g_scene", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL, "g_mask", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL, "g_sampler", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };

    GraphicsPipelineStateCreateInfo maskedSpriteInfo = spriteInfo;
    maskedSpriteInfo.PSODesc.Name = "DrawMaskedSprite PSO";
    maskedSpriteInfo.pVS = maskedSpriteShaders_.VS;
    maskedSpriteInfo.pPS = maskedSpriteShaders_.PS;
    maskedSpriteInfo.PSODesc.ResourceLayout.Variables = maskedSpriteVars;
    maskedSpriteInfo.PSODesc.ResourceLayout.NumVariables = _countof(maskedSpriteVars);
    device_->CreateGraphicsPipelineState(maskedSpriteInfo, &maskedSpritePsoAndSrb_.pPSO);
    if (maskedSpritePsoAndSrb_.pPSO) {
        maskedSpritePsoAndSrb_.pPSO->CreateShaderResourceBinding(&maskedSpritePsoAndSrb_.pSRB, true);
    } else {
        qWarning() << "[ShaderManager] Failed to create PSO:" << "DrawMaskedSprite PSO";
    }
}

void ShaderManager::Impl::createUtilityFamilyPSOs()
{
    if (!device_) {
        return;
    }

    static const LayoutElement thickLineLayoutElems[] = {
        LayoutElement{0, 0, 2, VT_FLOAT32, false},
        LayoutElement{1, 0, 4, VT_FLOAT32, false}
    };
    static const ShaderResourceVariableDesc thickLineVars[] = {
        { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
    };

    GraphicsPipelineStateCreateInfo thickLineInfo;
    thickLineInfo.PSODesc.Name = "DrawThickLine PSO";
    thickLineInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    thickLineInfo.pVS = thickLineShaders_.VS;
    thickLineInfo.pPS = thickLineShaders_.PS;
    auto& thickGP = thickLineInfo.GraphicsPipeline;
    thickGP.NumRenderTargets = 1;
    thickGP.RTVFormats[0] = rtvFormat_;
    thickGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    thickGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
    thickGP.DepthStencilDesc.DepthEnable = False;
    thickGP.InputLayout.LayoutElements = thickLineLayoutElems;
    thickGP.InputLayout.NumElements = _countof(thickLineLayoutElems);
    thickLineInfo.PSODesc.ResourceLayout.Variables = thickLineVars;
    thickLineInfo.PSODesc.ResourceLayout.NumVariables = _countof(thickLineVars);
    device_->CreateGraphicsPipelineState(thickLineInfo, &thickLinePsoAndSrb_.pPSO);
    if (thickLinePsoAndSrb_.pPSO) {
        thickLinePsoAndSrb_.pPSO->CreateShaderResourceBinding(&thickLinePsoAndSrb_.pSRB, true);
    }

    static const LayoutElement dotLineLayoutElems[] = {
        LayoutElement{0, 0, 2, VT_FLOAT32, false},
        LayoutElement{1, 0, 4, VT_FLOAT32, false},
        LayoutElement{2, 0, 1, VT_FLOAT32, false}
    };
    static const ShaderResourceVariableDesc dotLineVars[] = {
        { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL,  "DotLineCB",   SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
    };

    GraphicsPipelineStateCreateInfo dotLineInfo;
    dotLineInfo.PSODesc.Name = "DrawDotLine PSO";
    dotLineInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    dotLineInfo.pVS = dotLineShaders_.VS;
    dotLineInfo.pPS = dotLineShaders_.PS;
    auto& dotGP = dotLineInfo.GraphicsPipeline;
    dotGP.NumRenderTargets = 1;
    dotGP.RTVFormats[0] = rtvFormat_;
    dotGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    dotGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
    dotGP.DepthStencilDesc.DepthEnable = False;
    dotGP.InputLayout.LayoutElements = dotLineLayoutElems;
    dotGP.InputLayout.NumElements = _countof(dotLineLayoutElems);
    dotLineInfo.PSODesc.ResourceLayout.Variables = dotLineVars;
    dotLineInfo.PSODesc.ResourceLayout.NumVariables = _countof(dotLineVars);
    device_->CreateGraphicsPipelineState(dotLineInfo, &dotLinePsoAndSrb_.pPSO);
    if (dotLinePsoAndSrb_.pPSO) {
        dotLinePsoAndSrb_.pPSO->CreateShaderResourceBinding(&dotLinePsoAndSrb_.pSRB, true);
    }

    static const ShaderResourceVariableDesc checkerVars[] = {
        { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL,  "ViewerHelperCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
    };
    static const LayoutElement checkerLayoutElems[] = {
        LayoutElement{0, 0, 2, VT_FLOAT32, false},
        LayoutElement{1, 0, 4, VT_FLOAT32, false}
    };

    GraphicsPipelineStateCreateInfo checkerInfo;
    checkerInfo.PSODesc.Name = "Checkerboard PSO";
    checkerInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    checkerInfo.pVS = solidShaders_.VS;
    checkerInfo.pPS = checkerboardShaders_.PS;
    auto& checkerGP = checkerInfo.GraphicsPipeline;
    checkerGP.NumRenderTargets = 1;
    checkerGP.RTVFormats[0] = rtvFormat_;
    checkerGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    checkerGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
    checkerGP.DepthStencilDesc.DepthEnable = False;
    checkerGP.InputLayout.LayoutElements = checkerLayoutElems;
    checkerGP.InputLayout.NumElements = _countof(checkerLayoutElems);
    checkerInfo.PSODesc.ResourceLayout.Variables = checkerVars;
    checkerInfo.PSODesc.ResourceLayout.NumVariables = _countof(checkerVars);
    device_->CreateGraphicsPipelineState(checkerInfo, &checkerboardPsoAndSrb_.pPSO);
    if (checkerboardPsoAndSrb_.pPSO) {
        checkerboardPsoAndSrb_.pPSO->CreateShaderResourceBinding(&checkerboardPsoAndSrb_.pSRB, true);
    }

    GraphicsPipelineStateCreateInfo gridInfo = checkerInfo;
    gridInfo.PSODesc.Name = "Grid PSO";
    gridInfo.pPS = gridShaders_.PS;
    device_->CreateGraphicsPipelineState(gridInfo, &gridPsoAndSrb_.pPSO);
    if (gridPsoAndSrb_.pPSO) {
        gridPsoAndSrb_.pPSO->CreateShaderResourceBinding(&gridPsoAndSrb_.pSRB, true);
    }

    static const LayoutElement gizmoLayoutElems[] = {
        LayoutElement{0, 0, 3, VT_FLOAT32, false},
        LayoutElement{1, 0, 4, VT_FLOAT32, false}
    };
    static const ShaderResourceVariableDesc gizmoVars[] = {
        { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
    };

    GraphicsPipelineStateCreateInfo gizmoInfo;
    gizmoInfo.PSODesc.Name = "Gizmo3D PSO";
    gizmoInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    gizmoInfo.pVS = gizmo3DShaders_.VS;
    gizmoInfo.pPS = gizmo3DShaders_.PS;
    auto& gizmoGP = gizmoInfo.GraphicsPipeline;
    gizmoGP.NumRenderTargets = 1;
    gizmoGP.RTVFormats[0] = rtvFormat_;
    gizmoGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_LINE_LIST;
    gizmoGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
    gizmoGP.DepthStencilDesc.DepthEnable = True;
    gizmoGP.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_ALWAYS;
    gizmoGP.BlendDesc.RenderTargets[0].BlendEnable = True;
    gizmoGP.BlendDesc.RenderTargets[0].SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    gizmoGP.BlendDesc.RenderTargets[0].DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    gizmoGP.InputLayout.LayoutElements = gizmoLayoutElems;
    gizmoGP.InputLayout.NumElements = _countof(gizmoLayoutElems);
    gizmoInfo.PSODesc.ResourceLayout.Variables = gizmoVars;
    gizmoInfo.PSODesc.ResourceLayout.NumVariables = _countof(gizmoVars);
    device_->CreateGraphicsPipelineState(gizmoInfo, &gizmo3DPsoAndSrb_.pPSO);
    if (gizmo3DPsoAndSrb_.pPSO) {
        gizmo3DPsoAndSrb_.pPSO->CreateShaderResourceBinding(&gizmo3DPsoAndSrb_.pSRB, true);
    }
}

void ShaderManager::Impl::createPSOs()
{
    if (!device_) {
        return;
    }
    tbb::task_group tasks;
    tasks.run([this]() { createLineFamilyPSOs(); });
    tasks.run([this]() { createSpriteFamilyPSOs(); });
    tasks.run([this]() { createUtilityFamilyPSOs(); });
    tasks.wait();

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
