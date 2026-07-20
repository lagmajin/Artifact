#include "globals.hlsli"

Texture2D<float4> input : register(t0);
RWTexture2D<float4> output : register(u0);

cbuffer DualKawaseConstants : register(b0)
{
    uint2 inputSize;
    uint2 outputSize;
    float offset;
    float intensity;
};

float4 sampleClamped(float2 uv)
{
    return input.SampleLevel(sampler_linear_clamp, uv, 0);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= outputSize.x || id.y >= outputSize.y) return;
    float2 texel = 1.0 / float2(inputSize);
    float2 uv = (float2(id.xy) + 0.5) / float2(outputSize);
    float2 d = texel * max(offset, 0.5);
    float4 sum = 0.0;
    sum += sampleClamped(uv + float2(-d.x, -d.y));
    sum += sampleClamped(uv + float2( d.x, -d.y));
    sum += sampleClamped(uv + float2(-d.x,  d.y));
    sum += sampleClamped(uv + float2( d.x,  d.y));
    output[id.xy] = sum * (0.25 * max(intensity, 0.0));
}
