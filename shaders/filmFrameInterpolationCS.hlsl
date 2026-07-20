#include "globals.hlsli"

Texture2D<float4> previousFrame : register(t0);
Texture2D<float4> nextFrame : register(t1);
Texture2D<float2> previousMotion : register(t2);
Texture2D<float2> nextMotion : register(t3);
RWTexture2D<float4> output : register(u0);

cbuffer FilmInterpolationConstants : register(b0)
{
    uint2 imageSize;
    float blend;
    float motionScale;
    float confidence;
    float _padding;
};

float4 sampleFrame(Texture2D<float4> frame, float2 uv)
{
    return frame.SampleLevel(sampler_linear_clamp, uv, 0);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= imageSize.x || id.y >= imageSize.y) return;
    float2 uv = (float2(id.xy) + 0.5) / float2(imageSize);
    float2 previousOffset = previousMotion.Load(int3(id.xy, 0)) * motionScale;
    float2 nextOffset = nextMotion.Load(int3(id.xy, 0)) * motionScale;
    float4 a = sampleFrame(previousFrame, uv - previousOffset * blend);
    float4 b = sampleFrame(nextFrame, uv + nextOffset * (1.0 - blend));
    float motionConfidence = saturate(confidence);
    output[id.xy] = lerp(lerp(previousFrame.Load(int3(id.xy, 0)),
                              nextFrame.Load(int3(id.xy, 0)), blend),
                         lerp(a, b, blend), motionConfidence);
}
