module;
#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <utility>

#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"

#include <QString>
#include <QSize>

export module Artifact.Effect.FrameSampler;

import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

export namespace Artifact
{
using namespace ArtifactCore;

/// Stores rendered layer frames for temporal effect lookback.
/// Bounded ring of recent frames identified by composition frame number,
/// so a temporal effect can read frames that are not the one being evaluated.
///
/// Retention is bounded two ways: by frame count (setMaxHistoryFrames) and by
/// CPU bytes (setHistoryByteBudget).  Eviction is oldest-first by insertion
/// order, which stays correct for reverse playback and for seek targets.
class LIBRARY_DLL_API ArtifactEffectFrameSampler : public IEffectFrameSampler
{
public:
    ArtifactEffectFrameSampler() = default;
    ~ArtifactEffectFrameSampler() override = default;

    /// Store the current frame's rendered result for the active layer.
    /// Called by the render host after rasterizer effects complete
    /// for a given layer + composition frame.  The revision identifies the
    /// layer content/parameter state the frame was rendered at; store a
    /// different revision to invalidate older lookback frames.
    void storeLayerFrame(const QString& layerId,
                         std::int64_t  compositionFrame,
                         const ImageF32x4RGBAWithCache& image,
                         std::uint64_t revision = 0);

    /// Maximum number of past frames to retain per layer.
    void setMaxHistoryFrames(int count) { maxHistoryFrames_ = count; }
    int  maxHistoryFrames() const { return maxHistoryFrames_; }

    /// Hard bound on the CPU bytes retained for temporal lookback.
    /// Frames are evicted oldest-first once the budget is exceeded.
    /// A single frame larger than the budget is never retained.
    void setHistoryByteBudget(std::size_t bytes) { historyByteBudget_ = bytes; }
    std::size_t historyByteBudget() const { return historyByteBudget_; }

    /// Drop every retained frame whose stored revision differs from the given
    /// one.  Hosts call this before evaluating temporal effects so that a
    /// parameter or content change cannot leak stale pixels into a lookback.
    /// Returns the number of dropped frames.
    std::size_t invalidateIfRevisionChanged(const QString& layerId,
                                            std::uint64_t revision);

    /// Drop every retained frame for one layer.
    std::size_t invalidateLayer(const QString& layerId);

    /// Drop every retained frame.  Hosts call this on seek, on any
    /// discontinuous time jump, and on composition change, because history
    /// recorded before the discontinuity is not a valid temporal neighbour.
    void invalidateAll();

    /// Singleton accessor for wiring into EffectContext factories.
    static ArtifactEffectFrameSampler& instance()
    {
        static ArtifactEffectFrameSampler s;
        return s;
    }

    /// IEffectFrameSampler overrides.
    bool sampleCurrentLayerFrame(
        std::int64_t compositionFrame,
        ImageF32x4RGBAWithCache& out) override;

    bool sampleCurrentLayerFrameRelative(
        std::int64_t frameOffset,
        ImageF32x4RGBAWithCache& out) override;

    bool sampleNamedInput(
        const QString& inputId,
        std::int64_t compositionFrame,
        ImageF32x4RGBAWithCache& out) override;

    /// Set the active layer ID used by the sampler.
    /// The render host should call this before effect dispatch.
    void setActiveLayerId(const QString& layerId) { activeLayerId_ = layerId; }
    QString activeLayerId() const { return activeLayerId_; }

    /// Bind relative-frame requests to the frame being evaluated.  This is
    /// required for seeks and non-sequential render-queue evaluation; using
    /// the newest cached frame is only correct during strictly forward playback.
    void setCurrentCompositionFrame(std::int64_t frame) { currentCompositionFrame_ = frame; }

private:
    /// Shared lookup + copy used by all three IEffectFrameSampler methods.
    bool copyFrame(const QString& historyKey,
                   std::int64_t frame,
                   ImageF32x4RGBAWithCache& out);

    QString activeLayerId_;
    std::int64_t currentCompositionFrame_ = 0;
    std::size_t historyByteBudget_ = 256u * 1024u * 1024u;

    struct LayerFrameHistory
    {
        std::unordered_map<std::int64_t, ImageF32x4RGBAWithCache> frames;
        // Insertion order, used for O(1) eviction.  Frames are appended in
        // render order and popped from the front, so the front is always the
        // oldest retained frame.  Unlike the previous smallest-key scan this
        // stays correct for reverse playback and for seek targets.
        std::list<std::int64_t> order;
        // Revision each stored frame was rendered at, so a content or
        // parameter change can invalidate history instead of leaking stale
        // pixels into a temporal lookback.
        std::unordered_map<std::int64_t, std::uint64_t> revisions;
        std::size_t retainedBytes = 0;

        std::size_t frameBytes(const ImageF32x4RGBAWithCache& image) const;
        void insertFrame(std::int64_t frame,
                         const ImageF32x4RGBAWithCache& image,
                         std::uint64_t revision);
        void eraseFrame(std::int64_t frame);
        // Limits are passed in because a nested class cannot reach the
        // enclosing type's members.
        void trim(int maxFrames, std::size_t byteBudget);
        void clear();
    };

    std::unordered_map<QString, LayerFrameHistory> history_;
    int maxHistoryFrames_ = 64;
};

} // namespace Artifact
