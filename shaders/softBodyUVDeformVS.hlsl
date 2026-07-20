#include "globals.hlsli"

struct SoftBodyUVVertex
{
    float2 position;
    float2 uv;
};

StructuredBuffer<SoftBodyUVVertex> softBodyVertices : register(t0);

cbuffer SoftBodyDeformConstants : register(b0)
{
    float4x4 localToClip;
    uint vertexCount;
    uint _padding0;
    uint _padding1;
    uint _padding2;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;
    const uint safeId = min(vertexId, max(vertexCount, 1u) - 1u);
    const SoftBodyUVVertex vertex = softBodyVertices[safeId];
    output.position = mul(localToClip, float4(vertex.position, 0.0, 1.0));
    output.uv = vertex.uv;
    return output;
}
