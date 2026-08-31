module;

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDateTime>
#include <QRegularExpression>
#include <memory>
#include <vector>

module Artifact.Render.Batch;

import Artifact.Project.Manager;
import Artifact.Render.Queue.Service;
import Artifact.Render.Queue.Presets;

namespace Artifact {

class ArtifactBatchRenderer::Impl {
public:
    ArtifactRenderQueueService* queueService = nullptr;
    ArtifactProjectManager* projectManager = nullptr;
};

bool BatchTemplate::isValid() const noexcept
{
    return !name.trimmed().isEmpty() &&
           !name.contains(QRegularExpression(R"([\\/:*?\"<>|])")) &&
           !outputDirectory.trimmed().isEmpty() &&
           !fileNamePattern.trimmed().isEmpty() &&
           overrideWidth >= 0 && overrideHeight >= 0 &&
           overrideFps >= 0.0 && overrideFps <= 240.0 &&
           ((startFrame < 0 && endFrame < 0) ||
            (startFrame >= 0 && endFrame >= startFrame)) &&
           overrideBitrate >= 0 && framePadding >= 1 && framePadding <= 12;
}

bool ArtifactBatchRenderer::validateTemplate(const BatchTemplate& tmpl, QString* error)
{
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };
    if (tmpl.name.trimmed().isEmpty()) return fail(QStringLiteral("Template name is empty"));
    if (tmpl.name.contains(QRegularExpression(R"([\\/:*?\"<>|])"))) return fail(QStringLiteral("Template name contains an invalid path character"));
    if (tmpl.outputDirectory.trimmed().isEmpty()) return fail(QStringLiteral("Output directory is empty"));
    if (tmpl.fileNamePattern.trimmed().isEmpty()) return fail(QStringLiteral("File name pattern is empty"));
    if (tmpl.overrideWidth < 0 || tmpl.overrideHeight < 0) return fail(QStringLiteral("Override dimensions must be non-negative"));
    if (tmpl.overrideFps < 0.0 || tmpl.overrideFps > 240.0) return fail(QStringLiteral("Override FPS is outside the supported range"));
    if ((tmpl.startFrame < 0) != (tmpl.endFrame < 0) || (tmpl.endFrame >= 0 && tmpl.endFrame < tmpl.startFrame)) return fail(QStringLiteral("Frame range is invalid"));
    if (tmpl.overrideBitrate < 0) return fail(QStringLiteral("Override bitrate must be non-negative"));
    if (tmpl.framePadding < 1 || tmpl.framePadding > 12) return fail(QStringLiteral("Frame padding must be between 1 and 12"));
    return true;
}

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

QStringList ArtifactBatchRenderer::lastBatchErrors() const { return lastBatchErrors_; }
void ArtifactBatchRenderer::clearBatchErrors() { lastBatchErrors_.clear(); }

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

static QString presetOutputExtension(const ArtifactRenderFormatPreset* preset)
{
    if (!preset) {
        return QStringLiteral("mp4");
    }
    if (!preset->container.trimmed().isEmpty()) {
        return preset->container.trimmed().toLower();
    }
    if (!preset->codec.trimmed().isEmpty()) {
        return preset->codec.trimmed().toLower();
    }
    return QStringLiteral("mp4");
}

int ArtifactBatchRenderer::addAllCompositions(
    const QString& outputDir,
    const QString& fileNamePattern)
{
    clearBatchErrors();
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
    clearBatchErrors();
    auto* queue = impl_->queueService;
    if (!queue || outputDir.trimmed().isEmpty() || fileNamePattern.trimmed().isEmpty()) return 0;

    int added = 0;
    auto& pm = ArtifactProjectManager::getInstance();

    for (const auto& id : ids) {
        const auto found = pm.findComposition(id);
        if (!found.success) continue;

        auto comp = found.ptr.lock();
        if (!comp) continue;

        const QString compName = comp->settings().compositionName().toQString();
        const QString safeName = resolveFileNamePattern(fileNamePattern, compName);

        const auto* preset = ArtifactRenderFormatPresetManager::instance().findPresetById(
            QStringLiteral("h264_mp4_standard"));
        const QString outputExt = presetOutputExtension(preset);

        QDir dir(outputDir);
        if (!dir.exists()) dir.mkpath(".");
        const QString outputPath = dir.filePath(safeName + QStringLiteral(".") + outputExt);

        queue->addRenderQueueWithPreset(id, compName, QStringLiteral("h264_mp4_standard"));

        const int compIndex = queue->jobCount() - 1;
        if (compIndex >= 0) {
            queue->setJobOutputPathAt(compIndex, outputPath);
            int startF = 0, endF = 1;
            if (queue->jobFrameRangeAt(compIndex, &startF, &endF)) {
                // Already set by addRenderQueueForComposition
            }
        }

        added++;
    }

    if (added > 0) {
        batchJobsAdded(added);
    }
    return added;
}

