module;
#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QTimerEvent>
#include <QVector>
#include <QStringList>
#include <cstdint>
#include <wobjectimpl.h>

module Artifact.Widgets.AppDebuggerWidget;

import Core.Diagnostics.Trace;
import Application.AppSettings;
import Artifact.Service.Playback;
import Artifact.Service.Project;
import Artifact.Render.Queue.Service;
import Artifact.Widgets.CompositionRenderController;
import Artifact.Widgets.FramePipelineViewWidget;
import Artifact.Widgets.FrameDebugViewWidget;
import Artifact.Widgets.FrameResourceInspectorWidget;
import Artifact.Widgets.FrameStateDiffWidget;
import Artifact.Widgets.TraceTimelineWidget;
import Thread.Helper;
import Frame.Debug;
import Playback.State;
import Utils.String.UniString;

namespace Artifact {

W_OBJECT_IMPL(AppDebuggerWidget)

class AppDebuggerWidget::Impl {
public:
    class CaptureHistoryListWidget : public QListWidget {
    public:
        explicit CaptureHistoryListWidget(Impl* impl, QWidget* parent = nullptr)
            : QListWidget(parent), impl_(impl)
        {}

    protected:
        void currentChanged(const QModelIndex& current, const QModelIndex& previous) override
        {
            QListWidget::currentChanged(current, previous);
            if (impl_) {
                impl_->updateCaptureHistorySelection(current.row());
            }
        }

    private:
        Impl* impl_ = nullptr;
    };

    AppDebuggerWidget* owner_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QWidget* overviewPage_ = nullptr;
    QLabel* overviewSummary_ = nullptr;
    QWidget* capturePage_ = nullptr;
    QLabel* captureSummary_ = nullptr;
    FramePipelineViewWidget* capturePipelineView_ = nullptr;
    FrameResourceInspectorWidget* captureResourceView_ = nullptr;
    TraceTimelineWidget* captureTraceTimelineView_ = nullptr;
    CaptureHistoryListWidget* captureHistoryList_ = nullptr;
    QPlainTextEdit* captureHistoryText_ = nullptr;
    FrameDebugViewWidget* captureDetailView_ = nullptr;
    ArtifactCore::FrameDebugBundle captureBundle_;
    bool hasCaptureBundle_ = false;
    int captureSelectedRow_ = -1;
    QWidget* statePage_ = nullptr;
    QLabel* stateSummary_ = nullptr;
    QPlainTextEdit* stateText_ = nullptr;
    QWidget* playbackPage_ = nullptr;
    QLabel* playbackSummary_ = nullptr;
    QPlainTextEdit* playbackText_ = nullptr;
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
        capturePage_ = new QWidget(tabs_);
        auto* captureLayout = new QVBoxLayout(capturePage_);
        captureLayout->setContentsMargins(0, 0, 0, 0);
        captureLayout->setSpacing(0);
        captureSummary_ = new QLabel(capturePage_);
        captureSummary_->setTextFormat(Qt::PlainText);
        captureSummary_->setWordWrap(true);
        captureSummary_->setMinimumHeight(56);
        captureLayout->addWidget(captureSummary_);

