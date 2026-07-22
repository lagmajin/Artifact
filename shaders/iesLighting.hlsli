#include \"lightingHF.hlsli\"

// ─── IES Light Profile Sampling ───
// Uses a 2D LUT texture (256x128 RGBA16F) pre-built from .ies data.
// The texture maps vertical angle (0-180) to U and horizontal angle (0-360) to V.
// Sampled intensity modulates the light's attenuation.

Texture2D g_iesLutTexture : register(t30);
SamplerState g_iesLutSampler : register(s30);

float SampleIESProfile(float3 lightDir, float3 lightForward)
{
    // Compute angles relative to light's forward direction
    float3 lDir = normalize(lightDir);
    float3 lFwd = normalize(lightForward);

    // Vertical angle: angle from light forward (0 = forward, 180 = backward)
    float cosVert = dot(lDir, lFwd);
    float vertAngle = acos(cosVert) * (180.0f / M_PI);  // 0..180

    // Horizontal angle: azimuth around light forward
    float3 right = abs(lFwd.y) < 0.999f ? normalize(cross(float3(0,1,0), lFwd)) : float3(1,0,0);
    float3 up = cross(lFwd, right);
    float horizAngle = atan2(dot(lDir, up), dot(lDir, right));
    horizAngle = degrees(horizAngle);
    if (horizAngle < 0.0f) horizAngle += 360.0f;

    // Sample LUT
    float u = vertAngle / 180.0f;    // normalize to 0..1
    float v = horizAngle / 360.0f;   // normalize to 0..1
    float intensity = g_iesLutTexture.SampleLevel(g_iesLutSampler, float2(u, v), 0).r;
    return intensity;
}
