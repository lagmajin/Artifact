module Artifact.Application.Manager.Access;

import Artifact.Tool.Manager;

namespace Artifact
{
void* createApplicationToolManager()
{
  return new ArtifactToolManager();
}

void destroyApplicationToolManager(void* object)
{
  delete static_cast<ArtifactToolManager*>(object);
}
}