        auto* captureSplitter = new QSplitter(Qt::Horizontal, capturePage_);
        auto* captureLeftSplitter = new QSplitter(Qt::Vertical, captureSplitter);
        capturePipelineView_ = new FramePipelineViewWidget(captureLeftSplitter);
        captureTraceTimelineView_ = new TraceTimelineWidget(captureLeftSplitter);
        captureLeftSplitter->addWidget(capturePipelineView_);
        captureLeftSplitter->addWidget(captureTraceTimelineView_);
        captureLeftSplitter->setStretchFactor(0, 3);
        captureLeftSplitter->setStretchFactor(1, 2);
        captureResourceView_ = new FrameResourceInspectorWidget(captureSplitter);
        captureSplitter->addWidget(captureLeftSplitter);
        captureSplitter->addWidget(captureResourceView_);
        captureSplitter->setStretchFactor(0, 3);
        captureSplitter->setStretchFactor(1, 2);
        captureLayout->addWidget(captureSplitter);
        auto* captureHistorySplitter = new QSplitter(Qt::Horizontal, capturePage_);
        captureHistoryList_ = new CaptureHistoryListWidget(this, captureHistorySplitter);
        captureHistoryList_->setSelectionMode(QAbstractItemView::SingleSelection);
        captureHistoryList_->setMinimumWidth(280);
        auto* captureHistoryRightSplitter = new QSplitter(Qt::Vertical, captureHistorySplitter);
        captureHistoryText_ = new QPlainTextEdit(captureHistoryRightSplitter);
        captureHistoryText_->setReadOnly(true);
        captureHistoryText_->setLineWrapMode(QPlainTextEdit::NoWrap);
        captureDetailView_ = new FrameDebugViewWidget(captureHistoryRightSplitter);
        captureHistoryRightSplitter->addWidget(captureHistoryText_);
        captureHistoryRightSplitter->addWidget(captureDetailView_);
        captureHistoryRightSplitter->setStretchFactor(0, 1);
        captureHistoryRightSplitter->setStretchFactor(1, 2);
        captureHistorySplitter->addWidget(captureHistoryList_);
        captureHistorySplitter->addWidget(captureHistoryRightSplitter);
        captureHistorySplitter->setStretchFactor(0, 1);
        captureHistorySplitter->setStretchFactor(1, 3);
        captureLayout->addWidget(captureHistorySplitter);

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
        playbackPage_ = new QWidget(tabs_);
        auto* playbackLayout = new QVBoxLayout(playbackPage_);
        playbackLayout->setContentsMargins(0, 0, 0, 0);
        playbackLayout->setSpacing(0);
        playbackSummary_ = new QLabel(playbackPage_);
        playbackSummary_->setTextFormat(Qt::PlainText);
        playbackSummary_->setWordWrap(true);
        playbackSummary_->setMinimumHeight(56);
        playbackLayout->addWidget(playbackSummary_);
        playbackText_ = new QPlainTextEdit(playbackPage_);
        playbackText_->setReadOnly(true);
        playbackText_->setLineWrapMode(QPlainTextEdit::NoWrap);
        playbackLayout->addWidget(playbackText_);
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

        tabs_->addTab(capturePage_, QStringLiteral("Capture"));
        tabs_->addTab(statePage_, QStringLiteral("State"));
        tabs_->addTab(playbackPage_, QStringLiteral("Playback"));
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

    static QString previewQualityText()
    {
        const auto* settings = ArtifactCore::ArtifactAppSettings::instance();
        if (!settings) {
            return QStringLiteral("<no settings>");
        }
        const QString quality = settings->previewQualityText().trimmed();
        return quality.isEmpty() ? QStringLiteral("<default>") : quality;
    }

    static QString cacheHealthText(const ArtifactCore::FrameDebugSnapshot& snapshot)
    {
        if (snapshot.resources.empty()) {
            return QStringLiteral("cache=unknown");
        }
        int hitCount = 0;
        int staleCount = 0;
        for (const auto& resource : snapshot.resources) {
            if (resource.cacheHit) {
                ++hitCount;
            }
            if (resource.stale) {
                ++staleCount;
            }
        }
        const int total = static_cast<int>(snapshot.resources.size());
        const int hitPercent = total > 0 ? static_cast<int>(std::lround((hitCount * 100.0) / total)) : 0;
        return QStringLiteral("cache=%1/%2 hit (%3%%) stale=%4")
                .arg(hitCount)
                .arg(total)
                .arg(hitPercent)
                .arg(staleCount);
    }

    static QString ramPreviewText(ArtifactPlaybackService* playbackSvc)
    {
        if (!playbackSvc) {
            return QStringLiteral("ramPreview=<no service>");
        }
        const int cachedFrames = playbackSvc->ramPreviewCachedFrameCount();
        const float hitRate = playbackSvc->ramPreviewHitRate() * 100.0f;
        const auto range = playbackSvc->ramPreviewRange();
        return QStringLiteral("ramPreview=%1 frames hit=%2%% range=%3-%4")
                .arg(cachedFrames)
                .arg(QString::number(hitRate, 'f', 1))
                .arg(range.start())
                .arg(range.end());
    }

