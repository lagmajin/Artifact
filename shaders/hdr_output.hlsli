#include \"globals.hlsli\"

// ─── HDR Output Transfer Functions ───
// Converts linear scene-referred color to display-referred:
//   PQ  (ST.2084)  → HDR10 / Dolby Vision
//   HLG (ARIB B67) → HDR broadcast
//   scRGB          → Linear HDR (pass-through for 16-bit float swapchain)

static const float PQ_m1 = 2610.0f / 16384.0f;   // 0.1593...
static const float PQ_m2 = 2523.0f / 4096.0f * 128.0f;  // 78.84375
static const float PQ_c1 = 3424.0f / 4096.0f;    // 0.8359...
static const float PQ_c2 = 2413.0f / 4096.0f * 32.0f;   // 18.8515...
static const float PQ_c3 = 2392.0f / 4096.0f * 32.0f;   // 18.6875...

// PQ (ST.2084) - Perceptual Quantizer
// Input:  linear luminance [0, 10000] nits
// Output: perceptual signal [0, 1]
float3 LinearToPQ(float3 linearColor, float peakNits)
{
    // Normalize to 10000 nits reference
    float3 n = linearColor * (10000.0f / peakNits);

    float3 n_m1 = pow(abs(n), PQ_m1);
    float3 result = pow((PQ_c1 + PQ_c2 * n_m1) / (1.0f + PQ_c3 * n_m1), PQ_m2);
    return result;
}

float3 PQToLinear(float3 pqColor)
{
    float3 pq_m2_inv = pow(abs(pqColor), 1.0f / PQ_m2);
    float3 numerator = max(pq_m2_inv - PQ_c1, 0.0f);
    float3 denominator = max(PQ_c2 - PQ_c3 * pq_m2_inv, 1e-10f);
    return pow(numerator / denominator, 1.0f / PQ_m1);
}

// HLG (Hybrid Log-Gamma, ARIB STD-B67)
// Input:  linear scene light [0, 12] 
// Output: display signal [0, 1]
float3 LinearToHLG(float3 linearColor)
{
    static const float a = 0.17883277f;
    static const float b = 1.0f - 4.0f * a;
    static const float c = 0.5f - a * log(4.0f * a);

    float3 result;
    for (int i = 0; i < 3; ++i) {
        float L = linearColor[i];
        if (L <= 1.0f / 12.0f) {
            result[i] = sqrt(3.0f * L);
        } else {
            result[i] = a * log(12.0f * L - b) + c;
        }
    }
    return result;
}

// scRGB - Linear pass-through for 16-bit float swapchain
// No encoding needed; just pass linear values through.
float3 LinearToscRGB(float3 linearColor)
{
    return linearColor;
}

// ─── HDR Tonemap + Output ───

enum HDRDisplayMode
{
    HDR_DISPLAY_SDR         = 0,
    HDR_DISPLAY_HDR10_PQ    = 1,
    HDR_DISPLAY_HLG         = 2,
    HDR_DISPLAY_scRGB       = 3,
};

struct HDRDisplayParams
{
    int   displayMode;      // HDRDisplayMode
    float peakNits;         // Display peak brightness (e.g. 1000, 4000)
    float paperWhiteNits;   // SDR reference white in nits (e.g. 80, 200)
    float saturationBoost;  // 1.0 = neutral
};

// Apply gamut expansion for HDR (preserves hue, expands luminance + saturation)
float3 ExpandGamutForHDR(float3 color, float3 luminance, float boost)
{
    float lum = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float3 hdrColor = lerp(float3(lum, lum, lum), color, boost);
    return hdrColor;
}

// Main HDR output function - call at end of post-processing, before present
float3 ApplyHDRDisplay(float3 linearSceneColor, HDRDisplayParams params)
{
    // Map scene linear to display linear (SDR range remapping)
    // Scene values > 1.0 represent HDR content
    float3 displayLinear = linearSceneColor / params.paperWhiteNits * 80.0f;

    // Optional gamut expansion for HDR modes
    if (params.displayMode != HDR_DISPLAY_SDR) {
        displayLinear = ExpandGamutForHDR(displayLinear, displayLinear, params.saturationBoost);
    }

    switch (params.displayMode) {
    case HDR_DISPLAY_HDR10_PQ:
        return LinearToPQ(displayLinear, params.peakNits);
    case HDR_DISPLAY_HLG:
        return LinearToHLG(displayLinear);
    case HDR_DISPLAY_scRGB:
        return LinearToscRGB(displayLinear);
    default: // SDR
        return displayLinear; // Should go through sRGB EOTF upstream
    }
}
