module;

#include <QList>

export module Artifact.Render.CompositionRevealPreparation;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Render.Pipeline;

export namespace Artifact {

// Keep Composition's reveal-resource setup behind the renderer context
// boundary. The caller chooses the render lane; this helper does not dispatch
// to a worker or change the immediate-context ownership contract.
bool prepareCompositionRevealResources(
    ArtifactIRenderer& renderer, RenderPipeline& pipeline,
    const QList<ArtifactAbstractLayerPtr>& layers) {
  const auto context = renderer.immediateContext();
  if (!context || !pipeline.prepareReveal(context.RawPtr())) {
    return false;
  }
  pipeline.prepareRevealMaps(context.RawPtr(), layers);
  return true;
}

} // namespace Artifact
