module;
module Artifact.Preview.Pipeline;

import Artifact.Layer.Svg;
import Artifact.Layer.FormParticle;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;

namespace Artifact
{
bool tryDrawSvgPreview(ArtifactIRenderer* renderer,
                      ArtifactAbstractLayer* layerPtr)
{
  if (const auto svgLayer = dynamic_cast<ArtifactSvgLayer*>(layerPtr)) {
    if (svgLayer->isLoaded()) {
      svgLayer->draw(renderer);
      return true;
    }
  }
  return false;
}

bool tryDrawOtherLayerPreview(ArtifactIRenderer* renderer,
                              ArtifactAbstractLayer* layerPtr)
{
  if (layerPtr->isParticleLayer()) {
    layerPtr->draw(renderer);
    return true;
   }
  if (const auto formParticleLayer = dynamic_cast<ArtifactFormParticleLayer*>(layerPtr)) {
    formParticleLayer->draw(renderer);
    return true;
   }
  return false;
}
}
