module;

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDateTime>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <memory>
#include <vector>

module Artifact.Render.Batch;

import Artifact.Project.Manager;
import Artifact.Render.Queue.Service;
import Artifact.Render.Queue.Presets;

namespace Artifact {

namespace {

struct BatchRenderPlan {
    ArtifactCore::CompositionID id;
    QString compName;
    QString outputPath;
    QString outputFormat;
    QString codec;
    QString codecProfile;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    int bitrate = 0;
    bool valid = false;
};

} // namespace

class ArtifactBatchRenderer::Impl {
public:
    ArtifactRenderQueueService* queueService = nullptr;
    ArtifactProjectManager* projectManager = nullptr;
};

ArtifactBatchRenderer::ArtifactBatchRenderer(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
    impl_->queueService = ArtifactRenderQueueService::instance();
}

ArtifactBatchRenderer::~ArtifactBatchRenderer()
{
    delete impl_;
}

ArtifactBatchRenderer* ArtifactBatchRenderer::instance()
{
    static ArtifactBatchRenderer inst;
    return &inst;
}

QString ArtifactBatchRenderer::resolveFileNamePattern(
    const QString& pattern,
    const QString& compName,
    int frameNumber)
{
    QString result = pattern;
    const QString date = QDateTime::currentDateTime().toString("yyyyMMdd");
    const QString time = QDateTime::currentDateTime().toString("HHmmss");
    result.replace("%compName%", compName);
    result.replace("%date%", date);
    result.replace("%time%", time);
    if (frameNumber >= 0) {
        result.replace("%frame%", QString::number(frameNumber).rightJustified(4, '0'));
    }
    // sanitize
    result.replace(QRegularExpression(R"([\\/:*?"<>|]+)"), "_");
    return result;
}

int ArtifactBatchRenderer::addAllCompositions(
    const QString& outputDir,
    const QString& fileNamePattern)
{
    auto& pm = ArtifactProjectManager::getInstance();
    const auto items = pm.projectItems();
    QVector<ArtifactCore::CompositionID> compIds;
    for (const auto* item : items) {
        if (!item) continue;
        // CompositionItem 型かどうかは type() メンバで判定
        // 安全のため dynamic_cast を試す
        if (auto* compItem = dynamic_cast<const CompositionItem*>(item)) {
            compIds.push_back(compItem->compositionId);
        }
    }
    return addCompositions(compIds, outputDir, fileNamePattern);
}

int ArtifactBatchRenderer::addCompositions(
    const QVector<ArtifactCore::CompositionID>& ids,
    const QString& outputDir,
    const QString& fileNamePattern)
{
    auto* queue = impl_->queueService;
    if (!queue) return 0;

    auto& pm = ArtifactProjectManager::getInstance();
    QVector<BatchRenderPlan> plans(ids.size());
    QDir outputRoot(outputDir);
    if (!outputRoot.exists()) {
        outputRoot.mkpath(".");
    }

    tbb::parallel_for(tbb::blocked_range<int>(0, ids.size()),
        [&](const tbb::blocked_range<int>& range) {
            for (int i = range.begin(); i < range.end(); ++i) {
                const auto& id = ids[i];
                const auto found = pm.findComposition(id);
                if (!found.success) continue;

                auto comp = found.ptr.lock();
                if (!comp) continue;

                const QString compName = comp->name();
                const QString safeName = resolveFileNamePattern(fileNamePattern, compName);
                plans[i].id = id;
                plans[i].compName = compName;
                plans[i].outputPath = outputRoot.filePath(safeName + ".mp4");
                plans[i].valid = true;
            }
        });

    int added = 0;
    for (const auto& plan : plans) {
        if (!plan.valid) continue;

        queue->addRenderQueueForComposition(plan.id, plan.compName);

        const int compIndex = queue->jobCount() - 1;
        if (compIndex >= 0) {
            queue->setJobOutputPathAt(compIndex, plan.outputPath);
            int startF = 0, endF = 1;
            if (queue->jobFrameRangeAt(compIndex, &startF, &endF)) {
                // Already set by addRenderQueueForComposition
            }
        }

        added++;
    }

    if (added > 0) {
        Q_EMIT batchJobsAdded(added);
    }
    return added;
}

int ArtifactBatchRenderer::addCompositionsWithTemplate(
    const QVector<ArtifactCore::CompositionID>& ids,
    const BatchTemplate& tmpl)
{
    auto* queue = impl_->queueService;
    if (!queue) return 0;

    auto& pm = ArtifactProjectManager::getInstance();
    QVector<BatchRenderPlan> plans(ids.size());
    QDir outputRoot(tmpl.outputDirectory);
    if (!outputRoot.exists()) {
        outputRoot.mkpath(".");
    }
    const auto* preset = ArtifactRenderFormatPresetManager::instance().findPresetById(tmpl.presetId);
    const QString ext = preset ? preset->container : "mp4";

    tbb::parallel_for(tbb::blocked_range<int>(0, ids.size()),
        [&](const tbb::blocked_range<int>& range) {
            for (int i = range.begin(); i < range.end(); ++i) {
                const auto& id = ids[i];
                const auto found = pm.findComposition(id);
                if (!found.success) continue;

                auto comp = found.ptr.lock();
                if (!comp) continue;

                const QString compName = comp->name();
                const QString resolvedName = resolveFileNamePattern(tmpl.fileNamePattern, compName);
                plans[i].id = id;
                plans[i].compName = compName;
                plans[i].outputPath = outputRoot.filePath(resolvedName + "." + ext);
                plans[i].valid = true;
            }
        });

    int added = 0;
    for (const auto& plan : plans) {
        if (!plan.valid) continue;

        queue->addRenderQueueForComposition(plan.id, plan.compName);
        const int idx = queue->jobCount() - 1;
        if (idx >= 0) {
            queue->setJobOutputPathAt(idx, plan.outputPath);
            if (!tmpl.presetId.isEmpty()) {
                QString outFmt, codec, codecProfile;
                int w, h, bitrate;
                double fps;
                if (queue->jobOutputSettingsAt(idx, &outFmt, &codec, &codecProfile, &w, &h, &fps, &bitrate)) {
                    queue->setJobOutputSettingsAt(idx, outFmt, codec, codecProfile,
                        tmpl.overrideWidth > 0 ? tmpl.overrideWidth : w,
                        tmpl.overrideHeight > 0 ? tmpl.overrideHeight : h,
                        tmpl.overrideFps > 0.0 ? tmpl.overrideFps : fps,
                        bitrate);
                }
            }
        }
        added++;
    }

    if (added > 0) {
        Q_EMIT batchJobsAdded(added);
    }
    return added;
}

QString ArtifactBatchRenderer::resolveTemplateDir() const
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataDir + "/batch_templates";
}

