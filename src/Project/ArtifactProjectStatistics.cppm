module;

#pragma warning(push)
#pragma warning(disable: 2382)  // redefinition; different exception specifications (std::ranges)

#include <QString>
#include <QVector>
#include <QMap>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFile>
#include <QSaveFile>
#include <QFont>
#include <QFontDatabase>
#include <QRawFont>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>

#pragma warning(pop)

module Artifact.Project.Statistics;




import Artifact.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Project.ProjectVisitor;
import Font.LicenseRegistry;

namespace Artifact {

void ProjectStatsCollector::onComposition(
    const ArtifactCore::MetadataNode& node) {
    ++stats_.compositionCount;
    stats_.compositionDetails.push_back(
        CompositionStats{node.name, 0, 0, 0});
}

void ProjectStatsCollector::reset() {
    stats_ = {};
}

void ProjectStatsCollector::onLayer(const ArtifactCore::MetadataNode&) {
    ++stats_.totalLayerCount;
    if (!stats_.compositionDetails.isEmpty()) {
        ++stats_.compositionDetails.back().layerCount;
    }
}

void ProjectStatsCollector::onEffect(const ArtifactCore::MetadataNode& node) {
    ++stats_.totalEffectCount;
    if (!stats_.compositionDetails.isEmpty()) {
        ++stats_.compositionDetails.back().effectCount;
    }
    stats_.effectUsageMap[node.type]++;
}

void ArtifactProjectStatistics::collectMetadata(
    ArtifactProject* project,
    const QVector<ArtifactCore::MetadataCollector*>& collectors) {
    if (!project || collectors.isEmpty()) return;

    ArtifactCore::ProjectVisitor visitor;
    const auto settings = project->settings();
    visitor.beginProject(QStringLiteral("project"), settings.projectName());

    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Footage) {
            auto* footage = static_cast<FootageItem*>(item);
            if (footage && !footage->filePath.isEmpty()) {
                visitor.visitProperty(QStringLiteral("footage"),
                                       QStringLiteral("filePath"),
                                       QStringLiteral("ExternalFile"),
                                       footage->filePath);
            }
        }
        if (item->type() == eProjectItemType::Composition) {
            auto* compositionItem = static_cast<CompositionItem*>(item);
            const auto findResult = project->findComposition(compositionItem->compositionId);
            if (findResult.success) {
                if (auto comp = findResult.ptr.lock()) {
                    const QString compId = compositionItem->compositionId.toString();
                    visitor.visitComposition(compId, compositionItem->name);
                    for (const auto& layer : comp->allLayer()) {
                        if (!layer) continue;
                    visitor.visitLayer(layer->id().toString(),
                                       layer->layerName(),
                                       layer->className());
                        for (const auto& effect : layer->getEffects()) {
                            if (!effect) continue;
                            visitor.visitEffect(
                                QString(), effect->displayName().toQString(),
                                effect->displayName().toQString());
                        }
                        const QJsonObject layerJson = layer->toJson();
                        std::function<void(const QJsonValue&, const QString&)> visitJson =
                            [&](const QJsonValue& value, const QString& path) {
                                if (value.isObject()) {
                                    const auto object = value.toObject();
                                    for (auto it = object.begin(); it != object.end(); ++it) {
                                        const QString childPath = path.isEmpty()
                                            ? it.key() : path + QStringLiteral(".") + it.key();
                                        visitJson(it.value(), childPath);
                                    }
                                } else if (value.isArray()) {
                                    const auto array = value.toArray();
                                    for (int i = 0; i < array.size(); ++i) {
                                        visitJson(array.at(i), path + QStringLiteral("[%1]").arg(i));
                                    }
                                } else if (const QString normalizedPath = path.toLower();
                                           normalizedPath.endsWith(QStringLiteral("sourcepath")) ||
                                           normalizedPath.endsWith(QStringLiteral("filepath")) ||
                                           normalizedPath.endsWith(QStringLiteral("fontfamily")) ||
                                           normalizedPath.endsWith(QStringLiteral("fontpath")) ||
                                           normalizedPath.endsWith(QStringLiteral("fontfile"))) {
                                    visitor.visitProperty(layer->id().toString(), path,
                                                           QStringLiteral("MetadataValue"),
                                                           value.toVariant());
                                }
                            };
                        visitJson(layerJson, QString());
                    }
                }
            }
        }
        for (auto* child : item->children) traverse(child);
    };

    for (auto* root : project->projectItems()) traverse(root);
    visitor.collect(collectors);
}

