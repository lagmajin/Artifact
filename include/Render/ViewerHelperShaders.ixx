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
}
