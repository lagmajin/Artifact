module Artifact.Application.Manager.Access;

import Artifact.Tool.MotionSketchTool;

namespace Artifact
{
void* createApplicationMotionSketchTool()
{
  return new ArtifactMotionSketchTool();
}

void destroyApplicationMotionSketchTool(void* object)
{
  delete static_cast<ArtifactMotionSketchTool*>(object);
}
}
