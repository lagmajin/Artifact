module Artifact.Application.Manager.Access;

import EnvironmentVariable;

namespace Artifact
{
void* createApplicationEnvironmentVariableManager()
{
  return new ArtifactCore::EnvironmentVariableManager();
}

void destroyApplicationEnvironmentVariableManager(void* object)
{
  delete static_cast<ArtifactCore::EnvironmentVariableManager*>(object);
}
}
