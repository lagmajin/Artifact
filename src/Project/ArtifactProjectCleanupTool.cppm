module;
#include <utility>

#include <QStringList>
#include <QSet>
#include <QDebug>
#include <QFileInfo>
#include <QVariant>
#include <QMetaType>

module Artifact.Project.Cleanup;

import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Property.Abstract;
import Property.Group;

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

    auto maybeTrackPath = [&](const QString& maybePath) {
        const QString trimmed = maybePath.trimmed();
        if (trimmed.isEmpty()) return;
        usedAssetPaths.insert(trimmed);
        const QFileInfo fi(trimmed);
        if (!fi.fileName().isEmpty()) {
            usedAssetPaths.insert(fi.fileName()); // fallback match key
        }
    };

    // 2. 使用中アセットの収集 (コンポジション内の全レイヤーを調査)
    std::function<void(ProjectItem*)> collectUsed = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                auto comp = res.ptr.lock();
                if (comp) {
                    for (auto layer : comp->allLayer()) {
                        if (!layer) continue;
                        const auto groups = layer->getLayerPropertyGroups();
                        for (const auto& group : groups) {
                            for (const auto& prop : group.allProperties()) {
                                if (!prop) continue;
                                const QString propName = prop->getName().toLower();
                                if (!propName.contains(QStringLiteral("source")) &&
                                    !propName.contains(QStringLiteral("path")) &&
                                    !propName.contains(QStringLiteral("file"))) {
                                    continue;
                                }
                                const QVariant v = prop->getValue();
                                if (v.typeId() == QMetaType::QString) {
                                    maybeTrackPath(v.toString());
                                } else if (v.canConvert<QString>()) {
                                    maybeTrackPath(v.toString());
                                }
                            }
                        }
                    }
                }
            }
        }
        for (auto child : item->children) collectUsed(child);
    };
    for (auto root : items) collectUsed(root);

    // 差分（未使用）を抽出
    QStringList unused;
    for (const auto& path : allAssetPaths) {
        QFileInfo fi(path);
        const QString fileName = fi.fileName();
        if (usedAssetPaths.contains(path) || (!fileName.isEmpty() && usedAssetPaths.contains(fileName))) {
            continue;
        }
        unused.append(path);
    }
    return unused;
}

int ArtifactProjectCleanupTool::removeUnusedAssets(ArtifactProject* project) {
    // 実際のリセットロジック（現在未実装）
    qDebug() << "ArtifactProjectCleanupTool::removeUnusedAssets: Not fully implemented yet.";
    return 0;
}

} // namespace Artifact
