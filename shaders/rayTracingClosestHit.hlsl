#include "globals.hlsli"
#include "rayTracingMinimalPayload.hlsli"

StructuredBuffer<float3> vertexPositions : register(t0);

[shader("closesthit")]
void main(inout ArtifactRayPayload payload, in ArtifactRayAttributes attributes)
{
    payload.radiance = float3(attributes.barycentrics, 1.0 - attributes.barycentrics.x - attributes.barycentrics.y);
    payload.hitDistance = RayTCurrent();
    payload.hit = 1;
}
