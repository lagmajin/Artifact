// Generated from App/shaders/experimental_contour_posterCS.hlsl
// SHA-256: 56dda40dfa0dabd032cf8d72f992d2129ddf96d290bf179fbefa9e515b576e41
// Generic resident prelude supplies g_Width, g_Height, g_P0..g_P3.
#pragma once

static constexpr const char kExperimentalContourPosterResidentShader[] = R"CONTOUR(
// Experimental research candidate. Registered through the generic resident effect path.
// Input/output: float BGRA values in linear premultiplied color space.
// Host runtime behavior, color descriptors and GPU parity remain under evaluation.
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);



float4 LoadLogical(int2 p)
{
    p = clamp(p, int2(0, 0), int2((int)g_Width - 1, (int)g_Height - 1));
    return g_InputTexture.Load(int3(p, 0)).bgra;
}

// Straight luminance is undefined for fully transparent pixels.  Ignore their
// color in neighboring derivatives and preserve the source at the center.
float WeightedDomainDifference(float4 neighbor, float centerDomain)
{
    if (neighbor.a <= g_P1) return 0.0;
    float3 straight = neighbor.rgb / neighbor.a;
    float luminance = dot(straight, float3(0.2126, 0.7152, 0.0722));
    float domain = sqrt(max(luminance, 0.0));
    float confidence = smoothstep(g_P1, 2.0 * g_P1, neighbor.a);
    return neighbor.a * confidence * (domain - centerDomain);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_Width || id.y >= g_Height) return;
    const int2 p = int2(id.xy);
    float4 center = LoadLogical(p);
    if (g_P1 <= 0.0 || center.a <= g_P1 || g_P0 <= 0.0) {
        g_OutputTexture[id.xy] = center.bgra;
        return;
    }

    float3 straight = center.rgb / center.a;
    float luminance = dot(straight, float3(0.2126, 0.7152, 0.0722));
    float domain = sqrt(max(luminance, 0.0));
    float quantizedDomain = floor(domain * g_P0 + 0.5) / g_P0;
    float quantizedLuminance = quantizedDomain * quantizedDomain;
    float ratio = luminance > 1.0e-8 ? quantizedLuminance / luminance : 0.0;

    // Fixed denominator keeps the contribution of a near-transparent
    // neighbor bounded instead of normalizing it up to full strength.
    float gx = 0.5 * (WeightedDomainDifference(LoadLogical(p + int2(1, 0)), domain)
                    - WeightedDomainDifference(LoadLogical(p - int2(1, 0)), domain));
    float gy = 0.5 * (WeightedDomainDifference(LoadLogical(p + int2(0, 1)), domain)
                    - WeightedDomainDifference(LoadLogical(p - int2(0, 1)), domain));
    float gradient = length(float2(gx, gy));
    float phase = abs(frac(domain * g_P0 + 0.5) - 0.5);
    float distance = gradient > 1.0e-6 ? (phase / g_P0) / gradient : 1.0e20;
    float lineStrength = saturate((g_P3 - distance) / (g_P3 * 0.5));
    float3 outputStraight = straight * ratio * (1.0 - g_P2 * lineStrength);
    float confidence = smoothstep(g_P1, 2.0 * g_P1, center.a);
    float3 outputPremultiplied = lerp(center.rgb, outputStraight * center.a, confidence);
    g_OutputTexture[id.xy] = float4(outputPremultiplied, center.a).bgra;
}

)CONTOUR";
