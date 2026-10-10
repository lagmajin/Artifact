module;
#include <compare>
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
#include <QRectF>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <Texture.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <BasicMath.hpp>
export module Artifact.Render.RenderCommandBuffer;

import Text.Style;
import Graphics.ParticleData;

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
    float4              tint        = {1.0f, 1.0f, 1.0f, 1.0f};
    float               opacity     = 1.0f;
    float               rollDegrees = 0.0f;
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

using DrawPacket = std::variant<
    SolidRectPkt, SolidRectXformPkt, GradientRectPkt,
    LinePkt, QuadPkt, DotLinePkt, SolidTriPkt, SolidCirclePkt,
    CheckerboardPkt, GridPkt, RectOutlinePkt,
    SpritePkt, SpriteXformPkt, MaskedSpritePkt,
    AtlasSpritePkt, AtlasSpriteXformPkt, TexturedTriangleXformPkt,
    BillboardPkt, BillboardImagePkt, ParticlePkt,
    GlyphTextPkt, GlyphTextXformPkt
>;

class RenderCommandBuffer {
public:
    void setTargetRTV(ITextureView* target) {
        retainedTargetRTV_ = target;
        targetRTV_ = target;
    }

    ITextureView* targetRTV() const noexcept { return targetRTV_; }

    void reset()  {
        packets_.clear();
        targetRTV_ = nullptr;
        retainedTargetRTV_ = nullptr;
    }
    // Reserve on the owning frame/worker slot before recording so steady-state
    // append and buffer merges can reuse storage.
    void reserve(std::size_t packetCapacity) { packets_.reserve(packetCapacity); }
    std::size_t capacity() const noexcept { return packets_.capacity(); }

    template <typename Packet>
    void append(Packet&& packet) {
        packets_.emplace_back(std::forward<Packet>(packet));
        pinTextureViews(packets_.back());
    }

    // Append without allowing vector growth. Reserve on a cold path first;
    // false means the packet was not appended and the buffer is unchanged.
    template <typename Packet>
    bool tryAppend(Packet&& packet) {
        if (packets_.size() >= packets_.capacity()) return false;
        packets_.emplace_back(std::forward<Packet>(packet));
        pinTextureViews(packets_.back());
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
    const std::vector<DrawPacket>& packets() const { return packets_; }

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
        for (auto& packet : source.packets_) {
            packets_.emplace_back(std::move(packet));
        }
        source.reset();
    }
    static void pinTextureViews(DrawPacket& pkt) {
        std::visit([](auto& packet) {
            if constexpr (requires { packet.pSRV; packet.retainedSRV; }) {
                packet.retainedSRV = packet.pSRV;
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
