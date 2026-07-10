module;

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QJsonObject>
export module Artifact.Project.Statistics;





import Artifact.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Utils.Id;
import Project.MetadataCollector;
import Font.LicenseRegistry;

export namespace Artifact {

struct CompositionStats {
    QString name;
    int layerCount = 0;
    int effectCount = 0;
    long long totalDurationFrames = 0;
};

struct ProjectStats {
    QString projectName;
    int compositionCount = 0;
    int totalLayerCount = 0;
    int totalEffectCount = 0;
    QMap<QString, int> effectUsageMap; // Effect Type -> Usage Count
    QVector<CompositionStats> compositionDetails;
};

class ArtifactProjectStatistics {
public:
    static ProjectStats collect(ArtifactProject* project);
    static CompositionStats collectForComposition(ArtifactAbstractComposition* comp);
    static void collectMetadata(
        ArtifactProject* project,
        const QVector<ArtifactCore::MetadataCollector*>& collectors);
    static ArtifactCore::MetadataReport collectMetadataReport(
        ArtifactProject* project,
        const QVector<ArtifactCore::MetadataCollector*>& collectors);
    static ArtifactCore::MetadataReport collectDefaultMetadataReport(
        ArtifactProject* project);
    static ArtifactCore::MetadataReport collectFontUsageReport(
        ArtifactProject* project,
        const ArtifactCore::FontLicenseRegistry* licenseRegistry = nullptr);
    static QStringList collectAndCopyFontFiles(
        ArtifactProject* project, const QString& outputDirectory);
    static bool writeFontUsageManifest(
        ArtifactProject* project,
        const QString& jsonPath,
        const QString& csvPath = {},
        const ArtifactCore::FontLicenseRegistry* licenseRegistry = nullptr);
    static bool exportFontUsagePackage(
        ArtifactProject* project,
        const QString& outputDirectory,
        const ArtifactCore::FontLicenseRegistry* licenseRegistry = nullptr);
};

class ProjectStatsCollector final : public ArtifactCore::MetadataCollector {
public:
    void reset() override;
    void onComposition(const ArtifactCore::MetadataNode& node) override;
    void onLayer(const ArtifactCore::MetadataNode& node) override;
    void onEffect(const ArtifactCore::MetadataNode& node) override;
    ArtifactCore::MetadataReport report() const override;
    const ProjectStats& stats() const { return stats_; }

private:
    ProjectStats stats_;
};

class ProjectMetadataValueCollector final
    : public ArtifactCore::MetadataCollector {
public:
    void reset() override;
    void onProperty(const ArtifactCore::MetadataNode& node) override;
    ArtifactCore::MetadataReport report() const override;
    const QStringList& externalFiles() const { return externalFiles_; }

private:
    QStringList externalFiles_;
};

class FontUsageCollector final : public ArtifactCore::MetadataCollector {
public:
    void reset() override;
    void setLicenseRegistry(const ArtifactCore::FontLicenseRegistry* registry) {
        licenseRegistry_ = registry;
    }
    void onProperty(const ArtifactCore::MetadataNode& node) override;
    ArtifactCore::MetadataReport report() const override;
    QJsonObject manifestJson() const;
    QString manifestCsv() const;
    QStringList copyFontFiles(const QString& outputDirectory) const;
    const QStringList& families() const { return families_; }
    const QStringList& files() const { return files_; }

private:
    QStringList families_;
    QStringList files_;
    const ArtifactCore::FontLicenseRegistry* licenseRegistry_ = nullptr;
};

} // namespace Artifact
