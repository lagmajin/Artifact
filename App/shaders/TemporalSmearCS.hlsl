#include "globals.hlsli"

// --- Constant Buffer ---
cbuffer TemporalSmearParams : register(b0)
{
    float2 velocityScale;   // scale velocity vector (1.0 = raw)
    float  smearAmount;     // 0.0 = no smear, 1.0 = full velocity length
    int    sampleCount;     // number of samples along velocity (2–16)
    float  sampleJitter;    // 0.0 = no jitter, 1.0 = max jitter
};

// --- Resources ---
Texture2D<float4> inputColor  : register(t0);  // current frame RGBA
Texture2D<float2> inputVelocity : register(t1); // velocity in pixel/frame (RG)
RWTexture2D<float4> outputColor : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 dim;
    inputColor.GetDimensions(dim.x, dim.y);
    if (DTid.x >= dim.x || DTid.y >= dim.y)
        return;

    float2 uv = (float2(DTid.xy) + 0.5) / float2(dim);
    float2 vel = inputVelocity.SampleLevel(pointClamp, uv, 0) * velocityScale;

    // Clamp velocity to reasonable range
    vel = clamp(vel, -64.0, 64.0);

    float4 accum = inputColor[DTid.xy];
    float  totalWeight = 1.0;

    if (smearAmount > 0.001 && sampleCount > 0)
    {
        // Strand-based random jitter each sample to hide banding
        uint seed = DTid.x * 0x1f1f1f1fu + DTid.y * 0x2d7b2d7bu;

        for (int i = 1; i <= sampleCount; ++i)
        {
            float t = (float(i) / float(sampleCount)) * smearAmount;
            float jitter = 0.0;
            if (sampleJitter > 0.0)
            {
                // Simple LCG noise
                seed = seed * 1103515245u + 12345u;
                jitter = float(seed & 0x7fffffffu) / float(0x80000000u);
                jitter = (jitter - 0.5) * 2.0 * sampleJitter * 0.5;
                t += jitter;
            }
            t = clamp(t, -1.0, 1.0);

            float2 samplePos = float2(DTid.xy) + vel * float(sampleCount) * t;
            int2   sampleCoord = int2(round(samplePos));
            sampleCoord = clamp(sampleCoord, int2(0, 0), int2(dim) - 1);

            accum += inputColor[sampleCoord];
            totalWeight += 1.0;
        }
    }

    outputColor[DTid.xy] = accum / totalWeight;
}
