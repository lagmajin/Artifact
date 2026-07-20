#include "globals.hlsli"

Texture2D<float> shadowMap : register(t0);
Texture2D<float4> receiverData : register(t1);
RWTexture2D<float> output : register(u0);

cbuffer ShadowConstants : register(b0)
{
    uint2 imageSize;
    uint2 shadowMapSize;
    float depthBias;
    float normalBias;
};

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= imageSize.x || id.y >= imageSize.y) return;

    float4 receiver = receiverData.Load(int3(id.xy, 0));
    float2 shadowUv = receiver.xy;
    float receiverDepth = receiver.z;
    float receiverNormalTerm = saturate(receiver.w);
    float2 texel = 1.0 / float2(shadowMapSize);
    float bias = depthBias + normalBias * (1.0 - receiverNormalTerm);
    float visibility = 0.0;

    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float2 uv = shadowUv + float2(x, y) * texel;
            int2 p = clamp(int2(uv * float2(shadowMapSize)), int2(0, 0), int2(shadowMapSize) - 1);
            float blockerDepth = shadowMap.Load(int3(p, 0));
            visibility += step(receiverDepth - bias, blockerDepth);
        }
    }

    output[id.xy] = visibility / 9.0;
}