int ArtifactBatchRenderer::addAudioCompositions(
    const QVector<ArtifactCore::CompositionID>& ids,
    const QString& outputDir,
    bool pcm24,
    const QString& fileNamePattern)
{
    auto* queue = impl_->queueService;
    if (!queue || outputDir.trimmed().isEmpty() || fileNamePattern.trimmed().isEmpty()) return 0;

    const QString presetId = pcm24 ? QStringLiteral("wav_pcm_s24")
                                   : QStringLiteral("wav_pcm_s16");
    const auto* preset = ArtifactRenderFormatPresetManager::instance().findPresetById(presetId);
    if (!preset) return 0;

    QDir dir(outputDir);
    if (!dir.exists()) dir.mkpath(QStringLiteral("."));
    if (!dir.exists()) return 0;
    auto& pm = ArtifactProjectManager::getInstance();
    int added = 0;
    for (const auto& id : ids) {
        const auto found = pm.findComposition(id);
        if (!found.success) continue;
        const auto comp = found.ptr.lock();
        if (!comp) continue;
        const QString compName = comp->settings().compositionName().toQString();
        const QString stem = resolveFileNamePattern(fileNamePattern, compName);
        queue->addRenderQueueWithPreset(id, compName, presetId);
        const int index = queue->jobCount() - 1;
        if (index >= 0) {
            queue->setJobOutputPathAt(index, dir.filePath(stem + QStringLiteral(".wav")));
            ++added;
        }
    }
    if (added > 0) batchJobsAdded(added);
    return added;
}

int ArtifactBatchRenderer::addCompositionsWithTemplate(
    const QVector<ArtifactCore::CompositionID>& ids,
    const BatchTemplate& tmpl)
{
    auto* queue = impl_->queueService;
    if (!queue || !validateTemplate(tmpl)) return 0;

    int added = 0;
    auto& pm = ArtifactProjectManager::getInstance();

    for (const auto& id : ids) {
        const auto found = pm.findComposition(id);
        if (!found.success) continue;

        auto comp = found.ptr.lock();
        if (!comp) continue;

        const QString compName = comp->settings().compositionName().toQString();
        const QString resolvedName = resolveFileNamePattern(tmpl.fileNamePattern, compName);
        QDir dir(tmpl.outputDirectory);
        if (!dir.exists()) dir.mkpath(".");

        // Determine extension from preset
        const auto* preset = ArtifactRenderFormatPresetManager::instance().findPresetById(tmpl.presetId);
        const QString ext = presetOutputExtension(preset);
        const QString outputPath = dir.filePath(resolvedName + "." + ext);

        queue->addRenderQueueWithPreset(id, compName, tmpl.presetId.isEmpty()
                                                     ? QStringLiteral("h264_mp4_standard")
                                                     : tmpl.presetId);
        const int idx = queue->jobCount() - 1;
        if (idx >= 0) {
            queue->setJobOutputPathAt(idx, outputPath);
            QString outFmt;
            QString codec;
            QString codecProfile;
            int w = 0;
            int h = 0;
            int bitrate = 0;
            double fps = 0.0;
            queue->setJobFramePaddingAt(idx, tmpl.framePadding);
            if (queue->jobOutputSettingsAt(idx, &outFmt, &codec, &codecProfile, &w, &h, &fps, &bitrate)) {
                const QString useFormat = tmpl.format.isEmpty() ? outFmt : tmpl.format;
                const QString useCodec = tmpl.codec.isEmpty() ? codec : tmpl.codec;
                const QString useProfile = tmpl.codecProfile.isEmpty() ? codecProfile : tmpl.codecProfile;
                const int useBitrate = tmpl.overrideBitrate > 0 ? tmpl.overrideBitrate : bitrate;
                queue->setJobOutputSettingsAt(idx, useFormat, useCodec, useProfile,
                    tmpl.overrideWidth > 0 ? tmpl.overrideWidth : w,
                    tmpl.overrideHeight > 0 ? tmpl.overrideHeight : h,
                    tmpl.overrideFps > 0.0 ? tmpl.overrideFps : fps,
                    useBitrate);
            }
            if (tmpl.startFrame >= 0 && tmpl.endFrame >= tmpl.startFrame) {
                queue->setJobFrameRangeAt(idx, tmpl.startFrame, tmpl.endFrame);
            }
        }
        added++;
    }

    if (added > 0) {
        batchJobsAdded(added);
    }
    return added;
}

void ArtifactBatchRenderer::batchJobsAdded(int)
{
}

QString ArtifactBatchRenderer::resolveTemplateDir() const
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataDir + "/batch_templates";
}

QVector<BatchTemplate> ArtifactBatchRenderer::availableTemplates() const
{
    constexpr qint64 kMaxBatchTemplateBytes = 8LL * 1024LL * 1024LL;
    QVector<BatchTemplate> result;
    QDir dir(resolveTemplateDir());
    if (!dir.exists()) return result;

    for (const auto& fi : dir.entryInfoList({"*.json"}, QDir::Files)) {
        if (fi.size() <= 0 || fi.size() > kMaxBatchTemplateBytes) continue;
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
        tmpl.format = obj["format"].toString();
        tmpl.codec = obj["codec"].toString();
        tmpl.codecProfile = obj["codecProfile"].toString();
        tmpl.overrideBitrate = obj["overrideBitrate"].toInt(0);
        tmpl.framePadding = obj["framePadding"].toInt(4);
        result.push_back(tmpl);
    }
    return result;
}

bool ArtifactBatchRenderer::saveTemplate(const BatchTemplate& tmpl)
{
    const QString templateName = tmpl.name.trimmed();
    if (!validateTemplate(tmpl)) {
        return false;
    }
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
    obj["format"] = tmpl.format;
    obj["codec"] = tmpl.codec;
    obj["codecProfile"] = tmpl.codecProfile;
    obj["overrideBitrate"] = tmpl.overrideBitrate;
    obj["framePadding"] = tmpl.framePadding;

    QSaveFile file(dir.filePath(templateName + ".json"));
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QByteArray payload = QJsonDocument(obj).toJson();
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
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
    tmpl.format = QStringLiteral("MP4");
    tmpl.codec = QStringLiteral("H.264");
    tmpl.overrideBitrate = 8000;
    return tmpl;
}

} // namespace Artifact
