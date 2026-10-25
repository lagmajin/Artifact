module;

#include <functional>
#include <QString>

module Artifact.Project;

import Artifact.Layer.Svg;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;
import Artifact.Layer.Abstract;
import Memory.SharedPtr;
import Utils.Id;

namespace Artifact {

bool tryCreateSvgLayerCopy(
    const ArtifactCore::CompositionID& compositionId,
    const ArtifactCore::SharedPtr<ArtifactAbstractLayer>& sourceLayer,
    const QString& layerName,
    const std::function<ArtifactLayerResult(ArtifactLayerInitParams&)>& createLayer,
    ArtifactLayerResult* result)
{
    if (!sourceLayer || !result) {
        return false;
    }
    const auto svgLayer = ArtifactCore::dynamicPointerCast<ArtifactSvgLayer>(
        sourceLayer);
    if (!svgLayer) {
        return false;
    }
    ArtifactSvgInitParams params(layerName);
    params.setSvgPath(svgLayer->sourcePath());
    *result = createLayer(params);
    return true;
}

} // namespace Artifact
