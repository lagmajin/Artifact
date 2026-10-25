module Artifact.Application.Manager.Access;

import Artifact.Project.Manager;

namespace Artifact
{
void* applicationProjectManagerOpaque()
{
  static ArtifactProjectManager manager;
  return &manager;
}
}
