#include "globals.hlsli"

Texture2D<float> depthTexture : register(t0);
Texture2D<float4> normalTexture : register(t1);
RWTexture2D<float> output : register(u0);

cbuffer CacaoConstants : register(b0)
{
    uint2 imageSize;
    float radius;
    float power;
    float bias;
    float _padding;
};

static const float2 kSamples[8] = {
    float2( 0.3535,  0.3535), float2(-0.3535,  0.3535),
    float2( 0.3535, -0.3535), float2(-0.3535, -0.3535),
    float2( 0.7071,  0.0),    float2(-0.7071,  0.0),
    float2( 0.0,    0.7071),  float2( 0.0,   -0.7071)
};

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= imageSize.x || id.y >= imageSize.y) return;

    int2 pixel = int2(id.xy);
    float centerDepth = depthTexture.Load(int3(pixel, 0));
    float3 normal = normalize(normalTexture.Load(int3(pixel, 0)).xyz * 2.0 - 1.0);
    float occlusion = 0.0;
    float validSamples = 0.0;
    float sampleRadius = max(radius, 1.0);

    [unroll]
    for (uint i = 0; i < 8; ++i) {
        int2 samplePixel = pixel + int2(round(kSamples[i] * sampleRadius));
        samplePixel = clamp(samplePixel, int2(0, 0), int2(imageSize) - 1);
        float sampleDepth = depthTexture.Load(int3(samplePixel, 0));
        float depthDelta = sampleDepth - centerDepth;
        float rangeWeight = saturate(1.0 - abs(depthDelta) / max(sampleRadius * 0.01, 0.0001));
        float depthTest = step(bias, depthDelta);
        occlusion += depthTest * rangeWeight;
        validSamples += 1.0;
    }

    // Keep the normal input active in the contract; grazing surfaces receive
    // slightly less occlusion than front-facing samples.
    float normalWeight = saturate(abs(normal.z));
    float ao = 1.0 - (occlusion / max(validSamples, 1.0)) * normalWeight;
    output[id.xy] = pow(saturate(ao), max(power, 0.001));
}
