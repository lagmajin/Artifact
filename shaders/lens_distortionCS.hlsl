// Lens Distortion Compute Shader
// Barrel/pincushion distortion with zoom compensation

RWTexture2D<float4> Output : register(u0);
Texture2D<float4> Input : register(t0);
SamplerState InputSampler : register(s0);

cbuffer LensDistortionParams : register(b0) {
    float Distortion;   // -100 to 100
    float CenterX;      // 0.0 to 1.0 (normalized)
    float CenterY;      // 0.0 to 1.0 (normalized)
    float Zoom;         // 0.1 to 3.0
    float Invert;       // 0.0 or 1.0
    float Width;
    float Height;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelPos = dispatchThreadID.xy;
    uint2 dimensions;
    Input.GetDimensions(dimensions.x, dimensions.y);

    float2 uv = float2(pixelPos) / float2(dimensions);
    float2 center = float2(CenterX, CenterY);
    float2 delta = uv - center;

    float maxR = 0.5f;
    float r = length(delta) / maxR;

    float k = Distortion / 100.0f;
    if (Invert > 0.5f) k = -k;

    float rDistorted = r * (1.0f + k * r * r);
    rDistorted /= Zoom;

    float2 srcUV;
    if (r > 1e-6f) {
        float scale = rDistorted / r;
        srcUV = center + delta * scale;
    } else {
        srcUV = center;
    }

    float4 color = Input.SampleLevel(InputSampler, srcUV, 0);
    Output[pixelPos] = color;
}
