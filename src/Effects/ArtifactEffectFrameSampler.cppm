module;
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include <QString>
#include <QDebug>

module Artifact.Effect.FrameSampler;

import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace Artifact
{
using namespace ArtifactCore;

// ---------------------------------------------------------------------------
// ArtifactEffectFrameSampler
// ---------------------------------------------------------------------------

void ArtifactEffectFrameSampler::storeLayerFrame(
    const QString&                layerId,
    const std::int64_t            compositionFrame,
    const ImageF32x4RGBAWithCache& image)
{
    if (layerId.isEmpty() || maxHistoryFrames_ <= 0)
        return;

    auto& layerHistory = history_[layerId];
    layerHistory.frames[compositionFrame] = image;

    // Evict oldest entries if we exceeded the limit.
    if (static_cast<int>(layerHistory.frames.size()) > maxHistoryFrames_)
    {
        std::int64_t oldestKey = std::numeric_limits<std::int64_t>::max();
        for (const auto& [frame, _] : layerHistory.frames)
        {
            if (frame < oldestKey)
                oldestKey = frame;
        }
        layerHistory.frames.erase(oldestKey);
    }
}

bool ArtifactEffectFrameSampler::sampleCurrentLayerFrame(
    const std::int64_t    compositionFrame,
    ImageF32x4RGBAWithCache& out)
{
    if (activeLayerId_.isEmpty())
        return false;

    const auto histIt = history_.find(activeLayerId_);
    if (histIt == history_.end())
        return false;

    const auto& frames = histIt->second.frames;
    const auto  frameIt = frames.find(compositionFrame);
    if (frameIt == frames.end())
        return false;

    out = frameIt->second;
    return true;
}

bool ArtifactEffectFrameSampler::sampleCurrentLayerFrameRelative(
    const std::int64_t    frameOffset,
    ImageF32x4RGBAWithCache& out)
{
    if (activeLayerId_.isEmpty())
        return false;

    const auto histIt = history_.find(activeLayerId_);
    if (histIt == history_.end())
        return false;

    const auto& frames = histIt->second.frames;
    if (frames.empty())
        return false;

    // Find the "current" frame — assume the most recently stored frame
    // is the current one.
    std::int64_t currentKey = std::numeric_limits<std::int64_t>::min();
    for (const auto& [frame, _] : frames)
    {
        if (frame > currentKey)
            currentKey = frame;
    }

    const std::int64_t targetFrame = currentKey + frameOffset;
    const auto         frameIt = frames.find(targetFrame);
    if (frameIt == frames.end())
        return false;

    out = frameIt->second;
    return true;
}

bool ArtifactEffectFrameSampler::sampleNamedInput(
    const QString&       inputId,
    const std::int64_t   compositionFrame,
    ImageF32x4RGBAWithCache& out)
{
    // Named inputs defer to the layer history keyed by inputId.
    if (inputId.isEmpty())
        return false;

    const auto histIt = history_.find(inputId);
    if (histIt == history_.end())
        return false;

    const auto& frames = histIt->second.frames;
    const auto  frameIt = frames.find(compositionFrame);
    if (frameIt == frames.end())
        return false;

    out = frameIt->second;
    return true;
}

} // namespace Artifact