ArtifactCore::MetadataReport ArtifactProjectStatistics::collectMetadataReport(
    ArtifactProject* project,
    const QVector<ArtifactCore::MetadataCollector*>& collectors) {
    if (!project || collectors.isEmpty()) return {};
    ArtifactProjectStatistics::collectMetadata(project, collectors);
    ArtifactCore::MetadataReport result;
    for (auto* collector : collectors) {
        if (collector) result.merge(collector->report());
    }
    return result;
}

ArtifactCore::MetadataReport ArtifactProjectStatistics::collectDefaultMetadataReport(
    ArtifactProject* project) {
    if (!project) return {};
    ProjectStatsCollector statsCollector;
    ProjectMetadataValueCollector valueCollector;
    FontUsageCollector fontCollector;
    const QVector<ArtifactCore::MetadataCollector*> collectors{
        &statsCollector, &valueCollector, &fontCollector};
    return collectMetadataReport(project, collectors);
}

ArtifactCore::MetadataReport ArtifactProjectStatistics::collectFontUsageReport(
    ArtifactProject* project,
    const ArtifactCore::FontLicenseRegistry* licenseRegistry) {
    if (!project) return {};
    FontUsageCollector collector;
    collector.setLicenseRegistry(licenseRegistry);
    const QVector<ArtifactCore::MetadataCollector*> collectors{&collector};
    return collectMetadataReport(project, collectors);
}

QStringList ArtifactProjectStatistics::collectAndCopyFontFiles(
    ArtifactProject* project, const QString& outputDirectory) {
    if (!project) return {};
    FontUsageCollector collector;
    const QVector<ArtifactCore::MetadataCollector*> collectors{&collector};
    collectMetadata(project, collectors);
    return collector.copyFontFiles(outputDirectory);
}

bool ArtifactProjectStatistics::writeFontUsageManifest(
    ArtifactProject* project, const QString& jsonPath, const QString& csvPath,
    const ArtifactCore::FontLicenseRegistry* licenseRegistry) {
    const QString normalizedJsonPath = jsonPath.trimmed();
    const QString normalizedCsvPath = csvPath.trimmed();
    if (!project || normalizedJsonPath.isEmpty()) return false;
    FontUsageCollector collector;
    collector.setLicenseRegistry(licenseRegistry);
    const QVector<ArtifactCore::MetadataCollector*> collectors{&collector};
    collectMetadata(project, collectors);

    QSaveFile jsonFile(normalizedJsonPath);
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray jsonData =
        QJsonDocument(collector.manifestJson()).toJson(QJsonDocument::Indented);
    if (jsonFile.write(jsonData) != jsonData.size() || !jsonFile.commit()) return false;

    if (normalizedCsvPath.isEmpty()) return true;
    QSaveFile csvFile(normalizedCsvPath);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray csvData = collector.manifestCsv().toUtf8();
    return csvFile.write(csvData) == csvData.size() && csvFile.commit();
}

