module;
#include <compare>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>
#include <variant>
#include <cstdint>
#include <utility>
#include <QFont>
#include <QImage>
#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <Texture.h>
#include <Buffer.h>
#include <PipelineState.h>
#include <ShaderResourceBinding.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <BasicMath.hpp>
export module Artifact.Render.RenderCommandBuffer;

import Text.Style;
import Text.GlyphAtlas;
import Core.ArtifactArray;
import Graphics.ParticleData;

export namespace Artifact {

using namespace Diligent;

// Logical viewport state captured by the renderer and replayed after a
// context binds its render target. Deferred contexts do not inherit the
// immediate context's viewport or scissor state.
struct RenderViewportState {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    Uint32 targetWidth = 0;
    Uint32 targetHeight = 0;
    bool enabled = false;
};

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

struct GradientRectParams {
    float4 startColor;
    float4 endColor;
    // x=fill type (1 linear, 2 radial, 3 conical, 4 repeat, 5 mirror), y=angle degrees,
    // z=reverse flag, w=scale.
    float4 mode;
    // xy=center, z=linear offset, w=local-rect aspect ratio.
    float4 centerOffset;
    // x=1 when encoded sRGB endpoints must be decoded before interpolation.
    float4 colorContract;
};

static_assert(sizeof(GradientRectParams) == sizeof(float4) * 5);

struct GradientRectPkt {
    RenderSolidRectTransform2D mat;
    GradientRectParams params;
    float opacity;
    float _pad[3];
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

struct MpmSurfaceVertex {
    float x = 0.0f;
    float y = 0.0f;
    float a = 0.0f;
};

// A bounded CPU mesh packet consumed by the existing solid-triangle batch.
// Positions are composition-space coordinates; xform captures the viewport.
struct MpmSurfacePkt {
    RenderSolidTransform2D xform;
    float4 color{1.0f, 1.0f, 1.0f, 1.0f};
    ArtifactCore::ArtifactArray<MpmSurfaceVertex> vertices;
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
    // Packet data can outlive its texture-cache entry until submit/reset.
    RefCntAutoPtr<ITextureView> retainedSRV;
    float              opacity = 1.0f;
    float              _pad[3];
};

struct SpriteXformPkt {
    RenderSolidRectTransform2D mat;
    ITextureView*          pSRV    = nullptr;
    RefCntAutoPtr<ITextureView> retainedSRV;
    float                  opacity = 1.0f;
    float                  _pad[3];
};

struct ClonerSpriteXformPkt {
    RenderSolidRectTransform2D mat;
    RenderSolidRectTransform2D fallbackMat;
    ITextureView* pSRV = nullptr;
    RefCntAutoPtr<ITextureView> retainedSRV;
    IBuffer* placementBuffer = nullptr;
    RefCntAutoPtr<IBuffer> retainedPlacementBuffer;
    // Borrowed from placementBuffer; retainedPlacementBuffer keeps its owner alive.
    IBufferView* placementSRV = nullptr;
    std::uint32_t firstPlacement = 0;
    std::uint32_t instanceCount = 0;
    float localWidth = 1.0f;
    float localHeight = 1.0f;
    float localX = 0.0f;
    float localY = 0.0f;
    float opacity = 1.0f;
    // Local packet index; RenderCommandBuffer rebases it when merging buffers.
    std::size_t placementDispatchPacketIndex = static_cast<std::size_t>(-1);
};

// Parameters copied into the dynamic constant buffer when the ordered
// dispatch packet is recorded on the selected Diligent context.
struct ClonerGridDispatchParams {
    Diligent::Uint32 columns = 0;
    Diligent::Uint32 rows = 0;
    Diligent::Uint32 depth = 0;
    Diligent::Uint32 candidateCount = 0;
    float spacingX = 0.0f;
    float spacingY = 0.0f;
    float spacingZ = 0.0f;
    Diligent::Uint32 outputCapacity = 0;
    Diligent::Uint32 outputOffset = 0;
    float outputOffsetPadding0 = 0.0f;
    float outputOffsetPadding1 = 0.0f;
    float outputOffsetPadding2 = 0.0f;
    float distributionParamX = 0.0f;
    float distributionParamY = 0.0f;
    float distributionParamZ = 0.0f;
    float placementRotationStep = 0.0f;
    float modifierTranslationX = 0.0f;
    float modifierTranslationY = 0.0f;
    float modifierTranslationZ = 0.0f;
    float modifierRotationDegrees = 0.0f;
    float modifierScaleX = 1.0f;
    float modifierScaleY = 1.0f;
    float padding2 = 0.0f;
    float padding3 = 0.0f;
};

struct ClonerGridDispatchPkt {
    ClonerGridDispatchParams params;
    IPipelineState* pPSO = nullptr;
    RefCntAutoPtr<IPipelineState> retainedPSO;
    IShaderResourceBinding* pSRB = nullptr;
    RefCntAutoPtr<IShaderResourceBinding> retainedSRB;
    IBuffer* paramsBuffer = nullptr;
    RefCntAutoPtr<IBuffer> retainedParamsBuffer;
    IBuffer* outputBuffer = nullptr;
    RefCntAutoPtr<IBuffer> retainedOutputBuffer;
    bool recordingSucceeded = false;
};

struct AtlasSpritePkt {
    RenderSolidTransform2D xform;
    ITextureView*      pSRV    = nullptr;
    RefCntAutoPtr<ITextureView> retainedSRV;
    float4             uvRect; // x=u0, y=v0, z=u1, w=v1
    float4             color;  // rgb=color, a=opacity
};

struct AtlasSpriteXformPkt {
    RenderSolidRectTransform2D mat;
    ITextureView*          pSRV    = nullptr;
    RefCntAutoPtr<ITextureView> retainedSRV;
    float4                 uvRect;
    float4                 color;
};

// A single textured triangle using the existing transformed-sprite PSO.
// Positions are normalized to the packet's local unit rectangle; the matrix
// maps that rectangle into canvas space.
struct TexturedTriangleXformPkt {
    RenderSolidRectTransform2D mat;
    float2 p0;
    float2 p1;
    float2 p2;
    float2 uv0;
    float2 uv1;
    float2 uv2;
    ITextureView* pSRV = nullptr;
    RefCntAutoPtr<ITextureView> retainedSRV;
    float4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct MaskedSpritePkt {
    RenderSolidTransform2D xform;
    ITextureView*      sceneSRV = nullptr;
    RefCntAutoPtr<ITextureView> retainedSceneSRV;
    ITextureView*      maskSRV  = nullptr;
    RefCntAutoPtr<ITextureView> retainedMaskSRV;
    float              opacity   = 1.0f;
    float              _pad[3];
};

struct BillboardPkt {
    QVector3D           center;
    QVector2D           size;
    ITextureView*       pSRV        = nullptr;
    RefCntAutoPtr<ITextureView> retainedSRV;
    float4              tint        = {1.0f, 1.0f, 1.0f, 1.0f};
    float               opacity     = 1.0f;
    float               rollDegrees = 0.0f;
};

struct BillboardImagePkt {
    QVector3D           center;
    QVector2D           size;
    QImage              image;
    // Keep a submit-time image texture alive until its command list executes.
    RefCntAutoPtr<ITexture> retainedTexture;
    float4              tint        = {1.0f, 1.0f, 1.0f, 1.0f};
    float               opacity     = 1.0f;
    float               rollDegrees = 0.0f;
};

struct CardPkt {
    QRectF localRect;
    QMatrix4x4 modelMatrix;
    QMatrix4x4 viewMatrix;
    QMatrix4x4 projectionMatrix;
    ITextureView* depthDSV = nullptr;
    RefCntAutoPtr<ITextureView> retainedDepthDSV;
    float4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
    float opacity = 1.0f;
    bool writeDepth = true;
};

struct TexturedCardPkt {
    QRectF localRect;
    QMatrix4x4 modelMatrix;
    QMatrix4x4 viewMatrix;
    QMatrix4x4 projectionMatrix;
    ITextureView* pSRV = nullptr;
    RefCntAutoPtr<ITextureView> retainedSRV;
    ITextureView* depthDSV = nullptr;
    RefCntAutoPtr<ITextureView> retainedDepthDSV;
    float opacity = 1.0f;
};

struct ShapeTrianglesPkt {
    std::vector<QVector2D> vertices;
    QMatrix4x4 modelMatrix;
    QMatrix4x4 viewMatrix;
    QMatrix4x4 projectionMatrix;
    ITextureView* depthDSV = nullptr;
    RefCntAutoPtr<ITextureView> retainedDepthDSV;
    float4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
    float opacity = 1.0f;
    bool writeDepth = true;
};

struct ParticlePkt {
    ArtifactCore::ParticleRenderData data;
    QMatrix4x4                       viewMatrix;
    QMatrix4x4                       projMatrix;
    ITextureView*                    depthDSV = nullptr;
    RefCntAutoPtr<ITextureView>      retainedDepthDSV;
};

struct GlyphTextPkt {
    QRectF              rect;
    RenderSolidTransform2D xform;
    QString             text;
    QFont               font;
    float4              color;
    float4              outlineColor     = { 0.0f, 0.0f, 0.0f, 0.0f };
    int                 alignment        = 0;
    float               opacity          = 1.0f;
    float               outlineThickness = 0.0f;  // canvas-space pixels; 0 = no outline
    std::size_t         preparedGlyphOffset = 0;
    std::size_t         preparedGlyphCount = 0;
    bool                prepared = false;
};

struct GlyphTextXformPkt {
    QRectF              rect;
    QMatrix4x4          transform;
    QString             text;
    QFont               font;
    float4              color;
    float4              outlineColor     = { 0.0f, 0.0f, 0.0f, 0.0f };
    int                 alignment        = 0;
    float               opacity          = 1.0f;
    float               outlineThickness = 0.0f;
    float               devicePixelRatio = 1.0f;
};

// Bounded CPU snapshot consumed by deferred glyph recording. Atlas mutation,
// font fallback, and shaping stay on the owner; workers only read this data.
struct PreparedGlyphSubmission {
    QPointF basePosition;
    QPointF offsetPosition;
    float offsetRotation = 0.0f;
    float offsetScale = 1.0f;
    float offsetOpacity = 1.0f;
    ArtifactCore::GlyphRect rect;
};

using DrawPacket = std::variant<
    SolidRectPkt, SolidRectXformPkt, GradientRectPkt,
    LinePkt, QuadPkt, DotLinePkt, SolidTriPkt, MpmSurfacePkt, SolidCirclePkt,
    CheckerboardPkt, GridPkt, RectOutlinePkt,
    SpritePkt, SpriteXformPkt, ClonerGridDispatchPkt, ClonerSpriteXformPkt,
    MaskedSpritePkt,
    AtlasSpritePkt, AtlasSpriteXformPkt, TexturedTriangleXformPkt,
    BillboardPkt, BillboardImagePkt, CardPkt, TexturedCardPkt,
    ShapeTrianglesPkt, ParticlePkt,
    GlyphTextPkt, GlyphTextXformPkt
>;

class RenderCommandBuffer {
public:
    static constexpr std::size_t kRecycledParticleBufferLimit = 4;

