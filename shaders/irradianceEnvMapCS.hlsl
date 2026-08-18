#include "globals.hlsli"
#include "ShaderInterop_Renderer.h"

// Diffuse image-based lighting convolution. The dispatch writes one face of
// a low-resolution irradiance cubemap per z slice. It intentionally shares
// FilterEnvmapPushConstants with the existing prefilter pass so both passes
// can use the same resource setup and resolution contract.
PUSHCONSTANT(push, FilterEnvmapPushConstants);

static const uint IRRADIANCE_SAMPLES = 64;

float3 cosineSampleHemisphere(float2 xi)
{
    const float phi = 2.0 * PI * xi.x;
    const float radius = sqrt(xi.y);
    const float x = radius * cos(phi);
    const float y = sqrt(max(0.0, 1.0 - xi.y));
    const float z = radius * sin(phi);
    return float3(x, y, z);
}

float3 tangentToWorld(float3 sampleDirection, float3 normal)
{
    float3 up = abs(normal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    return tangent * sampleDirection.x + normal * sampleDirection.y +
           bitangent * sampleDirection.z;
}

[numthreads(GENERATEMIPCHAIN_2D_BLOCK_SIZE, GENERATEMIPCHAIN_2D_BLOCK_SIZE, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= push.filterResolution.x || DTid.y >= push.filterResolution.y)
        return;

    TextureCube input = bindless_cubemaps[push.texture_input];
    RWTexture2DArray<float4> output = bindless_rwtextures2DArray[push.texture_output];
    float2 uv = (float2(DTid.xy) + 0.5) * push.filterResolution_rcp.xy;
    float3 normal = normalize(uv_to_cubemap(uv, DTid.z));
    float3 irradiance = 0.0;

    for (uint sampleIndex = 0; sampleIndex < IRRADIANCE_SAMPLES; ++sampleIndex)
    {
        float2 xi = hammersley2d(sampleIndex, IRRADIANCE_SAMPLES);
        float3 localDirection = cosineSampleHemisphere(xi);
        float3 worldDirection = tangentToWorld(localDirection, normal);
        irradiance += input.SampleLevel(sampler_linear_clamp, worldDirection, 0).rgb;
    }

    irradiance *= PI / float(IRRADIANCE_SAMPLES);
    output[uint3(DTid.xy, DTid.z)] = float4(max(irradiance, 0.0), 1.0);
}
