module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <utility>

#include <QDebug>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/TextureView.h>

#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"

module Artifact.Render.Pipeline;

import std;
import Layer.Blend;
import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Graphics.LayerBlendPipeline;
import Graphics.GPUcomputeContext;
import Graphics.Compute;

import Artifact.Render.Config;
import Memory.SharedPtr;

namespace Artifact
{
 using namespace Diligent;
 using ArtifactCore::BlendMode;
 using ArtifactCore::GpuContext;
 using ArtifactCore::LayerBlendPipeline;

 namespace
 {
  inline constexpr const char* kScreenSpaceGlobalIlluminationShader = R"(
cbuffer SSGIParams : register(b0)
{
    uint g_InputWidth;
    uint g_InputHeight;
    uint g_OutputWidth;
    uint g_OutputHeight;
    uint g_RaySteps;
    float g_Intensity;
    float g_DepthThickness;
    float g_RadiusPixels;
};

Texture2D<float> g_Depth : register(t0);
Texture2D<float4> g_Normal : register(t1);
Texture2D<float4> g_Albedo : register(t2);
RWTexture2D<float4> g_Output : register(u0);

static const float2 kDirections[8] = {
    float2(1.0, 0.0), float2(0.7071, 0.7071),
    float2(0.0, 1.0), float2(-0.7071, 0.7071),
    float2(-1.0, 0.0), float2(-0.7071, -0.7071),
    float2(0.0, -1.0), float2(0.7071, -0.7071)
};

[numthreads(8, 8, 1)]
void ScreenSpaceGICS(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_OutputWidth || dispatchId.y >= g_OutputHeight) return;

    const float2 outputUV = (float2(dispatchId.xy) + 0.5) /
                            float2(g_OutputWidth, g_OutputHeight);
    const int2 centerPixel = clamp(
        int2(outputUV * float2(g_InputWidth, g_InputHeight)),
        int2(0, 0), int2(g_InputWidth - 1, g_InputHeight - 1));
    const float centerDepth = g_Depth.Load(int3(centerPixel, 0));
    if (centerDepth >= 0.999999) {
        g_Output[dispatchId.xy] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    const float3 centerNormal = normalize(
        g_Normal.Load(int3(centerPixel, 0)).xyz * 2.0 - 1.0);
    float occlusion = 0.0;
    float totalWeight = 0.0;
    const uint stepCount = clamp(g_RaySteps, 1u, 24u);

    [unroll]
    for (uint directionIndex = 0; directionIndex < 8; ++directionIndex) {
        const float2 direction = kDirections[directionIndex];
        [loop]
        for (uint stepIndex = 1; stepIndex <= 24; ++stepIndex) {
            if (stepIndex > stepCount) break;
            const float distanceWeight = 1.0 -
                (float(stepIndex - 1) / max(1.0, float(stepCount)));
            const float radius = g_RadiusPixels *
                (float(stepIndex) / float(stepCount));
            const int2 samplePixel = clamp(
                centerPixel + int2(round(direction * radius)),
                int2(0, 0), int2(g_InputWidth - 1, g_InputHeight - 1));
            const float sampleDepth = g_Depth.Load(int3(samplePixel, 0));
            if (sampleDepth >= 0.999999) continue;

            // D3D depth increases away from the camera. Only a sample that
            // is meaningfully closer than the shaded point can occlude it.
            const float occluder = saturate(
                (centerDepth - sampleDepth - g_DepthThickness) /
                max(g_DepthThickness * 4.0, 0.000001));
            if (occluder <= 0.0) continue;

            const float3 sampleNormal = normalize(
                g_Normal.Load(int3(samplePixel, 0)).xyz * 2.0 - 1.0);
            const float normalWeight = saturate(dot(centerNormal, sampleNormal));
            const float weight = normalWeight * distanceWeight;
            occlusion += occluder * weight;
            totalWeight += weight;
        }
    }

    const float ao = 1.0 - saturate(
        (totalWeight > 0.0 ? occlusion / totalWeight : 0.0) * g_Intensity);
    g_Output[dispatchId.xy] = float4(ao, ao, ao, 1.0);
}
)";

  inline constexpr const char* kScreenSpaceAOCompositeShader = R"(
Texture2D<float4> g_SourceColor : register(t0);
Texture2D<float4> g_AOMask : register(t1);
RWTexture2D<float4> g_DestinationColor : register(u0);

[numthreads(8, 8, 1)]
void ScreenSpaceAOCompositeCS(uint3 dispatchId : SV_DispatchThreadID)
{
    uint width, height;
    g_DestinationColor.GetDimensions(width, height);
    if (dispatchId.x >= width || dispatchId.y >= height) return;
    uint aoWidth, aoHeight;
    g_AOMask.GetDimensions(aoWidth, aoHeight);
    const uint2 aoPixel = min(dispatchId.xy * uint2(aoWidth, aoHeight) /
                              uint2(width, height),
                              uint2(aoWidth - 1, aoHeight - 1));
    const float4 source = g_SourceColor.Load(int3(dispatchId.xy, 0));
    const float ao = saturate(g_AOMask.Load(int3(aoPixel, 0)).r);
    g_DestinationColor[dispatchId.xy] = float4(source.rgb * ao, source.a);
}
)";

  inline constexpr const char* kFastApproximateAntiAliasingShader = R"(
Texture2D<float4> g_SourceColor : register(t0);
RWTexture2D<float4> g_DestinationColor : register(u0);

float luma(float3 rgb) { return dot(rgb, float3(0.299, 0.587, 0.114)); }

[numthreads(8, 8, 1)]
void FastApproximateAntiAliasingCS(uint3 dispatchId : SV_DispatchThreadID)
{
    uint width, height;
    g_DestinationColor.GetDimensions(width, height);
    if (dispatchId.x >= width || dispatchId.y >= height) return;

    const int2 p = int2(dispatchId.xy);
    const int2 maxP = int2(width - 1, height - 1);
    const float4 center = g_SourceColor.Load(int3(p, 0));
    const float3 north = g_SourceColor.Load(int3(clamp(p + int2(0, -1), int2(0, 0), maxP), 0)).rgb;
    const float3 south = g_SourceColor.Load(int3(clamp(p + int2(0,  1), int2(0, 0), maxP), 0)).rgb;
    const float3 west  = g_SourceColor.Load(int3(clamp(p + int2(-1, 0), int2(0, 0), maxP), 0)).rgb;
    const float3 east  = g_SourceColor.Load(int3(clamp(p + int2( 1, 0), int2(0, 0), maxP), 0)).rgb;
    const float centerLuma = luma(center.rgb);
    const float minLuma = min(centerLuma, min(min(luma(north), luma(south)), min(luma(west), luma(east))));
    const float maxLuma = max(centerLuma, max(max(luma(north), luma(south)), max(luma(west), luma(east))));
    const float contrast = maxLuma - minLuma;
    // Preserve flat color and text/UI-like areas; only soften true high-contrast
    // raster edges. The cross filter is intentionally bounded to avoid blur.
    const float edge = saturate((contrast - 0.0312) / max(maxLuma * 0.125, 0.001));
    const float3 filtered = (north + south + west + east + center.rgb * 2.0) / 6.0;
    g_DestinationColor[dispatchId.xy] = float4(lerp(center.rgb, filtered, edge * 0.55), center.a);
}
)";

  inline constexpr const char* kGaussianBlurShader = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer GaussianBlurParams : register(b0)
{
    float g_Sigma;
    uint g_Horizontal;
    float2 g_Pad;
};

float gaussianWeight(float x, float sigma)
{
    return exp(-0.5 * x * x / max(0.0001, sigma * sigma));
}

[numthreads(8, 8, 1)]
void GaussianBlurCS(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;
    const float sigma = max(0.1, g_Sigma);
    const int radius = min(64, (int)ceil(sigma * 3.0));
    float4 sum = 0.0;
    float weightSum = 0.0;
    [loop] for (int i = -radius; i <= radius; ++i) {
        int2 p = int2(id.xy);
        if (g_Horizontal != 0) p.x = clamp(p.x + i, 0, int(width) - 1);
        else p.y = clamp(p.y + i, 0, int(height) - 1);
        const float weight = gaussianWeight((float)i, sigma);
        const float4 sample = g_InputTexture.Load(int3(p, 0));
        sum.rgb += sample.rgb * sample.a * weight;
        sum.a += sample.a * weight;
        weightSum += weight;
    }
    sum /= max(weightSum, 0.0001);
    if (sum.a > 0.00001) sum.rgb /= sum.a;
    g_OutputTexture[id.xy] = sum;
}
)";

  inline constexpr const char* kVignetteShader = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer VignetteParams : register(b0)
{
    float g_Amount;
    float g_Radius;
    float g_Feather;
    float g_CenterX;
    float g_CenterY;
    float3 g_Pad;
};