    void setTargetRTV(ITextureView* target) {
        retainedTargetRTV_ = target;
        targetRTV_ = target;
    }

    ITextureView* targetRTV() const noexcept { return targetRTV_; }

    void reset()  {
        for (auto& packet : packets_) {
            if (auto* particle = std::get_if<ParticlePkt>(&packet)) {
                if (recycledParticleBuffers_.size() < recycledParticleBuffers_.capacity()) {
                    recycledParticleBuffers_.emplace_back();
                    recycledParticleBuffers_.back().swap(particle->data.particles);
                }
            } else if (auto* surface = std::get_if<MpmSurfacePkt>(&packet)) {
                if (recycledMpmSurfaceBuffers_.size() <
                    recycledMpmSurfaceBuffers_.capacity()) {
                    const std::size_t index = recycledMpmSurfaceBuffers_.size();
                    recycledMpmSurfaceBuffers_.append(
                        ArtifactCore::ArtifactArray<MpmSurfaceVertex>{});
                    recycledMpmSurfaceBuffers_[index].swap(surface->vertices);
                }
            }
        }
        packets_.clear();
        preparedGlyphs_.removeAll();
        preparedGlyphAtlasSRV_ = nullptr;
        targetRTV_ = nullptr;
        retainedTargetRTV_ = nullptr;
    }
    // Reserve on the owning frame/worker slot before recording so steady-state
    // append and buffer merges can reuse storage.
    void reserve(std::size_t packetCapacity) {
        packets_.reserve(packetCapacity);
        recycledParticleBuffers_.reserve(
            std::min(packetCapacity, kRecycledParticleBufferLimit));
        // A surface packet can consume one full bounded mesh buffer per MPM
        // layer in the frame. Match the packet capacity so reset() can recycle
        // every accepted surface without dropping later layers' storage.
        recycledMpmSurfaceBuffers_.reserve(packetCapacity);
    }
    std::size_t capacity() const noexcept { return packets_.capacity(); }
    static constexpr std::size_t kPreparedGlyphCapacity = 4096;
    void reservePreparedGlyphs() {
        preparedGlyphs_.reserve(kPreparedGlyphCapacity);
    }
    ArtifactCore::ArtifactArray<PreparedGlyphSubmission>& preparedGlyphs() noexcept {
        return preparedGlyphs_;
    }
    const ArtifactCore::ArtifactArray<PreparedGlyphSubmission>&
    preparedGlyphs() const noexcept { return preparedGlyphs_; }
    void setPreparedGlyphAtlasSRV(ITextureView* srv) noexcept {
        preparedGlyphAtlasSRV_ = srv;
    }
    ITextureView* preparedGlyphAtlasSRV() const noexcept {
        return preparedGlyphAtlasSRV_.RawPtr();
    }

