module;
#include <utility>
#include <QByteArray>

export module Render.Shader.ViewerHelpers;

export namespace Artifact
{
    // Checkerboard Pixel Shader
    inline const QByteArray g_checkerboardPS = R"HLSL(
    cbuffer ViewerHelperCB : register(b1)
    {
        float tileSize;
        float thickness;
        float2 padding;
        float4 color1;
        float4 color2;
    };

    struct PSInput
    {
        float4 pos : SV_POSITION;
    };

    float4 main(PSInput input) : SV_TARGET
    {
        // Using SV_POSITION which is in pixels
        float2 tile = floor(input.pos.xy / tileSize);
        float checker = fmod(tile.x + tile.y, 2.0);
        return lerp(color1, color2, checker);
    }
    )HLSL";

    // Grid Pixel Shader
    // PSInput receives TEXCOORD0 from drawSolidRectVSSource (unit UV, 0..1).
    // canvas_pos = gridOrigin + uv * canvasSize reconstructs absolute canvas
    // coordinates, keeping grid lines anchored to the composition origin.
    inline const QByteArray g_gridPS = R"HLSL(
    cbuffer ViewerHelperCB : register(b1)
    {
        float spacing;
        float thickness;
        float2 canvasSize;   // grid quad size in canvas pixels
        float4 gridColor;
        float4 gridOrigin;   // xy = grid quad top-left in canvas coordinates
    };

    struct PSInput
    {
        float4 pos   : SV_POSITION;
        float2 uv    : TEXCOORD0;    // unit quad UV forwarded from VS
    };

    float4 main(PSInput input) : SV_TARGET
    {
        // Reconstruct the absolute canvas position so grid lines stay locked
        // to the composition origin (0,0) regardless of the quad's placement.
        float2 canvas_pos = gridOrigin.xy + input.uv * canvasSize;
        // Symmetric distance to the nearest grid line, correct for negative
        // canvas coordinates (viewport extends into the pasteboard).
        float2 grid = abs(fmod(canvas_pos, spacing));
        grid = min(grid, spacing - grid);
        if (grid.x < thickness || grid.y < thickness)
            return gridColor;
        discard;
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    )HLSL";

    // Viewport exposure and clipping warning compute shader (display-only).
    // Applied to the final composited surface just before presentation so
    // HDR highlights/shadows can be inspected without touching the render
    // output, the render queue, or the color sampler/scopes readback.
    // gain is in stops; gamma, saturation and clipping thresholds are linear.
    // Defaults (gain=0, gamma=1, saturation=1) are an exact identity.
    inline const QByteArray g_viewportExposureCS = R"HLSL(
    Texture2D<float4> g_ExposureSrc : register(t0);
    RWTexture2D<float4> g_ExposureDst : register(u0);

    cbuffer ExposureParams : register(b0)
    {
        float gain;
        float gamma;
        float saturation;
        float clippingWarningsEnabled;
        float underThreshold;
        float overThreshold;
        float _pad0;
        float _pad1;
    };

    [numthreads(8,8,1)]
    void main(uint3 id : SV_DispatchThreadID)
    {
        uint outWidth, outHeight;
        g_ExposureDst.GetDimensions(outWidth, outHeight);
        if (id.x >= outWidth || id.y >= outHeight) return;

        const float4 source = g_ExposureSrc[id.xy];
        // The final composition accumulator is premultiplied. Apply display
        // transforms and clipping tests to straight linear RGB, then restore
        // premultiplication so transparent edges retain their coverage.
        if (source.a <= 0.0)
        {
            g_ExposureDst[id.xy] = float4(0.0, 0.0, 0.0, 0.0);
            return;
        }
        const float3 straightSource = source.rgb / source.a;
        // stops -> linear multiplier
        const float linearGain = exp2(gain);
        const float3 adjustedLinear = straightSource * linearGain;
        float3 c = saturate(adjustedLinear);
        c = pow(max(c, 0.0), 1.0 / max(gamma, 1e-3));
        const float luma = dot(c, float3(0.2126, 0.7152, 0.0722));
        c = lerp(float3(luma, luma, luma), c, max(saturation, 0.0));
        if (clippingWarningsEnabled > 0.5)
        {
            const bool over = any(adjustedLinear >= overThreshold);
            const bool under = dot(adjustedLinear,
                                   float3(0.2126, 0.7152, 0.0722)) <= underThreshold;
            if (over)
                c = float3(1.0, 0.03, 0.02);
            else if (under)
                c = float3(0.02, 0.08, 1.0);
        }
        // Preserve coverage and alpha while returning premultiplied RGB.
        g_ExposureDst[id.xy] = float4(c * source.a, source.a);
    }
    )HLSL";
}