[numthreads(8, 8, 1)]
void VignetteCS(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;
    const float cx = g_CenterX * width;
    const float cy = g_CenterY * height;
    const float maxX = max(cx, width - cx);
    const float maxY = max(cy, height - cy);
    const float maxDistance = sqrt(maxX * maxX + maxY * maxY) *
                              max(g_Radius, 0.0001);
    const float distance = length(float2(id.x - cx, id.y - cy));
    const float feather = clamp(g_Feather, 0.01, 2.0);
    const float edge = saturate((distance - maxDistance * feather) /
                                (maxDistance * (1.0 - feather) + 0.001));
    const float factor = 1.0 - edge * saturate(g_Amount);
    const float4 color = g_InputTexture.Load(int3(id.xy, 0));
    g_OutputTexture[id.xy] = float4(color.rgb * factor, color.a);
}
)";

  inline constexpr const char* kSharpenShader = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer SharpenParams : register(b0)
{
    float g_Amount;
    float g_Sigma;
    float g_Threshold;
    float g_Pad;
};

float4 sampleClamped(int2 p, uint width, uint height)
{
    return g_InputTexture.Load(int3(clamp(p, int2(0, 0),
        int2(width - 1, height - 1)), 0));
}

[numthreads(8, 8, 1)]
void SharpenCS(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;
    const float sigma = max(g_Sigma, 0.1);
    const int radius = max(1, (int)ceil(sigma * 3.0));
    float4 blurred = float4(0.0, 0.0, 0.0, 0.0);
    float weightSum = 0.0;
    [loop] for (int y = -radius; y <= radius; ++y) {
        [loop] for (int x = -radius; x <= radius; ++x) {
            const float distanceSquared = float(x * x + y * y);
            const float weight = exp(-0.5 * distanceSquared / (sigma * sigma));
            blurred += sampleClamped(int2(id.xy) + int2(x, y), width, height) * weight;
            weightSum += weight;
        }
    }
    const float4 source = g_InputTexture.Load(int3(id.xy, 0));
    blurred /= max(weightSum, 0.0001);
    float4 result = source + (source - blurred) * g_Amount;
    if (g_Threshold > 0.0) {
        const float3 difference = abs(source.rgb - blurred.rgb) * g_Amount;
        const float enabled = step(g_Threshold,
            max(difference.r, max(difference.g, difference.b)));
        result.rgb = lerp(source.rgb, result.rgb, enabled);
    }
    g_OutputTexture[id.xy] = float4(max(result.rgb, 0.0), source.a);
}
)";

  inline constexpr const char* kStripesShader = R"(
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer StripesParams : register(b0)
{
    float g_Frequency;
    float g_Angle;
    float g_Thickness;
    float g_Offset;
};

[numthreads(8, 8, 1)]
void StripesCS(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;
    const float radians = g_Angle * 3.14159265359 / 180.0;
    const float projection = (float(id.x) * cos(radians) + float(id.y) * sin(radians)) *
        max(g_Frequency, 0.5) / max(float(width), float(height)) + g_Offset;
    const float stripe = abs(projection - trunc(projection)) < saturate(g_Thickness) ? 1.0 : 0.0;
    g_OutputTexture[id.xy] = float4(stripe, stripe, stripe, 1.0);
}
)";

  inline constexpr const char* kHexGridShader = R"(
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer HexGridParams : register(b0)
{
    float g_CellSize;
    float g_LineWidth;
    float g_Angle;
    float g_Pad;
};

[numthreads(8, 8, 1)]
void HexGridCS(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;
    const float cell = max(g_CellSize, 4.0);
    const float line = max(g_LineWidth, 0.5);
    const float radians = g_Angle * 0.0174532925;
    const float rotatedX = float(id.x) * cos(radians) - float(id.y) * sin(radians);
    const float rotatedY = float(id.x) * sin(radians) + float(id.y) * cos(radians);
    const float hexHeight = cell * 0.8660254;
    const float q = rotatedX / cell;
    const float r = rotatedY / hexHeight;
    const float qFraction = q - floor(q);
    const float rFraction = r - floor(r);
    const int row = int(floor(r));
    const float2 delta = (row & 1) == 0
        ? float2((qFraction - 0.5) * cell, (rFraction - 0.5) * hexHeight)
        : float2(qFraction * cell, (rFraction - 0.5) * hexHeight);
    const float distance = max(abs(delta.x) / cell, abs(delta.y) / hexHeight) * 2.0;
    const float value = distance > 1.0 - line / cell ? 0.0 : 1.0;
    g_OutputTexture[id.xy] = float4(value, value, value, 1.0);
}
)";

  inline constexpr const char* kChromaticAberrationShader = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer ChromaticAberrationParams : register(b0)
{
    float g_RedShift;
    float g_BlueShift;
    float g_CenterX;
    float g_CenterY;
};

float4 sampleClamped(int2 p, uint width, uint height)
{
    return g_InputTexture.Load(int3(clamp(p, int2(0, 0),
        int2(width - 1, height - 1)), 0));
}

[numthreads(8, 8, 1)]
void ChromaticAberrationCS(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;
    const float2 position = float2(id.xy);
    const float2 center = float2(g_CenterX * width, g_CenterY * height);
    const float2 delta = position - center;
    const float distance = length(delta);
    const float2 direction = distance > 0.0 ? delta / distance : float2(0.0, 0.0);
    const float normalizedDistance = distance / max(max(width, height) * 0.7, 1.0);
    const float4 red = sampleClamped(
        int2(position + direction * normalizedDistance * g_RedShift + 0.5),
        width, height);
    const float4 blue = sampleClamped(
        int2(position - direction * normalizedDistance * g_BlueShift + 0.5),
        width, height);
    float4 output = g_InputTexture.Load(int3(id.xy, 0));
    output.r = red.r;
    output.b = blue.b;
    g_OutputTexture[id.xy] = output;
}
)";

  struct GaussianBlurParams
  {
   float sigma = 0.0f;
   Uint32 horizontal = 0;
   float pad[2] = {};
  };

  struct VignetteParams
  {
   float amount = 0.0f;
   float radius = 0.0f;
   float feather = 0.0f;
   float centerX = 0.5f;
   float centerY = 0.5f;
   float pad[3] = {};
  };

  struct SharpenParams
  {
   float amount = 1.0f;
   float sigma = 1.0f;
   float threshold = 0.0f;
   float pad = 0.0f;
  };

  struct StripesParams
  {
   float frequency = 10.0f;
   float angle = 0.0f;
   float thickness = 0.5f;
   float offset = 0.0f;
  };

  struct HexGridParams
  {
   float cellSize = 32.0f;
   float lineWidth = 2.0f;
   float angle = 0.0f;
   float pad = 0.0f;
  };

  struct ChromaticAberrationParams
  {
   float redShift = 0.0f;
   float blueShift = 0.0f;
   float centerX = 0.5f;
   float centerY = 0.5f;
  };

  inline constexpr const char* kScreenSpaceGlobalIlluminationResolveShader = R"(
cbuffer SSGIResolveParams : register(b0)
{
    uint g_InputWidth;
    uint g_InputHeight;
    uint g_OutputWidth;
    uint g_OutputHeight;
    uint g_HistoryValid;
    uint g_TemporalEnabled;
    uint g_DenoiseEnabled;
    float g_HistoryWeight;
    float g_DepthSigma;
    float g_NormalSigma;
    float g_VelocityScale;
    float _padding0;
};

Texture2D<float4> g_RawGI : register(t0);
Texture2D<float4> g_HistoryGI : register(t1);
Texture2D<float> g_ResolveDepth : register(t2);
Texture2D<float4> g_ResolveNormal : register(t3);
Texture2D<float4> g_ResolveVelocity : register(t4);
RWTexture2D<float4> g_ResolvedGI : register(u0);