    // Exchange packet storage and retained target ownership without copying.
    // Used by the renderer's single bounded in-flight deferred-record slot.
    void swapContents(RenderCommandBuffer& other) noexcept {
        packets_.swap(other.packets_);
        recycledParticleBuffers_.swap(other.recycledParticleBuffers_);
        recycledMpmSurfaceBuffers_.swap(other.recycledMpmSurfaceBuffers_);
        preparedGlyphs_.swap(other.preparedGlyphs_);
        preparedGlyphAtlasSRV_.swap(other.preparedGlyphAtlasSRV_);
        std::swap(targetRTV_, other.targetRTV_);
        retainedTargetRTV_.swap(other.retainedTargetRTV_);
    }

    std::vector<ArtifactCore::ParticleVertex> takeRecycledParticleBuffer() {
        if (recycledParticleBuffers_.empty()) return {};
        auto buffer = std::move(recycledParticleBuffers_.back());
        recycledParticleBuffers_.pop_back();
        return buffer;
    }

    ArtifactCore::ArtifactArray<MpmSurfaceVertex> takeRecycledMpmSurfaceBuffer() {
        if (recycledMpmSurfaceBuffers_.isEmpty()) return {};
        auto buffer = std::move(
            recycledMpmSurfaceBuffers_[recycledMpmSurfaceBuffers_.size() - 1]);
        recycledMpmSurfaceBuffers_.removeLast();
        return buffer;
    }

