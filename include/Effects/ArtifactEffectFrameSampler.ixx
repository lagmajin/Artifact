module;
#include <cstdint>
#include <unordered_map>
#include <utility>

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
/// Simple ring of last N frames identified by composition frame number.
class ArtifactEffectFrameSampler : public IEffectFrameSampler
{
public:
    ArtifactEffectFrameSampler() = default;
    ~ArtifactEffectFrameSampler() override = default;

    /// Store the current frame's rendered result for the active layer.
    /// Called by the render host after rasterizer effects complete
    /// for a given layer + composition frame.
    void storeLayerFrame(const QString& layerId,
                         std::int64_t  compositionFrame,
                         const ImageF32x4RGBAWithCache& image);

    /// Maximum number of past frames to retain per layer.
    void setMaxHistoryFrames(int count) { maxHistoryFrames_ = count; }
    int  maxHistoryFrames() const { return maxHistoryFrames_; }

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

private:
    QString activeLayerId_;

    struct LayerFrameHistory
    {
        std::unordered_map<std::int64_t, ImageF32x4RGBAWithCache> frames;
    };

    std::unordered_map<QString, LayerFrameHistory> history_;
    int maxHistoryFrames_ = 64;
};

} // namespace Artifact