[numthreads(8, 8, 1)]
void ScreenSpaceGIResolveCS(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_OutputWidth || dispatchId.y >= g_OutputHeight) return;

    const float2 outputSize = float2(g_OutputWidth, g_OutputHeight);
    const float2 inputSize = float2(g_InputWidth, g_InputHeight);
    const float2 uv = (float2(dispatchId.xy) + 0.5) / outputSize;
    const int2 fullPixel = clamp(int2(uv * inputSize), int2(0, 0),
                                 int2(g_InputWidth - 1, g_InputHeight - 1));
    const float centerDepth = g_ResolveDepth.Load(int3(fullPixel, 0));
    const float3 centerNormal = normalize(
        g_ResolveNormal.Load(int3(fullPixel, 0)).xyz * 2.0 - 1.0);

    float3 filtered = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;
    const int filterRadius = g_DenoiseEnabled != 0 ? 1 : 0;
    [loop]
    for (int y = -1; y <= 1; ++y) {
        [loop]
        for (int x = -1; x <= 1; ++x) {
            if (abs(x) > filterRadius || abs(y) > filterRadius) continue;
            const int2 sampleOutput = clamp(
                int2(dispatchId.xy) + int2(x, y), int2(0, 0),
                int2(g_OutputWidth - 1, g_OutputHeight - 1));
            const float2 sampleUV =
                (float2(sampleOutput) + 0.5) / outputSize;
            const int2 sampleFull = clamp(int2(sampleUV * inputSize),
                int2(0, 0), int2(g_InputWidth - 1, g_InputHeight - 1));
            const float sampleDepth =
                g_ResolveDepth.Load(int3(sampleFull, 0));
            const float3 sampleNormal = normalize(
                g_ResolveNormal.Load(int3(sampleFull, 0)).xyz * 2.0 - 1.0);
            const float depthWeight = exp(-abs(sampleDepth - centerDepth) /
                                          max(g_DepthSigma, 0.000001));
            const float normalWeight = pow(
                saturate(dot(centerNormal, sampleNormal)), g_NormalSigma);
            const float spatialWeight = (x == 0 && y == 0) ? 1.0 : 0.75;
            const float weight = depthWeight * normalWeight * spatialWeight;
            filtered += g_RawGI.Load(int3(sampleOutput, 0)).rgb * weight;
            totalWeight += weight;
        }
    }
    filtered = totalWeight > 0.0
        ? filtered / totalWeight
        : g_RawGI.Load(int3(dispatchId.xy, 0)).rgb;

    if (g_TemporalEnabled != 0 && g_HistoryValid != 0) {
        const float2 velocity =
            (g_ResolveVelocity.Load(int3(fullPixel, 0)).xy * 2.0 - 1.0) *
            g_VelocityScale;
        const int2 historyPixel = clamp(
            int2(round(float2(dispatchId.xy) - velocity * outputSize)),
            int2(0, 0), int2(g_OutputWidth - 1, g_OutputHeight - 1));
        const float3 history = g_HistoryGI.Load(int3(historyPixel, 0)).rgb;
        filtered = lerp(filtered, history, saturate(g_HistoryWeight));
    }

    g_ResolvedGI[dispatchId.xy] = float4(filtered, 1.0);
}
)";

  struct alignas(16) ScreenSpaceGlobalIlluminationParams
  {
   Uint32 inputWidth = 0;
   Uint32 inputHeight = 0;
   Uint32 outputWidth = 0;
   Uint32 outputHeight = 0;
   Uint32 raySteps = 8;
   float intensity = 1.0f;
   float depthThickness = 0.01f;
   float radiusPixels = 24.0f;
  };

  struct alignas(16) ScreenSpaceGlobalIlluminationResolveParams
  {
   Uint32 inputWidth = 0;
   Uint32 inputHeight = 0;
   Uint32 outputWidth = 0;
   Uint32 outputHeight = 0;
   Uint32 historyValid = 0;
   Uint32 temporalEnabled = 1;
   Uint32 denoiseEnabled = 1;
   float historyWeight = 0.9f;
   float depthSigma = 0.01f;
   float normalSigma = 16.0f;
   float velocityScale = 1.0f;
   float padding0 = 0.0f;
  };

  struct TextureBundle
  {
   RefCntAutoPtr<ITexture> texture;
   RefCntAutoPtr<ITextureView> srv;
   RefCntAutoPtr<ITextureView> uav;
   RefCntAutoPtr<ITextureView> rtv;
  };

  bool createTextureBundle(IRenderDevice* device,
                           Uint32 width,
                           Uint32 height,
                           TEXTURE_FORMAT format,
                           BIND_FLAGS bindFlags,
                           const char* name,
                           TextureBundle& bundle)
  {
   if (!device || width == 0 || height == 0)
   {
    return false;
   }

   TextureDesc desc;
   desc.Name = name;
   desc.Type = RESOURCE_DIM_TEX_2D;
   desc.Width = width;
   desc.Height = height;
   desc.Format = format;
   desc.MipLevels = 1;
   desc.ArraySize = 1;
   desc.SampleCount = 1;
   desc.Usage = USAGE_DEFAULT;
   desc.BindFlags = bindFlags;

   bundle = {};
   device->CreateTexture(desc, nullptr, &bundle.texture);
   if (!bundle.texture)
   {
    qWarning() << "[RenderPipeline] CreateTexture failed for" << name
               << "size=" << width << "x" << height << "format=" << int(format);
    return false;
   }

   const bool needsSrv = (bindFlags & BIND_SHADER_RESOURCE) != 0;
   const bool needsUav = (bindFlags & BIND_UNORDERED_ACCESS) != 0;
   const bool needsRtv = (bindFlags & BIND_RENDER_TARGET) != 0;

   bundle.srv = needsSrv
                    ? bundle.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
                    : nullptr;
   bundle.uav = needsUav
                    ? bundle.texture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS)
                    : nullptr;
   bundle.rtv = needsRtv
                    ? bundle.texture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)
                    : nullptr;

   if ((needsSrv && !bundle.srv) || (needsUav && !bundle.uav) ||
       (needsRtv && !bundle.rtv))
   {
    qWarning() << "[RenderPipeline] Missing default views for" << name;
    bundle = {};
    return false;
   }

   return true;
  }
 } // namespace

 class RenderPipeline::Impl
 {
 public:
  RefCntAutoPtr<IRenderDevice> device_;
  TextureBundle accum_;
  TextureBundle temp_;
 TextureBundle layer_;
 TextureBundle layerFloat_;
  std::array<TextureBundle, 3> matteSources_;
  TextureBundle emission_;
  TextureBundle normal_;
  TextureBundle velocity_;
  TextureBundle objectId_;
  TextureBundle materialId_;
  TextureBundle albedo_;
  TextureBundle screenSpaceGI_;
  ArtifactCore::SharedPtr<GpuContext> screenSpaceGIContext_;
  ArtifactCore::SharedPtr<GpuContext> blendContext_;
  std::unique_ptr<LayerBlendPipeline> blendPipeline_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> screenSpaceGIExecutor_;
  RefCntAutoPtr<IBuffer> screenSpaceGIParams_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> screenSpaceGIResolveExecutor_;
  RefCntAutoPtr<IBuffer> screenSpaceGIResolveParams_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> screenSpaceAOCompositeExecutor_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> fastApproximateAntiAliasingExecutor_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> gaussianBlurExecutor_;
  RefCntAutoPtr<IBuffer> gaussianBlurParams_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> sharpenExecutor_;
  RefCntAutoPtr<IBuffer> sharpenParams_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> stripesExecutor_;
  RefCntAutoPtr<IBuffer> stripesParams_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> hexGridExecutor_;
  RefCntAutoPtr<IBuffer> hexGridParams_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> vignetteExecutor_;
  RefCntAutoPtr<IBuffer> vignetteParams_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> chromaticAberrationExecutor_;
  RefCntAutoPtr<IBuffer> chromaticAberrationParams_;
  TextureBundle screenSpaceGIHistory_[2];
  Uint32 screenSpaceGIHistoryWriteIndex_ = 0;
  bool screenSpaceGIHistoryValid_ = false;
  Uint32 width_ = 0;
  Uint32 height_ = 0;
  TEXTURE_FORMAT format_ = TEX_FORMAT_UNKNOWN;
  bool emissionEnabled_ = false;
 };

 RenderPipeline::RenderPipeline()
     : impl_(new Impl())
 {
 }

 RenderPipeline::~RenderPipeline()
 {
  destroy();
  delete impl_;
  impl_ = nullptr;
 }

