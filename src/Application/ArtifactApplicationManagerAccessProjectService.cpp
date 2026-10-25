module Artifact.Application.Manager.Access;

import Artifact.Service.Project;

namespace Artifact
{
void* applicationProjectServiceOpaque()
{
  return ArtifactProjectService::instance();
}
}
