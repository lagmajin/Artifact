module;

#include <QStringList>
#include <QSet>
#include <QDebug>

module Artifact.Project.Cleanup;

import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Property.Abstract;

namespace Artifact {

QStringList ArtifactProjectCleanupTool::findUnusedAssetPaths(ArtifactProject* project) {
    if (!project) return {};

    QSet<QString> allAssetPaths;
    QSet<QString> usedAssetPaths;

    auto items = project->projectItems();
    
    // 1. 全アセットパスの収集
    std::function<void(ProjectItem*)> collectAll = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Footage) {
            auto* footage = static_cast<FootageItem*>(item);
            allAssetPaths.insert(footage->filePath);
        }
        for (auto child : item->children) collectAll(child);
    };
    for (auto root : items) collectAll(root);

    // 2. 使用中アセットの収集 (コンポジション内の全レイヤーを調査)
    // 注意: 現状のレイヤー実装に "SourcePath" などの統一プロパティがあると仮定するか、
    // 将来的な拡張ポイントとしてスタブ化します。
    std::function<void(ProjectItem*)> collectUsed = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                auto comp = res.ptr.lock();
                if (comp) {
                    for (auto layer : comp->allLayer()) {
                        // ここでレイヤーが持っているアセットパスを特定するロジックが入る
                        // 例: layer->getProperty("Source")->getValue().toString() など
                    }
                }
            }
        }
        for (auto child : item->children) collectUsed(child);
    };
    for (auto root : items) collectUsed(root);

    // 差分（未使用）を抽出
    QSet<QString> unusedSet = allAssetPaths - usedAssetPaths;
    return unusedSet.values();
}

int ArtifactProjectCleanupTool::removeUnusedAssets(ArtifactProject* project) {
    // 実際のリセットロジック（現在未実装）
    qDebug() << "ArtifactProjectCleanupTool::removeUnusedAssets: Not fully implemented yet.";
    return 0;
}

} // namespace Artifact
