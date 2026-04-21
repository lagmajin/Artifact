module;
#include <vector>
#include <QString>

export module Artifact.Diagnostics.AppValidationRules;

import Core.Diagnostics.DiagnosticEngine;
import Core.Diagnostics.ProjectDiagnostic;

export namespace Artifact {

class ArtifactMissingFileRule : public ArtifactCore::IValidationRule {
public:
    ArtifactMissingFileRule();
    auto validate(const void* project) -> std::vector<ArtifactCore::ProjectDiagnostic> override;
};

class ArtifactPerformanceRule : public ArtifactCore::IValidationRule {
public:
    ArtifactPerformanceRule();
    auto validate(const void* project) -> std::vector<ArtifactCore::ProjectDiagnostic> override;
};

class ArtifactMatteReferenceRule : public ArtifactCore::IValidationRule {
public:
    ArtifactMatteReferenceRule();
    auto validate(const void* project) -> std::vector<ArtifactCore::ProjectDiagnostic> override;
};

} // namespace Artifact
