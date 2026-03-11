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

struct AutoRepairOptions {
    bool repairFrameRanges = true;
    bool removeMissingAssets = false;
    bool normalizeCompositionRanges = true;
    bool removeBrokenReferences = true;
};

struct AutoRepairResult {
    int fixedCount = 0;
    int skippedCount = 0;
    QVector<HealthIssue> appliedFixes;
};

class ArtifactProjectHealthChecker {
public:
    static ProjectHealthReport check(ArtifactProject* project);
    static AutoRepairResult checkAndRepair(ArtifactProject* project, const AutoRepairOptions& options = {});

private:
    static void checkCircularReferences(ArtifactProject* project, ProjectHealthReport& report);
    static void checkDuplicateIDs(ArtifactProject* project, ProjectHealthReport& report);
    static void checkFrameRanges(ArtifactProject* project, ProjectHealthReport& report);
    static void checkMissingAssets(ArtifactProject* project, ProjectHealthReport& report);
    static void checkBrokenReferences(ArtifactProject* project, ProjectHealthReport& report);
    static void repairFrameRanges(ArtifactProject* project, AutoRepairResult& result, const AutoRepairOptions& options);
    static void repairMissingAssets(ArtifactProject* project, AutoRepairResult& result, const AutoRepairOptions& options);
    static void repairBrokenReferences(ArtifactProject* project, AutoRepairResult& result, const AutoRepairOptions& options);
};

} // namespace Artifact