    void returnRecycledMpmSurfaceBuffer(
        ArtifactCore::ArtifactArray<MpmSurfaceVertex>&& buffer) {
        if (recycledMpmSurfaceBuffers_.size() >=
                recycledMpmSurfaceBuffers_.capacity()) {
            return;
        }
        recycledMpmSurfaceBuffers_.append(std::move(buffer));
    }

    template <typename Packet>
    void append(Packet&& packet) {
        packets_.emplace_back(std::forward<Packet>(packet));
        pinPacketResources(packets_.back());
    }

    // Append without allowing vector growth. Reserve on a cold path first;
    // false means the packet was not appended and the buffer is unchanged.
    template <typename Packet>
    bool tryAppend(Packet&& packet) {
        if (packets_.size() >= packets_.capacity()) return false;
        packets_.emplace_back(std::forward<Packet>(packet));
        pinPacketResources(packets_.back());
        return true;
    }

    // Merge a completed worker-local buffer in caller-defined order. Each
    // buffer remains single-writer; this operation itself is intentionally
    // serialized after worker completion. This convenience form may grow the
    // destination and belongs on a cold path or after capacity planning.
    bool appendBuffer(RenderCommandBuffer& source) {
        if (!canAppendBuffer(source)) return false;
        if (source.packets_.empty()) return true;
        packets_.reserve(packets_.size() + source.packets_.size());
        moveAppendBuffer(source);
        return true;
    }

