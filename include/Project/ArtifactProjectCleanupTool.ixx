module;
#include <utility>

#include <QStringList>
export module Artifact.Project.Cleanup;

import Artifact.Project;
import Artifact.Project.Items;

export namespace Artifact {

class ArtifactProjectCleanupTool {
public:
    // 全アセット（FootageItem 等）のうち、どのレイヤー（Composition 内）からも参照されていないパスのリストを返す
    static QStringList findUnusedAssetPaths(ArtifactProject* project);

    // 未使用アセットをプロジェクトツリー（ownedItems）から物理的に削除する
    static int removeUnusedAssets(ArtifactProject* project);
};

// Project View の「使用回数」表示の正本。
// 同一のツリー走査が Proxy / Presentation 側に重複していたため、本ツールへ
// 集約し UI 側は参照のみ行う。
int projectItemUsageCount(ArtifactProject* project, const ProjectItem* item);

} // namespace Artifact