bool ArtifactProjectStatistics::exportFontUsagePackage(
    ArtifactProject* project, const QString& outputDirectory,
    const ArtifactCore::FontLicenseRegistry* licenseRegistry) {
    const QString directory = outputDirectory.trimmed();
    if (!project || directory.isEmpty() || !QDir().mkpath(directory)) {
        return false;
    }
    FontUsageCollector collector;
    collector.setLicenseRegistry(licenseRegistry);
    const QVector<ArtifactCore::MetadataCollector*> collectors{&collector};
    collectMetadata(project, collectors);
    if (licenseRegistry) {
        for (const auto& family : collector.families()) {
            if (!licenseRegistry->isAttested(family)) {
                qWarning() << "Font usage report: license is not attested for"
                           << family;
            }
        }
    }
    const QStringList copiedFiles = collector.copyFontFiles(directory);
    if (copiedFiles.size() < collector.files().size()) return false;
    if (licenseRegistry) {
        for (const auto& license : licenseRegistry->entries()) {
            if (!license.licenseFilePath.trimmed().isEmpty() &&
                !QFileInfo::exists(license.licenseFilePath)) {
                return false;
            }
            const QFileInfo sourceInfo(license.licenseFilePath);
            if (!sourceInfo.exists() || !sourceInfo.isFile()) continue;
            QString targetPath =
                QDir(directory).filePath(sourceInfo.fileName());
            if (QFileInfo::exists(targetPath) &&
                QFileInfo(targetPath).absoluteFilePath() !=
                    sourceInfo.absoluteFilePath()) {
                const QString baseName = sourceInfo.completeBaseName();
                const QString suffix = sourceInfo.completeSuffix();
                for (int index = 1; QFileInfo::exists(targetPath); ++index) {
                    const QString candidate = suffix.isEmpty()
                        ? QStringLiteral("%1_%2").arg(baseName).arg(index)
                        : QStringLiteral("%1_%2.%3").arg(baseName).arg(index).arg(suffix);
                    targetPath = QDir(directory).filePath(candidate);
                }
            }
            if (!QFileInfo::exists(targetPath) &&
                !QFile::copy(sourceInfo.absoluteFilePath(), targetPath)) {
                return false;
            }
        }
    }
    return writeFontUsageManifest(
        project, QDir(directory).filePath(QStringLiteral("font-usage.json")),
        QDir(directory).filePath(QStringLiteral("font-usage.csv")),
        licenseRegistry);
}

ArtifactCore::MetadataReport ProjectStatsCollector::report() const {
    ArtifactCore::MetadataReport report;
    report.addCount(QStringLiteral("composition.count"), stats_.compositionCount);
    report.addCount(QStringLiteral("layer.count"), stats_.totalLayerCount);
    report.addCount(QStringLiteral("effect.count"), stats_.totalEffectCount);
    return report;
}

void ProjectMetadataValueCollector::onProperty(
    const ArtifactCore::MetadataNode& node) {
    const QString value = node.value.toString().trimmed();
    if (value.isEmpty()) return;
    const QString key = node.name.toLower();
    if (key.endsWith(QStringLiteral("sourcepath")) ||
        key.endsWith(QStringLiteral("filepath")) ||
        key.endsWith(QStringLiteral("fontpath")) ||
        key.endsWith(QStringLiteral("fontfile"))) {
        if (!externalFiles_.contains(value)) externalFiles_.push_back(value);
    }
}

void ProjectMetadataValueCollector::reset() {
    externalFiles_.clear();
}

ArtifactCore::MetadataReport ProjectMetadataValueCollector::report() const {
    ArtifactCore::MetadataReport report;
    for (const auto& path : externalFiles_) {
        report.add(QStringLiteral("external.file"), path);
    }
    report.addCount(QStringLiteral("external.file.count"), externalFiles_.size());
    return report;
}

void FontUsageCollector::onProperty(const ArtifactCore::MetadataNode& node) {
    const QString value = node.value.toString().trimmed();
    if (value.isEmpty()) return;
    const QString key = node.name.toLower();
    if (key.endsWith(QStringLiteral("fontfamily"))) {
        if (!families_.contains(value)) families_.push_back(value);

        const QFont font(value);
        const QRawFont rawFont = QRawFont::fromFont(font, QFontDatabase::Any);
        // Qt6's QRawFont does not expose the backing font file path.
        const QString resolvedPath;
        if (!resolvedPath.trimmed().isEmpty() && !files_.contains(resolvedPath)) {
            files_.push_back(resolvedPath);
        }
    } else if (key.endsWith(QStringLiteral("fontpath")) ||
               key.endsWith(QStringLiteral("fontfile"))) {
        if (!files_.contains(value)) files_.push_back(value);
    }
}

void FontUsageCollector::reset() {
    families_.clear();
    files_.clear();
}

QStringList FontUsageCollector::copyFontFiles(
    const QString& outputDirectory) const {
    QStringList copiedFiles;
    const QString destination = outputDirectory.trimmed();
    if (destination.isEmpty() || !QDir().mkpath(destination)) return copiedFiles;

    for (const auto& sourcePath : files_) {
        const QFileInfo sourceInfo(sourcePath);
        if (!sourceInfo.exists() || !sourceInfo.isFile()) continue;

        QString targetPath = QDir(destination).filePath(sourceInfo.fileName());
        if (QFileInfo::exists(targetPath)) {
            if (QFileInfo(targetPath).absoluteFilePath() ==
                sourceInfo.absoluteFilePath()) {
                copiedFiles.push_back(targetPath);
                continue;
            }
            const QString baseName = sourceInfo.completeBaseName();
            const QString suffix = sourceInfo.completeSuffix();
            for (int index = 1; QFileInfo::exists(targetPath); ++index) {
                const QString candidate = suffix.isEmpty()
                    ? QStringLiteral("%1_%2").arg(baseName).arg(index)
                    : QStringLiteral("%1_%2.%3").arg(baseName).arg(index).arg(suffix);
                targetPath = QDir(destination).filePath(candidate);
            }
        }
        if (QFile::copy(sourcePath, targetPath)) copiedFiles.push_back(targetPath);
    }
    return copiedFiles;
}

