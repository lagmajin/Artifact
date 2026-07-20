#include "globals.hlsli"

// Anime4K-inspired, edge-directed reconstruction pass.
// This is intentionally kept as an ArtifactStudio shader asset rather than a
// direct source copy. It uses the input image's luma structure to preserve
// anime-style line edges while upscaling a preview surface.

Texture2D<float4> input : register(t0);
RWTexture2D<float4> output : register(u0);

cbuffer Anime4KConstants : register(b0)
{
    uint2 inputSize;
    uint2 outputSize;
    float edgeStrength;
    float lineBlend;
    float2 _padding;
};

float luma(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

float4 sampleInput(int2 pixel)
{
    pixel = clamp(pixel, int2(0, 0), int2(inputSize) - 1);
    return input.Load(int3(pixel, 0));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= outputSize.x || dispatchId.y >= outputSize.y)
        return;

    float2 sourcePosition = (float2(dispatchId.xy) + 0.5) * float2(inputSize) / float2(outputSize) - 0.5;
    int2 base = int2(floor(sourcePosition));
    float2 f = frac(sourcePosition);

    float4 c00 = sampleInput(base);
    float4 c10 = sampleInput(base + int2(1, 0));
    float4 c01 = sampleInput(base + int2(0, 1));
    float4 c11 = sampleInput(base + int2(1, 1));

    float gx = abs(luma(c10.rgb) - luma(c00.rgb)) + abs(luma(c11.rgb) - luma(c01.rgb));
    float gy = abs(luma(c01.rgb) - luma(c00.rgb)) + abs(luma(c11.rgb) - luma(c10.rgb));
    float edge = saturate(max(gx, gy) * edgeStrength);

    // Interpolate along the direction with the smaller gradient. This avoids
    // blurring a strong horizontal or vertical line during preview upscale.
    float4 horizontal = lerp(c00, c10, f.x);
    float4 horizontalBottom = lerp(c01, c11, f.x);
    float4 vertical = lerp(c00, c01, f.y);
    float4 verticalRight = lerp(c10, c11, f.y);
    float4 edgeAware = gy < gx
        ? lerp(vertical, verticalRight, f.x)
        : lerp(horizontal, horizontalBottom, f.y);

    float4 bilinear = lerp(horizontal, horizontalBottom, f.y);
    float4 result = lerp(bilinear, edgeAware, edge * lineBlend);
    output[dispatchId.xy] = float4(result.rgb, bilinear.a);
}