QVector<BatchTemplate> ArtifactBatchRenderer::availableTemplates() const
{
    QVector<BatchTemplate> result;
    QDir dir(resolveTemplateDir());
    if (!dir.exists()) return result;

    for (const auto& fi : dir.entryInfoList({"*.json"}, QDir::Files)) {
        QFile file(fi.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (!doc.isObject()) continue;
        const auto obj = doc.object();
        BatchTemplate tmpl;
        tmpl.name = obj["name"].toString();
        tmpl.outputDirectory = obj["outputDirectory"].toString();
        tmpl.fileNamePattern = obj["fileNamePattern"].toString("%compName%_%date%");
        tmpl.presetId = obj["presetId"].toString();
        tmpl.overrideWidth = obj["overrideWidth"].toInt(0);
        tmpl.overrideHeight = obj["overrideHeight"].toInt(0);
        tmpl.overrideFps = obj["overrideFps"].toDouble(0.0);
        tmpl.startFrame = obj["startFrame"].toInt(-1);
        tmpl.endFrame = obj["endFrame"].toInt(-1);
        result.push_back(tmpl);
    }
    return result;
}

bool ArtifactBatchRenderer::saveTemplate(const BatchTemplate& tmpl)
{
    QDir dir(resolveTemplateDir());
    if (!dir.exists()) dir.mkpath(".");

    QJsonObject obj;
    obj["name"] = tmpl.name;
    obj["outputDirectory"] = tmpl.outputDirectory;
    obj["fileNamePattern"] = tmpl.fileNamePattern;
    obj["presetId"] = tmpl.presetId;
    obj["overrideWidth"] = tmpl.overrideWidth;
    obj["overrideHeight"] = tmpl.overrideHeight;
    obj["overrideFps"] = tmpl.overrideFps;
    obj["startFrame"] = tmpl.startFrame;
    obj["endFrame"] = tmpl.endFrame;

    QFile file(dir.filePath(tmpl.name + ".json"));
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(obj).toJson());
    file.close();
    return true;
}

bool ArtifactBatchRenderer::deleteTemplate(const QString& name)
{
    QDir dir(resolveTemplateDir());
    return dir.remove(name + ".json");
}

BatchTemplate ArtifactBatchRenderer::defaultTemplate() const
{
    BatchTemplate tmpl;
    tmpl.name = "Default";
    tmpl.outputDirectory = QDir::homePath() + "/Desktop";
    tmpl.fileNamePattern = "%compName%_%date%";
    tmpl.presetId = "h264_mp4_standard";
    tmpl.overrideWidth = 0;
    tmpl.overrideHeight = 0;
    tmpl.overrideFps = 0.0;
    tmpl.startFrame = -1;
    tmpl.endFrame = -1;
    return tmpl;
}

} // namespace Artifact