    // Merge completed worker buffers in the supplied stable order. Validate
    // the entire batch before moving any packet, then reserve once on the
    // cold/growth-capable path.
    bool appendBuffers(const std::span<RenderCommandBuffer*> sources) {
        std::size_t packetCount = 0;
        if (!canAppendBuffers(sources, packetCount)) return false;
        if (packetCount == 0) return true;
        packets_.reserve(packets_.size() + packetCount);
        for (RenderCommandBuffer* source : sources) {
            if (source && !source->packets_.empty()) moveAppendBuffer(*source);
        }
        return true;
    }

    // No-allocation merge for a frame hot path. The caller reserves enough
    // packet capacity before recording; false leaves both buffers untouched.
    bool tryAppendBuffer(RenderCommandBuffer& source) {
        if (!canAppendBuffer(source) ||
            source.packets_.size() > packets_.capacity() - packets_.size()) {
            return false;
        }
        if (source.packets_.empty()) return true;
        moveAppendBuffer(source);
        return true;
    }

    // Bounded fan-in for a frame hot path. On failure, destination and every
    // source remain unchanged; on success, no destination allocation occurs.
    bool tryAppendBuffers(const std::span<RenderCommandBuffer*> sources) {
        std::size_t packetCount = 0;
        if (!canAppendBuffers(sources, packetCount) ||
            packetCount > packets_.capacity() - packets_.size()) {
            return false;
        }
        if (packetCount == 0) return true;
        for (RenderCommandBuffer* source : sources) {
            if (source && !source->packets_.empty()) moveAppendBuffer(*source);
        }
        return true;
    }

    bool empty()  const { return packets_.empty(); }
    std::size_t size() const { return packets_.size(); }
    std::vector<DrawPacket>& packetsMutable() noexcept { return packets_; }
    const std::vector<DrawPacket>& packets() const noexcept { return packets_; }

private:
    bool canAppendBuffers(const std::span<RenderCommandBuffer*> sources,
                          std::size_t& packetCount) const noexcept {
        packetCount = 0;
        ITextureView* mergedTarget = targetRTV_;
        for (std::size_t index = 0; index < sources.size(); ++index) {
            RenderCommandBuffer* source = sources[index];
            if (!source || source == this) return false;
            for (std::size_t prior = 0; prior < index; ++prior) {
                if (sources[prior] == source) return false;
            }
            if (source->targetRTV_) {
                if (mergedTarget && mergedTarget != source->targetRTV_) return false;
                mergedTarget = source->targetRTV_;
            }
            const std::size_t available = packets_.max_size() - packets_.size();
            if (source->packets_.size() > available - packetCount) return false;
            packetCount += source->packets_.size();
        }
        return true;
    }

    bool canAppendBuffer(const RenderCommandBuffer& source) const noexcept {
        return this != &source &&
               (!targetRTV_ || !source.targetRTV_ ||
                targetRTV_ == source.targetRTV_) &&
               source.packets_.size() <= packets_.max_size() - packets_.size();
    }

