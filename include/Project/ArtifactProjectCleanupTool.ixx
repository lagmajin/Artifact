module;
#include <utility>

#include <QStringList>
export module Artifact.Project.Cleanup;

import Artifact.Project;

export namespace Artifact {

class ArtifactProjectCleanupTool {
public:
    // 全アセット（FootageItem 等）のうち、どのレイヤー（Composition 内）からも参照されていないパスのリストを返す
    static QStringList findUnusedAssetPaths(ArtifactProject* project);

    // 未使用アセットをプロジェクトツリー（ownedItems）から物理的に削除する
    static int removeUnusedAssets(ArtifactProject* project);
};

} // namespace Artifact