    static QString renderTimingText(const ArtifactCore::FrameDebugSnapshot& snapshot,
                                    CompositionRenderController* controller)
    {
        if (!controller) {
            return QStringLiteral("renderTiming=<no controller>");
        }
        const double lastMs = controller->lastFrameTimeMs();
        const double avgMs = controller->averageFrameTimeMs();
        const double gpuMs = snapshot.renderGpuFrameMs;
        const double lastFps = lastMs > 0.0 ? 1000.0 / lastMs : 0.0;
        const double avgFps = avgMs > 0.0 ? 1000.0 / avgMs : 0.0;
        return QStringLiteral("renderTiming=cpu %1ms (%2fps) avg %3ms (%4fps) gpu %5ms draw=%6 pso=%7 srb=%8 buf=%9")
                .arg(QString::number(lastMs, 'f', 1))
                .arg(QString::number(lastFps, 'f', 1))
                .arg(QString::number(avgMs, 'f', 1))
                .arg(QString::number(avgFps, 'f', 1))
                .arg(QString::number(gpuMs, 'f', 1))
                .arg(QString::number(static_cast<qulonglong>(snapshot.renderCost.drawCalls)))
                .arg(QString::number(static_cast<qulonglong>(snapshot.renderCost.psoSwitches)))
                .arg(QString::number(static_cast<qulonglong>(snapshot.renderCost.srbCommits)))
                .arg(QString::number(static_cast<qulonglong>(snapshot.renderCost.bufferUpdates)));
    }

    static QString playbackQualityText(ArtifactPlaybackService* playbackSvc,
                                       const ArtifactCore::FrameDebugSnapshot& snapshot,
                                       CompositionRenderController* controller)
    {
        const QString previewQuality = previewQualityText();
        const QString cacheHealth = cacheHealthText(snapshot);
        const QString ramPreview = ramPreviewText(playbackSvc);
        const QString renderTiming = renderTimingText(snapshot, controller);
        const QString audioOffset = playbackSvc
                                        ? QStringLiteral("audioOffset=%1s")
                                              .arg(QString::number(playbackSvc->audioOffsetSeconds(), 'f', 3))
                                        : QStringLiteral("audioOffset=<no service>");
        const QString droppedFrames = playbackSvc
                                          ? QStringLiteral("droppedFrames=%1")
                                                .arg(playbackSvc->droppedFrameCount())
                                          : QStringLiteral("droppedFrames=<no service>");
        return QStringLiteral("preview=%1  %2  %3  %4  %5  %6")
                .arg(previewQuality, cacheHealth, ramPreview, renderTiming, audioOffset, droppedFrames);
    }

    static QString sharedThreadPoolText()
    {
        const auto snapshot = ArtifactCore::sharedBackgroundThreadPoolSnapshot();
        return QStringLiteral("pool=%1 active=%2/%3 expiry=%4ms")
                .arg(snapshot.poolName.isEmpty() ? QStringLiteral("<unnamed>")
                                                 : snapshot.poolName)
                .arg(snapshot.activeThreadCount)
                .arg(snapshot.maxThreadCount)
                .arg(snapshot.expiryTimeoutMs);
    }

    static QString playbackDiagnosticsText(ArtifactPlaybackService* playbackSvc,
                                           const ArtifactCore::FrameDebugSnapshot& snapshot,
                                           CompositionRenderController* controller)
    {
        QStringList parts;
        parts << QStringLiteral("state=%1")
                     .arg(playbackSvc ? playbackStateText(playbackSvc->state())
                                      : QStringLiteral("<no service>"));
        parts << QStringLiteral("frame=%1").arg(snapshot.frame.framePosition());
        parts << QStringLiteral("renderLast=%1ms")
                     .arg(QString::number(snapshot.renderLastFrameMs, 'f', 2));
        parts << QStringLiteral("renderAvg=%1ms")
                     .arg(QString::number(snapshot.renderAverageFrameMs, 'f', 2));
        parts << (controller ? renderTimingText(snapshot, controller)
                             : QStringLiteral("renderTiming=<no controller>"));
        parts << sharedThreadPoolText();
        parts << QStringLiteral("audioOffset=%1s")
                     .arg(playbackSvc ? QString::number(playbackSvc->audioOffsetSeconds(), 'f', 3)
                                      : QStringLiteral("<no service>"));
        parts << QStringLiteral("dropped=%1")
                     .arg(playbackSvc ? QString::number(playbackSvc->droppedFrameCount())
                                      : QStringLiteral("<no service>"));
        return parts.join(QStringLiteral("  "));
    }

