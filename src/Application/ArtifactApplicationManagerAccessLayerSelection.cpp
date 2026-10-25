module Artifact.Application.Manager.Access;

import Artifact.Layers.Selection.Manager;

namespace Artifact
{
void* applicationLayerSelectionManagerOpaque()
{
  return ArtifactLayerSelectionManager::instance();
}
}
