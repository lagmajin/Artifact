module;
#include <vector>
#include <variant>
#include <cstdint>
#include <Texture.h>
#include <BasicMath.hpp>
export module Artifact.Render.RenderCommandBuffer;

export namespace Artifact {

using namespace Diligent;

struct RenderSolidTransform2D {
    float2 offset;
    float2 scale;
    float2 screenSize;
};

struct RenderSolidRectTransform2D {
    float4 row0;
    float4 row1;
    float4 row2;
    float4 row3;
};

struct SolidRectPkt {
    RenderSolidTransform2D xform;
    float4             color;
};

struct SolidRectXformPkt {
    RenderSolidRectTransform2D mat;
    float4                 color;
};

struct LinePkt {
    RenderSolidTransform2D xform;
    float2 p1, p2;
    float4 c1, c2;
};

struct QuadPkt {
    RenderSolidTransform2D xform;
    float2 p0, p1, p2, p3;
    float4 color;
};

struct DotLinePkt {
    RenderSolidTransform2D xform;
    struct Vert { float2 pos; float4 color; float dist; float _pad; };
    Vert   verts[4];
    float  thickness, spacing;
    float  _pad[2];
};

struct SolidTriPkt {
    RenderSolidTransform2D xform;
    float2 p0, p1, p2;
    float4 color;
};

struct SolidCirclePkt {
    RenderSolidTransform2D xform;
    float cx, cy, radius;
    float _pad;
    float4 color;
};

struct CBViewerHelperData {
    float  param0;
    float  param1;
    float  _pad[2];
    float4 color1;
    float4 color2;
};

struct CheckerboardPkt {
    RenderSolidTransform2D xform;
    CBViewerHelperData helper;
    float4             baseColor;
};

struct GridPkt {
    RenderSolidTransform2D xform;
    CBViewerHelperData helper;
    float4             baseColor;
};

struct RectOutlinePkt {
    RenderSolidTransform2D xform;
    float4             color;
};

struct SpritePkt {
    RenderSolidTransform2D xform;
    ITextureView*      pSRV    = nullptr;
    float              opacity = 1.0f;
    float              _pad[3];
};

struct SpriteXformPkt {
    RenderSolidRectTransform2D mat;
    ITextureView*          pSRV    = nullptr;
    float                  opacity = 1.0f;
    float                  _pad[3];
};

struct MaskedSpritePkt {
    RenderSolidTransform2D xform;
    ITextureView*      sceneSRV = nullptr;
    ITextureView*      maskSRV  = nullptr;
    float              opacity   = 1.0f;
    float              _pad[3];
};

using DrawPacket = std::variant<
    SolidRectPkt, SolidRectXformPkt,
    LinePkt, QuadPkt, DotLinePkt, SolidTriPkt, SolidCirclePkt,
    CheckerboardPkt, GridPkt, RectOutlinePkt,
    SpritePkt, SpriteXformPkt, MaskedSpritePkt
>;

class RenderCommandBuffer {
public:
    ITextureView* targetRTV = nullptr;

    void reset()  { packets_.clear(); targetRTV = nullptr; }
    void append(DrawPacket pkt) { packets_.push_back(std::move(pkt)); }
    bool empty()  const { return packets_.empty(); }
    const std::vector<DrawPacket>& packets() const { return packets_; }

private:
    std::vector<DrawPacket> packets_;
};

} // namespace Artifact
