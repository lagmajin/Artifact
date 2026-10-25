module Artifact.Application.Manager.Access;

import Artifact.Service.ActiveContext;

namespace Artifact
{
void* createApplicationActiveContextService()
{
  return new ArtifactActiveContextService();
}

void destroyApplicationActiveContextService(void* object)
{
  delete static_cast<ArtifactActiveContextService*>(object);
}
}
