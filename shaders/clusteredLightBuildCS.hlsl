#include \"globals.hlsli\"
#include \"lightingHF.hlsli\"
#include \"clusteredLighting.hlsli\"

// ─── Clustered Light Build Compute Shader ───
// Dispatched after light culling, before shading.
// For each cluster: tests all active lights, writes light indices.

RWStructuredBuffer<uint> g_clusterLightIndices : register(u4);
RWStructuredBuffer<uint> g_clusterLightGrid   : register(u5);

// Grid structure: [clusterCount * 2] uint2 per cluster: (offset, count)
// Light indices stored in g_clusterLightIndices at offset.

[numthreads(CLUSTER_TILE_SIZE_X / 4, CLUSTER_TILE_SIZE_Y / 4, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint tilesX = GetClusterTilesX();
    uint tilesY = GetClusterTilesY();

    if (DTid.x >= tilesX || DTid.y >= tilesY) return;

    // Process all depth slices for this tile
    for (uint slice = 0; slice < CLUSTER_DEPTH_SLICES; ++slice) {
        uint3 coord = uint3(DTid.x, DTid.y, slice);
        uint clusterIdx = GetClusterIndex(coord);
        ClusterAABB aabb = ComputeClusterAABB(coord);

        // Count lights in this cluster
        uint lightCount = 0;
        uint baseOffset = clusterIdx * MAX_LIGHTS_PER_CLUSTER;

        // Iterate all lights
        uint totalLights = GetFrame().light_count;
        for (uint i = 0; i < totalLights && lightCount < MAX_LIGHTS_PER_CLUSTER; ++i) {
            ShaderEntity light = load_entity(i);
            if (light.GetEntityType() != ENTITY_TYPE_DIRECTIONALLIGHT &&
                light.GetEntityType() != ENTITY_TYPE_POINTLIGHT &&
                light.GetEntityType() != ENTITY_TYPE_SPOTLIGHT) continue;

            if (LightIntersectsCluster(light, aabb)) {
                g_clusterLightIndices[baseOffset + lightCount] = i;
                ++lightCount;
            }
        }

        // Write grid header
        uint gridBase = clusterIdx * 2;
        g_clusterLightGrid[gridBase] = baseOffset;
        g_clusterLightGrid[gridBase + 1] = lightCount;
    }
}