    void moveAppendBuffer(RenderCommandBuffer& source) {
        if (!targetRTV_) setTargetRTV(source.targetRTV_);
        const std::size_t packetBase = packets_.size();
        for (std::size_t sourceIndex = 0;
             sourceIndex < source.packets_.size(); ++sourceIndex) {
            auto& packet = source.packets_[sourceIndex];
            if (auto* clonerSprite =
                    std::get_if<ClonerSpriteXformPkt>(&packet)) {
                const std::size_t dispatchIndex =
                    clonerSprite->placementDispatchPacketIndex;
                if (dispatchIndex < sourceIndex &&
                    std::holds_alternative<ClonerGridDispatchPkt>(
                        source.packets_[dispatchIndex])) {
                    clonerSprite->placementDispatchPacketIndex += packetBase;
                }
            }
            packets_.emplace_back(std::move(packet));
        }
        source.reset();
    }
    static void pinPacketResources(DrawPacket& pkt) {
        std::visit([](auto& packet) {
            if constexpr (requires { packet.pSRV; packet.retainedSRV; }) {
                packet.retainedSRV = packet.pSRV;
            }
            if constexpr (requires {
                              packet.placementBuffer;
                              packet.retainedPlacementBuffer;
                          }) {
                packet.retainedPlacementBuffer = packet.placementBuffer;
            }
            if constexpr (requires {
                              packet.pPSO;
                              packet.retainedPSO;
                              packet.pSRB;
                              packet.retainedSRB;
                              packet.paramsBuffer;
                              packet.retainedParamsBuffer;
                              packet.outputBuffer;
                              packet.retainedOutputBuffer;
                          }) {
                packet.retainedPSO = packet.pPSO;
                packet.retainedSRB = packet.pSRB;
                packet.retainedParamsBuffer = packet.paramsBuffer;
                packet.retainedOutputBuffer = packet.outputBuffer;
            }
            if constexpr (requires { packet.depthDSV; packet.retainedDepthDSV; }) {
                packet.retainedDepthDSV = packet.depthDSV;
            }
            if constexpr (requires {
                              packet.sceneSRV;
                              packet.retainedSceneSRV;
                              packet.maskSRV;
                              packet.retainedMaskSRV;
                          }) {
                packet.retainedSceneSRV = packet.sceneSRV;
                packet.retainedMaskSRV = packet.maskSRV;
            }
        }, pkt);
    }

    std::vector<DrawPacket> packets_;
    std::vector<std::vector<ArtifactCore::ParticleVertex>> recycledParticleBuffers_;
    ArtifactCore::ArtifactArray<ArtifactCore::ArtifactArray<MpmSurfaceVertex>>
        recycledMpmSurfaceBuffers_;
    ArtifactCore::ArtifactArray<PreparedGlyphSubmission> preparedGlyphs_;
    RefCntAutoPtr<ITextureView> preparedGlyphAtlasSRV_;
    ITextureView* targetRTV_ = nullptr;
    RefCntAutoPtr<ITextureView> retainedTargetRTV_;
};

// Fixed-slot, reusable recording buffers for parallel CPU preparation.
// Reserve packet capacity before recording and use tryAppend to prevent worker
// writes from growing storage. Hold one WorkerBufferLease per task to enforce
// exclusive access while recording several packets.
// beginFrame/reserve/reset/merge are owner-thread operations guarded against
// concurrent worker acquisition. Each slot permits one lease at a time; merge
// only after every worker has completed and released its lease.
class RenderCommandBufferPool {
public:
    static constexpr std::size_t kMaximumBuffers = 64;

    class WorkerBufferLease {
    public:
        WorkerBufferLease() = default;
        ~WorkerBufferLease() { release(); }
        WorkerBufferLease(const WorkerBufferLease&) = delete;
        WorkerBufferLease& operator=(const WorkerBufferLease&) = delete;

        WorkerBufferLease(WorkerBufferLease&& other) noexcept
            : owner_(std::exchange(other.owner_, nullptr)),
              index_(other.index_), buffer_(std::exchange(other.buffer_, nullptr)) {}

        WorkerBufferLease& operator=(WorkerBufferLease&& other) noexcept {
            if (this == &other) return *this;
            release();
            owner_ = std::exchange(other.owner_, nullptr);
            index_ = other.index_;
            buffer_ = std::exchange(other.buffer_, nullptr);
            return *this;
        }

        explicit operator bool() const noexcept { return buffer_ != nullptr; }

        void setTargetRTV(ITextureView* target) {
            if (buffer_) buffer_->setTargetRTV(target);
        }

        template <typename Packet>
        bool tryAppend(Packet&& packet) {
            return buffer_ &&
                   buffer_->tryAppend(std::forward<Packet>(packet));
        }

