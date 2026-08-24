module;

#include <QString>
#include <QPointF>
#include <vector>

export module Artifact.Mask.RotoBridge;

import Artifact.Mask.Path;
import Artifact.Mask.LayerMask;
import Core.Mask.RotoMask;
import Utils.String.UniString;

export namespace Artifact {

// Conversion bridge between ArtifactCore::RotoMask (per-vertex animated,
// time-based keyframes) and Artifact::MaskPath (snapshot-based keyframes).
// These two modules evolved independently; this file provides lossless
// conversion at a single point in time.

[[nodiscard]] inline ArtifactCore::RotoVertex toRotoVertex(
    const MaskVertex& vertex) noexcept {
    return ArtifactCore::RotoVertex{vertex.position, vertex.inTangent,
                                    vertex.outTangent};
}

[[nodiscard]] inline MaskVertex toMaskVertex(
    const ArtifactCore::RotoVertex& vertex) noexcept {
    MaskVertex result;
    result.position = vertex.position;
    result.inTangent = vertex.inTangent;
    result.outTangent = vertex.outTangent;
    return result;
}

// Samples a RotoMask at `time` into a snapshot-based MaskPath.
[[nodiscard]] inline MaskPath toMaskPath(const ArtifactCore::RotoMask& roto,
                                         const double time) {
    MaskPath path;
    for (const auto& vertex : roto.sampleVertices(time)) {
        path.addVertex(toMaskVertex(vertex));
    }
    path.setClosed(roto.isClosed());
    path.setName(UniString(roto.name()));
    path.setMode(static_cast<MaskMode>(roto.mode()));
    path.setOpacity(roto.opacity(time));
    path.setFeather(roto.feather(time));
    path.setExpansion(roto.expansion(time));
    path.setInverted(roto.isInverted());
    return path;
}

// Converts a static MaskPath into an animated RotoMask at `time`.
[[nodiscard]] inline ArtifactCore::RotoMask toRotoMask(const MaskPath& path,
                                                       const double time) {
    ArtifactCore::RotoMask roto;
    roto.setName(path.name().toQString());
    roto.setClosed(path.isClosed());
    roto.setMode(static_cast<ArtifactCore::RotoMaskMode>(path.mode()));
    roto.setInverted(path.isInverted());
    roto.setOpacity(path.opacity(), time);
    roto.setFeather(path.feather(), time);
    roto.setExpansion(path.expansion(), time);

    const int count = path.vertexCount();
    // Iterate in reverse so the linked-list insertion order matches the
    // original vertex order.
    for (int i = count - 1; i >= 0; --i) {
        const auto& v = path.vertex(i);
        roto.addVertex(v.position, v.inTangent, v.outTangent);
    }
    return roto;
}

// Converts all paths of a LayerMask at `time`.
[[nodiscard]] inline Array<MaskPath> layerMaskToPaths(
    const LayerMask& mask, const double time) {
    Array<MaskPath> result;
    for (int i = 0; i < mask.maskPathCount(); ++i) {
        result.append(toMaskPath(mask.maskPath(i), time));
    }
    return result;
}

} // namespace Artifact
