#include \"globals.hlsli\"
#include \"lightingHF.hlsli\"

// ─── Clustered Lighting (Froxel-based) ───
// Extends tiled light culling with depth/Z slices.
// Each cluster = (tileX, tileY, depthSlice) stores a light index list.
//
// References:
//   Olsson et al. \"Clustered Deferred and Forward Shading\" (HPG 2012)

#ifndef CLUSTERED_LIGHTING_HLSLI
#define CLUSTERED_LIGHTING_HLSLI

// Cluster grid configuration
static const uint CLUSTER_TILE_SIZE_X = 64;
static const uint CLUSTER_TILE_SIZE_Y = 64;
static const uint CLUSTER_DEPTH_SLICES = 32;

// Number of tiles in X and Y
uint GetClusterTilesX() { return (GetCamera().internal_resolution.x + CLUSTER_TILE_SIZE_X - 1) / CLUSTER_TILE_SIZE_X; }
uint GetClusterTilesY() { return (GetCamera().internal_resolution.y + CLUSTER_TILE_SIZE_Y - 1) / CLUSTER_TILE_SIZE_Y; }

// ─── Depth slice mapping ───
// Uses exponential Z distribution: slice i = near * (far/near)^(i/N)
// This matches the perspective projection's non-linear depth distribution.
float ClusterDepthSliceNear(uint sliceIndex) {
    float nearZ = GetCamera().z_near;
    float farZ  = GetCamera().z_far;
    float ratio = farZ / nearZ;
    return nearZ * pow(ratio, float(sliceIndex) / float(CLUSTER_DEPTH_SLICES));
}

float ClusterDepthSliceFar(uint sliceIndex) {
    return ClusterDepthSliceNear(sliceIndex + 1);
}

// Get cluster index from pixel position + linear depth
uint3 GetClusterCoord(uint2 pixel, float linearDepth) {
    uint tileX = pixel.x / CLUSTER_TILE_SIZE_X;
    uint tileY = pixel.y / CLUSTER_TILE_SIZE_Y;

    // Find depth slice via binary search on exponential distribution
    float nearZ = GetCamera().z_near;
    float farZ  = GetCamera().z_far;
    float ratio = farZ / nearZ;
    // slice = floor(N * log(depth/near) / log(far/near))
    uint slice = uint(floor(float(CLUSTER_DEPTH_SLICES) * log(linearDepth / nearZ) / log(ratio)));
    slice = clamp(slice, 0, CLUSTER_DEPTH_SLICES - 1);

    return uint3(tileX, tileY, slice);
}

uint GetClusterIndex(uint3 coord) {
    uint tilesX = GetClusterTilesX();
    return coord.z * tilesX * GetClusterTilesY() + coord.y * tilesX + coord.x;
}

// ─── Cluster AABB (for light intersection) ───
struct ClusterAABB {
    float3 minPoint;
    float3 maxPoint;
};

ClusterAABB ComputeClusterAABB(uint3 coord) {
    ClusterAABB aabb;
    float nearZ = ClusterDepthSliceNear(coord.z);
    float farZ  = ClusterDepthSliceFar(coord.z);

    // Reconstruct frustum corners in view space
    float2 tileMin = float2(float(coord.x * CLUSTER_TILE_SIZE_X) / GetCamera().internal_resolution.x,
                            float(coord.y * CLUSTER_TILE_SIZE_Y) / GetCamera().internal_resolution.y);
    float2 tileMax = float2(float((coord.x + 1) * CLUSTER_TILE_SIZE_X) / GetCamera().internal_resolution.x,
                            float((coord.y + 1) * CLUSTER_TILE_SIZE_Y) / GetCamera().internal_resolution.y);

    // Convert from NDC to view space (assuming reverse-Z)
    float2 tanHalfFOV = GetCamera().tan_half_fov;
    aabb.minPoint = float3((tileMin * 2.0f - 1.0f) * tanHalfFOV * nearZ, nearZ);
    aabb.maxPoint = float3((tileMax * 2.0f - 1.0f) * tanHalfFOV * farZ, farZ);
    return aabb;
}

// ─── Light-Cluster intersection test ───
bool LightIntersectsCluster(ShaderEntity light, ClusterAABB aabb)
{
    float3 lightPos = light.position;
    float radius = light.range;

    // Sphere-AABB intersection (simplified)
    float3 closest = clamp(lightPos, aabb.minPoint, aabb.maxPoint);
    float3 diff = lightPos - closest;
    return dot(diff, diff) <= radius * radius;
}

#endif // CLUSTERED_LIGHTING_HLSLI
