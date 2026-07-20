struct ArtifactRayPayload
{
    float3 radiance;
    float hitDistance;
    uint hit;
};

struct ArtifactRayAttributes
{
    float2 barycentrics : SV_IntersectionAttributes;
};
