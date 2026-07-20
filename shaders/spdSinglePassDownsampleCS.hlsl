#include "globals.hlsli"

// SPD-inspired single-pass reduction building block.
// The full FidelityFX SPD dispatch can generate a complete mip chain from one
// dispatch. This pass keeps the same reduction shape while producing one
// explicitly bound destination level, making it safe to integrate first.

Texture2D<float4> input : register(t0);
RWTexture2D<float4> output : register(u0);

cbuffer SPDConstants : register(b0)
{
    uint2 inputSize;
    uint2 outputSize;
    uint sourceMip;
    uint _padding;
};

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= outputSize.x || id.y >= outputSize.y) return;

    uint2 base = id.xy * 2;
    uint2 maxPixel = max(inputSize, uint2(1, 1)) - 1;
    uint2 p00 = min(base + uint2(0, 0), maxPixel);
    uint2 p10 = min(base + uint2(1, 0), maxPixel);
    uint2 p01 = min(base + uint2(0, 1), maxPixel);
    uint2 p11 = min(base + uint2(1, 1), maxPixel);

    float4 value = input.Load(int3(p00, sourceMip));
    value += input.Load(int3(p10, sourceMip));
    value += input.Load(int3(p01, sourceMip));
    value += input.Load(int3(p11, sourceMip));
    output[id.xy] = value * 0.25;
}