ArtifactCore::MetadataReport FontUsageCollector::report() const {
    ArtifactCore::MetadataReport report;
    for (const auto& family : families_) {
        report.add(QStringLiteral("font.family"), family);
    }
    for (const auto& file : files_) {
        report.add(QStringLiteral("font.file"), file);
    }
    report.addCount(QStringLiteral("font.family.count"), families_.size());
    report.addCount(QStringLiteral("font.file.count"), files_.size());
    qint64 attestedCount = 0;
    qint64 unresolvedCount = 0;
    if (licenseRegistry_) {
        for (const auto& family : families_) {
            const QRawFont rawFont = QRawFont::fromFont(QFont(family), QFontDatabase::Any);
            const QString style = rawFont.isValid() ? rawFont.styleName() : QString();
            if (licenseRegistry_->isAttested(family, style)) ++attestedCount;
        }
    }
    for (const auto& family : families_) {
        const QRawFont rawFont = QRawFont::fromFont(QFont(family), QFontDatabase::Any);
        if (!rawFont.isValid()) ++unresolvedCount;
    }
    report.addCount(QStringLiteral("font.license.attested.count"), attestedCount);
    report.addCount(QStringLiteral("font.license.unattested.count"),
                    families_.size() - attestedCount);
    report.addCount(QStringLiteral("font.unresolved.count"), unresolvedCount);
    return report;
}

QJsonObject FontUsageCollector::manifestJson() const {
    QJsonObject result;
    QJsonArray fonts;
    for (const auto& family : families_) {
        QJsonObject font;
        font.insert(QStringLiteral("family"), family);
        const QRawFont rawFont = QRawFont::fromFont(
            QFont(family), QFontDatabase::Any);
        // Qt6's QRawFont does not expose the backing font file path.
        const QString filePath;
        font.insert(QStringLiteral("filePath"), filePath);
        font.insert(QStringLiteral("resolved"), !filePath.trimmed().isEmpty());
        font.insert(QStringLiteral("resolvedFamily"),
                    rawFont.isValid() ? rawFont.familyName() : QString());
        font.insert(QStringLiteral("style"),
                    rawFont.isValid() ? rawFont.styleName() : QString());
        font.insert(QStringLiteral("weight"),
                    rawFont.isValid() ? rawFont.weight() : 0);
        font.insert(QStringLiteral("italic"),
                    rawFont.isValid() && rawFont.style() == QFont::StyleItalic);
        const QString style = rawFont.isValid() ? rawFont.styleName() : QString();
        const ArtifactCore::FontLicenseEntry license =
            licenseRegistry_ ? licenseRegistry_->entry(family, style)
                             : ArtifactCore::FontLicenseEntry{};
        font.insert(QStringLiteral("licenseFilePath"),
                    license.licenseFilePath);
        font.insert(QStringLiteral("attestation"), license.attestation);
        font.insert(QStringLiteral("licenseStatus"),
                    license.isAttested() ? QStringLiteral("attested")
                                         : QStringLiteral("unattested"));
        fonts.append(font);
    }
    QJsonArray files;
    for (const auto& path : files_) files.append(path);
    QJsonObject counts;
    counts.insert(QStringLiteral("family"), families_.size());
    counts.insert(QStringLiteral("resolvedFile"), files_.size());
    qint64 attested = 0;
    qint64 unresolved = 0;
    if (licenseRegistry_) {
        for (const auto& family : families_) {
            const QRawFont rawFont = QRawFont::fromFont(QFont(family), QFontDatabase::Any);
            const QString style = rawFont.isValid() ? rawFont.styleName() : QString();
            if (licenseRegistry_->isAttested(family, style)) ++attested;
        }
    }
    counts.insert(QStringLiteral("licenseAttested"), attested);
    counts.insert(QStringLiteral("licenseUnattested"),
                 families_.size() - attested);
    for (const auto& fontValue : fonts) {
        if (!fontValue.toObject().value(QStringLiteral("resolved")).toBool()) ++unresolved;
    }
    counts.insert(QStringLiteral("unresolved"), unresolved);
    result.insert(QStringLiteral("version"), 1);
    result.insert(QStringLiteral("fonts"), fonts);
    result.insert(QStringLiteral("resolvedFiles"), files);
    result.insert(QStringLiteral("counts"), counts);
    return result;
}

