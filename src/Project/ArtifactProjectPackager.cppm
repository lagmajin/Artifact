module;

#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDebug>

module Artifact.Project.Packager;

import Artifact.Project;
import Artifact.Project.Items;

namespace Artifact {

QStringList ArtifactProjectPackager::getAllExternalFiles(ArtifactProject* project) {
    QStringList files;
    auto items = project->projectItems();
    
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Footage) {
            auto* footage = static_cast<FootageItem*>(item);
            if (!footage->filePath.isEmpty()) {
                files.append(footage->filePath);
            }
        }
        for (auto child : item->children) traverse(child);
    };
    for (auto root : items) traverse(root);
    
    files.removeDuplicates();
    return files;
}

bool ArtifactProjectPackager::collectAndPackage(ArtifactProject* project, const PackageSettings& settings) {
    if (!project || settings.targetDir.isEmpty()) return false;

    QDir dir(settings.targetDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) return false;
    }

    QDir assetsDir(dir.filePath("Assets"));
    assetsDir.mkpath(".");

    QStringList sourceFiles = getAllExternalFiles(project);
    
    qDebug() << "Packaging" << sourceFiles.size() << "files...";

    for (const auto& srcPath : sourceFiles) {
        QFileInfo fi(srcPath);
        QString destPath = assetsDir.filePath(fi.fileName());
        
        if (QFile::exists(srcPath)) {
            if (!QFile::copy(srcPath, destPath)) {
                qDebug() << "Failed to copy:" << srcPath;
                // Continue with others or fail?
            }
        }
    }

    // TODO: プロジェクトファイルのパスを Assets/xxx に書き換えた
    // パッケージ版プロジェクトファイルを targetDir に出力するロジック
    
    return true;
}

} // namespace Artifact
