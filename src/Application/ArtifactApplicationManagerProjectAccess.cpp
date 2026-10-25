module Artifact.Application.Manager;

import Artifact.Service.Project;

namespace Artifact
{
ArtifactProjectService* ArtifactApplicationManager::projectService() const
{
  return ArtifactProjectService::instance();
}
}