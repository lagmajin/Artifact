module;
#include <QObject>
#include <QList>
#include <QThread>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QDateTime>
#include <QFileInfo>
#include <QPointF>
#include <QRegularExpression>
#include <wobjectimpl.h>
#include <mutex>
#include <map>

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
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
module Artifact.Render.Queue.Service;




import Render.Queue.Manager;
import Artifact.Project.Manager;
import Artifact.Project.Items;
import Encoder.FFmpegEncoder;
import Image.ImageF32x4_RGBA;
import CvUtils;
import Utils.Id;
import Artifact.Render.SoftwareCompositor;
import Artifact.Render.IRenderer;

namespace Artifact
{
    // レンダリングジョブクラス
    class ArtifactRenderJob {
    public:
        enum class Status {
            Pending,       // 待機中
            Rendering,     // レンダリング中
            Completed,     // 完了
            Failed,        // 失敗
            Canceled       // キャンセル
        };

        ArtifactCore::CompositionID compositionId; // 対象コンポジションID
        QString compositionName;      // コンポジション名
        QString outputPath;           // 出力パス
        QString outputFormat;         // 出力形式 (MP4, PNG sequence等)
        QString codec;                // コーデック
        int resolutionWidth;          // 解像度幅
        int resolutionHeight;         // 解像度高さ
        double frameRate;             // フレームレート
        int bitrate;                  // ビットレート (kbps)
        int startFrame;               // 開始フレーム
        int endFrame;                 // 終了フレーム
        Status status;                // ステータス
        int progress;                 // 進捗率 (0-100)
        QString errorMessage;         // エラーメッセージ
        float overlayOffsetX;
        float overlayOffsetY;
        float overlayScale;
        float overlayRotationDeg;

        ArtifactRenderJob()
            : resolutionWidth(1920)
            , resolutionHeight(1080)
            , frameRate(30.0)
            , bitrate(8000)
            , startFrame(0)
            , endFrame(100)
            , status(Status::Pending)
            , progress(0)
            , overlayOffsetX(0.0f)
            , overlayOffsetY(0.0f)
            , overlayScale(1.0f)
            , overlayRotationDeg(0.0f)
        {
        }
    };

    // レンダリングキューマネージャクラス
    class ArtifactRenderQueueManager {
    public:
        void addJob(const ArtifactRenderJob& job) {
            jobs.append(job);
            if (jobAdded) jobAdded(jobs.size() - 1);
        }

        void removeJob(int index) {
            if (index >= 0 && index < jobs.size()) {
                jobs.removeAt(index);
                if (jobRemoved) jobRemoved(index);
            }
        }

        void removeAllJobs() {
            jobs.clear();
            if (allJobsRemoved) allJobsRemoved();
        }

        void moveJob(int fromIndex, int toIndex) {
            if (fromIndex >= 0 && fromIndex < jobs.size() && toIndex >= 0 && toIndex < jobs.size()) {
                ArtifactRenderJob job = jobs.takeAt(fromIndex);
                jobs.insert(toIndex, job);
                if (queueReordered) queueReordered(fromIndex, toIndex);
            }
        }

        void startRendering(int index) {
            if (index >= 0 && index < jobs.size()) {
                jobs[index].status = ArtifactRenderJob::Status::Rendering;
                if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
            }
        }

        void pauseRendering(int index) {
            if (index >= 0 && index < jobs.size()) {
                jobs[index].status = ArtifactRenderJob::Status::Pending;
                if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
            }
        }

        void cancelRendering(int index) {
            if (index >= 0 && index < jobs.size()) {
                jobs[index].status = ArtifactRenderJob::Status::Canceled;
                if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
            }
        }

        void startAllJobs() {
            for (int i = 0; i < jobs.size(); ++i) {
                if (jobs[i].status == ArtifactRenderJob::Status::Pending) {
                    jobs[i].status = ArtifactRenderJob::Status::Rendering;
                    if (jobStatusChanged) jobStatusChanged(i, jobs[i].status);
                }
            }
        }

        void setJobProgress(int index, int progress) {
            if (index < 0 || index >= jobs.size()) return;
            jobs[index].progress = std::clamp(progress, 0, 100);
            if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);
            if (jobUpdated) jobUpdated(index);
        }

