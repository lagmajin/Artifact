#ifndef ARTIFACT_IBL_HLSLI
#define ARTIFACT_IBL_HLSLI

// Shared image-based lighting helpers. The caller supplies the environment
// textures and samplers so this include remains usable by both the viewport
// shader and the future composition PBR path.

static const float ARTIFACT_PI = 3.14159265359;

float3 artifactFresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - saturate(cosTheta), 5.0);
}

float2 artifactIntegrateBRDF(float NdotV, float roughness,
                             Texture2D brdfLut, SamplerState linearClamp)
{
    return brdfLut.SampleLevel(linearClamp,
                               float2(saturate(NdotV), saturate(roughness)),
                               0.0).rg;
}

float3 artifactDiffuseIBL(TextureCube irradianceMap, SamplerState linearClamp,
                          float3 normal)
{
    return irradianceMap.SampleLevel(linearClamp, normalize(normal), 0.0).rgb;
}

float3 artifactSpecularIBL(TextureCube prefilteredEnvironment,
                           Texture2D brdfLut, SamplerState linearClamp,
                           float3 normal, float3 viewDirection,
                           float roughness, float3 F0)
{
    float3 reflectionDirection = reflect(-normalize(viewDirection),
                                         normalize(normal));
    uint width = 1;
    uint height = 1;
    uint mipCount = 1;
    prefilteredEnvironment.GetDimensions(0, width, height, mipCount);
    float lod = saturate(roughness) * max(0.0, float(mipCount) - 1.0);
    float3 prefiltered = prefilteredEnvironment.SampleLevel(
        linearClamp, normalize(reflectionDirection), lod).rgb;
    float2 brdf = artifactIntegrateBRDF(
        saturate(dot(normalize(normal), normalize(viewDirection))),
        saturate(roughness), brdfLut, linearClamp);
    float3 fresnel = artifactFresnelSchlick(
        dot(normalize(normal), normalize(viewDirection)), F0);
    return prefiltered * (fresnel * brdf.x + brdf.y);
}

#endif
