#include "globals.hlsli"

Texture2D<float4> colorTexture : register(t0);
Texture2D<float> depthTexture : register(t1);
RWTexture2D<float4> output : register(u0);

cbuffer HeightFogConstants : register(b0)
{
    uint2 imageSize;
    float nearPlane;
    float farPlane;
    float density;
    float heightFalloff;
    float groundLevel;
    float startDistance;
    float3 fogColor;
    float _padding0;
    float3 cameraPosition;
    float _padding1;
};

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= imageSize.x || id.y >= imageSize.y) return;

    float4 source = colorTexture.Load(int3(id.xy, 0));
    float depth = depthTexture.Load(int3(id.xy, 0));
    float distance = lerp(nearPlane, farPlane, saturate(depth));
    float distanceFactor = saturate((distance - startDistance) / max(farPlane - startDistance, 0.001));

    // The depth buffer is normalized, so this pass uses camera height as the
    // stable height reference and leaves world-position reconstruction to the
    // host-specific camera path.
    float heightDensity = density * exp(-max(cameraPosition.y - groundLevel, 0.0) * heightFalloff);
    float transmittance = exp(-heightDensity * distanceFactor * max(distance, 0.0));
    float fogAmount = 1.0 - saturate(transmittance);
    output[id.xy] = float4(lerp(source.rgb, fogColor, fogAmount), source.a);
}