QString FontUsageCollector::manifestCsv() const {
    const auto escape = [](const QString& value) {
        QString result = value;
        result.replace(QChar('"'), QStringLiteral("\"\""));
        return QStringLiteral("\"%1\"").arg(result);
    };
    QStringList lines;
    lines << QStringLiteral("family,filePath,resolved,resolvedFamily,style,weight,italic,licenseFilePath,attestation,licenseStatus");
    for (const auto& family : families_) {
        ArtifactCore::FontLicenseEntry license;
        const QRawFont rawFont = QRawFont::fromFont(
            QFont(family), QFontDatabase::Any);
        if (licenseRegistry_) {
            const QString style = rawFont.isValid() ? rawFont.styleName() : QString();
            license = licenseRegistry_->entry(family, style);
        }
        // Qt6's QRawFont does not expose the backing font file path.
        const QString filePath;
        lines << QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10")
                     .arg(escape(family))
                     .arg(escape(filePath))
                     .arg(!filePath.trimmed().isEmpty() ? QStringLiteral("true")
                                                        : QStringLiteral("false"))
                     .arg(escape(rawFont.isValid() ? rawFont.familyName() : QString()))
                     .arg(escape(rawFont.isValid() ? rawFont.styleName() : QString()))
                     .arg(rawFont.isValid() ? QString::number(rawFont.weight()) : QStringLiteral("0"))
                     .arg(rawFont.isValid() && rawFont.style() == QFont::StyleItalic
                              ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(escape(license.licenseFilePath))
                     .arg(escape(license.attestation))
                     .arg(license.isAttested() ? QStringLiteral("attested")
                                                : QStringLiteral("unattested"));
    }
    return lines.join(QChar('\n'));
}

// We need a way to access the private container_ or use existing public methods.
// ArtifactProject has projectItems(), we can use that to find compositions.

CompositionStats ArtifactProjectStatistics::collectForComposition(ArtifactAbstractComposition* comp) {
    if (!comp) return {};

    CompositionStats stats;
    stats.name = comp->objectName(); // Or get from project items if needed
    stats.layerCount = comp->layerCount();
    
    auto layers = comp->allLayer();
    for (const auto& layer : layers) {
        if (!layer) continue;
        auto effects = layer->getEffects();
        stats.effectCount += static_cast<int>(effects.size());
    }
    
    // Duration
    auto range = comp->frameRange();
    stats.totalDurationFrames = range.duration();
    
    return stats;
}

ProjectStats ArtifactProjectStatistics::collect(ArtifactProject* project) {
    if (!project) return {};

    ProjectStats stats;
    stats.projectName = project->settings().projectName();

    auto items = project->projectItems();
    
    // Simple recursive traversal of project items to find all compositions
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto findRes = project->findComposition(compItem->compositionId);
            if (findRes.success) {
                auto comp = findRes.ptr.lock();
                if (comp) {
                    stats.compositionCount++;
                    auto cStats = collectForComposition(comp.get());
                    cStats.name = compItem->name.toQString(); // Use names from tree
                    stats.totalLayerCount += cStats.layerCount;
                    stats.totalEffectCount += cStats.effectCount;
                    stats.compositionDetails.push_back(cStats);
                    
                    // Count effect usage
                    auto layers = comp->allLayer();
                    for (const auto& layer : layers) {
                        for (const auto& effect : layer->getEffects()) {
                           QString typeName = effect->displayName().toQString();
                           stats.effectUsageMap[typeName]++;
                        }
                    }
                }
            }
        }
        
        for (auto child : item->children) {
            traverse(child);
        }
    };

    for (auto root : items) {
        traverse(root);
    }

    return stats;
}

} // namespace Artifact
