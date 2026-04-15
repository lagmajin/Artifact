module;
#include <vector>
#include <QString>

export module Artifact.Diagnostics.AppValidationRules;

import Core.Diagnostics.DiagnosticEngine;
import Core.Diagnostics.ProjectDiagnostic;

export namespace Artifact {

/// <summary>
/// ファイル参照切れを検出するルール
/// </summary>
class ArtifactMissingFileRule : public ArtifactCore::IValidationRule {
public:
    ArtifactMissingFileRule();
    auto validate(const void* project) -> std::vector<ArtifactCore::ProjectDiagnostic> override;
};

/// <summary>
/// 解像度などのパフォーマンス問題を検出するルール
/// </summary>
class ArtifactPerformanceRule : public ArtifactCore::IValidationRule {
public:
    ArtifactPerformanceRule();
    auto validate(const void* project) -> std::vector<ArtifactCore::ProjectDiagnostic> override;
};

} // namespace Artifact
