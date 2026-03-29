// ACES Color Management — GPU Shader Constants
// Used by the composition renderer for on-the-fly ACES conversion

#ifndef SHADERINTEROP_ACES_H
#define SHADERINTEROP_ACES_H

// ACES working space options
#define ACES_SPACE_ACESSCG  0   // AP1 Linear (recommended)
#define ACES_SPACE_ACESCT   1   // AP1 log (editing friendly)

// ACES output transform types
#define ACES_OUTPUT_SDR_SRGB       0
#define ACES_OUTPUT_SDR_REC709     1
#define ACES_OUTPUT_SDR_P3_D65     2
#define ACES_OUTPUT_HDR_REC2020_PQ 3
#define ACES_OUTPUT_HDR_P3_D65_HLG 4

// ACES constant buffer for compute/vertex shaders
struct ACESParams
{
    // Input → Working space conversion (3x3 matrix, row-major)
    float3x3 inputToWorking;

    // Working → Output space conversion (3x3 matrix, row-major)
    float3x3 workingToOutput;

    // RRT params (simplified filmic curve)
    float rrtSlope;       // 2.51
    float rrtOffset;      // 0.03
    float rrtShoulder;    // 2.43
    float rrtToe;         // 0.59
    float rrtKnee;        // 0.14

    // Flags
    int workingSpace;     // ACES_SPACE_ACESSCG or ACES_SPACE_ACESCT
    int outputTransform;  // ACES_OUTPUT_* enum
    int applyRRT;         // 1 = apply simplified RRT, 0 = skip
    int applyOETF;        // 1 = apply output transfer function, 0 = skip (linear output)

    // OETF parameters (gamma for sRGB/Rec.709, or PQ/HLG curve params)
    float oetfGamma;      // 2.2 for sRGB, 2.4 for Rec.709
    float oetfLinearScale; // scale for linear segment of sRGB EOTF
    float padding1;
    float padding2;
};

// ACES RRT (simplified filmic curve) — same as C++ applySimpleRRT
float3 acesSimpleRRT(float3 x, ACESParams params)
{
    // (x * (slope * x + offset)) / (x * (shoulder * x + toe) + knee)
    float3 num = x * (params.rrtSlope * x + params.rrtOffset);
    float3 den = x * (params.rrtShoulder * x + params.rrtToe) + params.rrtKnee;
    return max(num / max(den, 1e-6), 0.0);
}

// sRGB OETF (linear → encoded)
float3 acesSRGBEncode(float3 linear)
{
    // IEC 61966-2-1
    float3 c = max(linear, 0.0);
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow(c, 1.0 / 2.4) - 0.055;
    return (c < 0.0031308) ? lo : hi;
}

// PQ OETF (linear → PQ encoded) for HDR output
float3 acesPQEncode(float3 linear)
{
    // Rec.2100 PQ (SMPTE ST 2084)
    float m1 = 0.1593017578125;
    float m2 = 78.84375;
    float c1 = 0.8359375;
    float c2 = 18.8515625;
    float c3 = 18.6875;

    float3 p = pow(abs(linear) / 10000.0, m1);
    float3 num = c1 + c2 * p;
    float3 den = 1.0 + c3 * p;
    return pow(num / den, m2);
}

// HLG OETF (linear → HLG encoded) for HDR output
float3 acesHLGEncode(float3 linear)
{
    // ARIB STD-B67 (HLG)
    float a = 0.17883277;
    float b = 0.28466892;
    float c = 0.55991073;

    float3 clamped = max(linear, 0.0);
    float3 lo = sqrt(3.0 * clamped);
    float3 hi = a * log(12.0 * clamped - b) + c;
    return (clamped <= (1.0 / 12.0)) ? lo : hi;
}

// Full ACES output transform (GPU version)
float3 applyACESOutput(float3 workingRGB, ACESParams params)
{
    float3 out = workingRGB;

    // 1. Gamut conversion
    out = mul(out, params.workingToOutput);

    // 2. RRT
    if (params.applyRRT) {
        out = acesSimpleRRT(out, params);
    }

    // 3. OETF
    if (params.applyOETF) {
        switch (params.outputTransform) {
            case ACES_OUTPUT_SDR_SRGB:
            case ACES_OUTPUT_SDR_P3_D65:
                out = acesSRGBEncode(out);
                break;
            case ACES_OUTPUT_SDR_REC709:
                out = acesSRGBEncode(out); // Rec.709 は近似的に sRGB と同等
                break;
            case ACES_OUTPUT_HDR_REC2020_PQ:
                out = acesPQEncode(out);
                break;
            case ACES_OUTPUT_HDR_P3_D65_HLG:
                out = acesHLGEncode(out);
                break;
        }
    }

    return out;
}

#endif // SHADERINTEROP_ACES_H