    static QString passSummaryText(const ArtifactCore::FrameDebugSnapshot& snapshot)
    {
        int failed = 0;
        int pending = 0;
        qint64 totalUs = 0;
        int draw = 0;
        int composite = 0;
        int upload = 0;
        for (const auto& pass : snapshot.passes) {
            totalUs += pass.durationUs;
            switch (pass.status) {
            case ArtifactCore::FrameDebugPassStatus::Failed:
                ++failed;
                break;
            case ArtifactCore::FrameDebugPassStatus::Pending:
                ++pending;
                break;
            default:
                break;
            }
            switch (pass.kind) {
            case ArtifactCore::FrameDebugPassKind::Draw:
                ++draw;
                break;
            case ArtifactCore::FrameDebugPassKind::Composite:
                ++composite;
                break;
            case ArtifactCore::FrameDebugPassKind::Upload:
                ++upload;
                break;
            default:
                break;
            }
        }
        return QStringLiteral("passes=%1 failed=%2 pending=%3 draw=%4 upload=%5 composite=%6 totalUs=%7")
                .arg(static_cast<int>(snapshot.passes.size()))
                .arg(failed)
                .arg(pending)
                .arg(draw)
                .arg(upload)
                .arg(composite)
                .arg(totalUs);
    }

    static QString topPassesText(const ArtifactCore::FrameDebugSnapshot& snapshot)
    {
        if (snapshot.passes.empty()) {
            return QStringLiteral("topPasses=<none>");
        }

        std::vector<ArtifactCore::FrameDebugPassRecord> passes = snapshot.passes;
        std::sort(passes.begin(), passes.end(), [](const auto& a, const auto& b) {
            if (a.durationUs == b.durationUs) {
                return a.name < b.name;
            }
            return a.durationUs > b.durationUs;
        });

        const int rows = std::min<int>(3, static_cast<int>(passes.size()));
        QStringList items;
        items.reserve(rows);
        for (int i = 0; i < rows; ++i) {
            const auto& pass = passes[static_cast<std::size_t>(i)];
            items << QStringLiteral("%1:%2us[%3/%4]")
                          .arg(pass.name.isEmpty() ? QStringLiteral("<unnamed>") : pass.name)
                          .arg(pass.durationUs)
                          .arg(ArtifactCore::toString(pass.kind))
                          .arg(ArtifactCore::toString(pass.status));
        }
        return QStringLiteral("topPasses=%1").arg(items.join(QStringLiteral(" | ")));
    }

    static QString captureEntryLabel(const ArtifactCore::FrameDebugCapture& capture, bool isCurrent)
    {
        const auto& snapshot = capture.snapshot;
        return QStringLiteral("%1 frame=%2  comp=%3  layer=%4  backend=%5  passes=%6  res=%7  att=%8")
                .arg(isCurrent ? QStringLiteral("[current]") : (capture.pinned ? QStringLiteral("[pinned]") : QStringLiteral("[history]")))
                .arg(snapshot.frame.framePosition())
                .arg(snapshot.compositionName.isEmpty() ? QStringLiteral("<none>") : snapshot.compositionName)
                .arg(snapshot.selectedLayerName.isEmpty() ? QStringLiteral("<none>") : snapshot.selectedLayerName)
                .arg(snapshot.renderBackend.isEmpty() ? QStringLiteral("<none>") : snapshot.renderBackend)
                .arg(static_cast<int>(snapshot.passes.size()))
                .arg(static_cast<int>(snapshot.resources.size()))
                .arg(static_cast<int>(snapshot.attachments.size()));
    }

    void updateCaptureHistorySelection(int row)
    {
        captureSelectedRow_ = row;
        updateCaptureHistoryText();
    }

    [[nodiscard]] bool captureAtRow(int row, ArtifactCore::FrameDebugCapture& out) const
    {
        if (!hasCaptureBundle_ || row < 0) {
            return false;
        }
        if (row == 0) {
            out = captureBundle_.capture;
            return true;
        }
        const int historyIndex = static_cast<int>(captureBundle_.history.size()) - row;
        if (historyIndex < 0 || historyIndex >= static_cast<int>(captureBundle_.history.size())) {
            return false;
        }
        out = captureBundle_.history[static_cast<std::size_t>(historyIndex)];
        return true;
    }

    void syncCaptureHistoryList()
    {
        if (!captureHistoryList_) {
            return;
        }

        const int desiredRow = captureSelectedRow_ < 0 ? 0 : std::min(captureSelectedRow_, static_cast<int>(captureBundle_.history.size()));
        captureHistoryList_->clear();
        if (!hasCaptureBundle_) {
            captureHistoryList_->addItem(QStringLiteral("<no capture yet>"));
            captureHistoryList_->setCurrentRow(0);
            return;
        }

        captureHistoryList_->addItem(captureEntryLabel(captureBundle_.capture, true));
        for (int i = static_cast<int>(captureBundle_.history.size()) - 1; i >= 0; --i) {
            captureHistoryList_->addItem(captureEntryLabel(captureBundle_.history[static_cast<std::size_t>(i)], false));
        }
        captureHistoryList_->setCurrentRow(std::min(desiredRow, captureHistoryList_->count() - 1));
    }

