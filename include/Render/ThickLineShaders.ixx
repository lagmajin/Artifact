module;
#include <utility>
#include <QByteArray>

export module Render.Shader.ThickLine;

export namespace Artifact
{
 // Vertex shader: pixel-space positions with pan offset via TransformCB
 inline const QByteArray g_thickLineVS = R"HLSL(
cbuffer TransformCB : register(b0)
{
    float2 offset;
    float2 scale;
    float2 screenSize;
};

struct VSInput
{
    float2 pos   : ATTRIB0;
    float4 color : ATTRIB1;
};

struct PSInput
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
};

PSInput main(VSInput input)
{
    PSInput output;
    float2 pos = input.pos * scale + offset;
    float2 ndc = pos / screenSize * 2.0f - float2(1.0f, 1.0f);
    ndc.y = -ndc.y;
    output.pos   = float4(ndc, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}
)HLSL";

 // Pixel shader: pass-through color
 inline const QByteArray g_thickLinePS = R"HLSL(
struct PSInput
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
};

float4 main(PSInput input) : SV_TARGET
{
    return input.color;
}
)HLSL";

 // Sprite vertex shader: local -> view -> NDC, UV/color forwarded to PS
 inline const QByteArray g_2DSpriteVS = R"HLSL(
cbuffer TransformCB : register(b0)
{
    float2 offset;
    float2 scale;
    float2 screenSize;
};

struct VSInput
{
    float2 pos   : ATTRIB0;
    float2 uv    : ATTRIB1;
    float4 color : ATTRIB2;
};

struct PSInput
{
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

PSInput main(VSInput input)
{
    PSInput output;
    float2 pos = input.pos * scale + offset;
    float2 ndc = pos / screenSize * 2.0f - float2(1.0f, 1.0f);
    ndc.y = -ndc.y;

    output.pos   = float4(ndc, 0.0f, 1.0f);
    output.uv    = input.uv;
    output.color = input.color;
    return output;
}
)HLSL";

 // Dotted Line shaders (Task 3)
 inline const QByteArray g_dotLineVS = R"HLSL(
cbuffer TransformCB : register(b0)
{
    float2 offset;
    float2 scale;
    float2 screenSize;
};

struct VSInput
{
    float2 pos   : ATTRIB0;
    float4 color : ATTRIB1;
    float  dist  : ATTRIB2;
};

struct PSInput
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
    float  dist  : DISTANCE0;
};

PSInput main(VSInput input)
{
    PSInput output;
    float2 pos = input.pos + offset;
    float2 ndc = (pos / screenSize) * 2.0f - float2(1.0f, 1.0f);
    ndc.y = -ndc.y;
    output.pos   = float4(ndc, 0.0f, 1.0f);
    output.color = input.color;
    output.dist  = input.dist;
    return output;
}
)HLSL";

 inline const QByteArray g_dotLinePS = R"HLSL(
cbuffer DotLineCB : register(b1)
{
    float thickness;
    float spacing;
    float2 padding;
};

struct PSInput
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
    float  dist  : DISTANCE0;
};

float4 main(PSInput input) : SV_TARGET
{
    float pattern = thickness + spacing;
    if (fmod(input.dist, pattern) > thickness)
        discard;
    return input.color;
}
)HLSL";

}
