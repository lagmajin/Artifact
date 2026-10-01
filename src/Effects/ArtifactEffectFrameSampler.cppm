module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

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
// LayerFrameHistory
// ---------------------------------------------------------------------------

// A stored frame is CPU-side RGBA float data.  Use the image dimensions rather
// than a per-pixel stride query so the budget tracks the real footprint of
// what is retained.
std::size_t ArtifactEffectFrameSampler::LayerFrameHistory::frameBytes(
    const ImageF32x4RGBAWithCache& image) const
{
    const int width  = image.width();
    const int height = image.height();
    if (width <= 0 || height <= 0)
        return 0;

    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u
         * sizeof(float);
}

void ArtifactEffectFrameSampler::LayerFrameHistory::insertFrame(
    std::int64_t                     frame,
    const ImageF32x4RGBAWithCache&    image,
    std::uint64_t                    revision)
{
    // Re-storing an existing frame must not double count bytes or duplicate
    // the order entry.
    if (frames.find(frame) != frames.end())
        eraseFrame(frame);

    const std::size_t bytes = frameBytes(image);
    frames[frame]  = image;
    revisions[frame] = revision;
    order.push_back(frame);
    retainedBytes += bytes;
}

void ArtifactEffectFrameSampler::LayerFrameHistory::eraseFrame(std::int64_t frame)
{
    const auto frameIt = frames.find(frame);
    if (frameIt == frames.end())
        return;

    const std::size_t bytes = frameBytes(frameIt->second);
    frames.erase(frameIt);
    revisions.erase(frame);
    if (retainedBytes >= bytes)
        retainedBytes -= bytes;
    else
        retainedBytes = 0;

    for (auto orderIt = order.begin(); orderIt != order.end(); ++orderIt)
    {
        if (*orderIt == frame)
        {
            order.erase(orderIt);
            break;
        }
    }
}

void ArtifactEffectFrameSampler::LayerFrameHistory::trim(int maxFrames,
                                                          std::size_t byteBudget)
{
    // Bounded by frame count, then by bytes.  The front of `order` is the
    // oldest retained frame, so both loops are bounded by how far over the
    // limit we are and each eraseFrame is O(history) at worst.  This runs
    // only on store, which already copies the frame, so it adds no new
    // allocation of its own beyond the order node.
    while (!order.empty() && static_cast<int>(frames.size()) > maxFrames)
    {
        eraseFrame(order.front());
    }

    while (!order.empty() && retainedBytes > byteBudget)
    {
        eraseFrame(order.front());
    }

    // A single frame larger than the whole budget is dropped rather than
    // retained, so the budget stays a hard bound.
    if (retainedBytes > byteBudget)
        clear();
}

void ArtifactEffectFrameSampler::LayerFrameHistory::clear()
{
    frames.clear();
    revisions.clear();
    order.clear();
    retainedBytes = 0;
}

// ---------------------------------------------------------------------------
// ArtifactEffectFrameSampler
// ---------------------------------------------------------------------------

void ArtifactEffectFrameSampler::storeLayerFrame(
    const QString&                layerId,
    const std::int64_t            compositionFrame,
    const ImageF32x4RGBAWithCache& image,
    std::uint64_t                 revision)
{
    if (layerId.isEmpty() || maxHistoryFrames_ <= 0)
        return;

    auto& layerHistory = history_[layerId];
    layerHistory.insertFrame(compositionFrame, image, revision);
    layerHistory.trim(maxHistoryFrames_, historyByteBudget_);
}

std::size_t ArtifactEffectFrameSampler::invalidateIfRevisionChanged(
    const QString& layerId, std::uint64_t revision)
{
    const auto histIt = history_.find(layerId);
    if (histIt == history_.end())
        return 0;

    auto& layerHistory = histIt->second;

    // Collect first, then erase.  Erasing while iterating `revisions` would
    // invalidate the iterator, and eraseFrame also touches that map.
    std::vector<std::int64_t> stale;
    for (const auto& [frame, frameRevision] : layerHistory.revisions)
    {
        if (frameRevision != revision)
            stale.push_back(frame);
    }

    for (const std::int64_t frame : stale)
        layerHistory.eraseFrame(frame);

    return stale.size();
}

std::size_t ArtifactEffectFrameSampler::invalidateLayer(const QString& layerId)
{
    const auto histIt = history_.find(layerId);
    if (histIt == history_.end())
        return 0;

    histIt->second.clear();
    return 1;
}

void ArtifactEffectFrameSampler::invalidateAll()
{
    for (auto& [layerId, layerHistory] : history_)
    {
        (void)layerId;
        layerHistory.clear();
    }
}

bool ArtifactEffectFrameSampler::sampleCurrentLayerFrame(
    const std::int64_t    compositionFrame,
    ImageF32x4RGBAWithCache& out)
{
    if (activeLayerId_.isEmpty())
        return false;

    return copyFrame(activeLayerId_, compositionFrame, out);
}

bool ArtifactEffectFrameSampler::sampleCurrentLayerFrameRelative(
    const std::int64_t    frameOffset,
    ImageF32x4RGBAWithCache& out)
{
    if (activeLayerId_.isEmpty())
        return false;

    return copyFrame(activeLayerId_, currentCompositionFrame_ + frameOffset, out);
}

bool ArtifactEffectFrameSampler::sampleNamedInput(
    const QString&       inputId,
    const std::int64_t   compositionFrame,
    ImageF32x4RGBAWithCache& out)
{
    // Named inputs defer to the layer history keyed by inputId.
    if (inputId.isEmpty())
        return false;

    return copyFrame(inputId, compositionFrame, out);
}

bool ArtifactEffectFrameSampler::copyFrame(
    const QString& historyKey, std::int64_t frame, ImageF32x4RGBAWithCache& out)
{
    const auto histIt = history_.find(historyKey);
    if (histIt == history_.end())
        return false;

    const auto& frames = histIt->second.frames;
    const auto  frameIt = frames.find(frame);
    if (frameIt == frames.end())
        return false;

    // A stored frame is immutable once recorded, so handing out a const reference
    // is safe; the caller copies only if it needs to mutate.
    out = frameIt->second;
    return true;
}

} // namespace Artifact
