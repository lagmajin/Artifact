module;
#include <utility>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
export module Artifact.Project.Packager;


import Artifact.Project;

export namespace Artifact {

struct PackageSettings {
    QString targetDir;
    bool includePreviewCache = false;
    bool renameAssetsToHash = false;
};

class ArtifactProjectPackager {
public:
    // プロジェクトで使用している全外部ファイルを targetDir にコピーし、
    // パスを書き換えた新しいプロジェクトファイルを保存する
    static bool collectAndPackage(ArtifactProject* project, const PackageSettings& settings);

private:
    static QStringList getAllExternalFiles(ArtifactProject* project);
};

} // namespace Artifact
