module;

#include <QString>
#include <QStringList>

module Menu.Render;

import Artifact.Render.Queue.Service;
import Utils.Id;

namespace Artifact {

struct RenderMenuQueueSnapshot {
    bool hasQueueService = false;
    bool hasJobs = false;
};

struct RenderMenuPreflightSnapshot {
    QString summary;
    QStringList details;
};

RenderMenuQueueSnapshot renderMenuQueueSnapshot()
{
    RenderMenuQueueSnapshot snapshot;
    auto* queueService = ArtifactRenderQueueService::instance();
    snapshot.hasQueueService = queueService != nullptr;
    snapshot.hasJobs = queueService && queueService->jobCount() > 0;
    return snapshot;
}

RenderMenuPreflightSnapshot renderMenuPreflightSnapshot()
{
    RenderMenuPreflightSnapshot snapshot;
    if (auto* queueService = ArtifactRenderQueueService::instance()) {
        const auto preflight = queueService->preflightAllRenderQueues();
        snapshot.summary =
            ArtifactRenderQueueService::formatPreflightSummary(preflight);
        snapshot.details =
            ArtifactRenderQueueService::formatPreflightDetails(preflight);
    }
    return snapshot;
}

bool addRenderMenuCompositionToQueue(const QString& compositionId,
                                     const QString& compositionName)
{
    auto* queueService = ArtifactRenderQueueService::instance();
    if (!queueService) {
        return false;
    }
    queueService->addRenderQueueForComposition(
        ArtifactCore::CompositionID(compositionId), compositionName);
    return true;
}

int addAllCompositionsToRenderQueue()
{
    auto* queueService = ArtifactRenderQueueService::instance();
    return queueService ? queueService->addAllCompositions() : -1;
}

void startAllRenderJobs()
{
    if (auto* service = ArtifactRenderQueueService::instance()) {
        service->startAllJobs();
    }
}

void pauseAllRenderJobs()
{
    if (auto* service = ArtifactRenderQueueService::instance()) {
        service->pauseAllJobs();
    }
}

void cancelAllRenderJobs()
{
    if (auto* service = ArtifactRenderQueueService::instance()) {
        service->cancelAllJobs();
    }
}

void clearAllRenderQueues()
{
    if (auto* service = ArtifactRenderQueueService::instance()) {
        service->removeAllRenderQueues();
    }
}

} // namespace Artifact