        void setJobCompleted(int index) {
            if (index < 0 || index >= jobs.size()) return;
            jobs[index].status = ArtifactRenderJob::Status::Completed;
            jobs[index].progress = 100;
            if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
            if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);
            if (jobUpdated) jobUpdated(index);
        }

        void setJobFailed(int index, const QString& message) {
            if (index < 0 || index >= jobs.size()) return;
            jobs[index].status = ArtifactRenderJob::Status::Failed;
            jobs[index].errorMessage = message;
            if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
            if (jobUpdated) jobUpdated(index);
        }

        void setJobOutputPath(int index, const QString& outputPath) {
            if (index < 0 || index >= jobs.size()) return;
            jobs[index].outputPath = outputPath;
            if (jobUpdated) jobUpdated(index);
        }

        bool jobFrameRangeAt(int index, int* startFrame, int* endFrame) const {
            if (index < 0 || index >= jobs.size()) {
                return false;
            }
            if (startFrame) *startFrame = jobs[index].startFrame;
            if (endFrame) *endFrame = jobs[index].endFrame;
            return true;
        }

        void setJobFrameRange(int index, int startFrame, int endFrame) {
            if (index < 0 || index >= jobs.size()) return;
            const int clampedStart = std::max(0, startFrame);
            const int clampedEnd = std::max(clampedStart, endFrame);
            jobs[index].startFrame = clampedStart;
            jobs[index].endFrame = clampedEnd;
            if (jobUpdated) jobUpdated(index);
        }

        bool jobOutputSettingsAt(
            int index,
            QString* outputFormat,
            QString* codec,
            int* width,
            int* height,
            double* fps,
            int* bitrateKbps) const
        {
            if (index < 0 || index >= jobs.size()) {
                return false;
            }
            const auto& job = jobs[index];
            if (outputFormat) *outputFormat = job.outputFormat;
            if (codec) *codec = job.codec;
            if (width) *width = job.resolutionWidth;
            if (height) *height = job.resolutionHeight;
            if (fps) *fps = job.frameRate;
            if (bitrateKbps) *bitrateKbps = job.bitrate;
            return true;
        }

        void setJobOutputSettings(
            int index,
            const QString& outputFormat,
            const QString& codec,
            int width,
            int height,
            double fps,
            int bitrateKbps)
        {
            if (index < 0 || index >= jobs.size()) return;
            auto& job = jobs[index];
            const QString fmt = outputFormat.trimmed();
            const QString cdc = codec.trimmed();
            job.outputFormat = fmt.isEmpty() ? QStringLiteral("MP4") : fmt;
            job.codec = cdc.isEmpty() ? QStringLiteral("H.264") : cdc;
            job.resolutionWidth = std::clamp(width, 16, 16384);
            job.resolutionHeight = std::clamp(height, 16, 16384);
            job.frameRate = std::clamp(fps, 1.0, 240.0);
            job.bitrate = std::clamp(bitrateKbps, 128, 200000);
            if (jobUpdated) jobUpdated(index);
        }

        void pauseAllJobs() {
            for (int i = 0; i < jobs.size(); ++i) {
                if (jobs[i].status == ArtifactRenderJob::Status::Rendering) {
                    jobs[i].status = ArtifactRenderJob::Status::Pending;
                    if (jobStatusChanged) jobStatusChanged(i, jobs[i].status);
                }
            }
        }

        void cancelAllJobs() {
            for (int i = 0; i < jobs.size(); ++i) {
                if (jobs[i].status == ArtifactRenderJob::Status::Rendering ||
                    jobs[i].status == ArtifactRenderJob::Status::Pending) {
                    jobs[i].status = ArtifactRenderJob::Status::Canceled;
                    if (jobStatusChanged) jobStatusChanged(i, jobs[i].status);
                }
            }
        }

        ArtifactRenderJob getJob(int index) const {
            return index >= 0 && index < jobs.size() ? jobs[index] : ArtifactRenderJob();
        }

        QString jobCompositionNameAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return {};
            }
            return jobs[index].compositionName;
        }

        QString jobStatusAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return QStringLiteral("Pending");
            }
            switch (jobs[index].status) {
            case ArtifactRenderJob::Status::Pending: return QStringLiteral("Pending");
            case ArtifactRenderJob::Status::Rendering: return QStringLiteral("Rendering");
            case ArtifactRenderJob::Status::Completed: return QStringLiteral("Completed");
            case ArtifactRenderJob::Status::Failed: return QStringLiteral("Failed");
            case ArtifactRenderJob::Status::Canceled: return QStringLiteral("Canceled");
            default: return QStringLiteral("Pending");
            }
        }

        QString jobOutputPathAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return {};
            }
            return jobs[index].outputPath;
        }

        int jobProgressAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return 0;
            }
            return jobs[index].progress;
        }

        QString jobErrorMessageAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return {};
            }
            return jobs[index].errorMessage;
        }

        bool jobOverlayTransformAt(int index, float* offsetX, float* offsetY, float* scale, float* rotationDeg) const {
            if (index < 0 || index >= jobs.size()) {
                return false;
            }
            const auto& job = jobs[index];
            if (offsetX) *offsetX = job.overlayOffsetX;
            if (offsetY) *offsetY = job.overlayOffsetY;
            if (scale) *scale = job.overlayScale;
            if (rotationDeg) *rotationDeg = job.overlayRotationDeg;
            return true;
        }

        void setJobOverlayTransform(int index, float offsetX, float offsetY, float scale, float rotationDeg) {
            if (index < 0 || index >= jobs.size()) {
                return;
            }
            auto& job = jobs[index];
            job.overlayOffsetX = offsetX;
            job.overlayOffsetY = offsetY;
            job.overlayScale = std::clamp(scale, 0.05f, 8.0f);
            job.overlayRotationDeg = rotationDeg;
            if (jobUpdated) jobUpdated(index);
        }

        bool resetJobForRerun(int index) {
            if (index < 0 || index >= jobs.size()) {
                return false;
            }
            auto& job = jobs[index];
            const bool wasRenderable = (job.status == ArtifactRenderJob::Status::Completed
                || job.status == ArtifactRenderJob::Status::Failed
                || job.status == ArtifactRenderJob::Status::Canceled);
            
            // 既存ファイルの上書き防止のためのリネーム処理 (_v1, _v2 ...)
            QFileInfo fi(job.outputPath);
            if (fi.exists() && job.status == ArtifactRenderJob::Status::Completed) {
                QString basePath = fi.path() + "/" + fi.completeBaseName();
                QString ext = fi.suffix();
                if (!ext.isEmpty()) ext = "." + ext;
                
                // すでに _v\d+ が付いている場合はそこからインクリメントを試みる
                QRegularExpression re("_v(\\d+)$");
                QRegularExpressionMatch match = re.match(basePath);
                int version = 1;
                if (match.hasMatch()) {
                    version = match.captured(1).toInt() + 1;
                    basePath = basePath.left(match.capturedStart());
                }

                QString newPath;
                do {
                    newPath = QString("%1_v%2%3").arg(basePath).arg(version).arg(ext);
                    version++;
                } while (QFile::exists(newPath));
                job.outputPath = newPath;
            }

            job.status = ArtifactRenderJob::Status::Pending;
            job.progress = 0;
            job.errorMessage.clear();
            if (jobStatusChanged) jobStatusChanged(index, job.status);
            if (jobProgressChanged) jobProgressChanged(index, job.progress);
            if (jobUpdated) jobUpdated(index);
            return wasRenderable;
        }

        int resetCompletedAndFailedJobsForRerun() {
            int changed = 0;
            for (int i = 0; i < jobs.size(); ++i) {
                const auto status = jobs[i].status;
                if (status == ArtifactRenderJob::Status::Completed
                    || status == ArtifactRenderJob::Status::Failed) {
                    resetJobForRerun(i);
                    ++changed;
                }
            }
            return changed;
        }

        int countJobsForComposition(const ArtifactCore::CompositionID& compositionId) const {
            int count = 0;
            for (const auto& job : jobs) {
                if (job.compositionId == compositionId) {
                    ++count;
                }
            }
            return count;
        }

        bool hasJobForComposition(const ArtifactCore::CompositionID& compositionId) const {
            return countJobsForComposition(compositionId) > 0;
        }

        ArtifactCore::CompositionID jobCompositionIdAt(int index) const {
            if (index < 0 || index >= jobs.size()) {
                return ArtifactCore::CompositionID::Nil();
            }
            return jobs[index].compositionId;
        }

        void removeJobsForComposition(const ArtifactCore::CompositionID& compositionId) {
            for (int i = jobs.size() - 1; i >= 0; --i) {
                if (jobs[i].compositionId == compositionId) {
                    jobs.removeAt(i);
                    if (jobRemoved) {
                        jobRemoved(i);
                    }
                }
            }
            if (jobs.isEmpty() && allJobsRemoved) {
                allJobsRemoved();
            }
        }

        int jobCount() const {
            return jobs.size();
        }

        int getTotalProgress() const {
            if (jobs.isEmpty()) return 0;

            int totalProgress = 0;
            int activeJobs = 0;

            for (const ArtifactRenderJob& job : jobs) {
                if (job.status == ArtifactRenderJob::Status::Rendering || job.status == ArtifactRenderJob::Status::Completed) {
                    totalProgress += job.progress;
                    activeJobs++;
                }
            }

            return activeJobs > 0 ? totalProgress / activeJobs : 0;
        }

        // シグナル
        std::function<void(int)> jobAdded;
        std::function<void(int)> jobRemoved;
        std::function<void(int)> jobUpdated;
        std::function<void(int, ArtifactRenderJob::Status)> jobStatusChanged;
        std::function<void(int, int)> jobStatusChangedForUi;
        std::function<void(int, int)> jobProgressChanged;
        std::function<void()> allJobsCompleted;
        std::function<void()> allJobsRemoved;
        std::function<void(int, int)> queueReordered;

    private:
        QList<ArtifactRenderJob> jobs;
    };

    class ArtifactRenderQueueService::Impl {
    public:
        Impl() {
            // ArtifactCoreのレンダリングキューマネージャにコールバックを登録
            auto& coreQueueManager = ArtifactCore::RendererQueueManager::instance();
            coreQueueManager.setRenderFrameFunc([this](const ArtifactCore::Id& compId, int frame, const QString& path) {
                // 順序制御を行いつつエンコーダに送る
                {
                    std::lock_guard<std::mutex> lock(encoderMutex);
                    // 本来はここでレンダリングされたImageを取得し、バッファに格納する
                    // frameBuffer[frame] = renderedImage; 

                    while (frameBuffer.count(nextFrameToEncode)) {
                        if (ffmpegEncoder) {
                            ffmpegEncoder->addImage(frameBuffer[nextFrameToEncode]);
                        }
                        frameBuffer.erase(nextFrameToEncode);
                        nextFrameToEncode++;
                    }
                }
            });

            queueManager.jobAdded = [this](int index) {
                handleJobAdded(index);
            };

            queueManager.jobRemoved = [this](int index) {
                handleJobRemoved(index);
            };

            queueManager.jobUpdated = [this](int index) {
                handleJobUpdated(index);
            };

            queueManager.jobStatusChanged = [this](int index, ArtifactRenderJob::Status status) {
                handleJobStatusChanged(index, status);
            };

            queueManager.jobProgressChanged = [this](int index, int progress) {
                handleJobProgressChanged(index, progress);
            };

            queueManager.queueReordered = [this](int fromIndex, int toIndex) {
                if (queueReordered) {
                    queueReordered(fromIndex, toIndex);
                }
            };
        }

        ~Impl() = default;

        ArtifactRenderQueueManager queueManager;
        std::unique_ptr<ArtifactCore::FFmpegEncoder> ffmpegEncoder;
        std::map<int, ArtifactCore::ImageF32x4_RGBA> frameBuffer;
        int nextFrameToEncode = 0;
        std::mutex encoderMutex;
        bool isRendering = false;

        QString resolveDummyOutputPath(const ArtifactRenderJob& job, int index) const {
            QString target = job.outputPath.trimmed();
            const QString safeName = job.compositionName.trimmed().isEmpty()
                ? QStringLiteral("Composition")
                : job.compositionName.trimmed();
            const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
            const QString fileName = QStringLiteral("%1_%2_%3.png").arg(safeName).arg(index + 1).arg(stamp);

            if (target.isEmpty()) {
                return QDir(QDir::homePath() + QStringLiteral("/Desktop")).filePath(fileName);
            }

            QFileInfo info(target);
            if (info.isDir()) {
                return QDir(info.absoluteFilePath()).filePath(fileName);
            }

            QString dir = info.absolutePath();
            if (dir.isEmpty()) {
                dir = QDir::homePath() + QStringLiteral("/Desktop");
            }
            QString base = info.completeBaseName();
            if (base.isEmpty()) {
                base = QStringLiteral("render");
            }
            return QDir(dir).filePath(base + QStringLiteral(".png"));
        }

        QImage renderSingleFrameDummy(const ArtifactRenderJob& job, int frameNumber) {
            const int width = std::max(16, job.resolutionWidth);
            const int height = std::max(16, job.resolutionHeight);
            QImage background(width, height, QImage::Format_ARGB32_Premultiplied);
            background.fill(QColor(24, 26, 30, 255));
            {
                QPainter painter(&background);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.fillRect(QRect(0, 0, width, height), QColor(32, 36, 42));
            }

            QImage foreground(width, height, QImage::Format_ARGB32_Premultiplied);
            foreground.fill(Qt::transparent);
            {
                QPainter painter(&foreground);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.fillRect(QRect(width / 16, height / 8, width * 7 / 8, height * 3 / 4), QColor(64, 84, 132, 220));
                painter.setPen(QColor(230, 230, 230));
                painter.drawText(QRect(0, 0, width, height / 3),
                                 Qt::AlignCenter,
                                 QStringLiteral("Artifact Dummy Render"));
                painter.drawText(QRect(0, height / 3, width, height / 3),
                                 Qt::AlignCenter,
                                 QStringLiteral("Comp: %1").arg(job.compositionName));
                painter.drawText(QRect(0, (height * 2) / 3, width, height / 3),
                                 Qt::AlignCenter,
                                 QStringLiteral("Frame: %1").arg(frameNumber));
            }

            QImage overlay(width, height, QImage::Format_ARGB32_Premultiplied);
            overlay.fill(Qt::transparent);
            {
                QPainter painter(&overlay);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 190, 90, 180));
                // Animate the ellipse based on frame number
                const int animOffset = (frameNumber * 5) % (width / 2);
                painter.drawEllipse(QRect(width * 3 / 5 - animOffset, height / 8, width / 4, height / 4));
            }

            SoftwareRender::CompositeRequest request;
            request.background = background;
            request.foreground = foreground;
            request.overlay = overlay;
            request.outputSize = QSize(width, height);
            request.backend = SoftwareRender::CompositeBackend::QtPainter;
            request.blendMode = ArtifactCore::BlendMode::Screen;
            request.cvEffect = SoftwareRender::CvEffectMode::None;
            request.overlayOpacity = 0.70f;
            request.overlayOffset = QPointF(job.overlayOffsetX, job.overlayOffsetY);
            request.overlayScale = job.overlayScale;
            request.overlayRotationDeg = job.overlayRotationDeg;
            request.useForeground = true;

            return SoftwareRender::compose(request);
        }

        bool renderSingleFrameGPU(const ArtifactRenderJob& job, int index, QString* outputPath, QString* errorMessage) {
            const int width  = std::max(16, job.resolutionWidth);
            const int height = std::max(16, job.resolutionHeight);

            ArtifactIRenderer renderer;
            renderer.initializeHeadless(width, height);
            renderer.clear();
            renderer.flush();

            QImage frame = renderer.readbackToImage();
            renderer.destroy();

            if (frame.isNull()) {
                if (errorMessage) *errorMessage = QStringLiteral("GPU readback failed");
                return false;
            }

            const QString outPath = resolveDummyOutputPath(job, index);
            if (outputPath) *outputPath = outPath;

            QDir outDir(QFileInfo(outPath).absolutePath());
            if (!outDir.exists() && !outDir.mkpath(QStringLiteral("."))) {
                if (errorMessage) *errorMessage = QStringLiteral("Failed to create output directory: %1").arg(outDir.absolutePath());
                return false;
            }

            if (!frame.save(outPath, "PNG")) {
                if (errorMessage) *errorMessage = QStringLiteral("Failed to save image: %1").arg(outPath);
                return false;
            }
            return true;
        }

        void syncCoreQueueModel() {
            auto& coreQueue = ArtifactCore::RendererQueueManager::instance();
            coreQueue.clearRenderQueue();
            const int count = queueManager.jobCount();
            for (int i = 0; i < count; ++i) {
                const auto job = queueManager.getJob(i);
                coreQueue.addJob(job.compositionId, job.compositionName);
            }
        }

        void handleJobAdded(int index) {
            if (jobAdded) jobAdded(index);
        }

        void handleJobRemoved(int index) {
            if (jobRemoved) jobRemoved(index);
        }

        void handleJobUpdated(int index) {
            if (jobUpdated) jobUpdated(index);
        }

        void handleJobStatusChanged(int index, ArtifactRenderJob::Status status) {
            if (jobStatusChangedForUi) jobStatusChangedForUi(index, static_cast<int>(status));
            if (jobStatusChanged) jobStatusChanged(index, status);
        }

        void handleJobProgressChanged(int index, int progress) {
            if (jobProgressChanged) jobProgressChanged(index, progress);
        }

        // シグナル
        std::function<void(int)> jobAdded;
        std::function<void(int)> jobRemoved;
        std::function<void(int)> jobUpdated;
        std::function<void(int, int)> jobStatusChangedForUi;
        std::function<void(int, ArtifactRenderJob::Status)> jobStatusChanged;
        std::function<void(int, int)> jobProgressChanged;
        std::function<void()> allJobsCompleted;
        std::function<void()> allJobsRemoved;
        std::function<void(int, int)> queueReordered;
    };

    W_OBJECT_IMPL(ArtifactRenderQueueService)

    ArtifactRenderQueueService::ArtifactRenderQueueService(QObject* parent /*= nullptr*/)
        : QObject(parent), impl_(new Impl) {
    }

    ArtifactRenderQueueService::~ArtifactRenderQueueService() {
        delete impl_;
    }

    ArtifactRenderQueueService* ArtifactRenderQueueService::instance()
    {
        static ArtifactRenderQueueService service;
        return &service;
    }

    void ArtifactRenderQueueService::addRenderQueue() {
        auto currentComposition = ArtifactProjectManager::getInstance().currentComposition();
        if (currentComposition) {
            const auto compositionName = currentComposition->settings().compositionName().toQString();
            addRenderQueueForComposition(currentComposition->id(), compositionName);
            return;
        }

        ArtifactRenderJob job;
        job.compositionId = ArtifactCore::CompositionID::Nil();
        job.compositionName = "New Render Job";
        job.status = ArtifactRenderJob::Status::Pending;
        job.outputPath = QDir::homePath() + "/Desktop/output.mp4";
        job.outputFormat = "MP4";
        job.codec = "H.264";
        job.resolutionWidth = 1920;
        job.resolutionHeight = 1080;
        job.frameRate = 30.0;
        job.bitrate = 8000;
        job.startFrame = 0;
        job.endFrame = 100;

        impl_->queueManager.addJob(job);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::addRenderQueueForComposition(const ArtifactCore::CompositionID& compositionId, const QString& compositionName)
    {
        ArtifactRenderJob job;
        job.compositionId = compositionId;
        job.compositionName = compositionName.trimmed().isEmpty() ? QStringLiteral("Composition") : compositionName.trimmed();
        job.status = ArtifactRenderJob::Status::Pending;
        job.outputPath = QDir::homePath() + "/Desktop/output.mp4";
        job.outputFormat = "MP4";
        job.codec = "H.264";
        job.resolutionWidth = 1920;
        job.resolutionHeight = 1080;
        job.frameRate = 30.0;
        job.bitrate = 8000;
        job.startFrame = 0;
        job.endFrame = 100;

        const auto found = ArtifactProjectManager::getInstance().findComposition(compositionId);
        if (found.success) {
            if (const auto comp = found.ptr.lock()) {
                const auto totalRange = comp->frameRange();
                const auto workAreaRange = comp->workAreaRange();
                job.startFrame = static_cast<int>(std::max<int64_t>(0, workAreaRange.start()));
                job.endFrame = static_cast<int>(std::max<int64_t>(job.startFrame + 1, workAreaRange.end()));
                job.frameRate = comp->frameRate().framerate();
                const QSize size = comp->settings().compositionSize();
                if (size.width() > 0 && size.height() > 0) {
                    job.resolutionWidth = size.width();
                    job.resolutionHeight = size.height();
                }
                if (!totalRange.isValid() || totalRange.duration() <= 0) {
                    job.startFrame = 0;
                    job.endFrame = 100;
                }
            }
        }

        impl_->queueManager.addJob(job);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::removeRenderQueue() {
        const int count = impl_->queueManager.jobCount();
        if (count <= 0) {
            return;
        }
        impl_->queueManager.removeJob(count - 1);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::removeRenderQueueAt(int index) {
        impl_->queueManager.removeJob(index);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::duplicateRenderQueueAt(int index)
    {
        const int count = impl_->queueManager.jobCount();
        if (index < 0 || index >= count) {
            return;
        }
        const ArtifactRenderJob source = impl_->queueManager.getJob(index);
        ArtifactRenderJob copy = source;
        copy.status = ArtifactRenderJob::Status::Pending;
        copy.progress = 0;
        copy.errorMessage.clear();
        if (!copy.compositionName.trimmed().isEmpty()) {
            copy.compositionName += QStringLiteral(" Copy");
        }

        impl_->queueManager.addJob(copy);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::moveRenderQueue(int fromIndex, int toIndex)
    {
        const int count = impl_->queueManager.jobCount();
        if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count || fromIndex == toIndex) {
            return;
        }
        impl_->queueManager.moveJob(fromIndex, toIndex);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::removeAllRenderQueues() {
        impl_->queueManager.removeAllJobs();
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::removeRenderQueuesForComposition(const ArtifactCore::CompositionID& compositionId)
    {
        impl_->queueManager.removeJobsForComposition(compositionId);
        impl_->syncCoreQueueModel();
    }

    bool ArtifactRenderQueueService::hasRenderQueueForComposition(const ArtifactCore::CompositionID& compositionId) const
    {
        return impl_->queueManager.hasJobForComposition(compositionId);
    }

    int ArtifactRenderQueueService::renderQueueCountForComposition(const ArtifactCore::CompositionID& compositionId) const
    {
        return impl_->queueManager.countJobsForComposition(compositionId);
    }

    ArtifactCore::CompositionID ArtifactRenderQueueService::jobCompositionIdAt(int index) const
    {
        return impl_->queueManager.jobCompositionIdAt(index);
    }

    QString ArtifactRenderQueueService::jobCompositionNameAt(int index) const
    {
        return impl_->queueManager.jobCompositionNameAt(index);
    }

    QString ArtifactRenderQueueService::jobStatusAt(int index) const
    {
        return impl_->queueManager.jobStatusAt(index);
    }

    int ArtifactRenderQueueService::jobProgressAt(int index) const
    {
        return impl_->queueManager.jobProgressAt(index);
    }

    QString ArtifactRenderQueueService::jobOutputPathAt(int index) const
    {
        return impl_->queueManager.jobOutputPathAt(index);
    }

    void ArtifactRenderQueueService::setJobOutputPathAt(int index, const QString& outputPath)
    {
        impl_->queueManager.setJobOutputPath(index, outputPath.trimmed());
        impl_->syncCoreQueueModel();
    }

    bool ArtifactRenderQueueService::jobFrameRangeAt(int index, int* startFrame, int* endFrame) const
    {
        return impl_->queueManager.jobFrameRangeAt(index, startFrame, endFrame);
    }

    void ArtifactRenderQueueService::setJobFrameRangeAt(int index, int startFrame, int endFrame)
    {
        impl_->queueManager.setJobFrameRange(index, startFrame, endFrame);
        impl_->syncCoreQueueModel();
    }

    bool ArtifactRenderQueueService::jobOutputSettingsAt(
        int index,
        QString* outputFormat,
        QString* codec,
        int* width,
        int* height,
        double* fps,
        int* bitrateKbps) const
    {
        return impl_->queueManager.jobOutputSettingsAt(index, outputFormat, codec, width, height, fps, bitrateKbps);
    }

    void ArtifactRenderQueueService::setJobOutputSettingsAt(
        int index,
        const QString& outputFormat,
        const QString& codec,
        int width,
        int height,
        double fps,
        int bitrateKbps)
    {
        impl_->queueManager.setJobOutputSettings(index, outputFormat, codec, width, height, fps, bitrateKbps);
        impl_->syncCoreQueueModel();
    }

    QString ArtifactRenderQueueService::jobErrorMessageAt(int index) const
    {
        return impl_->queueManager.jobErrorMessageAt(index);
    }

    bool ArtifactRenderQueueService::jobOverlayTransformAt(int index, float* offsetX, float* offsetY, float* scale, float* rotationDeg) const
    {
        return impl_->queueManager.jobOverlayTransformAt(index, offsetX, offsetY, scale, rotationDeg);
    }

    void ArtifactRenderQueueService::setJobOverlayTransform(int index, float offsetX, float offsetY, float scale, float rotationDeg)
    {
        impl_->queueManager.setJobOverlayTransform(index, offsetX, offsetY, scale, rotationDeg);
        impl_->syncCoreQueueModel();
    }

    void ArtifactRenderQueueService::resetJobForRerun(int index)
    {
        impl_->queueManager.resetJobForRerun(index);
        impl_->syncCoreQueueModel();
    }

    int ArtifactRenderQueueService::resetCompletedAndFailedJobsForRerun()
    {
        const int changed = impl_->queueManager.resetCompletedAndFailedJobsForRerun();
        if (changed > 0) {
            impl_->syncCoreQueueModel();
        }
        return changed;
    }

    void ArtifactRenderQueueService::startAllJobs() {
        impl_->queueManager.startAllJobs();

        const int count = impl_->queueManager.jobCount();
        for (int i = 0; i < count; ++i) {
            const auto job = impl_->queueManager.getJob(i);
            if (job.status != ArtifactRenderJob::Status::Rendering &&
                job.status != ArtifactRenderJob::Status::Pending) {
                continue;
            }

            impl_->queueManager.setJobProgress(i, 0);

            // エンコーダの初期化
            impl_->ffmpegEncoder = std::make_unique<ArtifactCore::FFmpegEncoder>();
            QFile outFile(job.outputPath);
            impl_->ffmpegEncoder->open(outFile);

            const int startF = job.startFrame;
            const int endF = job.endFrame;
            const int totalFrames = std::max(1, endF - startF + 1);

            QString error;
            std::atomic<bool> success = true;
            std::atomic<int> framesRendered = 0;
            std::mutex localEncoderMutex;
            int localNextFrame = startF;
            std::map<int, ArtifactCore::ImageF32x4_RGBA> localFrameBuffer;

            // TBBを利用したマルチフレームレンダリング (MFR)
            tbb::parallel_for(tbb::blocked_range<int>(startF, endF + 1),
                [&](const tbb::blocked_range<int>& r) {
                    for (int f = r.begin(); f != r.end(); ++f) {
                        if (!success.load(std::memory_order_relaxed)) break;

                        // To properly stop rendering if canceled/paused, we should check status
                        const auto currentJobStatus = impl_->queueManager.getJob(i).status;
                        if (currentJobStatus != ArtifactRenderJob::Status::Rendering) {
                            success.store(false, std::memory_order_relaxed);
                            break;
                        }

                        // Render frame (スレッドセーフなソフトウェアレンダリング)
                        QImage qimg = impl_->renderSingleFrameDummy(job, f);

                        // Add to encoder
                        if (!qimg.isNull()) {
                            cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(qimg);
                            ArtifactCore::ImageF32x4_RGBA frameImage;
                            frameImage.setFromCVMat(mat);

                            // エンコーダは順序を保証する必要があるため、ロックを取得してバッファ経由で渡す
                            std::lock_guard<std::mutex> lock(localEncoderMutex);
                            localFrameBuffer[f] = frameImage;
                            
                            while (localFrameBuffer.count(localNextFrame)) {
                                impl_->ffmpegEncoder->addImage(localFrameBuffer[localNextFrame]);
                                localFrameBuffer.erase(localNextFrame);
                                localNextFrame++;
                            }

                            int rendered = ++framesRendered;
                            // Update progress
                            int progress = static_cast<int>((static_cast<float>(rendered) / totalFrames) * 100);
                            // UI更新はメインスレッドから呼ばれる想定だが、setJobProgress内で適切に処理されると仮定
                            QMetaObject::invokeMethod(this, [this, i, progress]() {
                                impl_->queueManager.setJobProgress(i, progress);
                            }, Qt::QueuedConnection);
                            
                        } else {
                            success.store(false, std::memory_order_relaxed);
                            break;
                        }
                    }
                }
            );

            impl_->ffmpegEncoder->close();

            if (success.load()) {
                impl_->queueManager.setJobProgress(i, 100);
                impl_->queueManager.setJobCompleted(i);
            } else {
                impl_->queueManager.setJobFailed(i, QStringLiteral("Render interrupted or failed"));
            }
        }

        if (impl_->allJobsCompleted) {
            impl_->allJobsCompleted();
        }

        ArtifactCore::RendererQueueManager::instance().startRendering();
    }

    void ArtifactRenderQueueService::pauseAllJobs() {
        impl_->queueManager.pauseAllJobs();
    }

    void ArtifactRenderQueueService::cancelAllJobs() {
        impl_->queueManager.cancelAllJobs();
    }

    int ArtifactRenderQueueService::jobCount() const {
        return impl_->queueManager.jobCount();
    }

    int ArtifactRenderQueueService::getTotalProgress() const {
        return impl_->queueManager.getTotalProgress();
    }

    // シグナルセッターメソッド
    void ArtifactRenderQueueService::setJobAddedCallback(std::function<void(int)> callback) {
        impl_->jobAdded = callback;
    }

    void ArtifactRenderQueueService::setJobRemovedCallback(std::function<void(int)> callback) {
        impl_->jobRemoved = callback;
    }

    void ArtifactRenderQueueService::setJobUpdatedCallback(std::function<void(int)> callback) {
        impl_->jobUpdated = callback;
    }

    void ArtifactRenderQueueService::setJobProgressChangedCallback(std::function<void(int, int)> callback) {
        impl_->jobProgressChanged = callback;
    }

    void ArtifactRenderQueueService::setJobStatusChangedCallback(std::function<void(int, int)> callback) {
        impl_->jobStatusChangedForUi = callback;
    }

    void ArtifactRenderQueueService::setAllJobsCompletedCallback(std::function<void()> callback) {
        impl_->allJobsCompleted = callback;
    }

    void ArtifactRenderQueueService::setAllJobsRemovedCallback(std::function<void()> callback) {
        impl_->allJobsRemoved = callback;
    }

    void ArtifactRenderQueueService::setQueueReorderedCallback(std::function<void(int, int)> callback) {
        impl_->queueReordered = callback;
    }
};
