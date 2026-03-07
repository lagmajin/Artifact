#include <iostream>
#include <QCoreApplication>
#include <QDebug>
#include "Artifact.Render.Queue.Service.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // レンダリングキューサービスのインスタンスを取得
    auto& renderQueueService = ArtifactRenderQueueService::getInstance();

    // コールバックの設定
    renderQueueService.setJobAddedCallback([](int index) {
        qDebug() << "Job added at index:" << index;
    });

    renderQueueService.setJobStatusChangedCallback([](int index, ArtifactRenderJob::Status status) {
        QString statusText;
        switch (status) {
            case ArtifactRenderJob::Status::Pending:
                statusText = "Pending";
                break;
            case ArtifactRenderJob::Status::Rendering:
                statusText = "Rendering";
                break;
            case ArtifactRenderJob::Status::Completed:
                statusText = "Completed";
                break;
            case ArtifactRenderJob::Status::Failed:
                statusText = "Failed";
                break;
            case ArtifactRenderJob::Status::Canceled:
                statusText = "Canceled";
                break;
        }
        qDebug() << "Job" << index << "status changed to:" << statusText;
    });

    // ジョブの追加テスト
    qDebug() << "Adding jobs to render queue...";
    for (int i = 0; i < 3; ++i) {
        ArtifactRenderJob job;
        job.compositionName = QString("Composition %1").arg(i + 1);
        job.outputPath = QString("C:/temp/render%1.mp4").arg(i + 1);
        job.status = ArtifactRenderJob::Status::Pending;
        job.startFrame = 0;
        job.endFrame = 100;
        job.progress = 0;
        // 実際のサービスではaddRenderQueue()を使用するが、
        // このテストでは直接マネージャーを操作するか、
        // サービスのメソッドを使用します。
        // renderQueueService.addRenderQueue(job); // 実装されている場合はこれを使用
    }

    // ジョブ数の取得
    qDebug() << "Number of jobs in queue:" << renderQueueService.jobCount();

    // ジョブの開始テスト
    qDebug() << "Starting all jobs...";
    renderQueueService.startAllJobs();

    // ジョブのステータスチェック
    for (int i = 0; i < renderQueueService.jobCount(); ++i) {
        ArtifactRenderJob job = renderQueueService.getJob(i);
        qDebug() << "Job" << i << "status:" << static_cast<int>(job.status);
    }

    // 進捗更新テスト
    qDebug() << "Updating job progress...";
    for (int i = 0; i < renderQueueService.jobCount(); ++i) {
        // 実際のレンダリング中にはプログレスが更新されます
        // renderQueueService.updateJobProgress(i, (i + 1) * 25);
    }

    qDebug() << "Total progress:" << renderQueueService.getTotalProgress() << "%";

    // ジョブの一時停止テスト
    qDebug() << "Pausing all jobs...";
    renderQueueService.pauseAllJobs();

    // ジョブのステータス再チェック
    for (int i = 0; i < renderQueueService.jobCount(); ++i) {
        ArtifactRenderJob job = renderQueueService.getJob(i);
        qDebug() << "Job" << i << "status after pause:" << static_cast<int>(job.status);
    }

    // ジョブのクリアテスト
    qDebug() << "Clearing all jobs...";
    renderQueueService.removeAllRenderQueues();
    qDebug() << "Number of jobs after clear:" << renderQueueService.jobCount();

    qDebug() << "Render queue management test completed!";

    return 0;
}