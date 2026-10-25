module Artifact.Application.Manager.Access;

import Artifact.Tool.Brush;

namespace Artifact
{
void* createApplicationBrushTool()
{
  return new ArtifactBrushTool();
}

void destroyApplicationBrushTool(void* object)
{
  delete static_cast<ArtifactBrushTool*>(object);
}
}
