module;

#include <QString>
#include <QVector>
#include <QSet>
#include <QStack>
#include <QMap>

module Artifact.Project.Health;

import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Utils.Id;

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

    for (const auto& issue : report.issues) {
        if (issue.severity == HealthIssueSeverity::Error) {
            report.isHealthy = false;
            break;
        }
    }

    return report;
}

void ArtifactProjectHealthChecker::checkCircularReferences(ArtifactProject* project, ProjectHealthReport& report) {
    // 循環参照チェック (コンポジション A がコンポジション B を含み、B が A を含むようなケース)
    // 現状のレイヤー構造に PreComposeLayer 等がある場合、その依存グラフを走査します。
    
    // TODO: ここにグラフ巡回(DFS)による閉路検出ロジックを実装
    // 今回はスタブとして「チェック済み」のメッセージを残します。
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
                    if (range.duration().count() <= 0) {
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

} // namespace Artifact
