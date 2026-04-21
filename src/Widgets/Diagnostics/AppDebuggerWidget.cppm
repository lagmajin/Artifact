module;
#include <algorithm>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonDocument>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QTimerEvent>
#include <QVector>
#include <QStringList>
#include <cstdint>
#include <wobjectimpl.h>

module Artifact.Widgets.AppDebuggerWidget;

import Core.Diagnostics.Trace;
import Artifact.Service.Playback;
import Artifact.Service.Project;
import Artifact.Render.Queue.Service;
import Artifact.Widgets.CompositionRenderController;
import Artifact.Widgets.FramePipelineViewWidget;
import Artifact.Widgets.FrameResourceInspectorWidget;
import Artifact.Widgets.FrameStateDiffWidget;
import Artifact.Widgets.TraceTimelineWidget;
import Playback.State;
import Utils.String.UniString;

namespace Artifact {

W_OBJECT_IMPL(AppDebuggerWidget)

class AppDebuggerWidget::Impl {
public:
    AppDebuggerWidget* owner_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QPlainTextEdit* stateText_ = nullptr;
    QPlainTextEdit* traceText_ = nullptr;
    FramePipelineViewWidget* pipelineView_ = nullptr;
    FrameResourceInspectorWidget* resourceView_ = nullptr;
    FrameStateDiffWidget* diffView_ = nullptr;
    TraceTimelineWidget* traceTimelineView_ = nullptr;
    QPlainTextEdit* frameText_ = nullptr;
    QPlainTextEdit* diagnosticsText_ = nullptr;
    QPlainTextEdit* exportText_ = nullptr;
    CompositionRenderController* controller_ = nullptr;
    int timerId_ = 0;

    explicit Impl(AppDebuggerWidget* owner, CompositionRenderController* controller)
        : owner_(owner), controller_(controller)
    {}

