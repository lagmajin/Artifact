module;

#include <QJsonArray>

module Artifact.Project.Manager;

import Tracking.MotionTracker;

namespace Artifact {

void restoreProjectTrackers(const QJsonArray& trackerData)
{
    ArtifactCore::TrackerManager::instance().fromJson(trackerData);
}

} // namespace Artifact
