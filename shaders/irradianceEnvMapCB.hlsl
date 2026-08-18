#include "globals.hlsli"

struct EnvironmentConvolutionConstants
{
    uint2 resolution;
    float2 resolutionRcp;
    uint sampleCount;
    uint inputTextureIndex;
    uint outputTextureIndex;
    uint padding;
};

ConstantBuffer<EnvironmentConvolutionConstants> g_EnvironmentConvolution : register(b0);
TextureCube<float4> g_InputEnvironment : register(t0);
RWTexture2DArray<float4> g_OutputIrradiance : register(u0);
SamplerState g_EnvironmentSampler : register(s0);

static const float ENVIRONMENT_PI = 3.14159265359;

float2 radicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float3 cosineHemisphere(float2 xi)
{
    float phi = 2.0 * ENVIRONMENT_PI * xi.x;
    float radius = sqrt(xi.y);
    return float3(radius * cos(phi), sqrt(max(0.0, 1.0 - xi.y)),
                  radius * sin(phi));
}

float3 tangentToWorld(float3 localDirection, float3 normal)
{
    float3 up = abs(normal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    return tangent * localDirection.x + normal * localDirection.y +
           bitangent * localDirection.z;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_EnvironmentConvolution.resolution.x ||
        dispatchId.y >= g_EnvironmentConvolution.resolution.y)
        return;

    float2 uv = (float2(dispatchId.xy) + 0.5) *
                g_EnvironmentConvolution.resolutionRcp;
    float3 normal = normalize(uv_to_cubemap(uv, dispatchId.z));
    uint sampleCount = max(g_EnvironmentConvolution.sampleCount, 1u);
    float3 result = 0.0;
    for (uint sample = 0; sample < sampleCount; ++sample)
    {
        float2 xi = float2((float(sample) + 0.5) / float(sampleCount),
                           radicalInverseVdC(sample));
        result += g_InputEnvironment.SampleLevel(
            g_EnvironmentSampler,
            tangentToWorld(cosineHemisphere(xi), normal), 0).rgb;
    }
    g_OutputIrradiance[uint3(dispatchId.xy, dispatchId.z)] =
        float4(result * ENVIRONMENT_PI / float(sampleCount), 1.0);
}