    void updateCaptureHistoryText()
    {
        if (!captureHistoryText_) {
            return;
        }

        if (!hasCaptureBundle_) {
            captureHistoryText_->setPlainText(QStringLiteral("No capture yet."));
            if (captureDetailView_) {
                captureDetailView_->setFrameDebugSnapshot(ArtifactCore::FrameDebugSnapshot{
                    ArtifactCore::FramePosition(0),
                    0,
                    QString(),
                    QString(),
                    QString(),
                    QString(),
                    0.0,
                    0.0,
                    0.0,
                    ArtifactCore::RenderCostStats{},
                    ArtifactCore::FrameDebugCompareMode::Disabled,
                    QString(),
                    {},
                    {},
                    {},
                    false,
                    QString()
                });
            }
            return;
        }

        ArtifactCore::FrameDebugCapture selectedCapture;
        if (!captureAtRow(captureSelectedRow_ < 0 ? 0 : captureSelectedRow_, selectedCapture)) {
            selectedCapture = captureBundle_.capture;
        }
        const auto& current = captureBundle_.capture.snapshot;
        const auto& baseline = selectedCapture.snapshot;

        QStringList lines;
        lines << QStringLiteral("Capture Details");
        lines << QStringLiteral("selected: %1").arg(captureEntryLabel(selectedCapture, captureSelectedRow_ <= 0));
        lines << QStringLiteral("current: %1").arg(captureEntryLabel(captureBundle_.capture, true));
        lines << QStringLiteral("bundle: %1").arg(captureBundle_.label.isEmpty() ? QStringLiteral("<unnamed>") : captureBundle_.label);
        lines << QStringLiteral("createdAtMs: %1").arg(captureBundle_.createdAtMs);

        if (captureSelectedRow_ <= 0) {
            lines << QStringLiteral("comparison: current capture");
        } else {
            lines << QString();
            lines << QStringLiteral("Baseline vs current:");
            lines << QStringLiteral("  frame: %1 -> %2")
                          .arg(baseline.frame.framePosition())
                          .arg(current.frame.framePosition());
            lines << QStringLiteral("  composition: %1 -> %2")
                          .arg(baseline.compositionName.isEmpty() ? QStringLiteral("<none>") : baseline.compositionName,
                               current.compositionName.isEmpty() ? QStringLiteral("<none>") : current.compositionName);
            lines << QStringLiteral("  layer: %1 -> %2")
                          .arg(baseline.selectedLayerName.isEmpty() ? QStringLiteral("<none>") : baseline.selectedLayerName,
                               current.selectedLayerName.isEmpty() ? QStringLiteral("<none>") : current.selectedLayerName);
            lines << QStringLiteral("  backend: %1 -> %2")
                          .arg(baseline.renderBackend.isEmpty() ? QStringLiteral("<none>") : baseline.renderBackend,
                               current.renderBackend.isEmpty() ? QStringLiteral("<none>") : current.renderBackend);
            lines << QStringLiteral("  compareMode: %1 -> %2")
                          .arg(ArtifactCore::toString(baseline.compareMode),
                               ArtifactCore::toString(current.compareMode));
            lines << QStringLiteral("  compareTarget: %1 -> %2")
                          .arg(baseline.compareTargetId.isEmpty() ? QStringLiteral("<none>") : baseline.compareTargetId,
                               current.compareTargetId.isEmpty() ? QStringLiteral("<none>") : current.compareTargetId);
            lines << QStringLiteral("  passes: %1 -> %2")
                          .arg(static_cast<int>(baseline.passes.size()))
                          .arg(static_cast<int>(current.passes.size()));
            lines << QStringLiteral("  resources: %1 -> %2")
                          .arg(static_cast<int>(baseline.resources.size()))
                          .arg(static_cast<int>(current.resources.size()));
            lines << QStringLiteral("  attachments: %1 -> %2")
                          .arg(static_cast<int>(baseline.attachments.size()))
                          .arg(static_cast<int>(current.attachments.size()));
            lines << QStringLiteral("  failed: %1 -> %2")
                          .arg(baseline.failed ? QStringLiteral("true") : QStringLiteral("false"),
                               current.failed ? QStringLiteral("true") : QStringLiteral("false"));
            if (!baseline.failureReason.isEmpty() || !current.failureReason.isEmpty()) {
                lines << QStringLiteral("  failureReason: %1 -> %2")
                              .arg(baseline.failureReason.isEmpty() ? QStringLiteral("<none>") : baseline.failureReason,
                                   current.failureReason.isEmpty() ? QStringLiteral("<none>") : current.failureReason);
            }
        }

        auto appendPassPreview = [&lines](const QString& title, const ArtifactCore::FrameDebugSnapshot& snapshot) {
            lines << QString();
            lines << title;
            if (snapshot.passes.empty()) {
                lines << QStringLiteral("  <none>");
                return;
            }
            const int rows = std::min(static_cast<int>(snapshot.passes.size()), 6);
            for (int i = 0; i < rows; ++i) {
                const auto& pass = snapshot.passes[static_cast<std::size_t>(i)];
                lines << QStringLiteral("  #%1 %2 [%3/%4] us=%5 in=%6 out=%7")
                              .arg(i)
                              .arg(pass.name.isEmpty() ? QStringLiteral("<unnamed>") : pass.name,
                                   ArtifactCore::toString(pass.kind),
                                   ArtifactCore::toString(pass.status))
                              .arg(pass.durationUs)
                              .arg(static_cast<int>(pass.inputs.size()))
                              .arg(static_cast<int>(pass.outputs.size()));
            }
        };

        auto appendResourcePreview = [&lines](const QString& title, const ArtifactCore::FrameDebugSnapshot& snapshot) {
            lines << QString();
            lines << title;
            if (snapshot.resources.empty()) {
                lines << QStringLiteral("  <none>");
                return;
            }
            const int rows = std::min(static_cast<int>(snapshot.resources.size()), 6);
            for (int i = 0; i < rows; ++i) {
                const auto& resource = snapshot.resources[static_cast<std::size_t>(i)];
                lines << QStringLiteral("  #%1 %2 [%3] rel=%4 hit=%5 stale=%6")
                              .arg(i)
                              .arg(resource.label.isEmpty() ? QStringLiteral("<unnamed>") : resource.label,
                                   resource.type.isEmpty() ? QStringLiteral("<type?>") : resource.type)
                              .arg(resource.relation.isEmpty() ? QStringLiteral("<none>") : resource.relation)
                              .arg(resource.cacheHit ? QStringLiteral("true") : QStringLiteral("false"))
                              .arg(resource.stale ? QStringLiteral("true") : QStringLiteral("false"));
            }
        };

        auto appendAttachmentPreview = [&lines](const QString& title, const ArtifactCore::FrameDebugSnapshot& snapshot) {
            lines << QString();
            lines << title;
            if (snapshot.attachments.empty()) {
                lines << QStringLiteral("  <none>");
                return;
            }
            const int rows = std::min(static_cast<int>(snapshot.attachments.size()), 6);
            for (int i = 0; i < rows; ++i) {
                const auto& attachment = snapshot.attachments[static_cast<std::size_t>(i)];
                lines << QStringLiteral("  #%1 %2 [%3] readOnly=%4 tex=%5x%6")
                              .arg(i)
                              .arg(attachment.name.isEmpty() ? QStringLiteral("<unnamed>") : attachment.name,
                                   attachment.role.isEmpty() ? QStringLiteral("<none>") : attachment.role)
                              .arg(attachment.readOnly ? QStringLiteral("true") : QStringLiteral("false"))
                              .arg(attachment.texture.width)
                              .arg(attachment.texture.height);
            }
        };

        appendPassPreview(QStringLiteral("Selected capture passes:"), baseline);
        appendResourcePreview(QStringLiteral("Selected capture resources:"), baseline);
        appendAttachmentPreview(QStringLiteral("Selected capture attachments:"), baseline);

        lines << QString();
        lines << QStringLiteral("Recent captures:");
        if (!captureBundle_.history.empty()) {
            const int historyRows = std::min(static_cast<int>(captureBundle_.history.size()), 6);
            for (int i = 0; i < historyRows; ++i) {
                const auto& entry = captureBundle_.history[static_cast<std::size_t>(captureBundle_.history.size() - 1 - i)];
                lines << QStringLiteral("  - %1").arg(captureEntryLabel(entry, false));
            }
        } else {
            lines << QStringLiteral("  <none>");
        }

        captureHistoryText_->setPlainText(lines.join(QStringLiteral("\n")));
        if (captureDetailView_) {
            captureDetailView_->setFrameDebugSnapshot(selectedCapture.snapshot);
        }
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
            lines << QString();
            lines << QStringLiteral("Playback Quality");
            lines << QStringLiteral("previewQuality: %1").arg(previewQualityText());
            lines << QStringLiteral("cacheHealth: %1").arg(cacheHealthText(controllerSnapshot));
            lines << QStringLiteral("ramPreview: %1").arg(ramPreviewText(playbackSvc));
            lines << QStringLiteral("audioOffsetSeconds: %1")
                          .arg(playbackSvc ? QString::number(playbackSvc->audioOffsetSeconds(), 'f', 3)
                                           : QStringLiteral("<no service>"));
            lines << QStringLiteral("droppedFrames: %1")
                          .arg(playbackSvc ? QString::number(playbackSvc->droppedFrameCount())
                                           : QStringLiteral("<no service>"));
            lines << QStringLiteral("renderTiming: %1")
                          .arg(controller_ ? renderTimingText(controllerSnapshot, controller_)
                                           : QStringLiteral("<no controller>"));
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
            const QString qualityText = playbackQualityText(playbackSvc, controllerSnapshot, controller_);
            const QString queueText = queueSvc ? QString::number(queueSvc->jobCount())
                                               : QStringLiteral("<no service>");
            const QString projectText = projectSvc ? projectSvc->projectName().toQString()
                                                   : QStringLiteral("<no service>");

            stateSummary_->setText(QStringLiteral("project=%1  composition=%2  layer=%3  frame=%4  playback=%5  quality=%6  backend=%7  queueJobs=%8")
                                       .arg(projectText,
                                            compositionText,
                                            layerText)
                                       .arg(controllerSnapshot.frame.framePosition())
                                       .arg(playbackText)
                                       .arg(qualityText)
                                       .arg(backendText)
                                       .arg(queueText));
        }

