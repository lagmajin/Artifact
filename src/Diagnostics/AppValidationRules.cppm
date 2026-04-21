module;
#include <vector>
#include <QString>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QSet>

module Artifact.Diagnostics.AppValidationRules;

import Core.Diagnostics.DiagnosticEngine;
import Core.Diagnostics.ProjectDiagnostic;
import Artifact.Composition._2D;
import Artifact.Layer.Abstract;
import Artifact.Layer.Matte;

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
    for (const auto& layer : const_cast<ArtifactComposition*>(comp)->allLayer()) {
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
                    layer->id().toString() // IDを文字列に変換して渡す
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

    auto json = comp->toJson().object();
    int width = json.value("width").toInt(0);
    int height = json.value("height").toInt(0);

    // 解像度が 4K (3840x2160) を超える場合に警告
    if ((width > 3840 || height > 2160) && (width > 0 && height > 0)) {
        diagnostics.push_back(
            ArtifactCore::ProjectDiagnostic::createPerformanceWarning(
                QString("高解像度コンポジション: %1x%2").arg(width).arg(height),
                comp->id().toString()
            )
        );
    }

    return diagnostics;
}

// ============================================================================
// ArtifactMatteReferenceRule
// ============================================================================

ArtifactMatteReferenceRule::ArtifactMatteReferenceRule() {
    name_ = "MatteReferenceValidation";
    enabled_ = true;
}

auto ArtifactMatteReferenceRule::validate(const void* project) -> std::vector<ArtifactCore::ProjectDiagnostic> {
    std::vector<ArtifactCore::ProjectDiagnostic> diagnostics;
    auto* comp = static_cast<const ArtifactComposition*>(project);
    if (!comp) return diagnostics;

    const auto layers = const_cast<ArtifactComposition*>(comp)->allLayer();

    // Build lookup: layerId -> layer
    QHash<QString, std::shared_ptr<ArtifactAbstractLayer>> layerMap;
    for (const auto& layer : layers) {
        if (layer) {
            layerMap[layer->id().toString()] = layer;
        }
    }

    // Check each layer's matte references
    for (const auto& layer : layers) {
        if (!layer) continue;

        const auto matteRefs = layer->matteReferences();
        if (matteRefs.empty()) continue;

        const QString layerId = layer->id().toString();
        const QString layerName = layer->layerName();

        for (const auto& ref : matteRefs) {
            if (!ref.enabled) continue;

            const QString sourceId = ref.assetId.toString();

            // Check 1: missing source
            if (!layerMap.contains(sourceId)) {
                diagnostics.push_back(
                    ArtifactCore::ProjectDiagnostic::createMissingMatte(
                        QStringLiteral("Matte source '%1' not found").arg(sourceId),
                        layerId));
                continue;
            }

            // Check 2: self-reference
            if (sourceId == layerId) {
                diagnostics.push_back(
                    ArtifactCore::ProjectDiagnostic(
                        ArtifactCore::DiagnosticSeverity::Error,
                        ArtifactCore::DiagnosticCategory::Matte,
                        QStringLiteral("Layer '%1' references itself as matte source").arg(layerName)));
                auto& d = diagnostics.back();
                d.setSourceLayerId(layerId);
                d.setFixAction(QStringLiteral("Select a different layer as the matte source"));
                continue;
            }

            // Check 3: hidden source
            auto sourceLayer = layerMap.value(sourceId);
            if (sourceLayer && !sourceLayer->isVisible()) {
                diagnostics.push_back(
                    ArtifactCore::ProjectDiagnostic(
                        ArtifactCore::DiagnosticSeverity::Warning,
                        ArtifactCore::DiagnosticCategory::Matte,
                        QStringLiteral("Matte source '%1' for layer '%2' is hidden").arg(sourceLayer->layerName()).arg(layerName)));
                auto& d = diagnostics.back();
                d.setSourceLayerId(sourceId);
                d.setSourceCompId(comp->id().toString());
                d.setFixAction(QStringLiteral("Show the matte source layer"));
            }
        }

        // Check 4: cycle detection via matte chain
        QSet<QString> visited;
        QString currentId = layerId;
        std::vector<QString> chain;
        bool hasCycle = false;

        while (!currentId.isEmpty()) {
            if (visited.contains(currentId)) {
                hasCycle = true;
                break;
            }
            visited.insert(currentId);
            chain.push_back(currentId);

            auto currentLayer = layerMap.value(currentId);
            if (!currentLayer) break;

            const auto refs = currentLayer->matteReferences();
            bool foundNext = false;
            for (const auto& r : refs) {
                if (r.enabled && !r.assetId.isNil()) {
                    currentId = r.assetId.toString();
                    foundNext = true;
                    break;
                }
            }
            if (!foundNext) break;
        }

        if (hasCycle) {
            QString cycleStr;
            bool recording = false;
            for (const auto& id : chain) {
                if (id == currentId) recording = true;
                if (recording) {
                    if (!cycleStr.isEmpty()) cycleStr += QStringLiteral(" → ");
                    auto l = layerMap.value(id);
                    cycleStr += (l ? l->layerName() : id);
                }
            }
            cycleStr += QStringLiteral(" → ");
            auto firstInCycle = layerMap.value(currentId);
            cycleStr += (firstInCycle ? firstInCycle->layerName() : currentId);

            diagnostics.push_back(
                ArtifactCore::ProjectDiagnostic::createCircularDependency(
                    cycleStr, comp->id().toString()));
        }
    }

    return diagnostics;
}

} // namespace Artifact
