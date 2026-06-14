#include "globals.hlsli"
#include "ShaderInterop_Postprocess.h"

PUSHCONSTANT(postprocess, PostProcess);

#ifndef BLUR_FORMAT
#define BLUR_FORMAT float4
#endif

Texture2D<BLUR_FORMAT> input : register(t0);
RWTexture2D<BLUR_FORMAT> output : register(u0);

// Radius is interpreted in pixels. The shader uses a separable Gaussian kernel
// so the host can run this pass twice: horizontal, then vertical.
static const int MAX_RADIUS = 64;
static const int MAX_KERNEL = MAX_RADIUS * 2 + 1;
groupshared BLUR_FORMAT sharedLine[MAX_KERNEL];

float gaussianWeight(float x, float sigma)
{
    return exp(-0.5f * (x * x) / max(0.0001f, sigma * sigma));
}

[numthreads(256, 1, 1)]
void main(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID)
{
    const uint2 size = postprocess.resolution;
    if (dtid.x >= size.x || dtid.y >= size.y) {
        return;
    }

    const float radius = max(1.0f, postprocess.params0.x);
    const float sigma = max(0.5f, radius * 0.5f);
    const bool horizontal = postprocess.params0.y > 0.5f;
    const int r = min(MAX_RADIUS, (int)ceil(radius));
    const int kernel = r * 2 + 1;

    // Work item 0 loads the kernel weights into shared memory in a compact form.
    if (tid.x == 0) {
        for (int i = 0; i < kernel; ++i) {
            const int offset = i - r;
            sharedLine[i] = (BLUR_FORMAT)gaussianWeight((float)offset, sigma);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    BLUR_FORMAT accum = (BLUR_FORMAT)0;
    float weightSum = 0.0f;

    for (int i = -r; i <= r; ++i) {
        int2 samplePos = int2(dtid.xy);
        if (horizontal) {
            samplePos.x = clamp(samplePos.x + i, 0, int(size.x) - 1);
        } else {
            samplePos.y = clamp(samplePos.y + i, 0, int(size.y) - 1);
        }

        const float w = gaussianWeight((float)i, sigma);
        accum += input[uint2(samplePos)] * (BLUR_FORMAT)w;
        weightSum += w;
    }

    output[dtid.xy] = accum / max(weightSum, 0.0001f);
}
