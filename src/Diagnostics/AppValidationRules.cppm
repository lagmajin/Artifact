module;
#include <algorithm>
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
import Memory.SharedPtr;

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
            auto diagnostic = ArtifactCore::ProjectDiagnostic::createMissingFile(
                sourcePath,
                layer->id().toString());
            diagnostic.setSourceCompId(comp->id().toString());
            diagnostics.push_back(diagnostic);
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
        auto diagnostic = ArtifactCore::ProjectDiagnostic::createPerformanceWarning(
            QString("高解像度コンポジション: %1x%2").arg(width).arg(height),
            comp->id().toString());
        diagnostic.setSourceCompId(comp->id().toString());
        diagnostics.push_back(diagnostic);
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
    QHash<QString, SharedPtr<ArtifactAbstractLayer>> layerMap;
    for (const auto& layer : layers) {
        if (layer) {
            layerMap[layer->id().toString()] = layer;
        }
    }
    QSet<QString> reportedCycleKeys;

    // Check each layer's matte references
    for (const auto& layer : layers) {
        if (!layer) continue;

        const auto matteRefs = layer->matteReferences();
        if (matteRefs.empty()) continue;

        const QString layerId = layer->id().toString();
        const QString layerName = layer->layerName();

        for (const auto& ref : matteRefs) {
            if (!ref.enabled) continue;
            if (ref.sourceLayerId.isNil()) continue;

            const QString sourceId = ref.sourceLayerId.toString();

            // Check 1: missing source
            if (!layerMap.contains(sourceId) && ref.sourceAssetPath.isEmpty()) {
                auto diagnostic = ArtifactCore::ProjectDiagnostic::createMissingMatte(
                    QStringLiteral("Matte source '%1' not found").arg(sourceId),
                    layerId);
                diagnostic.setSourceCompId(comp->id().toString());
                diagnostics.push_back(diagnostic);
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
                d.setSourceCompId(comp->id().toString());
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

        // Check 4: cycle detection across every enabled matte edge.
        const auto reportCycle = [&](const QString& cycleId,
                                     const QStringList& chain) {
            const int cycleStart = chain.indexOf(cycleId);
            if (cycleStart < 0) return;

            QStringList cycleIds;
            for (int i = cycleStart; i < chain.size(); ++i) {
                cycleIds.push_back(chain.at(i));
            }
            QStringList sortedCycleIds = cycleIds;
            std::sort(sortedCycleIds.begin(), sortedCycleIds.end());
            const QString cycleKey = sortedCycleIds.join(QStringLiteral("|"));
            if (reportedCycleKeys.contains(cycleKey)) return;
            reportedCycleKeys.insert(cycleKey);

            QString cycleStr;
            for (const auto& id : cycleIds) {
                if (!cycleStr.isEmpty()) cycleStr += QStringLiteral(" → ");
                const auto cycleLayer = layerMap.value(id);
                cycleStr += cycleLayer ? cycleLayer->layerName() : id;
            }
            if (!cycleStr.isEmpty()) cycleStr += QStringLiteral(" → ");
            const auto firstInCycle = layerMap.value(cycleId);
            cycleStr += firstInCycle ? firstInCycle->layerName() : cycleId;

            diagnostics.push_back(
                ArtifactCore::ProjectDiagnostic::createCircularDependency(
                    cycleStr, comp->id().toString()));
            diagnostics.back().setSourceLayerId(cycleId);
        };

        QSet<QString> path;
        QStringList chain;
        const auto visit = [&](const QString& currentId,
                               const auto& self) -> void {
            if (path.contains(currentId)) {
                reportCycle(currentId, chain);
                return;
            }
            const auto currentLayer = layerMap.value(currentId);
            if (!currentLayer) return;

            path.insert(currentId);
            chain.push_back(currentId);
            for (const auto& ref : currentLayer->matteReferences()) {
                if (!ref.enabled || ref.sourceLayerId.isNil()) continue;
                const QString nextId = ref.sourceLayerId.toString();
                if (!layerMap.contains(nextId) || nextId == currentId) continue;
                self(nextId, self);
            }
            chain.removeLast();
            path.remove(currentId);
        };
        visit(layerId, visit);
    }

    return diagnostics;
}

} // namespace Artifact
