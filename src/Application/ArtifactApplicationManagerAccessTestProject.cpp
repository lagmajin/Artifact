module Artifact.Application.Manager.Access;

import Artifact.Test.ProjectManager;

namespace Artifact
{
void* createApplicationTestProjectManager()
{
  return new ArtifactTestProjectManager();
}

void destroyApplicationTestProjectManager(void* object)
{
  delete static_cast<ArtifactTestProjectManager*>(object);
}
}
