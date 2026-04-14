module;
#include <vector>
#include <QString>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

module Artifact.Diagnostics.AppValidationRules;

import Core.Diagnostics.DiagnosticEngine;
import Core.Diagnostics.ProjectDiagnostic;
import Artifact.Composition._2D;

namespace Artifact {

// ============================================================================
// ArtifactMissingFileRule
// ============================================================================

ArtifactMissingFileRule::ArtifactMissingFileRule() {
    name_ = "MissingFileValidation";
    enabled_ = true;
}

auto ArtifactMissingFileRule::validate(const void* project) -> std::vector<ArtifactCore::ProjectDiagnostic> {
    std::vector<ArtifactCore::ProjectDiagnostic> diagnostics;
    auto* comp = static_cast<const ArtifactComposition*>(project);
    if (!comp) return diagnostics;

    // コンポジション内の全レイヤーをチェック
    for (const auto& layer : comp->allLayer()) {
        if (!layer) continue;

        auto json = layer->toJson();
        QString sourcePath;

        // 様々なレイヤータイプのソースパスを確認
        if (json.contains("video.sourcePath")) {
            sourcePath = json.value("video.sourcePath").toString();
        } else if (json.contains("image.sourcePath")) {
            sourcePath = json.value("image.sourcePath").toString();
        } else if (json.contains("svg.sourcePath")) {
            sourcePath = json.value("svg.sourcePath").toString();
        } else if (json.contains("sourcePath")) {
            sourcePath = json.value("sourcePath").toString();
        }

        // パスがあり、かつファイルが存在しない場合
        if (!sourcePath.isEmpty() && !QFileInfo::exists(sourcePath)) {
            diagnostics.push_back(
                ArtifactCore::ProjectDiagnostic::createMissingFile(
                    sourcePath,
                    QString::fromStdString(layer->id().toString()) // IDを文字列に変換して渡す
                )
            );
        }
    }

    return diagnostics;
}

// ============================================================================
// ArtifactPerformanceRule
// ============================================================================

ArtifactPerformanceRule::ArtifactPerformanceRule() {
    name_ = "PerformanceValidation";
    enabled_ = true;
}

auto ArtifactPerformanceRule::validate(const void* project) -> std::vector<ArtifactCore::ProjectDiagnostic> {
    std::vector<ArtifactCore::ProjectDiagnostic> diagnostics;
    auto* comp = static_cast<const ArtifactComposition*>(project);
    if (!comp) return diagnostics;

    auto json = comp->toJson();
    int width = json.value("width").toInt(0);
    int height = json.value("height").toInt(0);

    // 解像度が 4K (3840x2160) を超える場合に警告
    if ((width > 3840 || height > 2160) && (width > 0 && height > 0)) {
        diagnostics.push_back(
            ArtifactCore::ProjectDiagnostic::createPerformanceWarning(
                QString("高解像度コンポジション: %1x%2").arg(width).arg(height),
                QString::fromStdString(comp->id().toString())
            )
        );
    }

    return diagnostics;
}

} // namespace Artifact
