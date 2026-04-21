module;
#include <algorithm>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonDocument>
#include <QLabel>
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
    QWidget* overviewPage_ = nullptr;
    QLabel* overviewSummary_ = nullptr;
    QWidget* statePage_ = nullptr;
    QLabel* stateSummary_ = nullptr;
    QPlainTextEdit* stateText_ = nullptr;
    QWidget* tracePage_ = nullptr;
    QLabel* traceSummary_ = nullptr;
    QPlainTextEdit* traceText_ = nullptr;
    QWidget* pipelinePage_ = nullptr;
    QLabel* pipelineSummary_ = nullptr;
    FramePipelineViewWidget* pipelineView_ = nullptr;
    QWidget* resourcePage_ = nullptr;
    QLabel* resourceSummary_ = nullptr;
    FrameResourceInspectorWidget* resourceView_ = nullptr;
    QWidget* diffPage_ = nullptr;
    QLabel* diffSummary_ = nullptr;
    FrameStateDiffWidget* diffView_ = nullptr;
    QWidget* traceTimelinePage_ = nullptr;
    QLabel* traceTimelineSummary_ = nullptr;
    TraceTimelineWidget* traceTimelineView_ = nullptr;
    QWidget* framePage_ = nullptr;
    QLabel* frameSummary_ = nullptr;
    QPlainTextEdit* frameText_ = nullptr;
    QWidget* diagnosticsPage_ = nullptr;
    QLabel* diagnosticsSummary_ = nullptr;
    QPlainTextEdit* diagnosticsText_ = nullptr;
    QWidget* exportPage_ = nullptr;
    QLabel* exportSummary_ = nullptr;
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

        overviewPage_ = new QWidget(owner_);
        auto* overviewLayout = new QVBoxLayout(overviewPage_);
        overviewLayout->setContentsMargins(0, 0, 0, 0);
        overviewLayout->setSpacing(0);
        overviewSummary_ = new QLabel(overviewPage_);
        overviewSummary_->setTextFormat(Qt::PlainText);
        overviewSummary_->setWordWrap(true);
        overviewSummary_->setMinimumHeight(56);
        overviewLayout->addWidget(overviewSummary_);
        layout->addWidget(overviewPage_);

        tabs_ = new QTabWidget(owner_);
        statePage_ = new QWidget(tabs_);
        auto* stateLayout = new QVBoxLayout(statePage_);
        stateLayout->setContentsMargins(0, 0, 0, 0);
        stateLayout->setSpacing(0);
        stateSummary_ = new QLabel(statePage_);
        stateSummary_->setTextFormat(Qt::PlainText);
        stateSummary_->setWordWrap(true);
        stateSummary_->setMinimumHeight(56);
        stateLayout->addWidget(stateSummary_);
        stateText_ = new QPlainTextEdit(statePage_);
        stateText_->setReadOnly(true);
        stateText_->setLineWrapMode(QPlainTextEdit::NoWrap);
        stateLayout->addWidget(stateText_);
        tracePage_ = new QWidget(tabs_);
        auto* traceLayout = new QVBoxLayout(tracePage_);
        traceLayout->setContentsMargins(0, 0, 0, 0);
        traceLayout->setSpacing(0);
        traceSummary_ = new QLabel(tracePage_);
        traceSummary_->setTextFormat(Qt::PlainText);
        traceSummary_->setWordWrap(true);
        traceSummary_->setMinimumHeight(56);
        traceLayout->addWidget(traceSummary_);
        traceText_ = new QPlainTextEdit(tracePage_);
        traceText_->setReadOnly(true);
        traceText_->setLineWrapMode(QPlainTextEdit::NoWrap);
        traceLayout->addWidget(traceText_);
        pipelinePage_ = new QWidget(tabs_);
        auto* pipelineLayout = new QVBoxLayout(pipelinePage_);
        pipelineLayout->setContentsMargins(0, 0, 0, 0);
        pipelineLayout->setSpacing(0);
        pipelineSummary_ = new QLabel(pipelinePage_);
        pipelineSummary_->setTextFormat(Qt::PlainText);
        pipelineSummary_->setWordWrap(true);
        pipelineSummary_->setMinimumHeight(56);
        pipelineLayout->addWidget(pipelineSummary_);
        pipelineView_ = new FramePipelineViewWidget(pipelinePage_);
        pipelineLayout->addWidget(pipelineView_);
        resourcePage_ = new QWidget(tabs_);
        auto* resourceLayout = new QVBoxLayout(resourcePage_);
        resourceLayout->setContentsMargins(0, 0, 0, 0);
        resourceLayout->setSpacing(0);
        resourceSummary_ = new QLabel(resourcePage_);
        resourceSummary_->setTextFormat(Qt::PlainText);
        resourceSummary_->setWordWrap(true);
        resourceSummary_->setMinimumHeight(56);
        resourceLayout->addWidget(resourceSummary_);
        resourceView_ = new FrameResourceInspectorWidget(resourcePage_);
        resourceLayout->addWidget(resourceView_);
        diffPage_ = new QWidget(tabs_);
        auto* diffLayout = new QVBoxLayout(diffPage_);
        diffLayout->setContentsMargins(0, 0, 0, 0);
        diffLayout->setSpacing(0);
        diffSummary_ = new QLabel(diffPage_);
        diffSummary_->setTextFormat(Qt::PlainText);
        diffSummary_->setWordWrap(true);
        diffSummary_->setMinimumHeight(56);
        diffLayout->addWidget(diffSummary_);
        diffView_ = new FrameStateDiffWidget(diffPage_);
        diffLayout->addWidget(diffView_);
        traceTimelinePage_ = new QWidget(tabs_);
        auto* traceTimelineLayout = new QVBoxLayout(traceTimelinePage_);
        traceTimelineLayout->setContentsMargins(0, 0, 0, 0);
        traceTimelineLayout->setSpacing(0);
        traceTimelineSummary_ = new QLabel(traceTimelinePage_);
        traceTimelineSummary_->setTextFormat(Qt::PlainText);
        traceTimelineSummary_->setWordWrap(true);
        traceTimelineSummary_->setMinimumHeight(56);
        traceTimelineLayout->addWidget(traceTimelineSummary_);
        traceTimelineView_ = new TraceTimelineWidget(traceTimelinePage_);
        traceTimelineLayout->addWidget(traceTimelineView_);
        framePage_ = new QWidget(tabs_);
        auto* frameLayout = new QVBoxLayout(framePage_);
        frameLayout->setContentsMargins(0, 0, 0, 0);
        frameLayout->setSpacing(0);
        frameSummary_ = new QLabel(framePage_);
        frameSummary_->setTextFormat(Qt::PlainText);
        frameSummary_->setWordWrap(true);
        frameSummary_->setMinimumHeight(56);
        frameLayout->addWidget(frameSummary_);
        frameText_ = new QPlainTextEdit(framePage_);
        frameText_->setReadOnly(true);
        frameText_->setLineWrapMode(QPlainTextEdit::NoWrap);
        frameLayout->addWidget(frameText_);
        diagnosticsPage_ = new QWidget(tabs_);
        auto* diagnosticsLayout = new QVBoxLayout(diagnosticsPage_);
        diagnosticsLayout->setContentsMargins(0, 0, 0, 0);
        diagnosticsLayout->setSpacing(0);
        diagnosticsSummary_ = new QLabel(diagnosticsPage_);
        diagnosticsSummary_->setTextFormat(Qt::PlainText);
        diagnosticsSummary_->setWordWrap(true);
        diagnosticsSummary_->setMinimumHeight(56);
        diagnosticsLayout->addWidget(diagnosticsSummary_);
        diagnosticsText_ = new QPlainTextEdit(diagnosticsPage_);
        diagnosticsText_->setReadOnly(true);
        diagnosticsText_->setLineWrapMode(QPlainTextEdit::NoWrap);
        diagnosticsLayout->addWidget(diagnosticsText_);
        exportPage_ = new QWidget(tabs_);
        auto* exportLayout = new QVBoxLayout(exportPage_);
        exportLayout->setContentsMargins(0, 0, 0, 0);
        exportLayout->setSpacing(0);
        exportSummary_ = new QLabel(exportPage_);
        exportSummary_->setTextFormat(Qt::PlainText);
        exportSummary_->setWordWrap(true);
        exportSummary_->setMinimumHeight(56);
        exportLayout->addWidget(exportSummary_);
        exportText_ = new QPlainTextEdit(exportPage_);
        exportText_->setReadOnly(true);
        exportText_->setLineWrapMode(QPlainTextEdit::NoWrap);
        exportLayout->addWidget(exportText_);

        tabs_->addTab(statePage_, QStringLiteral("State"));
        tabs_->addTab(tracePage_, QStringLiteral("Trace"));
        tabs_->addTab(pipelinePage_, QStringLiteral("Pipeline"));
        tabs_->addTab(resourcePage_, QStringLiteral("Resource"));
        tabs_->addTab(diffPage_, QStringLiteral("State Diff"));
        tabs_->addTab(traceTimelinePage_, QStringLiteral("Trace Timeline"));
        tabs_->addTab(framePage_, QStringLiteral("Frame"));
        tabs_->addTab(diagnosticsPage_, QStringLiteral("Diagnostics"));
        tabs_->addTab(exportPage_, QStringLiteral("Export"));

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

        if (stateSummary_) {
            const QString compositionText = controllerSnapshot.compositionName.isEmpty()
                                                ? QStringLiteral("<none>")
                                                : controllerSnapshot.compositionName;
            const QString layerText = controllerSnapshot.selectedLayerName.isEmpty()
                                            ? QStringLiteral("<none>")
                                            : controllerSnapshot.selectedLayerName;
            const QString backendText = controllerSnapshot.renderBackend.isEmpty()
                                            ? QStringLiteral("<none>")
                                            : controllerSnapshot.renderBackend;
            const QString playbackText = playbackSvc ? playbackStateText(playbackSvc->state())
                                                     : QStringLiteral("<no service>");
            const QString queueText = queueSvc ? QString::number(queueSvc->jobCount())
                                               : QStringLiteral("<no service>");
            const QString projectText = projectSvc ? projectSvc->projectName().toQString()
                                                   : QStringLiteral("<no service>");

            stateSummary_->setText(QStringLiteral("project=%1  composition=%2  layer=%3  frame=%4  playback=%5  backend=%6  queueJobs=%7")
                                       .arg(projectText,
                                            compositionText,
                                            layerText)
                                       .arg(controllerSnapshot.frame.framePosition())
                                       .arg(playbackText)
                                       .arg(backendText)
                                       .arg(queueText));
        }

        if (overviewSummary_) {
            const QString projectText = projectSvc ? projectSvc->projectName().toQString()
                                                   : QStringLiteral("<no service>");
            const QString compositionText = controllerSnapshot.compositionName.isEmpty()
                                                ? QStringLiteral("<none>")
                                                : controllerSnapshot.compositionName;
            const QString layerText = controllerSnapshot.selectedLayerName.isEmpty()
                                            ? QStringLiteral("<none>")
                                            : controllerSnapshot.selectedLayerName;
            const QString playbackText = playbackSvc ? playbackStateText(playbackSvc->state())
                                                     : QStringLiteral("<no service>");
            const QString backendText = controllerSnapshot.renderBackend.isEmpty()
                                            ? QStringLiteral("<none>")
                                            : controllerSnapshot.renderBackend;
            QString hotThreadText = QStringLiteral("<none>");
            int hotThreadDepth = 0;
            for (const auto& thread : trace.threads) {
                if (thread.lockDepth > hotThreadDepth) {
                    hotThreadDepth = thread.lockDepth;
                    hotThreadText = thread.threadName.isEmpty() ? QStringLiteral("<unnamed>") : thread.threadName;
                }
            }
            QString lastCrashText = QStringLiteral("<none>");
            if (!trace.crashes.isEmpty()) {
                lastCrashText = trace.crashes.back().summary.isEmpty()
                                    ? QStringLiteral("<no-summary>")
                                    : trace.crashes.back().summary.left(48);
            }
            int failedPasses = 0;
            qint64 totalPassUs = 0;
            for (const auto& pass : controllerSnapshot.passes) {
                totalPassUs += pass.durationUs;
                if (pass.status == ArtifactCore::FrameDebugPassStatus::Failed) {
                    ++failedPasses;
                }
            }
            const QString healthText = controllerSnapshot.failed
                                           ? QStringLiteral("failed")
                                           : (failedPasses > 0 ? QStringLiteral("pass failed") : QStringLiteral("ok"));
            const QString compareText = controllerSnapshot.compareMode == ArtifactCore::FrameDebugCompareMode::Disabled
                                            ? QStringLiteral("off")
                                            : ArtifactCore::toString(controllerSnapshot.compareMode);
            overviewSummary_->setText(QStringLiteral("project=%1  composition=%2  layer=%3  frame=%4  playback=%5  backend=%6  health=%7  compare=%8  passes=%9  crashes=%10  traceEvents=%11  hotThread=%12(%13)  lastCrash=%14")
                                          .arg(projectText,
                                               compositionText,
                                               layerText)
                                          .arg(controllerSnapshot.frame.framePosition())
                                          .arg(playbackText)
                                          .arg(backendText)
                                          .arg(healthText)
                                          .arg(compareText)
                                          .arg(static_cast<int>(controllerSnapshot.passes.size()))
                                          .arg(static_cast<int>(trace.crashes.size()))
                                          .arg(static_cast<int>(trace.events.size()))
                                          .arg(hotThreadText)
                                          .arg(hotThreadDepth)
                                          .arg(lastCrashText));
            overviewSummary_->setToolTip(QStringLiteral("failedPasses=%1 totalPassUs=%2 queueJobs=%3 traceThreads=%4")
                                             .arg(failedPasses)
                                             .arg(totalPassUs)
                                             .arg(queueSvc ? queueSvc->jobCount() : 0)
                                             .arg(static_cast<int>(trace.threads.size())));
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

        if (traceSummary_) {
            int lockDepthTotal = 0;
            QString hotThreadName = QStringLiteral("<none>");
            int hotThreadDepth = 0;
            for (const auto& thread : trace.threads) {
                lockDepthTotal += std::max(0, thread.lockDepth);
                if (thread.lockDepth > hotThreadDepth) {
                    hotThreadDepth = thread.lockDepth;
                    hotThreadName = thread.threadName.isEmpty() ? QStringLiteral("<unnamed>") : thread.threadName;
                }
            }
            const auto lastFrameIndex = trace.frames.empty() ? -1 : trace.frames.back().frameIndex;
            const auto lastSpanNs = trace.frames.empty() ? 0 : (trace.frames.back().frameEndNs - trace.frames.back().frameStartNs);
            traceSummary_->setText(QStringLiteral("frames=%1  events=%2  scopes=%3  locks=%4  crashes=%5  hotThread=%6(%7)  lastFrame=%8 spanNs=%9")
                                       .arg(static_cast<int>(trace.frames.size()))
                                       .arg(static_cast<int>(trace.events.size()))
                                       .arg(static_cast<int>(trace.scopes.size()))
                                       .arg(static_cast<int>(trace.locks.size()))
                                       .arg(static_cast<int>(trace.crashes.size()))
                                       .arg(hotThreadName)
                                       .arg(hotThreadDepth)
                                       .arg(lastFrameIndex)
                                       .arg(lastSpanNs));
            traceSummary_->setToolTip(QStringLiteral("openLockDepth=%1").arg(lockDepthTotal));
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

        if (frameSummary_) {
            const auto frameIndex = controllerSnapshot.frame.framePosition();
            const auto passCount = static_cast<int>(controllerSnapshot.passes.size());
            const auto resourceCount = static_cast<int>(controllerSnapshot.resources.size());
            const auto attachmentCount = static_cast<int>(controllerSnapshot.attachments.size());
            const auto compareText = controllerSnapshot.compareMode == ArtifactCore::FrameDebugCompareMode::Disabled
                                           ? QStringLiteral("compare: off")
                                           : QStringLiteral("compare: %1%2")
                                                 .arg(ArtifactCore::toString(controllerSnapshot.compareMode))
                                                 .arg(controllerSnapshot.compareTargetId.isEmpty()
                                                         ? QString()
                                                         : QStringLiteral(" -> %1").arg(controllerSnapshot.compareTargetId));

            int failedPasses = 0;
            qint64 totalPassUs = 0;
            QString lastFailedPass;
            for (const auto& pass : controllerSnapshot.passes) {
                totalPassUs += pass.durationUs;
                if (pass.status == ArtifactCore::FrameDebugPassStatus::Failed) {
                    ++failedPasses;
                    lastFailedPass = pass.name.isEmpty() ? QStringLiteral("<unnamed>") : pass.name;
                }
            }

            QString hint;
            if (controllerSnapshot.failed) {
                hint = controllerSnapshot.failureReason.isEmpty()
                        ? QStringLiteral("frame failed")
                        : QStringLiteral("frame failed: %1").arg(controllerSnapshot.failureReason);
            } else if (failedPasses > 0) {
                hint = lastFailedPass.isEmpty()
                        ? QStringLiteral("failed pass present")
                        : QStringLiteral("failed pass: %1").arg(lastFailedPass);
            } else if (controllerSnapshot.compareMode != ArtifactCore::FrameDebugCompareMode::Disabled) {
                hint = QStringLiteral("compare is enabled");
            } else {
                hint = QStringLiteral("frame looks stable");
            }

            frameSummary_->setText(QStringLiteral("Frame %1 | %2 | %3 | passes=%4 resources=%5 attachments=%6 totalPassUs=%7")
                                       .arg(frameIndex)
                                       .arg(controllerSnapshot.compositionName.isEmpty()
                                                ? QStringLiteral("<no composition>")
                                                : controllerSnapshot.compositionName)
                                       .arg(compareText)
                                       .arg(passCount)
                                       .arg(resourceCount)
                                       .arg(attachmentCount)
                                       .arg(totalPassUs));
            frameSummary_->setToolTip(QStringLiteral("%1\nfailedPasses=%2").arg(hint).arg(failedPasses));
        }

        if (pipelineView_) {
            pipelineView_->setFrameDebugSnapshot(controllerSnapshot, trace);
        }
        if (pipelineSummary_) {
            int failedPasses = 0;
            qint64 totalPassUs = 0;
            for (const auto& pass : controllerSnapshot.passes) {
                totalPassUs += pass.durationUs;
                if (pass.status == ArtifactCore::FrameDebugPassStatus::Failed) {
                    ++failedPasses;
                }
            }
            pipelineSummary_->setText(QStringLiteral("passes=%1  failed=%2  resources=%3  attachments=%4  totalPassUs=%5  compare=%6")
                                           .arg(static_cast<int>(controllerSnapshot.passes.size()))
                                           .arg(failedPasses)
                                           .arg(static_cast<int>(controllerSnapshot.resources.size()))
                                           .arg(static_cast<int>(controllerSnapshot.attachments.size()))
                                           .arg(totalPassUs)
                                           .arg(ArtifactCore::toString(controllerSnapshot.compareMode)));
        }

        if (resourceView_) {
            resourceView_->setFrameDebugSnapshot(controllerSnapshot, trace);
        }
        if (resourceSummary_) {
            int textureViews = 0;
            for (const auto& resource : controllerSnapshot.resources) {
                textureViews += resource.texture.valid ? 1 : 0;
            }
            for (const auto& attachment : controllerSnapshot.attachments) {
                textureViews += attachment.texture.valid ? 1 : 0;
            }
            resourceSummary_->setText(QStringLiteral("resources=%1  attachments=%2  textureViews=%3  frame=%4")
                                          .arg(static_cast<int>(controllerSnapshot.resources.size()))
                                          .arg(static_cast<int>(controllerSnapshot.attachments.size()))
                                          .arg(textureViews)
                                          .arg(controllerSnapshot.frame.framePosition()));
        }

        if (diffView_) {
            diffView_->setFrameDebugSnapshot(controllerSnapshot, trace);
        }
        if (diffSummary_) {
            const auto changedKey = QStringLiteral("%1|%2|%3|%4")
                                        .arg(controllerSnapshot.compositionName,
                                             controllerSnapshot.renderBackend,
                                             controllerSnapshot.playbackState,
                                             controllerSnapshot.selectedLayerName);
            diffSummary_->setText(QStringLiteral("compare=%1  target=%2  failed=%3  key=%4")
                                      .arg(ArtifactCore::toString(controllerSnapshot.compareMode))
                                      .arg(controllerSnapshot.compareTargetId.isEmpty() ? QStringLiteral("<none>")
                                                                                       : controllerSnapshot.compareTargetId)
                                      .arg(controllerSnapshot.failed ? QStringLiteral("true") : QStringLiteral("false"))
                                      .arg(changedKey));
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
        if (traceTimelineSummary_) {
            int lockDepthTotal = 0;
            int hotThreadCount = 0;
            for (const auto& thread : trace.threads) {
                lockDepthTotal += std::max(0, thread.lockDepth);
                if (thread.lockDepth > 0) {
                    ++hotThreadCount;
                }
            }
            traceTimelineSummary_->setText(QStringLiteral("lanes=%1  scopes=%2  locks=%3  crashes=%4  hotThreads=%5  lockDepth=%6")
                                               .arg(trace.frames.empty() ? 0 : static_cast<int>(trace.frames.back().lanes.size()))
                                               .arg(static_cast<int>(trace.scopes.size()))
                                               .arg(static_cast<int>(trace.locks.size()))
                                               .arg(static_cast<int>(trace.crashes.size()))
                                               .arg(hotThreadCount)
                                               .arg(lockDepthTotal));
        }

        if (diagnosticsText_) {
            QStringList lines;
            lines << QStringLiteral("Diagnostics");
            lines << QStringLiteral("frameSummary: %1").arg(hasControllerSnapshot ? QStringLiteral("available") : QStringLiteral("none"));
            lines << QStringLiteral("traceFrames: %1").arg(static_cast<int>(trace.frames.size()));
            lines << QStringLiteral("traceThreads: %1").arg(static_cast<int>(trace.threads.size()));
            lines << QStringLiteral("traceEvents: %1").arg(static_cast<int>(trace.events.size()));
            lines << QStringLiteral("traceCrashes: %1").arg(static_cast<int>(trace.crashes.size()));
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

        if (diagnosticsSummary_) {
            QString lastCrashText = QStringLiteral("<none>");
            if (!trace.crashes.isEmpty()) {
                const auto& crash = trace.crashes.back();
                lastCrashText = crash.summary.isEmpty() ? QStringLiteral("<no-summary>") : crash.summary.left(48);
            }
            diagnosticsSummary_->setText(QStringLiteral("traceFrames=%1  traceEvents=%2  crashes=%3  openLocks=%4  queueJobs=%5  playback=%6")
                                             .arg(static_cast<int>(trace.frames.size()))
                                             .arg(static_cast<int>(trace.events.size()))
                                             .arg(static_cast<int>(trace.crashes.size()))
                                             .arg([&trace]() {
                                                 int total = 0;
                                                 for (const auto& thread : trace.threads) {
                                                     total += std::max(0, thread.lockDepth);
                                                 }
                                                 return total;
                                             }())
                                             .arg(queueSvc ? queueSvc->jobCount() : 0)
                                             .arg(playbackSvc ? playbackStateText(playbackSvc->state()) : QStringLiteral("<no service>")));
            diagnosticsSummary_->setToolTip(QStringLiteral("lastCrash=%1").arg(lastCrashText));
        }

        if (exportText_) {
            QStringList lines;
            lines << QStringLiteral("App Debug Export");
            lines << QStringLiteral("summary:");
            lines << QStringLiteral("  frame: %1").arg(controllerSnapshot.frame.framePosition());
            lines << QStringLiteral("  composition: %1")
                          .arg(controllerSnapshot.compositionName.isEmpty() ? QStringLiteral("<none>")
                                                                            : controllerSnapshot.compositionName);
            lines << QStringLiteral("  playback: %1")
                          .arg(playbackSvc ? playbackStateText(playbackSvc->state()) : QStringLiteral("<no service>"));
            lines << QStringLiteral("  traceFrames: %1").arg(static_cast<int>(trace.frames.size()));
            lines << QStringLiteral("  traceEvents: %1").arg(static_cast<int>(trace.events.size()));
            lines << QStringLiteral("  crashes: %1").arg(static_cast<int>(trace.crashes.size()));
            lines << QStringLiteral("FrameDebugSnapshot JSON:");
            lines << QString::fromUtf8(QJsonDocument(controllerSnapshot.toJson()).toJson(QJsonDocument::Indented));
            lines << QString();
            lines << QStringLiteral("TraceSnapshot JSON:");
            lines << QString::fromUtf8(QJsonDocument(ArtifactCore::toJson(trace)).toJson(QJsonDocument::Indented));
            exportText_->setPlainText(lines.join(QStringLiteral("\n")));
        }

        if (exportSummary_) {
            QString crashText = QStringLiteral("<none>");
            if (!trace.crashes.isEmpty()) {
                crashText = trace.crashes.back().summary.isEmpty() ? QStringLiteral("<no-summary>")
                                                                  : trace.crashes.back().summary.left(48);
            }
            exportSummary_->setText(QStringLiteral("ready to copy: frame=%1  traceEvents=%2  crashes=%3  compare=%4")
                                        .arg(controllerSnapshot.frame.framePosition())
                                        .arg(static_cast<int>(trace.events.size()))
                                        .arg(static_cast<int>(trace.crashes.size()))
                                        .arg(ArtifactCore::toString(controllerSnapshot.compareMode)));
            exportSummary_->setToolTip(QStringLiteral("latestCrash=%1").arg(crashText));
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
