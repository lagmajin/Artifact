module Artifact.Application.Manager.Access;

import Artifact.Composition.Manager;

namespace Artifact
{
void* createApplicationCompositionManager()
{
  return new ArtifactCompositionManager();
}

void destroyApplicationCompositionManager(void* object)
{
  delete static_cast<ArtifactCompositionManager*>(object);
}
}
