module;

#include <QString>
#include <QVector>
#include <QSet>
#include <QStack>
#include <QMap>
#include <QFileInfo>

module Artifact.Project.Health;

import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Composition; // CompositionLayerのインポート
import Utils.Id;
import Frame.Range;
import Frame.Position;

namespace Artifact {

ProjectHealthReport ArtifactProjectHealthChecker::check(ArtifactProject* project) {
    ProjectHealthReport report;
    if (!project) {
        report.isHealthy = false;
        report.issues.push_back({HealthIssueSeverity::Error, "Project pointer is null", "Project", "System"});
        return report;
    }

    checkCircularReferences(project, report);
    checkDuplicateIDs(project, report);
    checkFrameRanges(project, report);
    checkMissingAssets(project, report);

    for (const auto& issue : report.issues) {
        if (issue.severity == HealthIssueSeverity::Error) {
            report.isHealthy = false;
            break;
        }
    }

    return report;
}

AutoRepairResult ArtifactProjectHealthChecker::checkAndRepair(ArtifactProject* project, const AutoRepairOptions& options) {
    AutoRepairResult result;
    if (!project) {
        result.skippedCount++;
        result.appliedFixes.push_back({
            HealthIssueSeverity::Error,
            "Auto-repair skipped: project pointer is null",
            "Project",
            "System"
        });
        return result;
    }

    if (options.repairFrameRanges) {
        repairFrameRanges(project, result, options);
    }
    if (options.removeMissingAssets) {
        repairMissingAssets(project, result, options);
    }
    return result;
}

void ArtifactProjectHealthChecker::checkCircularReferences(ArtifactProject* project, ProjectHealthReport& report) {
    // 全コンポジションを収集
    auto items = project->projectItems();
    QMap<QString, ArtifactAbstractComposition*> compMap;
    QMap<QString, QString> compNames; // エラー報告用

    std::function<void(ProjectItem*)> gatherComps = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                if (auto comp = res.ptr.lock()) {
                    QString idStr = comp->id().toString();
                    compMap.insert(idStr, comp.get());
                    compNames.insert(idStr, compItem->name.toQString());
                }
            }
        }
        for (auto child : item->children) gatherComps(child);
    };

    for (auto root : items) gatherComps(root);

    // DFS (深さ優先探索) を用いた有向グラフの閉路検出
    QSet<QString> visited;
    QSet<QString> recStack;

    std::function<bool(const QString&, QStringList&)> dfs = [&](const QString& compId, QStringList& path) -> bool {
        visited.insert(compId);
        recStack.insert(compId);
        path.push_back(compNames.value(compId, compId)); // 名前をパスに記録

        if (compMap.contains(compId)) {
            auto comp = compMap[compId];
            for (auto layer : comp->allLayer()) {
                if (!layer) continue;
                // コンポジションレイヤーかどうかを判定
                if (auto compLayer = dynamic_cast<ArtifactCompositionLayer*>(layer.get())) {
                    QString targetId = compLayer->sourceCompositionId().toString();
                    if (!visited.contains(targetId)) {
                        if (dfs(targetId, path)) return true;
                    } else if (recStack.contains(targetId)) {
                        // 閉路を検出 (自身が再帰スタックに存在するノードに到達した)
                        path.push_back(compNames.value(targetId, targetId));
                        return true; 
                    }
                }
            }
        }

        recStack.remove(compId);
        path.pop_back();
        return false;
    };

    // 全てのコンポジションノードを起点としてチェック
    for (auto it = compMap.begin(); it != compMap.end(); ++it) {
        if (!visited.contains(it.key())) {
            QStringList path;
            if (dfs(it.key(), path)) {
                report.issues.push_back({
                    HealthIssueSeverity::Error,
                    QString("Circular reference (Infinite Loop) detected in composition nesting: %1").arg(path.join(" -> ")),
                    compNames.value(it.key(), "Composition"),
                    "CircularReference"
                });
            }
        }
    }
}

void ArtifactProjectHealthChecker::checkDuplicateIDs(ArtifactProject* project, ProjectHealthReport& report) {
    auto items = project->projectItems();
    
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                auto comp = res.ptr.lock();
                if (comp) {
                    QSet<QString> layerIds;
                    auto layers = comp->allLayer();
                    for (auto layer : layers) {
                        if (!layer) continue;
                        QString idStr = layer->id().toString();
                        if (layerIds.contains(idStr)) {
                            report.issues.push_back({
                                HealthIssueSeverity::Error,
                                QString("Duplicate Layer ID detected: %1").arg(idStr),
                                compItem->name.toQString(),
                                "DuplicateID"
                            });
                        }
                        layerIds.insert(idStr);
                    }
                }
            }
        }
        
        for (auto child : item->children) traverse(child);
    };

    for (auto root : items) traverse(root);
}