    void setupUI()
    {
        auto* layout = new QVBoxLayout(owner_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        tabs_ = new QTabWidget(owner_);
        stateText_ = createPage(QStringLiteral("State"));
        traceText_ = createPage(QStringLiteral("Trace"));
        pipelineView_ = new FramePipelineViewWidget(tabs_);
        resourceView_ = new FrameResourceInspectorWidget(tabs_);
        diffView_ = new FrameStateDiffWidget(tabs_);
        traceTimelineView_ = new TraceTimelineWidget(tabs_);
        frameText_ = createPage(QStringLiteral("Frame"));
        diagnosticsText_ = createPage(QStringLiteral("Diagnostics"));
        exportText_ = createPage(QStringLiteral("Export"));

        tabs_->addTab(stateText_, QStringLiteral("State"));
        tabs_->addTab(traceText_, QStringLiteral("Trace"));
        tabs_->addTab(pipelineView_, QStringLiteral("Pipeline"));
        tabs_->addTab(resourceView_, QStringLiteral("Resource"));
        tabs_->addTab(diffView_, QStringLiteral("State Diff"));
        tabs_->addTab(traceTimelineView_, QStringLiteral("Trace Timeline"));
        tabs_->addTab(frameText_, QStringLiteral("Frame"));
        tabs_->addTab(diagnosticsText_, QStringLiteral("Diagnostics"));
        tabs_->addTab(exportText_, QStringLiteral("Export"));

        layout->addWidget(tabs_);

        timerId_ = owner_->startTimer(250);
        refresh();
    }

    QPlainTextEdit* createPage(const QString&)
    {
        auto* edit = new QPlainTextEdit(owner_);
        edit->setReadOnly(true);
        edit->setLineWrapMode(QPlainTextEdit::NoWrap);
        return edit;
    }

    static QString playbackStateText(ArtifactCore::PlaybackState state)
    {
        switch (state) {
        case ArtifactCore::PlaybackState::Playing: return QStringLiteral("playing");
        case ArtifactCore::PlaybackState::Paused: return QStringLiteral("paused");
        case ArtifactCore::PlaybackState::Stopped: return QStringLiteral("stopped");
        }
        return QStringLiteral("unknown");
    }

    void refresh()
    {
        const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
        const auto projectSvc = ArtifactProjectService::instance();
        const auto playbackSvc = ArtifactPlaybackService::instance();
        const auto queueSvc = ArtifactRenderQueueService::instance();

        ArtifactCore::FrameDebugSnapshot controllerSnapshot;
        bool hasControllerSnapshot = false;
        if (controller_) {
            controllerSnapshot = controller_->frameDebugSnapshot();
            hasControllerSnapshot = true;
        } else if (playbackSvc) {
            controllerSnapshot = playbackSvc->frameDebugSnapshot();
            hasControllerSnapshot = true;
        } else if (queueSvc) {
            controllerSnapshot = queueSvc->frameDebugSnapshot();
            hasControllerSnapshot = true;
        }

        if (stateText_) {
            QStringList lines;
            lines << QStringLiteral("App Debugger");
            lines << QStringLiteral("timestamp: %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
            lines << QStringLiteral("project: %1")
                          .arg(projectSvc ? projectSvc->projectName().toQString() : QStringLiteral("<no service>"));
            lines << QStringLiteral("hasProject: %1")
                          .arg(projectSvc && projectSvc->hasProject() ? QStringLiteral("true") : QStringLiteral("false"));
            lines << QStringLiteral("composition: %1")
                          .arg(controllerSnapshot.compositionName.isEmpty() ? QStringLiteral("<none>")
                                                                           : controllerSnapshot.compositionName);
            lines << QStringLiteral("selectedLayer: %1")
                          .arg(controllerSnapshot.selectedLayerName.isEmpty() ? QStringLiteral("<none>")
                                                                              : controllerSnapshot.selectedLayerName);
            lines << QStringLiteral("playback: %1")
                          .arg(playbackSvc ? playbackStateText(playbackSvc->state()) : QStringLiteral("<no service>"));
            lines << QStringLiteral("currentFrame: %1")
                          .arg(controllerSnapshot.frame.framePosition());
            lines << QStringLiteral("renderBackend: %1")
                          .arg(controllerSnapshot.renderBackend.isEmpty() ? QStringLiteral("<none>")
                                                                          : controllerSnapshot.renderBackend);
            lines << QStringLiteral("queueJobs: %1")
                          .arg(queueSvc ? queueSvc->jobCount() : 0);
            lines << QStringLiteral("queueBackend: %1")
                          .arg(queueSvc ? queueSvc->renderBackend() == ArtifactRenderQueueService::RenderBackend::GPU
                                           ? QStringLiteral("gpu")
                                           : queueSvc->renderBackend() == ArtifactRenderQueueService::RenderBackend::CPU
                                               ? QStringLiteral("cpu")
                                               : QStringLiteral("auto")
                                        : QStringLiteral("<no service>"));
            lines << QStringLiteral("controller: %1")
                          .arg(controller_ ? QStringLiteral("available") : QStringLiteral("none"));
            stateText_->setPlainText(lines.join(QStringLiteral("\n")));
        }

        if (traceText_) {
            QStringList lines;
            lines << QStringLiteral("Trace");
            lines << QStringLiteral("frames: %1")
                          .arg(static_cast<int>(trace.frames.size()));
            lines << QStringLiteral("scopes: %1")
                          .arg(static_cast<int>(trace.scopes.size()));
            lines << QStringLiteral("locks: %1")
                          .arg(static_cast<int>(trace.locks.size()));
            lines << QStringLiteral("crashes: %1")
                          .arg(static_cast<int>(trace.crashes.size()));
            lines << QStringLiteral("threads: %1")
                          .arg(static_cast<int>(trace.threads.size()));
            if (!trace.frames.empty()) {
                const auto& frame = trace.frames.back();
                lines << QStringLiteral("lastFrame: %1").arg(frame.frameIndex);
                lines << QStringLiteral("lastFrameSpanNs: %1").arg(frame.frameEndNs - frame.frameStartNs);
                lines << QStringLiteral("lanes: %1").arg(static_cast<int>(frame.lanes.size()));
            }
            const int threadRows = std::min(static_cast<int>(trace.threads.size()), 6);
            for (int i = 0; i < threadRows; ++i) {
                const auto& thread = trace.threads[i];
                lines << QStringLiteral("thread[%1]: %2 sc=%3 lk=%4 cr=%5 depth=%6 last=%7 [0x%8]")
                              .arg(i)
                              .arg(thread.threadName)
                              .arg(thread.scopeCount)
                              .arg(thread.lockCount)
                              .arg(thread.crashCount)
                              .arg(thread.lockDepth)
                              .arg(thread.lastMutexName.isEmpty() ? QStringLiteral("<none>") : thread.lastMutexName)
                              .arg(QString::number(static_cast<unsigned long long>(thread.threadId), 16));
            }

            QHash<QString, int> lockAcquireCounts;
            QHash<QString, int> lockReleaseCounts;
            for (const auto& lock : trace.locks) {
                if (lock.acquired) {
                    ++lockAcquireCounts[lock.mutexName];
                } else {
                    ++lockReleaseCounts[lock.mutexName];
                }
            }
            if (!lockAcquireCounts.isEmpty() || !lockReleaseCounts.isEmpty()) {
                lines << QStringLiteral("locks:");
                QStringList names = lockAcquireCounts.keys();
                for (const auto& key : lockReleaseCounts.keys()) {
                    if (!names.contains(key)) {
                        names << key;
                    }
                }
                std::sort(names.begin(), names.end());
                const int lockRows = std::min(static_cast<int>(names.size()), 6);
                for (int i = 0; i < lockRows; ++i) {
                    const auto& name = names[i];
                    lines << QStringLiteral("  %1 acq=%2 rel=%3")
                                  .arg(name.isEmpty() ? QStringLiteral("<unnamed-mutex>") : name)
                                  .arg(lockAcquireCounts.value(name))
                                  .arg(lockReleaseCounts.value(name));
                }
            }

            if (!trace.threads.isEmpty()) {
                QVector<ArtifactCore::TraceThreadRecord> hotThreads = trace.threads;
                std::sort(hotThreads.begin(), hotThreads.end(), [](const auto& a, const auto& b) {
                    if (a.lockDepth == b.lockDepth) {
                        return a.lockCount > b.lockCount;
                    }
                    return a.lockDepth > b.lockDepth;
                });
                lines << QStringLiteral("hotThreads:");
                const int hotRows = std::min(static_cast<int>(hotThreads.size()), 4);
                for (int i = 0; i < hotRows; ++i) {
                    const auto& thread = hotThreads[static_cast<std::size_t>(i)];
                    lines << QStringLiteral("  %1 depth=%2 locks=%3 last=%4")
                                  .arg(thread.threadName.isEmpty() ? QStringLiteral("<unnamed>") : thread.threadName)
                                  .arg(thread.lockDepth)
                                  .arg(thread.lockCount)
                                  .arg(thread.lastMutexName.isEmpty() ? QStringLiteral("<none>") : thread.lastMutexName);
                }
            }

            if (!trace.locks.isEmpty()) {
                struct MutexChainRow {
                    QString name;
                    int balance = 0;
                    std::uint64_t lastThreadId = 0;
                    qint64 lastNs = 0;
                    bool lastAcquire = false;
                };
                QHash<QString, MutexChainRow> mutexChains;
                for (const auto& lock : trace.locks) {
                    const QString key = lock.mutexName.isEmpty() ? QStringLiteral("<unnamed-mutex>") : lock.mutexName;
                    auto& row = mutexChains[key];
                    row.name = key;
                    row.lastThreadId = lock.threadId;
                    row.lastNs = lock.timestampNs;
                    row.lastAcquire = lock.acquired;
                    if (lock.acquired) {
                        ++row.balance;
                    } else if (row.balance > 0) {
                        --row.balance;
                    }
                }
                QVector<MutexChainRow> rows;
                rows.reserve(mutexChains.size());
                for (auto it = mutexChains.cbegin(); it != mutexChains.cend(); ++it) {
                    rows.push_back(it.value());
                }
                std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
                    if (a.balance == b.balance) {
                        return a.lastNs > b.lastNs;
                    }
                    return a.balance > b.balance;
                });
                lines << QStringLiteral("mutexChains:");
                const int chainRows = std::min(static_cast<int>(rows.size()), 4);
                for (int i = 0; i < chainRows; ++i) {
                    const auto& row = rows[static_cast<std::size_t>(i)];
                    lines << QStringLiteral("  %1 bal=%2 last=%3 [0x%4]")
                                  .arg(row.name)
                                  .arg(row.balance)
                                  .arg(row.lastAcquire ? QStringLiteral("acquire") : QStringLiteral("release"))
                                  .arg(QString::number(static_cast<unsigned long long>(row.lastThreadId), 16));
                }
            }

            if (!trace.crashes.isEmpty()) {
                lines << QStringLiteral("recentCrashes:");
                const int crashRows = std::min(static_cast<int>(trace.crashes.size()), 3);
                for (int i = 0; i < crashRows; ++i) {
                    const auto& crash = trace.crashes[static_cast<std::size_t>(trace.crashes.size() - 1 - i)];
                    lines << QStringLiteral("  %1 thread=%2 [0x%3]")
                                  .arg(crash.summary.isEmpty() ? QStringLiteral("<no-summary>") : crash.summary.left(48))
                                  .arg(crash.threadName.isEmpty() ? QStringLiteral("<unnamed>") : crash.threadName)
                                  .arg(QString::number(static_cast<unsigned long long>(crash.threadId), 16));
                }
            }
            traceText_->setPlainText(lines.join(QStringLiteral("\n")));
        }

        if (frameText_) {
            QStringList lines;
            lines << QStringLiteral("Frame");
            lines << QStringLiteral("frame: %1").arg(controllerSnapshot.frame.framePosition());
            lines << QStringLiteral("timestampMs: %1").arg(controllerSnapshot.timestampMs);
            lines << QStringLiteral("failed: %1").arg(controllerSnapshot.failed ? QStringLiteral("true") : QStringLiteral("false"));
            if (!controllerSnapshot.failureReason.isEmpty()) {
                lines << QStringLiteral("failureReason: %1").arg(controllerSnapshot.failureReason);
            }
            lines << QStringLiteral("passes: %1").arg(static_cast<int>(controllerSnapshot.passes.size()));
            for (const auto& pass : controllerSnapshot.passes) {
                lines << QStringLiteral("  - %1 [%2/%3] inputs=%4 outputs=%5")
                              .arg(pass.name,
                                   ArtifactCore::toString(pass.kind),
                                   ArtifactCore::toString(pass.status))
                              .arg(static_cast<int>(pass.inputs.size()))
                              .arg(static_cast<int>(pass.outputs.size()));
            }
            lines << QStringLiteral("resources: %1").arg(static_cast<int>(controllerSnapshot.resources.size()));
            lines << QStringLiteral("attachments: %1").arg(static_cast<int>(controllerSnapshot.attachments.size()));
            frameText_->setPlainText(lines.join(QStringLiteral("\n")));
        }

        if (pipelineView_) {
            pipelineView_->setFrameDebugSnapshot(controllerSnapshot, trace);
        }

        if (resourceView_) {
            resourceView_->setFrameDebugSnapshot(controllerSnapshot, trace);
        }

        if (diffView_) {
            diffView_->setFrameDebugSnapshot(controllerSnapshot, trace);
        }

        if (traceTimelineView_) {
            std::uint64_t focusThreadId = 0;
            QString focusMutexName;
            if (!trace.threads.isEmpty()) {
                auto hotThreads = trace.threads;
                std::sort(hotThreads.begin(), hotThreads.end(), [](const auto& a, const auto& b) {
                    if (a.lockDepth == b.lockDepth) {
                        return a.lockCount > b.lockCount;
                    }
                    return a.lockDepth > b.lockDepth;
                });
                focusThreadId = hotThreads.first().threadId;
            }
            if (!trace.locks.isEmpty()) {
                struct MutexChainRow {
                    QString name;
                    int balance = 0;
                    qint64 lastNs = 0;
                };
                QHash<QString, MutexChainRow> mutexChains;
                for (const auto& lock : trace.locks) {
                    const QString key = lock.mutexName.isEmpty() ? QStringLiteral("<unnamed-mutex>") : lock.mutexName;
                    auto& row = mutexChains[key];
                    row.name = key;
                    row.lastNs = lock.timestampNs;
                    if (lock.acquired) {
                        ++row.balance;
                    } else if (row.balance > 0) {
                        --row.balance;
                    }
                }
                QVector<MutexChainRow> rows;
                rows.reserve(mutexChains.size());
                for (auto it = mutexChains.cbegin(); it != mutexChains.cend(); ++it) {
                    rows.push_back(it.value());
                }
                std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
                    if (a.balance == b.balance) {
                        return a.lastNs > b.lastNs;
                    }
                    return a.balance > b.balance;
                });
                if (!rows.isEmpty()) {
                    focusMutexName = rows.first().name;
                }
            }
            traceTimelineView_->setFocusedThreadId(focusThreadId);
            traceTimelineView_->setFocusedMutexName(focusMutexName);
            traceTimelineView_->setTraceSnapshot(trace);
        }

