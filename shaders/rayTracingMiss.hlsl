#include "globals.hlsli"
#include "rayTracingMinimalPayload.hlsli"

cbuffer RayTracingEnvironment : register(b0)
{
    float3 skyColor;
    float _padding;
};

[shader("miss")]
void main(inout ArtifactRayPayload payload)
{
    payload.radiance = skyColor;
    payload.hitDistance = -1.0;
    payload.hit = 0;
}
