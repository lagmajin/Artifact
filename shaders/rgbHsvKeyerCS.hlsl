#include "globals.hlsli"

Texture2D<float4> input : register(t0);
RWTexture2D<float4> output : register(u0);

cbuffer KeyerConstants : register(b0)
{
    uint2 imageSize;
    float3 keyColor;
    float keyHueRange;
    float keySaturationMin;
    float keyValueMin;
    float softness;
    float spillSuppression;
    float _padding;
};

float3 rgbToHsv(float3 c)
{
    float4 k = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(c.bg, k.wz), float4(c.gb, k.xy), step(c.b, c.g));
    float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-6;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float hueDistance(float a, float b)
{
    float d = abs(a - b);
    return min(d, 1.0 - d);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= imageSize.x || id.y >= imageSize.y) return;
    float4 source = input.Load(int3(id.xy, 0));
    float3 hsv = rgbToHsv(saturate(source.rgb));
    float hue = hueDistance(hsv.x, rgbToHsv(saturate(keyColor)).x);
    float hueLimit = max(keyHueRange, 1.0e-5);
    float hueMatch = 1.0 - smoothstep(hueLimit, hueLimit + max(softness, 0.0), hue);
    float satMatch = smoothstep(keySaturationMin, keySaturationMin + max(softness, 0.0), hsv.y);
    float valueMatch = smoothstep(keyValueMin, keyValueMin + max(softness, 0.0), hsv.z);
    float matte = saturate(hueMatch * satMatch * valueMatch);

    float3 color = source.rgb;
    color.g = lerp(color.g, min(color.g, max(color.r, color.b)),
                   saturate(spillSuppression) * (1.0 - matte));
    output[id.xy] = float4(color, source.a * (1.0 - matte));
}
