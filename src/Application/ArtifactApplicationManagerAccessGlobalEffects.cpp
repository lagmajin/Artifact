module Artifact.Application.Manager.Access;

import Artifact.Effects.Manager;

namespace Artifact
{
void* createApplicationGlobalEffectManager()
{
  return new ArtifactGlobalEffectManager();
}

void destroyApplicationGlobalEffectManager(void* object)
{
  delete static_cast<ArtifactGlobalEffectManager*>(object);
}
}