    private:
        friend class RenderCommandBufferPool;
        WorkerBufferLease(RenderCommandBufferPool* owner, std::size_t index,
                          RenderCommandBuffer* buffer) noexcept
            : owner_(owner), index_(index), buffer_(buffer) {}

        void release() noexcept {
            if (owner_) owner_->releaseWorker(index_);
            owner_ = nullptr;
            buffer_ = nullptr;
        }

        RenderCommandBufferPool* owner_ = nullptr;
        std::size_t index_ = 0;
        RenderCommandBuffer* buffer_ = nullptr;
    };

    bool beginFrame(const std::size_t workerCount) {
        if (workerCount > kMaximumBuffers) return false;
        MaintenanceGuard maintenance(*this);
        if (!maintenance) return false;
        const std::size_t previousCount =
            activeCount_.load(std::memory_order_relaxed);
        for (std::size_t index = 0; index < previousCount; ++index) {
            buffers_[index].reset();
            bufferPointers_[index] = nullptr;
        }
        for (std::size_t index = 0; index < workerCount; ++index) {
            bufferPointers_[index] = &buffers_[index];
        }
        activeCount_.store(workerCount, std::memory_order_release);
        return true;
    }

    // Start a frame only when every requested worker slot already has enough
    // packet storage. The check and activation share one maintenance gate, so
    // workers cannot acquire a slot between capacity validation and begin.
    bool beginFrameWithReservedCapacity(const std::size_t workerCount,
                                        const std::size_t packetCapacity) {
        if (workerCount > kMaximumBuffers) return false;
        MaintenanceGuard maintenance(*this);
        if (!maintenance) return false;

        const std::size_t previousCount =
            activeCount_.load(std::memory_order_relaxed);
        for (std::size_t index = 0; index < previousCount; ++index) {
            buffers_[index].reset();
            bufferPointers_[index] = nullptr;
        }
        activeCount_.store(0, std::memory_order_relaxed);
        for (std::size_t index = 0; index < workerCount; ++index) {
            if (buffers_[index].capacity() < packetCapacity) return false;
        }
        for (std::size_t index = 0; index < workerCount; ++index) {
            bufferPointers_[index] = &buffers_[index];
        }
        activeCount_.store(workerCount, std::memory_order_release);
        return true;
    }

    // Cold-path preparation: close the current frame, retain reusable packet
    // storage, and reserve every slot before any worker frame can be opened.
    bool reserveForWorkerCount(const std::size_t workerCount,
                               const std::size_t packetCapacity) {
        if (workerCount > kMaximumBuffers) return false;
        MaintenanceGuard maintenance(*this);
        if (!maintenance) return false;

        const std::size_t previousCount =
            activeCount_.load(std::memory_order_relaxed);
        for (std::size_t index = 0; index < previousCount; ++index) {
            buffers_[index].reset();
            bufferPointers_[index] = nullptr;
        }
        activeCount_.store(0, std::memory_order_relaxed);
        for (std::size_t index = 0; index < workerCount; ++index) {
            buffers_[index].reserve(packetCapacity);
        }
        return true;
    }

    bool reset() {
        MaintenanceGuard maintenance(*this);
        if (!maintenance) return false;
        const std::size_t previousCount =
            activeCount_.load(std::memory_order_relaxed);
        for (std::size_t index = 0; index < previousCount; ++index) {
            buffers_[index].reset();
            bufferPointers_[index] = nullptr;
        }
        activeCount_.store(0, std::memory_order_relaxed);
        return true;
    }

    std::size_t size() const noexcept {
        return activeCount_.load(std::memory_order_acquire);
    }

    bool hasReservedCapacity(const std::size_t workerCount,
                             const std::size_t packetCapacity) const noexcept {
        MaintenanceGuard maintenance(*this);
        if (!maintenance || workerCount !=
                activeCount_.load(std::memory_order_relaxed)) return false;
        for (std::size_t index = 0; index < workerCount; ++index) {
            if (buffers_[index].capacity() < packetCapacity) return false;
        }
        return true;
    }

