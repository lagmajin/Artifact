module;

#include <QString>
#include <QVector>
#include <QSet>

export module Artifact.Project.Health;

import Artifact.Project;
import Artifact.Composition.Abstract;
import Utils.Id;

export namespace Artifact {

enum class HealthIssueSeverity {
    Info,
    Warning,
    Error
};

struct HealthIssue {
    HealthIssueSeverity severity;
    QString message;
    QString targetName; // Composition name or Layer name
    QString category;   // "CircularReference", "InvalidID", "FrameRange", etc.
};

struct ProjectHealthReport {
    bool isHealthy = true;
    QVector<HealthIssue> issues;
};

class ArtifactProjectHealthChecker {
public:
    static ProjectHealthReport check(ArtifactProject* project);

private:
    static void checkCircularReferences(ArtifactProject* project, ProjectHealthReport& report);
    static void checkDuplicateIDs(ArtifactProject* project, ProjectHealthReport& report);
    static void checkFrameRanges(ArtifactProject* project, ProjectHealthReport& report);
};

} // namespace Artifact
