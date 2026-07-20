#include "globals.hlsli"

Texture2D<float4> baseColorTexture : register(t0);
Texture2D<float4> normalTexture : register(t1);

cbuffer ToonLightingConstants : register(b0)
{
    float3 lightDirection;
    float _padding0;
    float3 lightColor;
    float _padding1;
    float3 shadowColor;
    float bandCount;
    float rimPower;
    float rimStrength;
    float outlineThreshold;
    float _padding2;
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float3 main(PSInput input) : SV_Target
{
    float4 base = baseColorTexture.Sample(sampler_linear_clamp, input.uv);
    float3 normal = normalize(normalTexture.Sample(sampler_linear_clamp, input.uv).xyz * 2.0 - 1.0);
    float3 light = normalize(-lightDirection);
    float ndotl = saturate(dot(normal, light));
    float bands = max(bandCount, 2.0);
    float quantized = floor(ndotl * bands) / (bands - 1.0);
    float3 toonColor = lerp(shadowColor, base.rgb * lightColor, saturate(quantized));
    float rim = pow(saturate(1.0 - abs(normal.z)), max(rimPower, 0.001)) * rimStrength;
    return toonColor + rim * lightColor;
}
