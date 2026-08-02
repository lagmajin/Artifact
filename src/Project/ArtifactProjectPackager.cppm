module;
#include <utility>

#include <QString>
#include <QStringList>
#include <QVector>
#include <QDir>
#include <QHash>
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QDebug>

module Artifact.Project.Packager;

import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Project.Statistics;

namespace Artifact {

namespace {

QString packagedAssetNameForSource(const QString& srcPath,
                                   const PackageSettings& settings,
                                   const QSet<QString>& usedNames) {
    QFileInfo fi(srcPath);
    QString baseName = fi.completeBaseName();
    QString extension = fi.suffix();

    if (settings.renameAssetsToHash) {
        const auto hashValue = static_cast<qulonglong>(qHash(srcPath));
        baseName = QStringLiteral("%1_%2")
                       .arg(QString::number(hashValue, 16))
                       .arg(baseName);
    }

    QString candidate = extension.isEmpty() ? baseName : QStringLiteral("%1.%2").arg(baseName, extension);
    if (!usedNames.contains(candidate)) {
        return candidate;
    }

    int suffix = 1;
    QString uniqueName = candidate;
    do {
        uniqueName = extension.isEmpty()
                         ? QStringLiteral("%1_%2").arg(baseName).arg(suffix)
                         : QStringLiteral("%1_%2.%3").arg(baseName).arg(suffix).arg(extension);
        ++suffix;
    } while (usedNames.contains(uniqueName));

    return uniqueName;
}

void rewriteFootagePaths(QJsonValue& value, const QHash<QString, QString>& pathMap) {
    if (value.isObject()) {
        QJsonObject obj = value.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (it.key() == QStringLiteral("filePath") && it.value().isString()) {
                const QString oldPath = it.value().toString();
                if (pathMap.contains(oldPath)) {
                    it.value() = pathMap.value(oldPath);
                }
            } else {
                QJsonValue child = it.value();
                rewriteFootagePaths(child, pathMap);
                it.value() = child;
            }
        }
        value = obj;
        return;
    }
    if (value.isArray()) {
        QJsonArray array = value.toArray();
        for (int i = 0; i < array.size(); ++i) {
            QJsonValue child = array.at(i);
            rewriteFootagePaths(child, pathMap);
            array[i] = child;
        }
        value = array;
    }
}

} // namespace

QStringList ArtifactProjectPackager::getAllExternalFiles(ArtifactProject* project) {
    if (!project) return {};
    ProjectMetadataValueCollector collector;
    const QVector<ArtifactCore::MetadataCollector*> collectors{&collector};
    ArtifactProjectStatistics::collectMetadata(project, collectors);
    return collector.externalFiles();
}

bool ArtifactProjectPackager::collectAndPackage(ArtifactProject* project, const PackageSettings& settings) {
    const QString normalizedTargetDir = settings.targetDir.trimmed();
    if (!project || normalizedTargetDir.isEmpty()) return false;

    // Packaging creates a standalone project file, so it must enforce the
    // same metadata and tree invariants as the normal exporter before copying
    // any assets into the destination.
    const auto validationIssues = project->validate();
    QStringList blockingIssues;
    for (const auto& issue : validationIssues) {
        if (issue.severity == ProjectValidationIssue::Severity::Error) {
            blockingIssues.append(QStringLiteral("%1: %2")
                                      .arg(issue.field, issue.message));
        } else {
            qWarning() << "[Packager] Project validation notice:"
                       << issue.field << issue.message;
        }
    }
    QString treeError;
    if (!project->validateProjectTree(&treeError)) {
        blockingIssues.append(treeError);
    }
    if (!blockingIssues.isEmpty()) {
        qWarning() << "[Packager] Project validation failed:"
                   << blockingIssues.join(QStringLiteral("\n"));
        return false;
    }

    QDir dir(normalizedTargetDir);
    if (QFileInfo::exists(dir.absolutePath()) && !QFileInfo(dir.absolutePath()).isDir()) {
        qWarning() << "[Packager] Target path is not a directory:" << dir.absolutePath();
        return false;
    }
    if (!dir.exists()) {
        if (!dir.mkpath(".")) return false;
    }

    QDir assetsDir(dir.filePath("Assets"));
    if (!assetsDir.exists() && !assetsDir.mkpath(".")) {
        qWarning() << "[Packager] Failed to create Assets directory:"
                   << assetsDir.absolutePath();
        return false;
    }

    QStringList sourceFiles = getAllExternalFiles(project);
    QHash<QString, QString> pathMap;
    QSet<QString> usedNames;

    for (const auto& srcPath : sourceFiles) {
        const QFileInfo sourceInfo(srcPath);
        if (!sourceInfo.exists() || !sourceInfo.isFile()) {
            qWarning() << "[Packager] Missing external file:" << srcPath;
            return false;
        }
    }
    
    qDebug() << "Packaging" << sourceFiles.size() << "files...";

    for (const auto& srcPath : sourceFiles) {
        const QString destName = packagedAssetNameForSource(srcPath, settings, usedNames);
        QString destPath = assetsDir.filePath(destName);
        
        usedNames.insert(destName);
        pathMap.insert(srcPath, QStringLiteral("Assets/%1").arg(destName));
        if (QFile::exists(destPath)) {
            if (!QFile::remove(destPath)) {
                qWarning() << "[Packager] Failed to replace existing asset:" << destPath;
                return false;
            }
        }
        if (!QFile::copy(srcPath, destPath)) {
            qDebug() << "Failed to copy:" << srcPath;
            return false;
        }
    }

    QJsonObject projectJson = project->toJson();
    QJsonValue rootValue = projectJson;
    rewriteFootagePaths(rootValue, pathMap);
    projectJson = rootValue.toObject();

    QSaveFile projectFile(dir.filePath(QStringLiteral("project.json")));
    if (!projectFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "Failed to open packaged project file for writing";
        return false;
    }
    const QByteArray serializedProject =
        QJsonDocument(projectJson).toJson(QJsonDocument::Indented);
    if (projectFile.write(serializedProject) != serializedProject.size()) {
        qWarning() << "[Packager] Failed to write packaged project file:"
                   << projectFile.errorString();
        projectFile.cancelWriting();
        return false;
    }
    if (!projectFile.commit()) {
        qWarning() << "[Packager] Failed to commit packaged project file:"
                   << projectFile.errorString();
        return false;
    }
    
    return true;
}

} // namespace Artifact