        if (diagnosticsText_) {
            QStringList lines;
            lines << QStringLiteral("Diagnostics");
            lines << QStringLiteral("frameSummary: %1").arg(hasControllerSnapshot ? QStringLiteral("available") : QStringLiteral("none"));
            lines << QStringLiteral("traceFrames: %1").arg(static_cast<int>(trace.frames.size()));
            lines << QStringLiteral("traceThreads: %1").arg(static_cast<int>(trace.threads.size()));
            int openLocks = 0;
            for (const auto& thread : trace.threads) {
                openLocks += std::max(0, thread.lockDepth);
            }
            lines << QStringLiteral("openLocks: %1").arg(openLocks);
            lines << QStringLiteral("queueCount: %1").arg(queueSvc ? queueSvc->jobCount() : 0);
            lines << QStringLiteral("playbackState: %1")
                          .arg(playbackSvc ? playbackStateText(playbackSvc->state()) : QStringLiteral("<no service>"));
            if (controllerSnapshot.failed && !controllerSnapshot.failureReason.isEmpty()) {
                lines << QStringLiteral("failureReason: %1").arg(controllerSnapshot.failureReason);
            }
            diagnosticsText_->setPlainText(lines.join(QStringLiteral("\n")));
        }

        if (exportText_) {
            QStringList lines;
            lines << QStringLiteral("App Debug Export");
            lines << QStringLiteral("FrameDebugSnapshot JSON:");
            lines << QString::fromUtf8(QJsonDocument(controllerSnapshot.toJson()).toJson(QJsonDocument::Indented));
            lines << QString();
            lines << QStringLiteral("TraceSnapshot JSON:");
            lines << QString::fromUtf8(QJsonDocument(ArtifactCore::toJson(trace)).toJson(QJsonDocument::Indented));
            exportText_->setPlainText(lines.join(QStringLiteral("\n")));
        }
    }
};

AppDebuggerWidget::AppDebuggerWidget(CompositionRenderController* controller, QWidget* parent)
    : QWidget(parent), impl_(new Impl(this, controller))
{
    impl_->setupUI();
}

AppDebuggerWidget::~AppDebuggerWidget()
{
    delete impl_;
}

void AppDebuggerWidget::timerEvent(QTimerEvent* event)
{
    if (impl_ && event && event->timerId() == impl_->timerId_) {
        impl_->refresh();
        return;
    }
    QWidget::timerEvent(event);
}

} // namespace Artifact
