#include "globals.hlsli"

Texture2D<float4> input : register(t0);
RWTexture2D<float4> output : register(u0);

cbuffer GaussianBlurConstants : register(b0)
{
    uint2 imageSize;
    float radius;
    float sigma;
};

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= imageSize.x || id.y >= imageSize.y) return;

    int r = clamp((int)round(radius), 1, 16);
    float safeSigma = max(sigma, 0.001);
    float4 sum = 0.0;
    float weightSum = 0.0;
    for (int offset = -r; offset <= r; ++offset) {
        float x = (float)offset;
        float weight = exp(-(x * x) / (2.0 * safeSigma * safeSigma));
        int2 p = clamp(int2(id.xy) + int2(offset, 0), int2(0, 0), int2(imageSize) - 1);
        sum += input.Load(int3(p, 0)) * weight;
        weightSum += weight;
    }
    output[id.xy] = sum / max(weightSum, 0.0001);
}