bool RenderPipeline::initialize(IRenderDevice* device,
                                Uint32 width,
                                Uint32 height,
                                TEXTURE_FORMAT format,
                                bool enableEmission)
 {
  if (!device || width == 0 || height == 0)
  {
   destroy();
   return false;
  }

  const TEXTURE_FORMAT resolvedFormat = format != TEX_FORMAT_UNKNOWN
                                             ? format
                                             : RenderConfig::LinearColorFormat;

  const bool sameSize = impl_->device_ == device &&
                        impl_->width_ == width &&
                        impl_->height_ == height &&
                        impl_->format_ == resolvedFormat &&
                        impl_->emissionEnabled_ == enableEmission &&
                        ready();
  if (sameSize)
  {
   return true;
  }

  destroy();
  impl_->device_ = device;
  impl_->width_ = width;
  impl_->height_ = height;
  impl_->format_ = resolvedFormat;
  impl_->emissionEnabled_ = enableEmission;

  if (!createTextures(device, width, height, resolvedFormat, enableEmission))
  {
   destroy();
   return false;
  }

  return true;
 }

 void RenderPipeline::resize(Uint32 width, Uint32 height)
 {
  if (!impl_->device_ || width == 0 || height == 0)
  {
   destroy();
   return;
  }

  if (impl_->width_ == width && impl_->height_ == height && ready())
  {
   return;
  }

  initialize(impl_->device_, width, height, impl_->format_,
             impl_->emissionEnabled_);
 }

 void RenderPipeline::destroy()
 {
  impl_->accum_ = {};
  impl_->temp_ = {};
  impl_->layer_ = {};
  impl_->layerFloat_ = {};
  for (auto& matteSource : impl_->matteSources_) {
   matteSource = {};
  }
  impl_->emission_ = {};
  impl_->normal_ = {};
  impl_->velocity_ = {};
  impl_->objectId_ = {};
  impl_->materialId_ = {};
  impl_->albedo_ = {};
  impl_->screenSpaceGI_ = {};
  impl_->blendPipeline_.reset();
  impl_->blendContext_.reset();
  impl_->screenSpaceGIExecutor_.reset();
  impl_->screenSpaceGIContext_.reset();
  impl_->screenSpaceGIParams_.Release();
  impl_->screenSpaceGIResolveExecutor_.reset();
  impl_->screenSpaceGIResolveParams_.Release();
  impl_->screenSpaceAOCompositeExecutor_.reset();
  impl_->fastApproximateAntiAliasingExecutor_.reset();
  impl_->gaussianBlurExecutor_.reset();
  impl_->gaussianBlurParams_.Release();
  impl_->sharpenExecutor_.reset();
  impl_->sharpenParams_.Release();
  impl_->stripesExecutor_.reset();
  impl_->stripesParams_.Release();
  impl_->hexGridExecutor_.reset();
  impl_->hexGridParams_.Release();
  impl_->vignetteExecutor_.reset();
  impl_->vignetteParams_.Release();
  impl_->chromaticAberrationExecutor_.reset();
  impl_->chromaticAberrationParams_.Release();
  impl_->screenSpaceGIHistory_[0] = {};
  impl_->screenSpaceGIHistory_[1] = {};
  impl_->screenSpaceGIHistoryWriteIndex_ = 0;
  impl_->screenSpaceGIHistoryValid_ = false;
  impl_->width_ = 0;
  impl_->height_ = 0;
  impl_->format_ = TEX_FORMAT_UNKNOWN;
  impl_->emissionEnabled_ = false;
  impl_->device_ = nullptr;
 }

 bool RenderPipeline::applySpatialEffect(
    IDeviceContext* ctx, ITextureView* inputSRV,
    ITextureView* scratchUAV, ITextureView* outputUAV,
    const GpuSpatialEffectNode& node)
 {
  if (!ctx || !impl_->device_ || !inputSRV || !scratchUAV || !outputUAV ||
      scratchUAV == outputUAV ||
      impl_->width_ == 0 || impl_->height_ == 0) {
   return false;
  }
  if (node.kind == GpuSpatialEffectKind::HexGrid) {
   if (!impl_->hexGridExecutor_) {
    impl_->screenSpaceGIContext_ = impl_->screenSpaceGIContext_
        ? impl_->screenSpaceGIContext_
        : ArtifactCore::makeShared<GpuContext>(impl_->device_, ctx);
    impl_->hexGridExecutor_ =
        std::make_unique<ArtifactCore::ComputeExecutor>(*impl_->screenSpaceGIContext_);
    BufferDesc bufferDesc;
    bufferDesc.Name = "Composition Hex Grid Params";
    bufferDesc.Usage = USAGE_DYNAMIC;
    bufferDesc.Size = sizeof(HexGridParams);
    bufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
    bufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    impl_->device_->CreateBuffer(bufferDesc, nullptr, &impl_->hexGridParams_);
    static const ShaderResourceVariableDesc variables[] = {
        {SHADER_TYPE_COMPUTE, "HexGridParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    ArtifactCore::ComputePipelineDesc desc;
    desc.name = "Composition Hex Grid PSO";
    desc.shaderSource = kHexGridShader;
    desc.entryPoint = "HexGridCS";
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = variables;
    desc.variableCount = static_cast<Uint32>(sizeof(variables) / sizeof(variables[0]));
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    if (!impl_->hexGridParams_ || !impl_->hexGridExecutor_->build(desc) ||
        !impl_->hexGridExecutor_->createShaderResourceBinding(true) ||
        !impl_->hexGridExecutor_->setBuffer("HexGridParams", impl_->hexGridParams_)) {
     impl_->hexGridExecutor_.reset();
     impl_->hexGridParams_.Release();
     return false;
    }
   }
   void* mapped = nullptr;
   ctx->MapBuffer(impl_->hexGridParams_, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
   if (!mapped) return false;
   HexGridParams params;
   params.cellSize = std::clamp(node.parameters[0], 4.0f, 1024.0f);
   params.lineWidth = std::clamp(node.parameters[1], 0.5f, 1024.0f);
   params.angle = std::isfinite(node.parameters[2]) ? node.parameters[2] : 0.0f;
   std::memcpy(mapped, &params, sizeof(params));
   ctx->UnmapBuffer(impl_->hexGridParams_, MAP_WRITE);
   if (!impl_->hexGridExecutor_->setTextureView("g_OutputTexture", scratchUAV)) {
    return false;
   }
   impl_->hexGridExecutor_->dispatch(
       ctx, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
                impl_->width_, impl_->height_, 1, 8, 8, 1),
       RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
   CopyTextureAttribs copy;
   copy.pSrcTexture = scratchUAV->GetTexture();
   copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
   copy.pDstTexture = outputUAV->GetTexture();
   copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
   ctx->CopyTexture(copy);
   return true;
  }
  if (node.kind == GpuSpatialEffectKind::Stripes) {
   if (!impl_->stripesExecutor_) {
    impl_->screenSpaceGIContext_ = impl_->screenSpaceGIContext_
        ? impl_->screenSpaceGIContext_
        : ArtifactCore::makeShared<GpuContext>(impl_->device_, ctx);
    impl_->stripesExecutor_ =
        std::make_unique<ArtifactCore::ComputeExecutor>(*impl_->screenSpaceGIContext_);
    BufferDesc bufferDesc;
    bufferDesc.Name = "Composition Stripes Params";
    bufferDesc.Usage = USAGE_DYNAMIC;
    bufferDesc.Size = sizeof(StripesParams);
    bufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
    bufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    impl_->device_->CreateBuffer(bufferDesc, nullptr, &impl_->stripesParams_);
    static const ShaderResourceVariableDesc variables[] = {
        {SHADER_TYPE_COMPUTE, "StripesParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    ArtifactCore::ComputePipelineDesc desc;
    desc.name = "Composition Stripes PSO";
    desc.shaderSource = kStripesShader;
    desc.entryPoint = "StripesCS";
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = variables;
    desc.variableCount = static_cast<Uint32>(sizeof(variables) / sizeof(variables[0]));
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    if (!impl_->stripesParams_ || !impl_->stripesExecutor_->build(desc) ||
        !impl_->stripesExecutor_->createShaderResourceBinding(true) ||
        !impl_->stripesExecutor_->setBuffer("StripesParams", impl_->stripesParams_)) {
     impl_->stripesExecutor_.reset();
     impl_->stripesParams_.Release();
     return false;
    }
   }
   void* mapped = nullptr;
   ctx->MapBuffer(impl_->stripesParams_, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
   if (!mapped) return false;
   StripesParams params;
   params.frequency = std::clamp(node.parameters[0], 0.5f, 512.0f);
   params.angle = std::isfinite(node.parameters[1]) ? node.parameters[1] : 0.0f;
   params.thickness = std::clamp(node.parameters[2], 0.0f, 1.0f);
   params.offset = std::isfinite(node.parameters[3]) ? node.parameters[3] : 0.0f;
   std::memcpy(mapped, &params, sizeof(params));
   ctx->UnmapBuffer(impl_->stripesParams_, MAP_WRITE);
   if (!impl_->stripesExecutor_->setTextureView("g_OutputTexture", scratchUAV)) {
    return false;
   }
   impl_->stripesExecutor_->dispatch(
       ctx, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
                impl_->width_, impl_->height_, 1, 8, 8, 1),
       RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
   CopyTextureAttribs copy;
   copy.pSrcTexture = scratchUAV->GetTexture();
   copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
   copy.pDstTexture = outputUAV->GetTexture();
   copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
   ctx->CopyTexture(copy);
   return true;
  }
  if (node.kind == GpuSpatialEffectKind::Sharpen) {
   if (!impl_->sharpenExecutor_) {
    impl_->screenSpaceGIContext_ = impl_->screenSpaceGIContext_
        ? impl_->screenSpaceGIContext_
        : ArtifactCore::makeShared<GpuContext>(impl_->device_, ctx);
    impl_->sharpenExecutor_ =
        std::make_unique<ArtifactCore::ComputeExecutor>(*impl_->screenSpaceGIContext_);
    BufferDesc bufferDesc;
    bufferDesc.Name = "Composition Sharpen Params";
    bufferDesc.Usage = USAGE_DYNAMIC;
    bufferDesc.Size = sizeof(SharpenParams);
    bufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
    bufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    impl_->device_->CreateBuffer(bufferDesc, nullptr, &impl_->sharpenParams_);
    static const ShaderResourceVariableDesc variables[] = {
        {SHADER_TYPE_COMPUTE, "SharpenParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    ArtifactCore::ComputePipelineDesc desc;
    desc.name = "Composition Sharpen PSO";
    desc.shaderSource = kSharpenShader;
    desc.entryPoint = "SharpenCS";
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = variables;
    desc.variableCount = static_cast<Uint32>(sizeof(variables) / sizeof(variables[0]));
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    if (!impl_->sharpenParams_ || !impl_->sharpenExecutor_->build(desc) ||
        !impl_->sharpenExecutor_->createShaderResourceBinding(true) ||
        !impl_->sharpenExecutor_->setBuffer("SharpenParams", impl_->sharpenParams_)) {
     impl_->sharpenExecutor_.reset();
     impl_->sharpenParams_.Release();
     return false;
    }
   }
   void* mapped = nullptr;
   ctx->MapBuffer(impl_->sharpenParams_, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
   if (!mapped) return false;
   SharpenParams params;
   params.amount = std::clamp(node.parameters[0], 0.0f, 10.0f);
   params.sigma = std::clamp(node.parameters[1], 0.0f, 10.0f);
   params.threshold = std::clamp(node.parameters[2], 0.0f, 1.0f);
   std::memcpy(mapped, &params, sizeof(params));
   ctx->UnmapBuffer(impl_->sharpenParams_, MAP_WRITE);
   if (!impl_->sharpenExecutor_->setTextureView("g_InputTexture", inputSRV) ||
       !impl_->sharpenExecutor_->setTextureView("g_OutputTexture", scratchUAV)) {
    return false;
   }
   impl_->sharpenExecutor_->dispatch(
       ctx, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
                impl_->width_, impl_->height_, 1, 8, 8, 1),
       RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
   CopyTextureAttribs copy;
   copy.pSrcTexture = scratchUAV->GetTexture();
   copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
   copy.pDstTexture = outputUAV->GetTexture();
   copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
   ctx->CopyTexture(copy);
   return true;
  }
  if (node.kind == GpuSpatialEffectKind::ChromaticAberration) {
   if (!impl_->chromaticAberrationExecutor_) {
    impl_->screenSpaceGIContext_ = impl_->screenSpaceGIContext_
        ? impl_->screenSpaceGIContext_
        : ArtifactCore::makeShared<GpuContext>(impl_->device_, ctx);
    impl_->chromaticAberrationExecutor_ =
        std::make_unique<ArtifactCore::ComputeExecutor>(
            *impl_->screenSpaceGIContext_);
    BufferDesc bufferDesc;
    bufferDesc.Name = "Composition Chromatic Aberration Params";
    bufferDesc.Usage = USAGE_DYNAMIC;
    bufferDesc.Size = sizeof(ChromaticAberrationParams);
    bufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
    bufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    impl_->device_->CreateBuffer(bufferDesc, nullptr,
                                 &impl_->chromaticAberrationParams_);
    static const ShaderResourceVariableDesc variables[] = {
        {SHADER_TYPE_COMPUTE, "ChromaticAberrationParams",
         SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_InputTexture",
         SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_OutputTexture",
         SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    ArtifactCore::ComputePipelineDesc desc;
    desc.name = "Composition Chromatic Aberration PSO";
    desc.shaderSource = kChromaticAberrationShader;
    desc.entryPoint = "ChromaticAberrationCS";
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = variables;
    desc.variableCount = static_cast<Uint32>(sizeof(variables) / sizeof(variables[0]));
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    if (!impl_->chromaticAberrationParams_ ||
        !impl_->chromaticAberrationExecutor_->build(desc) ||
        !impl_->chromaticAberrationExecutor_->createShaderResourceBinding(true) ||
        !impl_->chromaticAberrationExecutor_->setBuffer(
            "ChromaticAberrationParams",
            impl_->chromaticAberrationParams_)) {
     impl_->chromaticAberrationExecutor_.reset();
     impl_->chromaticAberrationParams_.Release();
     return false;
    }
   }
   void* mapped = nullptr;
   ctx->MapBuffer(impl_->chromaticAberrationParams_, MAP_WRITE,
                  MAP_FLAG_DISCARD, mapped);
   if (!mapped) return false;
   ChromaticAberrationParams params;
   params.redShift = std::clamp(node.parameters[0], 0.0f, 50.0f);
   params.blueShift = std::clamp(node.parameters[1], 0.0f, 50.0f);
   params.centerX = std::clamp(node.parameters[2], 0.0f, 1.0f);
   params.centerY = std::clamp(node.parameters[3], 0.0f, 1.0f);
   std::memcpy(mapped, &params, sizeof(params));
   ctx->UnmapBuffer(impl_->chromaticAberrationParams_, MAP_WRITE);
   if (!impl_->chromaticAberrationExecutor_->setTextureView(
           "g_InputTexture", inputSRV) ||
       !impl_->chromaticAberrationExecutor_->setTextureView(
           "g_OutputTexture", scratchUAV)) {
    return false;
   }
   impl_->chromaticAberrationExecutor_->dispatch(
       ctx, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
                impl_->width_, impl_->height_, 1, 8, 8, 1),
       RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
   CopyTextureAttribs copy;
   copy.pSrcTexture = scratchUAV->GetTexture();
   copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
   copy.pDstTexture = outputUAV->GetTexture();
   copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
   ctx->CopyTexture(copy);
   return true;
  }
  if (node.kind == GpuSpatialEffectKind::Vignette) {
   if (!impl_->vignetteExecutor_) {
    impl_->screenSpaceGIContext_ = impl_->screenSpaceGIContext_
        ? impl_->screenSpaceGIContext_
        : ArtifactCore::makeShared<GpuContext>(impl_->device_, ctx);
    impl_->vignetteExecutor_ =
        std::make_unique<ArtifactCore::ComputeExecutor>(*impl_->screenSpaceGIContext_);
    BufferDesc bufferDesc;
    bufferDesc.Name = "Composition Vignette Params";
    bufferDesc.Usage = USAGE_DYNAMIC;
    bufferDesc.Size = sizeof(VignetteParams);
    bufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
    bufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    impl_->device_->CreateBuffer(bufferDesc, nullptr, &impl_->vignetteParams_);
    static const ShaderResourceVariableDesc variables[] = {
        {SHADER_TYPE_COMPUTE, "VignetteParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    ArtifactCore::ComputePipelineDesc desc;
    desc.name = "Composition Vignette PSO";
    desc.shaderSource = kVignetteShader;
    desc.entryPoint = "VignetteCS";
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = variables;
    desc.variableCount = static_cast<Uint32>(sizeof(variables) / sizeof(variables[0]));
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    if (!impl_->vignetteParams_ || !impl_->vignetteExecutor_->build(desc) ||
        !impl_->vignetteExecutor_->createShaderResourceBinding(true) ||
        !impl_->vignetteExecutor_->setBuffer(
            "VignetteParams", impl_->vignetteParams_)) {
     impl_->vignetteExecutor_.reset();
     impl_->vignetteParams_.Release();
     return false;
    }
   }
   void* mapped = nullptr;
   ctx->MapBuffer(impl_->vignetteParams_, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
   if (!mapped) return false;
   VignetteParams params;
   params.amount = std::clamp(node.parameters[0], 0.0f, 1.0f);
   params.radius = std::clamp(node.parameters[1], 0.0f, 2.0f);
   params.feather = std::clamp(node.parameters[2], 0.01f, 2.0f);
   params.centerX = std::clamp(node.parameters[3], 0.0f, 1.0f);
   params.centerY = std::clamp(node.parameters[4], 0.0f, 1.0f);
   std::memcpy(mapped, &params, sizeof(params));
   ctx->UnmapBuffer(impl_->vignetteParams_, MAP_WRITE);
   if (!impl_->vignetteExecutor_->setTextureView("g_InputTexture", inputSRV) ||
       !impl_->vignetteExecutor_->setTextureView("g_OutputTexture", scratchUAV)) {
    return false;
   }
   impl_->vignetteExecutor_->dispatch(
       ctx, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
                impl_->width_, impl_->height_, 1, 8, 8, 1),
       RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
   CopyTextureAttribs copy;
   copy.pSrcTexture = scratchUAV->GetTexture();
   copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
   copy.pDstTexture = outputUAV->GetTexture();
   copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
   ctx->CopyTexture(copy);
   return true;
  }
  if (node.kind != GpuSpatialEffectKind::SeparableGaussianBlur) return false;
  const float sigma = node.parameters[0];
  if (sigma <= 0.0f) return false;
  if (!impl_->gaussianBlurExecutor_) {
   impl_->screenSpaceGIContext_ = impl_->screenSpaceGIContext_
       ? impl_->screenSpaceGIContext_
       : ArtifactCore::makeShared<GpuContext>(impl_->device_, ctx);
   impl_->gaussianBlurExecutor_ =
       std::make_unique<ArtifactCore::ComputeExecutor>(*impl_->screenSpaceGIContext_);
   BufferDesc bufferDesc;
   bufferDesc.Name = "Composition Gaussian Blur Params";
   bufferDesc.Usage = USAGE_DYNAMIC;
   bufferDesc.Size = sizeof(GaussianBlurParams);
   bufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
   bufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
   impl_->device_->CreateBuffer(bufferDesc, nullptr, &impl_->gaussianBlurParams_);
   static const ShaderResourceVariableDesc variables[] = {
       {SHADER_TYPE_COMPUTE, "GaussianBlurParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
       {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
       {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
   };
   ArtifactCore::ComputePipelineDesc desc;
   desc.name = "Composition Gaussian Blur PSO";
   desc.shaderSource = kGaussianBlurShader;
   desc.entryPoint = "GaussianBlurCS";
   desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
   desc.variables = variables;
   desc.variableCount = static_cast<Uint32>(sizeof(variables) / sizeof(variables[0]));
   desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
   if (!impl_->gaussianBlurParams_ ||
       !impl_->gaussianBlurExecutor_->build(desc) ||
       !impl_->gaussianBlurExecutor_->createShaderResourceBinding(true) ||
       !impl_->gaussianBlurExecutor_->setBuffer(
           "GaussianBlurParams", impl_->gaussianBlurParams_)) {
    impl_->gaussianBlurExecutor_.reset();
    impl_->gaussianBlurParams_.Release();
    return false;
   }
  }
  auto dispatchPass = [&](ITextureView* source, ITextureView* destination,
                          Uint32 horizontal) {
   void* mapped = nullptr;
   ctx->MapBuffer(impl_->gaussianBlurParams_, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
   if (!mapped) return false;
   GaussianBlurParams params;
   params.sigma = std::clamp(sigma, 0.1f, 64.0f);
   params.horizontal = horizontal;
   std::memcpy(mapped, &params, sizeof(params));
   ctx->UnmapBuffer(impl_->gaussianBlurParams_, MAP_WRITE);
   if (!impl_->gaussianBlurExecutor_->setTextureView("g_InputTexture", source) ||
       !impl_->gaussianBlurExecutor_->setTextureView("g_OutputTexture", destination)) {
    return false;
   }
   impl_->gaussianBlurExecutor_->dispatch(
       ctx, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
                impl_->width_, impl_->height_, 1, 8, 8, 1),
       RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
   return true;
  };
  auto* scratchSRV = scratchUAV->GetTexture()->GetDefaultView(
      TEXTURE_VIEW_SHADER_RESOURCE);
  return scratchSRV && dispatchPass(inputSRV, scratchUAV, 1) &&
         dispatchPass(scratchSRV, outputUAV, 0);
 }

 bool RenderPipeline::applyPointwise(
    IDeviceContext* ctx,
    const ArtifactCore::PointwiseEffectStack& stack,
    ITextureView* backgroundSRV,
    ITextureView* lutSRV,
    ITextureView* historySRV)
 {
  if (!ctx || !ready() || stack.nodes().empty()) {
   return false;
  }
  if (!impl_->blendPipeline_) {
   impl_->blendContext_ = ArtifactCore::makeShared<GpuContext>(impl_->device_, ctx);
   impl_->blendPipeline_ = std::make_unique<LayerBlendPipeline>(impl_->blendContext_);
   if (!impl_->blendPipeline_->initialize()) {
    impl_->blendPipeline_.reset();
    impl_->blendContext_.reset();
    return false;
   }
  }

  const auto segments = stack.segments();
  if (segments.empty()) {
   return false;
  }
  for (const auto& segment : segments) {
   if (!ArtifactCore::PointwiseEffectFusion::validateSegment(
           stack.nodes(), segment).valid) {
    // A mixed stack must remain on the existing compositor path until its
    // non-pointwise boundary has a GPU implementation.  Do not apply only a
    // prefix, otherwise the visible result would silently lose effects.
    return false;
   }
  }
  if (!impl_->blendPipeline_->updatePointwiseParameters(ctx, stack)) {
   return false;
  }

  bool applied = false;
  for (const auto& segment : segments) {
   if (segment.nodeCount == 0) {
    continue;
   }
   const auto plan = ArtifactCore::PointwiseEffectFusion::makeComputePlan(
       "diligent", "rgba16f", stack.nodes(), segment,
       impl_->width_, impl_->height_);
   if (!plan.valid() || !impl_->blendPipeline_->applyPointwise(
           ctx, impl_->accum_.srv, impl_->temp_.uav,
           impl_->blendPipeline_->createPointwiseParameterBuffer(), plan,
           backgroundSRV, lutSRV, historySRV)) {
    return false;
   }
   std::swap(impl_->accum_, impl_->temp_);
   applied = true;
  }
  return applied;
 }

 bool RenderPipeline::ready() const
 {
 return impl_->device_ != nullptr && impl_->width_ > 0 && impl_->height_ > 0 &&
         impl_->accum_.texture && impl_->temp_.texture && impl_->layer_.texture &&
         impl_->layerFloat_.texture && impl_->matteSources_[0].texture &&
         impl_->matteSources_[1].texture && impl_->matteSources_[2].texture &&
         impl_->accum_.srv && impl_->accum_.uav && impl_->accum_.rtv &&
         impl_->temp_.srv && impl_->temp_.uav && impl_->temp_.rtv &&
         impl_->layer_.srv && impl_->layer_.rtv &&
         impl_->layerFloat_.srv && impl_->layerFloat_.uav &&
         impl_->matteSources_[0].srv &&
         impl_->matteSources_[1].srv &&
         impl_->matteSources_[2].srv &&
         (!impl_->emissionEnabled_ ||
          (impl_->emission_.texture && impl_->emission_.srv &&
           impl_->emission_.rtv &&
           impl_->normal_.texture && impl_->normal_.srv &&
           impl_->normal_.rtv &&
           impl_->velocity_.texture && impl_->velocity_.srv &&
           impl_->velocity_.rtv)) &&
         (!impl_->emissionEnabled_ ||
          (impl_->objectId_.texture && impl_->objectId_.srv &&
           impl_->objectId_.rtv &&
           impl_->materialId_.texture && impl_->materialId_.srv &&
           impl_->materialId_.rtv &&
           impl_->albedo_.texture && impl_->albedo_.srv &&
           impl_->albedo_.rtv));
 }

 bool RenderPipeline::renderComposition(
  IDeviceContext* ctx,
  const std::vector<ArtifactAbstractLayerPtr>& layers,
  int64_t currentFrame,
  ITextureView* outputRTV)
 {
  if (!ctx || !outputRTV || !ready())
  {
   return false;
  }

  // The controller currently owns the actual layer draw/blend loop.
  // Keep this entry point available for future consolidation.
  (void)layers;
  (void)currentFrame;
  const float clearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
  ctx->SetRenderTargets(1, &outputRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  ctx->ClearRenderTarget(outputRTV, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  return true;
 }

 ITextureView* RenderPipeline::accumSRV() const { return impl_->accum_.srv; }
 ITextureView* RenderPipeline::accumUAV() const { return impl_->accum_.uav; }
 ITextureView* RenderPipeline::accumRTV() const { return impl_->accum_.rtv; }
 ITextureView* RenderPipeline::tempSRV() const { return impl_->temp_.srv; }
 ITextureView* RenderPipeline::tempUAV() const { return impl_->temp_.uav; }
 ITextureView* RenderPipeline::tempRTV() const { return impl_->temp_.rtv; }
 ITextureView* RenderPipeline::layerSRV() const { return impl_->layer_.srv; }
 ITextureView* RenderPipeline::layerUAV() const { return impl_->layer_.uav; }
 ITextureView* RenderPipeline::layerRTV() const { return impl_->layer_.rtv; }
ITextureView* RenderPipeline::layerFloatSRV() const { return impl_->layerFloat_.srv; }
ITextureView* RenderPipeline::layerFloatUAV() const { return impl_->layerFloat_.uav; }
ITextureView* RenderPipeline::matteSourceSRV() const {
 return impl_->matteSources_[0].srv;
}
ITextureView* RenderPipeline::matteSourceSRV(int index) const {
 if (index < 0 || index >= static_cast<int>(impl_->matteSources_.size())) {
  return nullptr;
 }
 return impl_->matteSources_[static_cast<size_t>(index)].srv;
}
ITextureView* RenderPipeline::emissionSRV() const { return impl_->emission_.srv; }
ITextureView* RenderPipeline::emissionRTV() const { return impl_->emission_.rtv; }
bool RenderPipeline::hasEmissionTarget() const { return impl_->emissionEnabled_ && impl_->emission_.texture; }
ITextureView* RenderPipeline::normalSRV() const { return impl_->normal_.srv; }
ITextureView* RenderPipeline::normalRTV() const { return impl_->normal_.rtv; }
bool RenderPipeline::hasNormalTarget() const { return impl_->emissionEnabled_ && impl_->normal_.texture; }
ITextureView* RenderPipeline::velocitySRV() const { return impl_->velocity_.srv; }
ITextureView* RenderPipeline::velocityRTV() const { return impl_->velocity_.rtv; }
bool RenderPipeline::hasVelocityTarget() const { return impl_->emissionEnabled_ && impl_->velocity_.texture; }
ITextureView* RenderPipeline::objectIdSRV() const { return impl_->objectId_.srv; }
ITextureView* RenderPipeline::objectIdRTV() const { return impl_->objectId_.rtv; }
bool RenderPipeline::hasObjectIdTarget() const { return impl_->emissionEnabled_ && impl_->objectId_.texture; }
ITextureView* RenderPipeline::materialIdSRV() const { return impl_->materialId_.srv; }
ITextureView* RenderPipeline::materialIdRTV() const { return impl_->materialId_.rtv; }
bool RenderPipeline::hasMaterialIdTarget() const { return impl_->emissionEnabled_ && impl_->materialId_.texture; }
ITextureView* RenderPipeline::albedoSRV() const { return impl_->albedo_.srv; }
ITextureView* RenderPipeline::albedoRTV() const { return impl_->albedo_.rtv; }
bool RenderPipeline::hasAlbedoTarget() const { return impl_->emissionEnabled_ && impl_->albedo_.texture; }
GlobalIlluminationInputs RenderPipeline::globalIlluminationInputs(
    ITextureView* depthSRV) const
{
 GlobalIlluminationInputs inputs;
 inputs.depth = depthSRV;
 inputs.normal = normalSRV();
 inputs.albedo = albedoSRV();
 inputs.velocity = velocitySRV();
 inputs.emission = emissionSRV();
 return inputs;
}
bool RenderPipeline::dispatchScreenSpaceGlobalIllumination(
    IDeviceContext* ctx,
    const GlobalIlluminationInputs& inputs,
    float resolutionScale,
    Uint32 raySteps,
    float intensity,
    float depthThickness,
    bool temporalAccumulation,
    bool denoise)
{
 if (!ctx || !impl_->device_ || !inputs.validForScreenSpace() ||
     impl_->width_ == 0 || impl_->height_ == 0) {
  return false;
 }

 const float safeScale = std::clamp(resolutionScale, 0.25f, 1.0f);
 const Uint32 outputWidth = std::max(
     1u, static_cast<Uint32>(std::ceil(impl_->width_ * safeScale)));
 const Uint32 outputHeight = std::max(
     1u, static_cast<Uint32>(std::ceil(impl_->height_ * safeScale)));

 if (!impl_->screenSpaceGI_.texture ||
     impl_->screenSpaceGI_.texture->GetDesc().Width != outputWidth ||
     impl_->screenSpaceGI_.texture->GetDesc().Height != outputHeight) {
  if (!createTextureBundle(impl_->device_, outputWidth, outputHeight,
                           TEX_FORMAT_RGBA16_FLOAT,
                           BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS,
                           "RenderPipeline.ScreenSpaceGI",
                           impl_->screenSpaceGI_)) {
   return false;
  }
  impl_->screenSpaceGIHistoryValid_ = false;
 }

 for (auto& history : impl_->screenSpaceGIHistory_) {
  if (!history.texture || history.texture->GetDesc().Width != outputWidth ||
      history.texture->GetDesc().Height != outputHeight) {
   if (!createTextureBundle(impl_->device_, outputWidth, outputHeight,
                            TEX_FORMAT_RGBA16_FLOAT,
                            BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS,
                            "RenderPipeline.ScreenSpaceGIHistory",
                            history)) {
    return false;
   }
   impl_->screenSpaceGIHistoryValid_ = false;
  }
 }

 if (!impl_->screenSpaceGIExecutor_) {
  impl_->screenSpaceGIContext_ =
      ArtifactCore::makeShared<GpuContext>(impl_->device_, ctx);
  impl_->screenSpaceGIExecutor_ =
      std::make_unique<ArtifactCore::ComputeExecutor>(
          *impl_->screenSpaceGIContext_);

  BufferDesc paramsDesc;
  paramsDesc.Name = "ScreenSpaceGI Params";
  paramsDesc.Usage = USAGE_DYNAMIC;
  paramsDesc.Size = sizeof(ScreenSpaceGlobalIlluminationParams);
  paramsDesc.BindFlags = BIND_UNIFORM_BUFFER;
  paramsDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  impl_->device_->CreateBuffer(paramsDesc, nullptr,
                               &impl_->screenSpaceGIParams_);

  static const ShaderResourceVariableDesc variables[] = {
      {SHADER_TYPE_COMPUTE, "SSGIParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_Depth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_Normal", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_Albedo", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_Output", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  ArtifactCore::ComputePipelineDesc desc;
  desc.name = "ScreenSpaceGI PSO";
  desc.shaderSource = kScreenSpaceGlobalIlluminationShader;
  desc.entryPoint = "ScreenSpaceGICS";
  desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  desc.variables = variables;
  desc.variableCount = static_cast<Uint32>(
      sizeof(variables) / sizeof(variables[0]));
  desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  if (!impl_->screenSpaceGIParams_ ||
      !impl_->screenSpaceGIExecutor_->build(desc) ||
      !impl_->screenSpaceGIExecutor_->createShaderResourceBinding(true)) {
   impl_->screenSpaceGIExecutor_.reset();
   impl_->screenSpaceGIContext_.reset();
   impl_->screenSpaceGIParams_.Release();
   return false;
  }

  impl_->screenSpaceGIResolveExecutor_ =
      std::make_unique<ArtifactCore::ComputeExecutor>(
          *impl_->screenSpaceGIContext_);
  BufferDesc resolveParamsDesc;
  resolveParamsDesc.Name = "ScreenSpaceGI Resolve Params";
  resolveParamsDesc.Usage = USAGE_DYNAMIC;
  resolveParamsDesc.Size =
      sizeof(ScreenSpaceGlobalIlluminationResolveParams);
  resolveParamsDesc.BindFlags = BIND_UNIFORM_BUFFER;
  resolveParamsDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  impl_->device_->CreateBuffer(resolveParamsDesc, nullptr,
                               &impl_->screenSpaceGIResolveParams_);
  static const ShaderResourceVariableDesc resolveVariables[] = {
      {SHADER_TYPE_COMPUTE, "SSGIResolveParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_RawGI", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_HistoryGI", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_ResolveDepth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_ResolveNormal", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_ResolveVelocity", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_ResolvedGI", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  ArtifactCore::ComputePipelineDesc resolveDesc;
  resolveDesc.name = "ScreenSpaceGI Resolve PSO";
  resolveDesc.shaderSource = kScreenSpaceGlobalIlluminationResolveShader;
  resolveDesc.entryPoint = "ScreenSpaceGIResolveCS";
  resolveDesc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  resolveDesc.variables = resolveVariables;
  resolveDesc.variableCount = static_cast<Uint32>(
      sizeof(resolveVariables) / sizeof(resolveVariables[0]));
  resolveDesc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  if (!impl_->screenSpaceGIResolveParams_ ||
      !impl_->screenSpaceGIResolveExecutor_->build(resolveDesc) ||
      !impl_->screenSpaceGIResolveExecutor_->createShaderResourceBinding(true)) {
   impl_->screenSpaceGIResolveExecutor_.reset();
   impl_->screenSpaceGIResolveParams_.Release();
   impl_->screenSpaceGIExecutor_.reset();
   impl_->screenSpaceGIParams_.Release();
   impl_->screenSpaceGIContext_.reset();
   return false;
  }
 }

 ScreenSpaceGlobalIlluminationParams params;
 params.inputWidth = impl_->width_;
 params.inputHeight = impl_->height_;
 params.outputWidth = outputWidth;
 params.outputHeight = outputHeight;
 params.raySteps = std::clamp(raySteps, 1u, 24u);
 params.intensity = std::clamp(intensity, 0.0f, 8.0f);
 params.depthThickness = std::clamp(depthThickness, 0.0001f, 1.0f);
 params.radiusPixels = 32.0f * safeScale;

 void* mappedParams = nullptr;
 ctx->MapBuffer(impl_->screenSpaceGIParams_, MAP_WRITE,
                MAP_FLAG_DISCARD, mappedParams);
 if (!mappedParams) {
  return false;
 }
 std::memcpy(mappedParams, &params, sizeof(params));
 ctx->UnmapBuffer(impl_->screenSpaceGIParams_, MAP_WRITE);

 auto& executor = *impl_->screenSpaceGIExecutor_;
 if (!executor.setBuffer("SSGIParams", impl_->screenSpaceGIParams_) ||
     !executor.setTextureView("g_Depth", inputs.depth) ||
     !executor.setTextureView("g_Normal", inputs.normal) ||
     !executor.setTextureView("g_Albedo", inputs.albedo) ||
     !executor.setTextureView("g_Output", impl_->screenSpaceGI_.uav)) {
  return false;
 }

 const auto dispatch = ArtifactCore::ComputeExecutor::makeDispatchAttribs(
     outputWidth, outputHeight, 1, 8, 8, 1);
 executor.dispatch(ctx, dispatch,
                   RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

 const Uint32 writeIndex = impl_->screenSpaceGIHistoryWriteIndex_;
 const Uint32 readIndex = 1u - writeIndex;
 ScreenSpaceGlobalIlluminationResolveParams resolveParams;
 resolveParams.inputWidth = impl_->width_;
 resolveParams.inputHeight = impl_->height_;
 resolveParams.outputWidth = outputWidth;
 resolveParams.outputHeight = outputHeight;
 resolveParams.historyValid = impl_->screenSpaceGIHistoryValid_ ? 1u : 0u;
 resolveParams.temporalEnabled =
     temporalAccumulation && inputs.validForTemporalReuse() ? 1u : 0u;
 resolveParams.denoiseEnabled = denoise ? 1u : 0u;
 resolveParams.historyWeight = 0.9f;
 resolveParams.depthSigma = params.depthThickness;
 resolveParams.normalSigma = 16.0f;
 resolveParams.velocityScale = 1.0f;

 void* mappedResolveParams = nullptr;
 ctx->MapBuffer(impl_->screenSpaceGIResolveParams_, MAP_WRITE,
                MAP_FLAG_DISCARD, mappedResolveParams);
 if (!mappedResolveParams) {
  return false;
 }
 std::memcpy(mappedResolveParams, &resolveParams, sizeof(resolveParams));
 ctx->UnmapBuffer(impl_->screenSpaceGIResolveParams_, MAP_WRITE);

 auto& resolveExecutor = *impl_->screenSpaceGIResolveExecutor_;
 if (!resolveExecutor.setBuffer("SSGIResolveParams",
                                impl_->screenSpaceGIResolveParams_) ||
     !resolveExecutor.setTextureView("g_RawGI", impl_->screenSpaceGI_.srv) ||
     !resolveExecutor.setTextureView(
         "g_HistoryGI", impl_->screenSpaceGIHistory_[readIndex].srv) ||
     !resolveExecutor.setTextureView("g_ResolveDepth", inputs.depth) ||
     !resolveExecutor.setTextureView("g_ResolveNormal", inputs.normal) ||
     !resolveExecutor.setTextureView(
         "g_ResolveVelocity",
         inputs.velocity ? inputs.velocity : inputs.normal) ||
     !resolveExecutor.setTextureView(
         "g_ResolvedGI", impl_->screenSpaceGIHistory_[writeIndex].uav)) {
  return false;
 }
 resolveExecutor.dispatch(ctx, dispatch,
                          RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 impl_->screenSpaceGIHistoryWriteIndex_ = readIndex;
 impl_->screenSpaceGIHistoryValid_ = true;
 return true;
}
bool RenderPipeline::applyScreenSpaceAmbientOcclusion(
    IDeviceContext* ctx, ITextureView* sourceColor,
    ITextureView* destinationColor, ITextureView* aoMask)
{
 if (!ctx || !impl_->device_ || !sourceColor || !destinationColor || !aoMask ||
     sourceColor == destinationColor || impl_->width_ == 0 || impl_->height_ == 0) {
  return false;
 }
 if (!impl_->screenSpaceAOCompositeExecutor_) {
  impl_->screenSpaceGIContext_ = impl_->screenSpaceGIContext_
      ? impl_->screenSpaceGIContext_
      : ArtifactCore::makeShared<GpuContext>(impl_->device_, ctx);
  impl_->screenSpaceAOCompositeExecutor_ =
      std::make_unique<ArtifactCore::ComputeExecutor>(*impl_->screenSpaceGIContext_);
  static const ShaderResourceVariableDesc variables[] = {
      {SHADER_TYPE_COMPUTE, "g_SourceColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_AOMask", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_DestinationColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  ArtifactCore::ComputePipelineDesc desc;
  desc.name = "ScreenSpace AO Composite PSO";
  desc.shaderSource = kScreenSpaceAOCompositeShader;
  desc.entryPoint = "ScreenSpaceAOCompositeCS";
  desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  desc.variables = variables;
  desc.variableCount = static_cast<Uint32>(sizeof(variables) / sizeof(variables[0]));
  desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  if (!impl_->screenSpaceAOCompositeExecutor_->build(desc) ||
      !impl_->screenSpaceAOCompositeExecutor_->createShaderResourceBinding(true)) {
   impl_->screenSpaceAOCompositeExecutor_.reset();
   return false;
  }
 }
 auto& executor = *impl_->screenSpaceAOCompositeExecutor_;
 if (!executor.setTextureView("g_SourceColor", sourceColor) ||
     !executor.setTextureView("g_AOMask", aoMask) ||
     !executor.setTextureView("g_DestinationColor", destinationColor)) {
  return false;
 }
 executor.dispatch(ctx, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
     impl_->width_, impl_->height_, 1, 8, 8, 1),
     RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 return true;
}
bool RenderPipeline::applyFastApproximateAntiAliasing(
    IDeviceContext* ctx, ITextureView* sourceColor,
    ITextureView* destinationColor)
{
 if (!ctx || !impl_->device_ || !sourceColor || !destinationColor ||
     sourceColor == destinationColor || impl_->width_ == 0 || impl_->height_ == 0) {
  return false;
 }
 if (!impl_->fastApproximateAntiAliasingExecutor_) {
  impl_->screenSpaceGIContext_ = impl_->screenSpaceGIContext_
      ? impl_->screenSpaceGIContext_
      : ArtifactCore::makeShared<GpuContext>(impl_->device_, ctx);
  impl_->fastApproximateAntiAliasingExecutor_ =
      std::make_unique<ArtifactCore::ComputeExecutor>(*impl_->screenSpaceGIContext_);
  static const ShaderResourceVariableDesc variables[] = {
      {SHADER_TYPE_COMPUTE, "g_SourceColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_DestinationColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  ArtifactCore::ComputePipelineDesc desc;
  desc.name = "Composition FXAA PSO";
  desc.shaderSource = kFastApproximateAntiAliasingShader;
  desc.entryPoint = "FastApproximateAntiAliasingCS";
  desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  desc.variables = variables;
  desc.variableCount = static_cast<Uint32>(sizeof(variables) / sizeof(variables[0]));
  desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  if (!impl_->fastApproximateAntiAliasingExecutor_->build(desc) ||
      !impl_->fastApproximateAntiAliasingExecutor_->createShaderResourceBinding(true)) {
   impl_->fastApproximateAntiAliasingExecutor_.reset();
   return false;
  }
 }
 auto& executor = *impl_->fastApproximateAntiAliasingExecutor_;
 if (!executor.setTextureView("g_SourceColor", sourceColor) ||
     !executor.setTextureView("g_DestinationColor", destinationColor)) {
  return false;
 }
 executor.dispatch(ctx, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
     impl_->width_, impl_->height_, 1, 8, 8, 1),
     RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 return true;
}
ITextureView* RenderPipeline::screenSpaceGlobalIlluminationSRV() const
{
 if (impl_->screenSpaceGIHistoryValid_) {
  const Uint32 latestIndex = 1u - impl_->screenSpaceGIHistoryWriteIndex_;
  return impl_->screenSpaceGIHistory_[latestIndex].srv;
 }
 return impl_->screenSpaceGI_.srv;
}
void RenderPipeline::resetScreenSpaceGlobalIlluminationHistory()
{
 impl_->screenSpaceGIHistoryValid_ = false;
 impl_->screenSpaceGIHistoryWriteIndex_ = 0;
}
Uint32 RenderPipeline::screenSpaceGlobalIlluminationWidth() const
{
 return impl_->screenSpaceGI_.texture
            ? impl_->screenSpaceGI_.texture->GetDesc().Width
            : 0;
}
Uint32 RenderPipeline::screenSpaceGlobalIlluminationHeight() const
{
 return impl_->screenSpaceGI_.texture
            ? impl_->screenSpaceGI_.texture->GetDesc().Height
            : 0;
}
bool RenderPipeline::updateMatteSourceFromData(IDeviceContext* ctx,
                                               const void* data,
                                               Uint32 width,
                                               Uint32 height,
                                               Uint32 rowStride)
{
 return updateMatteSourceFromData(ctx, 0, data, width, height, rowStride);
}

bool RenderPipeline::updateMatteSourceFromData(IDeviceContext* ctx,
                                               int index,
                                               const void* data,
                                               Uint32 width,
                                               Uint32 height,
                                               Uint32 rowStride)
 {
  if (!ctx || !data || index < 0 ||
      index >= static_cast<int>(impl_->matteSources_.size())) return false;
  auto& matteSource = impl_->matteSources_[static_cast<size_t>(index)];
  if (!matteSource.texture) return false;
  if (width == 0 || height == 0) return false;

  const auto& texDesc = matteSource.texture->GetDesc();
  if (texDesc.Width != width || texDesc.Height != height) {
   if (!createTextureBundle(impl_->device_, width, height,
                            TEX_FORMAT_RGBA8_UNORM,
                            BIND_SHADER_RESOURCE,
                            "RenderPipeline.MatteSource", matteSource))
   {
    qWarning() << "[RenderPipeline] Matte source reallocation failed";
    return false;
   }
  }

  Box dstBox = {};
  dstBox.MinX = 0; dstBox.MaxX = width;
  dstBox.MinY = 0; dstBox.MaxY = height;
  dstBox.MinZ = 0; dstBox.MaxZ = 1;

  TextureSubResData subRes = {};
  subRes.pData = data;
  subRes.Stride = rowStride;

  ctx->UpdateTexture(matteSource.texture, 0, 0, dstBox, subRes,
                     RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                     RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  return true;
 }
 Uint32 RenderPipeline::width() const { return impl_->width_; }
 Uint32 RenderPipeline::height() const { return impl_->height_; }

 void RenderPipeline::swapAccumAndTemp()
 {
  std::swap(impl_->accum_, impl_->temp_);
 }

bool RenderPipeline::createTextures(IRenderDevice* device,
                                    Uint32 width,
                                    Uint32 height,
                                    TEXTURE_FORMAT format,
                                    bool enableEmission)
 {
  if (!createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS,
                           "RenderPipeline.Accum", impl_->accum_))
  {
   return false;
  }
  if (!createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS,
                           "RenderPipeline.Temp", impl_->temp_))
  {
   return false;
  }
   if (!createTextureBundle(device, width, height, RenderConfig::MainRTVFormat,
                            BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                            "RenderPipeline.Layer", impl_->layer_))
  {
   return false;
  }
  if (!createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS,
                           "RenderPipeline.LayerFloat", impl_->layerFloat_))
  {
   return false;
  }

  // Matte source: 8-bit RGBA (non-sRGB) for CPU-uploaded matte source layer content.
  // Non-sRGB format preserves QImage byte values as-is for correct luma calculation.
  for (size_t matteIndex = 0; matteIndex < impl_->matteSources_.size();
       ++matteIndex) {
   if (!createTextureBundle(device, width, height, TEX_FORMAT_RGBA8_UNORM,
                            BIND_SHADER_RESOURCE,
                            "RenderPipeline.MatteSource",
                            impl_->matteSources_[matteIndex])) {
    return false;
   }
  }

  if (enableEmission &&
      !createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.Emission", impl_->emission_))
  {
   return false;
  }
  if (enableEmission &&
      !createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.Normal", impl_->normal_))
  {
   return false;
  }
  if (enableEmission &&
      !createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.Velocity", impl_->velocity_))
  {
   return false;
  }
  if (enableEmission &&
      !createTextureBundle(device, width, height, TEX_FORMAT_RGBA16_FLOAT,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.ObjectId", impl_->objectId_))
  {
   return false;
  }
  if (enableEmission &&
      !createTextureBundle(device, width, height, TEX_FORMAT_RGBA16_FLOAT,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.MaterialId", impl_->materialId_))
  {
   return false;
  }
  if (enableEmission &&
      !createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.Albedo", impl_->albedo_))
  {
   return false;
  }

  return true;
}
}
