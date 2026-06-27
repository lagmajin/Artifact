// Liquify compute shader
// Supports: Push, Pinch, Bloat, Twirl, Turbulence, Pucker

Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

cbuffer LiquifyParams : register(b0) {
    int g_BrushType;        // 0=Push,1=Pinch,2=Bloat,3=Twirl,4=Turbulence,5=Pucker
    float g_Amount;         // -100..100
    float g_Radius;         // radius in pixels
    float g_CenterX;        // center X in pixels
    float g_CenterY;        // center Y in pixels
    float g_Angle;          // angle in degrees (Push direction / Twirl twist)
    int g_Seed;             // turbulence seed
    int g_Width;
    int g_Height;
    float g_Pad0;
    float g_Pad1;
};

// Simple hash for turbulence
float hash2D(float x, float y, int seed) {
    float h = sin(x * 127.1 + y * 311.7 + seed * 73.13) * 43758.5453;
    return h - floor(h);
}

// Bilinear sample from input texture
float4 sampleBilinear(float2 uv) {
    return g_InputTexture.SampleLevel(sampler_linear_clamp, uv, 0);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (DTid.x >= width || DTid.y >= height) return;

    float x = (float)DTid.x + 0.5;
    float y = (float)DTid.y + 0.5;

    float dx = x - g_CenterX;
    float dy = y - g_CenterY;
    float dist = sqrt(dx * dx + dy * dy);
    float maxR = g_Radius;

    float sx = x;
    float sy = y;

    if (maxR > 0.0f && dist < maxR) {
        float nd = dist / maxR;
        float falloff = 1.0 - nd * nd;
        falloff *= falloff;
        float amount = g_Amount / 100.0;

        if (g_BrushType == 0) { // Push
            float rad = g_Angle * 3.14159265 / 180.0;
            float pushX = cos(rad) * amount * maxR * falloff;
            float pushY = sin(rad) * amount * maxR * falloff;
            sx = x + pushX;
            sy = y + pushY;
        } else if (g_BrushType == 1) { // Pinch
            float scale = 1.0 - amount * falloff;
            scale = max(0.01, scale);
            sx = g_CenterX + dx * scale;
            sy = g_CenterY + dy * scale;
        } else if (g_BrushType == 2) { // Bloat
            float scale = 1.0 + amount * falloff;
            scale = max(0.01, scale);
            sx = g_CenterX + dx * scale;
            sy = g_CenterY + dy * scale;
        } else if (g_BrushType == 3) { // Twirl
            float rad = g_Angle * 3.14159265 / 180.0;
            float twist = rad * falloff * amount;
            float cosA = cos(twist);
            float sinA = sin(twist);
            sx = g_CenterX + dx * cosA - dy * sinA;
            sy = g_CenterY + dx * sinA + dy * cosA;
        } else if (g_BrushType == 4) { // Turbulence
            float noiseScale = 1.0 / (maxR * 0.5 + 1.0);
            float nx = hash2D(x * noiseScale, y * noiseScale, g_Seed) - 0.5;
            float ny = hash2D(y * noiseScale, x * noiseScale, g_Seed + 1) - 0.5;
            float displace = amount * maxR * falloff;
            sx = x + nx * displace * 2.0;
            sy = y + ny * displace * 2.0;
        } else if (g_BrushType == 5) { // Pucker
            float sign = (amount > 0) ? -1.0 : 1.0;
            float a = abs(amount);
            float scale = 1.0 + sign * a * falloff;
            scale = max(0.01, scale);
            sx = g_CenterX + dx * scale;
            sy = g_CenterY + dy * scale;
        }
    }

    // Normalize to UV for sampling
    float2 uv = float2((sx - 0.5) / (float)width, (sy - 0.5) / (float)height);
    float4 color = sampleBilinear(uv);
    g_OutputTexture[DTid.xy] = color;
}
