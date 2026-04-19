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
        float4 pos   : SV_POSITION;
        float4 color : COLOR0;
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
    // canvas_pos = uv * canvasSize gives composition-space coordinates,
    // ensuring grid lines stay fixed regardless of pan/zoom.
    inline const QByteArray g_gridPS = R"HLSL(
    cbuffer ViewerHelperCB : register(b1)
    {
        float spacing;
        float thickness;
        float2 canvasSize;   // composition dimensions in canvas pixels
        float4 gridColor;
        float4 unused;
    };

    struct PSInput
    {
        float4 pos   : SV_POSITION;
        float2 uv    : TEXCOORD0;    // unit quad UV forwarded from VS
    };

    float4 main(PSInput input) : SV_TARGET
    {
        float2 canvas_pos = input.uv * canvasSize;
        float2 grid = fmod(canvas_pos, spacing);
        if (grid.x < thickness || grid.y < thickness)
            return gridColor;
        discard;
    }
    )HLSL";
}
