module;
#include <QObject>
#include <QList>
#include <QThread>
#include <QDir>
#include <wobjectimpl.h>
module Artifact.Render.Queue.Service;

import std;
//import Container.MultiIndex;
//import Artifact.Render.Queue;
import Render.Queue.Manager;
import Artifact.Project.Manager;
import Artifact.Project.Items;

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

        ArtifactRenderJob()
            : resolutionWidth(1920)
            , resolutionHeight(1080)
            , frameRate(30.0)
            , bitrate(8000)
            , startFrame(0)
            , endFrame(100)
            , status(Status::Pending)
            , progress(0)
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
                if (jobUpdated) jobUpdated(toIndex);
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
                if (jobs[i].status == ArtifactRenderJob::Status::Rendering) {
                    jobs[i].status = ArtifactRenderJob::Status::Canceled;
                    if (jobStatusChanged) jobStatusChanged(i, jobs[i].status);
                }
            }
        }

        ArtifactRenderJob getJob(int index) const {
            return index >= 0 && index < jobs.size() ? jobs[index] : ArtifactRenderJob();
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
        std::function<void(int, int)> jobProgressChanged;
        std::function<void()> allJobsCompleted;
        std::function<void()> allJobsRemoved;

    private:
        QList<ArtifactRenderJob> jobs;
    };

    class ArtifactRenderQueueService::Impl {
    public:
        Impl() {
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
        }

        ~Impl() = default;

        ArtifactRenderQueueManager queueManager;
        QThread* renderingThread = nullptr;
        bool isRendering = false;

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
            if (jobStatusChanged) jobStatusChanged(index, status);
        }

        void handleJobProgressChanged(int index, int progress) {
            if (jobProgressChanged) jobProgressChanged(index, progress);
        }

        // シグナル
        std::function<void(int)> jobAdded;
        std::function<void(int)> jobRemoved;
        std::function<void(int)> jobUpdated;
        std::function<void(int, ArtifactRenderJob::Status)> jobStatusChanged;
        std::function<void(int, int)> jobProgressChanged;
        std::function<void()> allJobsCompleted;
        std::function<void()> allJobsRemoved;
    };

    W_OBJECT_IMPL(ArtifactRenderQueueService)

    ArtifactRenderQueueService::ArtifactRenderQueueService(QObject* parent /*= nullptr*/)
        : QObject(parent), impl_(new Impl) {
    }

    ArtifactRenderQueueService::~ArtifactRenderQueueService() {
        delete impl_;
    }

    void ArtifactRenderQueueService::addRenderQueue() {
        // TODO: Implement with proper composition API
        // auto& projectManager = ArtifactProjectManager::getInstance();
        // auto currentComp = projectManager.currentComposition();

        // Create a default job for now
        ArtifactRenderJob job;
        job.compositionName = "DefaultComposition";
        job.status = ArtifactRenderJob::Status::Pending;
        job.outputPath = QDir::homePath() + "/Desktop/output.mp4";
        job.outputFormat = "MP4";
        job.codec = "H.264";
        job.resolutionWidth = 1920;
        job.resolutionHeight = 1080;
        job.frameRate = 30.0;
        job.bitrate = 8000; // 8Mbps
        job.startFrame = 0;
        job.endFrame = 100;

        impl_->queueManager.addJob(job);
    }

    void ArtifactRenderQueueService::removeRenderQueue() {
        // 実装予定: 選択されたジョブを削除
    }

    void ArtifactRenderQueueService::removeAllRenderQueues() {
        impl_->queueManager.removeAllJobs();
    }

    void ArtifactRenderQueueService::startAllJobs() {
        impl_->queueManager.startAllJobs();
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

    // ArtifactRenderJob ArtifactRenderQueueService::getJob(int index) const {
    //     return impl_->queueManager.getJob(index);
    // }

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

    // void ArtifactRenderQueueService::setJobStatusChangedCallback(std::function<void(int, ArtifactRenderJob::Status)> callback) {
    //     impl_->jobStatusChanged = callback;
    // }

    void ArtifactRenderQueueService::setJobProgressChangedCallback(std::function<void(int, int)> callback) {
        impl_->jobProgressChanged = callback;
    }

    void ArtifactRenderQueueService::setAllJobsCompletedCallback(std::function<void()> callback) {
        impl_->allJobsCompleted = callback;
    }

    void ArtifactRenderQueueService::setAllJobsRemovedCallback(std::function<void()> callback) {
        impl_->allJobsRemoved = callback;
    }
};