        if (playbackText_ || playbackSummary_) {
            const QString poolText = sharedThreadPoolText();
            const QString playbackText = playbackSvc ? playbackStateText(playbackSvc->state())
                                                     : QStringLiteral("<no service>");
            const QString renderTiming = playbackSvc
                                            ? QStringLiteral("renderLast=%1ms  renderAvg=%2ms")
                                                  .arg(QString::number(controllerSnapshot.renderLastFrameMs, 'f', 2))
                                                  .arg(QString::number(controllerSnapshot.renderAverageFrameMs, 'f', 2))
                                            : QStringLiteral("renderLast=<no service>  renderAvg=<no service>");
            const QString playbackQuality = playbackQualityText(playbackSvc, controllerSnapshot, controller_);

            if (playbackSummary_) {
                playbackSummary_->setText(QStringLiteral("playback=%1  frame=%2  %3  pool=%4  queueJobs=%5")
                                              .arg(playbackText)
                                              .arg(controllerSnapshot.frame.framePosition())
                                              .arg(renderTiming)
                                              .arg(poolText)
                                              .arg(queueSvc ? queueSvc->jobCount() : 0));
                playbackSummary_->setToolTip(QStringLiteral("%1  %2")
                                                 .arg(playbackQuality, playbackDiagnosticsText(playbackSvc, controllerSnapshot, controller_)));
            }

            if (playbackText_) {
                QStringList lines;
                lines << QStringLiteral("Playback");
                lines << QStringLiteral("state: %1").arg(playbackText);
                lines << QStringLiteral("frame: %1").arg(controllerSnapshot.frame.framePosition());
                lines << QStringLiteral("passSummary: %1").arg(passSummaryText(controllerSnapshot));
                lines << QStringLiteral("topPasses: %1").arg(topPassesText(controllerSnapshot));
                lines << QStringLiteral("renderLastFrameMs: %1").arg(QString::number(controllerSnapshot.renderLastFrameMs, 'f', 2));
                lines << QStringLiteral("renderAverageFrameMs: %1").arg(QString::number(controllerSnapshot.renderAverageFrameMs, 'f', 2));
                lines << QStringLiteral("renderBackend: %1")
                              .arg(controllerSnapshot.renderBackend.isEmpty() ? QStringLiteral("<none>")
                                                                              : controllerSnapshot.renderBackend);
                lines << QStringLiteral("audioOffsetSeconds: %1")
                              .arg(playbackSvc ? QString::number(playbackSvc->audioOffsetSeconds(), 'f', 3)
                                               : QStringLiteral("<no service>"));
                lines << QStringLiteral("droppedFrames: %1")
                              .arg(playbackSvc ? QString::number(playbackSvc->droppedFrameCount())
                                               : QStringLiteral("<no service>"));
                lines << QStringLiteral("ramPreview: %1").arg(ramPreviewText(playbackSvc));
                lines << QStringLiteral("previewQuality: %1").arg(previewQualityText());
                lines << QStringLiteral("threadPool: %1").arg(poolText);
                lines << QStringLiteral("renderTiming: %1")
                              .arg(controller_ ? renderTimingText(controllerSnapshot, controller_)
                                           : QStringLiteral("<no controller>"));
                playbackText_->setPlainText(lines.join(QStringLiteral("\n")));
            }
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

        if (captureSummary_) {
            const QString compositionText = controllerSnapshot.compositionName.isEmpty()
                                                ? QStringLiteral("<none>")
                                                : controllerSnapshot.compositionName;
            const QString backendText = controllerSnapshot.renderBackend.isEmpty()
                                            ? QStringLiteral("<none>")
                                            : controllerSnapshot.renderBackend;
            const QString layerText = controllerSnapshot.selectedLayerName.isEmpty()
                                            ? QStringLiteral("<none>")
                                            : controllerSnapshot.selectedLayerName;
            captureSummary_->setText(QStringLiteral("frame=%1  composition=%2  layer=%3  backend=%4  passes=%5  resources=%6  attachments=%7  traceEvents=%8")
                                         .arg(controllerSnapshot.frame.framePosition())
                                         .arg(compositionText)
                                         .arg(layerText)
                                         .arg(backendText)
                                         .arg(static_cast<int>(controllerSnapshot.passes.size()))
                                         .arg(static_cast<int>(controllerSnapshot.resources.size()))
                                         .arg(static_cast<int>(controllerSnapshot.attachments.size()))
                                         .arg(static_cast<int>(trace.events.size())));
            captureSummary_->setToolTip(QStringLiteral("RenderDoc-like capture overview"));
        }

        if (hasControllerSnapshot) {
            ArtifactCore::FrameDebugCapture currentCapture;
            currentCapture.captureId = QStringLiteral("frame-%1").arg(controllerSnapshot.frame.framePosition());
            currentCapture.snapshot = controllerSnapshot;
            currentCapture.sourceFrameId = QStringLiteral("frame-%1").arg(controllerSnapshot.frame.framePosition());
            currentCapture.targetFrameId = currentCapture.sourceFrameId;
            currentCapture.pinned = controllerSnapshot.compareMode != ArtifactCore::FrameDebugCompareMode::Disabled;

            const QString currentFrameId = currentCapture.sourceFrameId;
            const QString lastFrameId = hasCaptureBundle_ ? captureBundle_.capture.sourceFrameId : QString();
            if (!hasCaptureBundle_ || lastFrameId != currentFrameId) {
                if (hasCaptureBundle_) {
                    captureBundle_.history.push_back(captureBundle_.capture);
                    while (captureBundle_.history.size() > 8) {
                        captureBundle_.history.erase(captureBundle_.history.begin());
                    }
                }
                captureBundle_.bundleId = QStringLiteral("app-debugger");
                captureBundle_.label = controllerSnapshot.compositionName.isEmpty()
                        ? QStringLiteral("App Debugger Capture")
                        : controllerSnapshot.compositionName;
                captureBundle_.createdAtMs = controllerSnapshot.timestampMs;
                captureBundle_.capture = currentCapture;
                hasCaptureBundle_ = true;
            } else {
                captureBundle_.capture = currentCapture;
            }
        }

        syncCaptureHistoryList();
        updateCaptureHistoryText();

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

        if (capturePipelineView_) {
            capturePipelineView_->setFrameDebugSnapshot(controllerSnapshot, trace);
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

        if (captureResourceView_) {
            captureResourceView_->setFrameDebugSnapshot(controllerSnapshot, trace);
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

        if (captureTraceTimelineView_) {
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
            captureTraceTimelineView_->setFocusedThreadId(focusThreadId);
            captureTraceTimelineView_->setFocusedMutexName(focusMutexName);
            captureTraceTimelineView_->setTraceSnapshot(trace);
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