void ArtifactProjectHealthChecker::checkFrameRanges(ArtifactProject* project, ProjectHealthReport& report) {
    auto items = project->projectItems();
    
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                auto comp = res.ptr.lock();
                if (comp) {
                    auto range = comp->frameRange();
                    if (range.duration() <= 0) {
                        report.issues.push_back({
                            HealthIssueSeverity::Warning,
                            "Composition duration is zero or negative",
                            compItem->name.toQString(),
                            "FrameRange"
                        });
                    }
                }
            }
        }
        
        for (auto child : item->children) traverse(child);
    };

    for (auto root : items) traverse(root);
}

void ArtifactProjectHealthChecker::checkMissingAssets(ArtifactProject* project, ProjectHealthReport& report) {
    auto items = project->projectItems();

    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;

        if (item->type() == eProjectItemType::Footage) {
            auto* footage = static_cast<FootageItem*>(item);
            QFileInfo fi(footage->filePath);
            if (!fi.exists() || !fi.isFile()) {
                report.issues.push_back({
                    HealthIssueSeverity::Error,
                    QString("Missing asset file: %1").arg(footage->filePath),
                    footage->name.toQString(),
                    "MissingAsset"
                });
            }
        }

        for (auto* child : item->children) {
            traverse(child);
        }
    };

    for (auto* root : items) {
        traverse(root);
    }
}

void ArtifactProjectHealthChecker::repairFrameRanges(ArtifactProject* project, AutoRepairResult& result, const AutoRepairOptions& options) {
    auto items = project->projectItems();
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto compRes = project->findComposition(compItem->compositionId);
            if (compRes.success) {
                if (auto comp = compRes.ptr.lock()) {
                    if (options.normalizeCompositionRanges) {
                        auto range = comp->frameRange();
                        if (range.duration() <= 0) {
                            const auto start = range.start();
                            comp->setFrameRange(ArtifactCore::FrameRange(start, start + 1));
                            result.fixedCount++;
                            result.appliedFixes.push_back({
                                HealthIssueSeverity::Warning,
                                QString("Fixed composition frame range to [%1, %2]").arg(start).arg(start + 1),
                                compItem->name.toQString(),
                                "FrameRange"
                            });
                        }
                    }

                    for (const auto& layer : comp->allLayer()) {
                        if (!layer) continue;
                        const int64_t inFrame = layer->inPoint().framePosition();
                        const int64_t outFrame = layer->outPoint().framePosition();
                        if (outFrame <= inFrame) {
                            layer->setOutPoint(ArtifactCore::FramePosition(inFrame + 1));
                            result.fixedCount++;
                            result.appliedFixes.push_back({
                                HealthIssueSeverity::Warning,
                                QString("Adjusted layer out-point from %1 to %2").arg(outFrame).arg(inFrame + 1),
                                layer->layerName(),
                                "FrameRange"
                            });
                        }
                    }
                }
            }
        }
        for (auto* child : item->children) traverse(child);
    };

    for (auto* root : items) {
        traverse(root);
    }
}

void ArtifactProjectHealthChecker::repairMissingAssets(ArtifactProject* project, AutoRepairResult& result, const AutoRepairOptions& options) {
    if (!options.removeMissingAssets) return;

    QVector<ProjectItem*> toRemove;
    auto items = project->projectItems();
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Footage) {
            auto* footage = static_cast<FootageItem*>(item);
            QFileInfo fi(footage->filePath);
            if (!fi.exists() || !fi.isFile()) {
                toRemove.push_back(item);
            }
        }
        for (auto* child : item->children) traverse(child);
    };
    for (auto* root : items) {
        traverse(root);
    }

    for (auto* item : toRemove) {
        auto* footage = static_cast<FootageItem*>(item);
        const QString filePath = footage->filePath;
        const QString targetName = footage->name.toQString();
        if (project->removeItem(item)) {
            result.fixedCount++;
            result.appliedFixes.push_back({
                HealthIssueSeverity::Warning,
                QString("Removed missing asset entry: %1").arg(filePath),
                targetName,
                "MissingAsset"
            });
        } else {
            result.skippedCount++;
        }
    }
}

} // namespace Artifact
