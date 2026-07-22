#include \"brdf.hlsli\"

// ─── Separable Subsurface Scattering (SSS) ───
// Based on Jimenez et al. \"Separable Subsurface Scattering\"
// and Disney's Burley diffusion profile.
//
// Usage: Call computeSSS() in the lighting pass to modulate
// diffuse lighting with subsurface scattering.

// Burley normalized diffusion profile
// d = distance in world units, radius = scatter radius in mm
float BurleyDiffusionProfile(float d, float radius)
{
    // Clamp d/radius to avoid singularity at 0
    float r = max(d / max(radius, 1e-5f), 1e-5f);

    // Burley (Disney) normalized diffusion:
    // R(r) = (exp(-r) + exp(-r/3)) / (8 * PI * r)
    float exp1 = exp(-r);
    float exp3 = exp(-r / 3.0f);
    return (exp1 + exp3) / (8.0f * M_PI * r);
}

// Pre-integrated falloff for separable SSS
// Samples the diffusion profile at kernel offsets and returns weights
struct SSSProfile
{
    float3 strength;        // per-channel scatter strength (RGB, 0-1)
    float   radius;         // scatter radius in mm
    float   maxRadiusMM;    // max effective radius (clamped for performance)

    // Kernel weight for a given distance
    float3 weight(float d) {
        float w = BurleyDiffusionProfile(d, radius);
        return strength * w;
    }
};

// Default skin profile
SSSProfile SkinProfile()
{
    SSSProfile p;
    p.strength = float3(0.45f, 0.25f, 0.15f); // R > G > B
    p.radius = 3.5f;  // mm
    p.maxRadiusMM = 10.0f;
    return p;
}

SSSProfile WaxProfile()
{
    SSSProfile p;
    p.strength = float3(0.8f, 0.7f, 0.5f);
    p.radius = 8.0f;
    p.maxRadiusMM = 25.0f;
    return p;
}

SSSProfile GenericProfile()
{
    SSSProfile p;
    p.strength = float3(0.3f, 0.3f, 0.3f);
    p.radius = 2.0f;
    p.maxRadiusMM = 5.0f;
    return p;
}

// Compute SSS contribution for a surface point given pre-blurred irradiance.
// Call from pixel shader:
//   float3 sssColor = burleySSS * ComputeSSS(profile, NdotL, shadow);
float3 ComputeSSS(SSSProfile profile, float NdotL, float shadow)
{
    // Wrap lighting for subsurface: light wraps around the surface
    float wrap = saturate((NdotL + profile.strength.r) / (1.0f + profile.strength.r));

    // Back-scattering contribution (simplified)
    float backScatter = saturate((-NdotL + profile.strength.g) / (1.0f + profile.strength.g));

    // Mix based on profile strength
    float3 front = wrap * shadow * profile.strength;
    float3 back  = backScatter * (1.0f - shadow) * profile.strength * 0.5f;

    return front + back;
}
