module;

#include <QJsonArray>

module Artifact.Project;

import Tracking.MotionTracker;

namespace Artifact {

QJsonArray serializeProjectTrackers()
{
    return ArtifactCore::TrackerManager::instance().toJson();
}

} // namespace Artifact