    WorkerBufferLease acquireWorkerBuffer(const std::size_t index) noexcept {
        // The state combines the maintenance gate and lease count in one word.
        // Whichever CAS wins prevents the other operation from crossing the
        // gate while it reads or mutates slot storage.
        std::size_t state = activityState_.load(std::memory_order_relaxed);
        for (;;) {
            if ((state & kMaintenanceBit) != 0 ||
                (state & kLeaseCountMask) == kLeaseCountMask) {
                return {};
            }
            if (activityState_.compare_exchange_weak(
                    state, state + 1, std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                break;
            }
        }
        if (index >= activeCount_.load(std::memory_order_relaxed)) {
            activityState_.fetch_sub(1, std::memory_order_acq_rel);
            return {};
        }
        bool expected = false;
        if (!workerActive_[index].compare_exchange_strong(
                expected, true, std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            activityState_.fetch_sub(1, std::memory_order_acq_rel);
            return {};
        }
        return WorkerBufferLease(this, index, &buffers_[index]);
    }

    bool reserveWorker(const std::size_t index,
                       const std::size_t packetCapacity) {
        MaintenanceGuard maintenance(*this);
        if (!maintenance || index >=
                activeCount_.load(std::memory_order_relaxed)) return false;
        buffers_[index].reserve(packetCapacity);
        return true;
    }

    template <typename Packet>
    bool tryAppend(const std::size_t index, Packet&& packet) {
        auto lease = acquireWorkerBuffer(index);
        return lease.tryAppend(std::forward<Packet>(packet));
    }

    bool reserveAll(const std::size_t packetCapacity) {
        MaintenanceGuard maintenance(*this);
        if (!maintenance) return false;
        const std::size_t activeCount =
            activeCount_.load(std::memory_order_relaxed);
        for (std::size_t index = 0; index < activeCount; ++index) {
            buffers_[index].reserve(packetCapacity);
        }
        return true;
    }

    bool mergeInto(RenderCommandBuffer& destination) {
        MaintenanceGuard maintenance(*this);
        if (!maintenance) return false;
        const std::size_t activeCount =
            activeCount_.load(std::memory_order_relaxed);
        return destination.appendBuffers(
            std::span<RenderCommandBuffer*>(bufferPointers_.data(), activeCount));
    }

    bool tryMergeInto(RenderCommandBuffer& destination) {
        MaintenanceGuard maintenance(*this);
        if (!maintenance) return false;
        const std::size_t activeCount =
            activeCount_.load(std::memory_order_relaxed);
        return destination.tryAppendBuffers(
            std::span<RenderCommandBuffer*>(bufferPointers_.data(), activeCount));
    }

private:
    static constexpr std::size_t kMaintenanceBit =
        std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1);
    static constexpr std::size_t kLeaseCountMask = kMaintenanceBit - 1;

    class MaintenanceGuard {
    public:
        explicit MaintenanceGuard(const RenderCommandBufferPool& owner) noexcept
            : owner_(&owner) {
            std::size_t expected = 0;
            if (!owner_->activityState_.compare_exchange_strong(
                    expected, kMaintenanceBit, std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                owner_ = nullptr;
            }
        }

        ~MaintenanceGuard() {
            if (owner_) {
                owner_->activityState_.store(0, std::memory_order_release);
            }
        }

        MaintenanceGuard(const MaintenanceGuard&) = delete;
        MaintenanceGuard& operator=(const MaintenanceGuard&) = delete;
        explicit operator bool() const noexcept { return owner_ != nullptr; }

    private:
        const RenderCommandBufferPool* owner_ = nullptr;
    };

    void releaseWorker(const std::size_t index) noexcept {
        workerActive_[index].store(false, std::memory_order_release);
        activityState_.fetch_sub(1, std::memory_order_acq_rel);
    }

    std::array<RenderCommandBuffer, kMaximumBuffers> buffers_;
    std::array<RenderCommandBuffer*, kMaximumBuffers> bufferPointers_{};
    std::array<std::atomic<bool>, kMaximumBuffers> workerActive_{};
    mutable std::atomic<std::size_t> activityState_{0};
    mutable std::atomic<std::size_t> activeCount_{0};
};

} // namespace Artifact
