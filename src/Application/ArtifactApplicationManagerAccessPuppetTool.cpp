module Artifact.Application.Manager.Access;

import Artifact.Tool.PuppetTool;

namespace Artifact
{
void* createApplicationPuppetTool()
{
  return new ArtifactPuppetTool();
}

void destroyApplicationPuppetTool(void* object)
{
  delete static_cast<ArtifactPuppetTool*>(object);
}
}
