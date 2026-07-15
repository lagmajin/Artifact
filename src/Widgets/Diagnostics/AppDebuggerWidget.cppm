module;
#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QAbstractItemView>
#include <QBoxLayout>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QColor>
#include <QIODevice>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QListWidget>
#include <QAbstractScrollArea>
#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QPalette>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QSplitter>
#include <QTabWidget>
#include <QShowEvent>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QTimerEvent>
#include <QStandardPaths>
#include <QVector>
#include <QStringList>
#include <cstdint>
#include <wobjectimpl.h>

module Artifact.Widgets.AppDebuggerWidget;

import Core.Diagnostics.Trace;
import Application.AppSettings;
import Artifact.Service.Playback;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Render.Queue.Service;
import Artifact.Widgets.CompositionRenderController;
import Artifact.Widgets.FramePipelineViewWidget;
import Artifact.Widgets.FrameDebugViewWidget;
import Artifact.Widgets.FrameResourceInspectorWidget;
import Artifact.Widgets.FrameStateDiffWidget;
import Artifact.Widgets.TraceTimelineWidget;
import Artifact.Widgets.DebugRenderHarnessWidget;
import Tracking.MotionTracker;
import Thread.Helper;
import Frame.Debug;
import Playback.State;
import Utils.String.UniString;

namespace Artifact {

W_OBJECT_IMPL(AppDebuggerWidget)

namespace {
void applyDebuggerSurfacePalette(QWidget* root, const QPalette& palette)
{
    if (!root) {
        return;
    }
    root->setAutoFillBackground(true);
    root->setAttribute(Qt::WA_StyledBackground, true);
    root->setPalette(palette);
    for (auto* child : root->findChildren<QWidget*>()) {
        if (!child || child->testAttribute(Qt::WA_PaintOnScreen)) {
            continue;
        }
        child->setAutoFillBackground(true);
        child->setAttribute(Qt::WA_StyledBackground, true);
        child->setPalette(palette);
        if (auto* scroll = qobject_cast<QAbstractScrollArea*>(child)) {
            if (auto* viewport = scroll->viewport()) {
                viewport->setAutoFillBackground(true);
                viewport->setAttribute(Qt::WA_StyledBackground, true);
                viewport->setPalette(palette);
            }
        }
    }
}

QString debugMcpStateFilePath()
{
    const QString envPath = qEnvironmentVariable("ARTIFACT_DEBUG_MCP_STATE_FILE");
    if (!envPath.trimmed().isEmpty()) {
        return envPath;
    }

    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir rootDir(tempRoot);
    if (!rootDir.exists(QStringLiteral("ArtifactStudio"))) {
        rootDir.mkpath(QStringLiteral("ArtifactStudio"));
    }
    return rootDir.filePath(QStringLiteral("ArtifactStudio/debug-mcp-state.json"));
}

QString frameDebugBundleFilePath()
{
    const QString envPath = qEnvironmentVariable("ARTIFACT_FRAME_DEBUG_BUNDLE_FILE");
    if (!envPath.trimmed().isEmpty()) {
        return envPath;
    }

    const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir appDataDir(appDataRoot);
    if (!appDataDir.exists()) {
        appDataDir.mkpath(QStringLiteral("."));
    }

    const QString projectRoot = ArtifactProjectManager::getInstance().currentProjectRootPath().trimmed();
    if (!projectRoot.isEmpty()) {
        const QString projectFolderName = QFileInfo(projectRoot).fileName().trimmed().isEmpty()
                                              ? QStringLiteral("project")
                                              : QFileInfo(projectRoot).fileName().trimmed();
        QDir projectDir(appDataDir.filePath(QStringLiteral("FrameDebug")));
        if (!projectDir.exists(projectFolderName)) {
            projectDir.mkpath(projectFolderName);
        }
        return projectDir.filePath(projectFolderName + QStringLiteral("/frame-debug-bundle.json"));
    }

    QDir rootDir(appDataDir.filePath(QStringLiteral("FrameDebug")));
    if (!rootDir.exists()) {
        rootDir.mkpath(QStringLiteral("."));
    }
    return rootDir.filePath(QStringLiteral("frame-debug-bundle.json"));
}

bool writeJsonObjectFile(const QString& filePath, const QJsonObject& object)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const QJsonDocument doc(object);
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        return false;
    }
    return file.commit();
}

QJsonObject readJsonObjectFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QJsonObject readDebugMcpStateObject()
{
    QFile file(debugMcpStateFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QString debugMcpFrameText(const QJsonObject& state)
{
    const QJsonObject session = state.value(QStringLiteral("session")).toObject();
    const QJsonValue pausedAtFrame = session.value(QStringLiteral("pausedAtFrame"));
    if (pausedAtFrame.isDouble()) {
        return QString::number(pausedAtFrame.toInt());
    }
    const QJsonObject lastBreakHit = state.value(QStringLiteral("lastBreakHit")).toObject();
    const QJsonObject snapshot = lastBreakHit.value(QStringLiteral("snapshot")).toObject();
    const QJsonObject playback = snapshot.value(QStringLiteral("playback")).toObject();
    const QJsonValue frameValue = playback.value(QStringLiteral("frame"));
    return frameValue.isDouble() ? QString::number(frameValue.toInt()) : QStringLiteral("<none>");
}

QString debugMcpLastHitText(const QJsonObject& state)
{
    const QJsonObject lastBreakHit = state.value(QStringLiteral("lastBreakHit")).toObject();
    if (lastBreakHit.isEmpty()) {
        return QStringLiteral("<none>");
    }

    const QJsonObject condition = lastBreakHit.value(QStringLiteral("condition")).toObject();
    const QString kind = condition.value(QStringLiteral("kind")).toString(QStringLiteral("<unknown>"));
    const QString label = condition.value(QStringLiteral("label")).toString().trimmed();
    const QString suffix = label.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(label);
    const QString reason = lastBreakHit.value(QStringLiteral("reason")).toString();
    const QString resumedAt = lastBreakHit.value(QStringLiteral("resumedAt")).toString();
    return resumedAt.isEmpty()
               ? QStringLiteral("%1%2  %3").arg(kind, suffix, reason)
               : QStringLiteral("%1%2  %3  resumed=%4")
                     .arg(kind, suffix, reason, resumedAt);
}

QString debugMcpStatusSummaryText(const QJsonObject& state)
{
    const QJsonObject sessionSummary = state.value(QStringLiteral("sessionSummary")).toObject();
    const QString summaryText = sessionSummary.value(QStringLiteral("text")).toString().trimmed();
    if (!summaryText.isEmpty()) {
        return summaryText;
    }

    const QJsonObject session = state.value(QStringLiteral("session")).toObject();
    const bool paused = session.value(QStringLiteral("paused")).toBool(false);
    const QString reason = session.value(QStringLiteral("pauseReason")).toString().trimmed();
    const QString frame = debugMcpFrameText(state);
    const QString lastAction = session.value(QStringLiteral("lastAction")).toString().trimmed();
    const QString tickCount = session.value(QStringLiteral("tickCount")).isDouble()
                                  ? QString::number(session.value(QStringLiteral("tickCount")).toInt(0))
                                  : QStringLiteral("0");
    return QStringLiteral("%1  reason=%2  frame=%3  action=%4  ticks=%5")
        .arg(paused ? QStringLiteral("paused") : QStringLiteral("running"))
        .arg(reason.isEmpty() ? QStringLiteral("<none>") : reason)
        .arg(frame)
        .arg(lastAction.isEmpty() ? QStringLiteral("<none>") : lastAction)
        .arg(tickCount);
}

QString debugMcpConditionValueText(const QJsonValue& value)
{
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    if (value.isString()) {
        return value.toString();
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    return QStringLiteral("<none>");
}

QString trackerTypeText(ArtifactCore::TrackerType type)
{
    switch (type) {
    case ArtifactCore::TrackerType::Point:
        return QStringLiteral("point");
    case ArtifactCore::TrackerType::Planar:
        return QStringLiteral("planar");
    case ArtifactCore::TrackerType::Spline:
        return QStringLiteral("spline");
    case ArtifactCore::TrackerType::Perspective:
        return QStringLiteral("perspective");
    }
    return QStringLiteral("unknown");
}

QString motionTrackerSummaryText()
{
    auto& manager = ArtifactCore::TrackerManager::instance();
    const auto trackers = manager.allTrackers();
    if (trackers.empty()) {
        return QStringLiteral("motionTrackers=0");
    }

    const ArtifactCore::MotionTracker* first = trackers.front();
    const QString firstName = first ? first->name() : QStringLiteral("<none>");
    const QString firstType = first ? trackerTypeText(first->trackerType()) : QStringLiteral("<none>");
    const int pointCount = first ? first->trackPointCount() : 0;
    const int regionCount = first ? first->trackRegionCount() : 0;
    const bool hasResult = first ? first->hasResult() : false;
    const int resultFrames = first && hasResult ? static_cast<int>(first->result().frameCount()) : 0;
    const double avgConfidence = first && hasResult ? first->averageConfidence() : 0.0;
    const int problemFrameCount = first && hasResult ? static_cast<int>(first->problemFrames().size()) : 0;

    return QStringLiteral("motionTrackers=%1  first=%2  type=%3  points=%4  regions=%5  resultFrames=%6  avgConf=%7  problemFrames=%8")
        .arg(static_cast<int>(trackers.size()))
        .arg(firstName.isEmpty() ? QStringLiteral("<unnamed>") : firstName)
        .arg(firstType)
        .arg(pointCount)
        .arg(regionCount)
        .arg(resultFrames)
        .arg(QString::number(avgConfidence * 100.0, 'f', 1) + QStringLiteral("%"))
        .arg(problemFrameCount);
}

QString debugMcpBreakConditionSummary(const QJsonObject& condition)
{
    const QString kind = condition.value(QStringLiteral("kind")).toString(QStringLiteral("<unknown>"));
    const QString label = condition.value(QStringLiteral("label")).toString().trimmed();
    const QString suffix = label.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(label);
    const bool enabled = condition.value(QStringLiteral("enabled")).toBool(true);
    return QStringLiteral("#%1 %2%3  %4  value=%5")
        .arg(condition.value(QStringLiteral("id")).toInt(-1))
        .arg(kind)
        .arg(suffix)
        .arg(enabled ? QStringLiteral("on") : QStringLiteral("off"))
        .arg(debugMcpConditionValueText(condition.value(QStringLiteral("value"))));
}

QString debugMcpBreakConditionsPreviewText(const QJsonArray& conditions, int limit = 3)
{
    if (conditions.isEmpty()) {
        return QStringLiteral("<none>");
    }

    QStringList lines;
    const int count = std::min(limit, static_cast<int>(conditions.size()));
    for (int i = 0; i < count; ++i) {
        lines << QStringLiteral("  - %1").arg(debugMcpBreakConditionSummary(conditions.at(i).toObject()));
    }
    if (conditions.size() > count) {
        lines << QStringLiteral("  ... %1 more").arg(conditions.size() - count);
    }
    return lines.join(QStringLiteral("\n"));
}

QString debugMcpPropertyPreviewText(const QJsonArray& properties, int limit = 3)
{
    if (properties.isEmpty()) {
        return QStringLiteral("<none>");
    }

    QStringList lines;
    const int count = std::min(limit, static_cast<int>(properties.size()));
    for (int i = 0; i < count; ++i) {
        const QJsonObject property = properties.at(i).toObject();
        const QString path = property.value(QStringLiteral("path")).toString(QStringLiteral("<unknown>"));
        const QString type = property.value(QStringLiteral("type")).toString(QStringLiteral("<unknown>"));
        const QString value = debugMcpConditionValueText(property.value(QStringLiteral("value")));
        lines << QStringLiteral("  - %1 [%2] = %3").arg(path, type, value);
    }
    if (properties.size() > count) {
        lines << QStringLiteral("  ... %1 more").arg(properties.size() - count);
    }
    return lines.join(QStringLiteral("\n"));
}

QString debugMcpHistoryActionLabel(const QString& type)
{
    return type == QStringLiteral("break-hit") ? QStringLiteral("hit")
        : type == QStringLiteral("resume") ? QStringLiteral("resume")
        : type == QStringLiteral("step") ? QStringLiteral("step")
        : type == QStringLiteral("reset-session") ? QStringLiteral("reset")
        : type == QStringLiteral("clear-history") ? QStringLiteral("clear")
        : type == QStringLiteral("read-last-break-hit") ? QStringLiteral("read-hit")
        : type == QStringLiteral("read-session-summary") ? QStringLiteral("read-summary")
        : type == QStringLiteral("read-history") ? QStringLiteral("read-history")
        : type == QStringLiteral("snapshot-read") ? QStringLiteral("snapshot")
        : type == QStringLiteral("set-mock-snapshot") ? QStringLiteral("mock")
        : type == QStringLiteral("set-break-condition") ? QStringLiteral("set")
        : type == QStringLiteral("update-break-condition") ? QStringLiteral("update")
        : type == QStringLiteral("clear-break-condition") ? QStringLiteral("clear-one")
        : type == QStringLiteral("clear-all-break-conditions") ? QStringLiteral("clear-all")
        : type == QStringLiteral("list-break-conditions") ? QStringLiteral("list")
        : type;
}

QString debugMcpBreakHistorySummaryText(const QJsonObject& state)
{
    const QJsonObject sessionSummary = state.value(QStringLiteral("sessionSummary")).toObject();
    const QString sharedSummary = sessionSummary.value(QStringLiteral("breakHistorySummary")).toString().trimmed();
    if (!sharedSummary.isEmpty()) {
        return sharedSummary;
    }
    const QJsonArray history = state.value(QStringLiteral("history")).toArray();
    const QString mode = sessionSummary.value(QStringLiteral("mode")).toString(QStringLiteral("<none>")).trimmed();
    const QString bridgePath = sessionSummary.value(QStringLiteral("bridgePath")).toString(QStringLiteral("<none>")).trimmed();
    const QString recentAction = sessionSummary.value(QStringLiteral("recentAction")).toString().trimmed().isEmpty()
                                     ? sessionSummary.value(QStringLiteral("lastAction")).toString().trimmed()
                                     : sessionSummary.value(QStringLiteral("recentAction")).toString().trimmed();
    const QString priorRecentAction = !sessionSummary.value(QStringLiteral("priorRecentAction")).toString().trimmed().isEmpty()
                                          ? sessionSummary.value(QStringLiteral("priorRecentAction")).toString().trimmed()
                                          : history.size() > 1
                                                ? debugMcpHistoryActionLabel(
                                                      history.at(history.size() - 2).toObject().value(QStringLiteral("type"))
                                                          .toString(QStringLiteral("<unknown>")))
                                                : QStringLiteral("<none>");
    const QString lastHit = debugMcpLastHitText(state);
    return QStringLiteral("mode=%1  bridge=%2  history=%3  recent=%4  prior=%5  lastHit=%6")
        .arg(mode.isEmpty() ? QStringLiteral("<none>") : mode)
        .arg(bridgePath.isEmpty() ? QStringLiteral("<none>") : bridgePath)
        .arg(history.size())
        .arg(recentAction.isEmpty() ? QStringLiteral("<none>") : recentAction)
        .arg(priorRecentAction)
        .arg(lastHit);
}

QString debugMcpSummaryPreviewText(const QJsonArray& history, const QJsonArray& summaryLines, int limit = 2)
{
    // Prefer the compact summary lines from the MCP server; raw history is the fallback.
    if (!summaryLines.isEmpty()) {
        QStringList lines;
        const int count = std::min(limit, static_cast<int>(summaryLines.size()));
        for (int i = 0; i < count; ++i) {
            const QString entry = summaryLines.at(static_cast<int>(summaryLines.size()) - 1 - i).toString().trimmed();
            lines << QStringLiteral("  %1. %2")
                          .arg(i + 1)
                          .arg(entry.isEmpty() ? QStringLiteral("<none>") : entry);
        }
        if (summaryLines.size() > count) {
            lines << QStringLiteral("  ... %1 more").arg(summaryLines.size() - count);
        }
        return lines.join(QStringLiteral("\n"));
    }

    if (history.isEmpty()) {
        return QStringLiteral("<none>");
    }

    QStringList lines;
    const int count = std::min(limit, static_cast<int>(history.size()));
    for (int i = 0; i < count; ++i) {
        const QJsonObject entry = history.at(static_cast<int>(history.size()) - 1 - i).toObject();
        const QString type = entry.value(QStringLiteral("type")).toString(QStringLiteral("<unknown>"));
        const QString conditionId = entry.value(QStringLiteral("conditionId")).isDouble()
                                        ? QString::number(entry.value(QStringLiteral("conditionId")).toInt())
                                        : QStringLiteral("-");
        const QString conditionKind = entry.value(QStringLiteral("conditionKind")).toString();
        const QString label = debugMcpHistoryActionLabel(type);
        const bool isBreakHit = type == QStringLiteral("break-hit");
        if (isBreakHit) {
            lines << QStringLiteral("  %1. %2  [%3/%4]")
                          .arg(i + 1)
                          .arg(label)
                          .arg(conditionId)
                          .arg(conditionKind.isEmpty() ? QStringLiteral("-") : conditionKind);
        } else {
            lines << QStringLiteral("  %1. %2")
                          .arg(i + 1)
                          .arg(label);
        }
    }
    if (history.size() > count) {
        lines << QStringLiteral("  ... %1 more").arg(history.size() - count);
    }
    return lines.join(QStringLiteral("\n"));
}
}

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
    QLabel* stateContextValue_ = nullptr;
    QLabel* stateContextDetail_ = nullptr;
    QLabel* statePlaybackValue_ = nullptr;
    QLabel* statePlaybackDetail_ = nullptr;
    QLabel* stateRenderValue_ = nullptr;
    QLabel* stateRenderDetail_ = nullptr;
    QLabel* stateAttentionValue_ = nullptr;
    QLabel* stateAttentionDetail_ = nullptr;
    QPlainTextEdit* stateText_ = nullptr;
    QWidget* playbackPage_ = nullptr;
    QLabel* playbackSummary_ = nullptr;
    QLabel* playbackStateValue_ = nullptr;
    QLabel* playbackStateDetail_ = nullptr;
    QLabel* playbackCacheValue_ = nullptr;
    QLabel* playbackCacheDetail_ = nullptr;
    QLabel* playbackRenderValue_ = nullptr;
    QLabel* playbackRenderDetail_ = nullptr;
    QLabel* playbackQueueValue_ = nullptr;
    QLabel* playbackQueueDetail_ = nullptr;
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
    QWidget* harnessPage_ = nullptr;
    DebugRenderHarnessWidget* harnessWidget_ = nullptr;
    QWidget* diagnosticsPage_ = nullptr;
    QLabel* diagnosticsSummary_ = nullptr;
    QPlainTextEdit* diagnosticsText_ = nullptr;
    QWidget* exportPage_ = nullptr;
    QLabel* exportSummary_ = nullptr;
    QPlainTextEdit* exportText_ = nullptr;
    QString captureBundlePath_;
    qint64 captureBundleStampMs_ = -1;
    CompositionRenderController* controller_ = nullptr;
    int timerId_ = 0;
    bool refreshing_ = false;

    explicit Impl(AppDebuggerWidget* owner, CompositionRenderController* controller)
        : owner_(owner), controller_(controller)
    {}

    static QFrame* addStatusCard(QBoxLayout* layout,
                                 const QString& title,
                                 QLabel*& value,
                                 QLabel*& detail,
                                 QWidget* parent)
    {
        auto* card = new QFrame(parent);
        card->setFrameShape(QFrame::StyledPanel);
        card->setFrameShadow(QFrame::Plain);
        card->setAutoFillBackground(true);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(10, 8, 10, 8);
        cardLayout->setSpacing(2);

        auto* titleLabel = new QLabel(title, card);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        cardLayout->addWidget(titleLabel);

        value = new QLabel(card);
        QFont valueFont = value->font();
        valueFont.setBold(true);
        valueFont.setPointSize(std::max(10, valueFont.pointSize() + 1));
        value->setFont(valueFont);
        value->setWordWrap(true);
        cardLayout->addWidget(value);

        detail = new QLabel(card);
        detail->setWordWrap(true);
        detail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        cardLayout->addWidget(detail);

        layout->addWidget(card, 1);
        return card;
    }

    void restoreCaptureBundle()
    {
        captureBundlePath_ = frameDebugBundleFilePath();
        const QJsonObject json = readJsonObjectFile(captureBundlePath_);
        if (json.isEmpty()) {
            captureBundleStampMs_ = -1;
            return;
        }

        const auto bundle = ArtifactCore::FrameDebugBundle::fromJson(json);
        if (bundle.capture.captureId.isEmpty() && bundle.history.empty()) {
            captureBundleStampMs_ = -1;
            return;
        }

        captureBundle_ = bundle;
        hasCaptureBundle_ = true;
        captureSelectedRow_ = 0;
        captureBundleStampMs_ = QFileInfo(captureBundlePath_).lastModified().toMSecsSinceEpoch();
    }

    void persistCaptureBundle() const
    {
        if (!hasCaptureBundle_) {
            return;
        }
        writeJsonObjectFile(captureBundlePath_.isEmpty() ? frameDebugBundleFilePath() : captureBundlePath_,
                            captureBundle_.toJson());
    }

    void refreshCaptureBundleFromDisk()
    {
        const QString path = captureBundlePath_.isEmpty() ? frameDebugBundleFilePath() : captureBundlePath_;
        const QFileInfo info(path);
        if (!info.exists()) {
            return;
        }
        const qint64 stampMs = info.lastModified().toMSecsSinceEpoch();
        if (stampMs <= 0 || stampMs == captureBundleStampMs_) {
            return;
        }

        const QJsonObject json = readJsonObjectFile(path);
        if (json.isEmpty()) {
            return;
        }
        const auto bundle = ArtifactCore::FrameDebugBundle::fromJson(json);
        if (bundle.capture.captureId.isEmpty() && bundle.history.empty()) {
            return;
        }

        captureBundle_ = bundle;
        hasCaptureBundle_ = true;
        captureSelectedRow_ = 0;
        captureBundleStampMs_ = stampMs;
    }

    void setupUI()
    {
        owner_->setAutoFillBackground(true);
        owner_->setAttribute(Qt::WA_StyledBackground, true);
        QPalette palette = owner_->palette();
        palette.setColor(QPalette::Window, QColor::fromRgb(28, 30, 36));
        palette.setColor(QPalette::WindowText, QColor::fromRgb(232, 240, 248));
        palette.setColor(QPalette::Base, QColor::fromRgb(20, 22, 28));
        palette.setColor(QPalette::Text, QColor::fromRgb(232, 240, 248));
        owner_->setPalette(palette);

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
        overviewSummary_->setMaximumHeight(72);
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
        restoreCaptureBundle();
        syncCaptureHistoryList();
        updateCaptureHistoryText();

        statePage_ = new QWidget(tabs_);
        auto* stateLayout = new QVBoxLayout(statePage_);
        stateLayout->setContentsMargins(0, 0, 0, 0);
        stateLayout->setSpacing(0);
        stateSummary_ = new QLabel(statePage_);
        stateSummary_->setTextFormat(Qt::PlainText);
        stateSummary_->setWordWrap(true);
        stateSummary_->setMinimumHeight(56);
        stateLayout->addWidget(stateSummary_);
        auto* stateCardsTop = new QHBoxLayout();
        stateCardsTop->setContentsMargins(8, 8, 8, 0);
        stateCardsTop->setSpacing(8);
        addStatusCard(stateCardsTop, QStringLiteral("CURRENT CONTEXT"),
                      stateContextValue_, stateContextDetail_, statePage_);
        addStatusCard(stateCardsTop, QStringLiteral("PLAYBACK"),
                      statePlaybackValue_, statePlaybackDetail_, statePage_);
        stateLayout->addLayout(stateCardsTop);
        auto* stateCardsBottom = new QHBoxLayout();
        stateCardsBottom->setContentsMargins(8, 8, 8, 8);
        stateCardsBottom->setSpacing(8);
        addStatusCard(stateCardsBottom, QStringLiteral("RENDER"),
                      stateRenderValue_, stateRenderDetail_, statePage_);
        addStatusCard(stateCardsBottom, QStringLiteral("ATTENTION / NEXT"),
                      stateAttentionValue_, stateAttentionDetail_, statePage_);
        stateLayout->addLayout(stateCardsBottom);
        auto* stateDetailsTitle = new QLabel(QStringLiteral("DETAILS / RAW SNAPSHOT"), statePage_);
        stateDetailsTitle->setContentsMargins(10, 2, 0, 2);
        stateLayout->addWidget(stateDetailsTitle);
        stateText_ = new QPlainTextEdit(statePage_);
        stateText_->setReadOnly(true);
        stateText_->setLineWrapMode(QPlainTextEdit::NoWrap);
        stateText_->setMaximumHeight(140);
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
        auto* playbackCardsTop = new QHBoxLayout();
        playbackCardsTop->setContentsMargins(8, 8, 8, 0);
        playbackCardsTop->setSpacing(8);
        addStatusCard(playbackCardsTop, QStringLiteral("PLAYBACK"),
                      playbackStateValue_, playbackStateDetail_, playbackPage_);
        addStatusCard(playbackCardsTop, QStringLiteral("RAM PREVIEW"),
                      playbackCacheValue_, playbackCacheDetail_, playbackPage_);
        playbackLayout->addLayout(playbackCardsTop);
        auto* playbackCardsBottom = new QHBoxLayout();
        playbackCardsBottom->setContentsMargins(8, 8, 8, 8);
        playbackCardsBottom->setSpacing(8);
        addStatusCard(playbackCardsBottom, QStringLiteral("FRAME TIME"),
                      playbackRenderValue_, playbackRenderDetail_, playbackPage_);
        addStatusCard(playbackCardsBottom, QStringLiteral("QUEUE"),
                      playbackQueueValue_, playbackQueueDetail_, playbackPage_);
        playbackLayout->addLayout(playbackCardsBottom);
        auto* playbackDetailsTitle = new QLabel(QStringLiteral("DETAILS / RAW SNAPSHOT"), playbackPage_);
        playbackDetailsTitle->setContentsMargins(10, 2, 0, 2);
        playbackLayout->addWidget(playbackDetailsTitle);
        playbackText_ = new QPlainTextEdit(playbackPage_);
        playbackText_->setReadOnly(true);
        playbackText_->setLineWrapMode(QPlainTextEdit::NoWrap);
        playbackText_->setMaximumHeight(140);
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
        harnessPage_ = new QWidget(tabs_);
        auto* harnessLayout = new QVBoxLayout(harnessPage_);
        harnessLayout->setContentsMargins(0, 0, 0, 0);
        harnessLayout->setSpacing(0);
        harnessWidget_ = new DebugRenderHarnessWidget(harnessPage_);
        harnessWidget_->setScenePreset(QStringLiteral("mixed-media"));
        harnessLayout->addWidget(harnessWidget_);
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
        tabs_->addTab(harnessPage_, QStringLiteral("Harness"));
        tabs_->addTab(diagnosticsPage_, QStringLiteral("Diagnostics"));
        tabs_->addTab(exportPage_, QStringLiteral("Export"));

        layout->addWidget(tabs_);

        timerId_ = owner_->startTimer(250);
        owner_->ensurePolished();
        layout->activate();
        applyDebuggerSurfacePalette(owner_, palette);
        for (auto* edit : owner_->findChildren<QPlainTextEdit*>()) {
            if (!edit) {
                continue;
            }
            edit->setPalette(palette);
            if (auto* viewport = edit->viewport()) {
                viewport->setAutoFillBackground(true);
                viewport->setPalette(palette);
            }
        }
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

    static QString resourceStateText(const ArtifactCore::FrameDebugSnapshot& snapshot,
                                     const QString& typeName,
                                     const QString& labelName)
    {
        for (const auto& resource : snapshot.resources) {
            if (resource.type == typeName || resource.label == labelName) {
                const QString note = resource.note.trimmed();
                if (note.startsWith(QStringLiteral("state="))) {
                    return note.section(QChar::Space, 0, 0);
                }
                const bool skipped = note.contains(QStringLiteral("skipped=")) ||
                                     resource.stale || !resource.cacheHit;
                return skipped ? QStringLiteral("skipped") : QStringLiteral("ok");
            }
        }
        return QStringLiteral("none");
    }

    static QString resourceNoteText(const ArtifactCore::FrameDebugSnapshot& snapshot,
                                    const QString& typeName,
                                    const QString& labelName)
    {
        for (const auto& resource : snapshot.resources) {
            if (resource.type == typeName || resource.label == labelName) {
                const QString note = resource.note.trimmed();
                return note.isEmpty() ? QStringLiteral("none") : note;
            }
        }
        return QStringLiteral("none");
    }

    static QString mediaHealthText(const ArtifactCore::FrameDebugSnapshot& snapshot)
    {
        return QStringLiteral("media videoState=%1 particleState=%2 particleDetail=%3 blendMaskContract=%4")
                .arg(resourceStateText(snapshot, QStringLiteral("video"), QStringLiteral("Video Decode")))
                .arg(resourceStateText(snapshot, QStringLiteral("particle"), QStringLiteral("Particle Draw")))
                .arg(resourceNoteText(snapshot, QStringLiteral("particle"), QStringLiteral("Particle Draw")))
                .arg(resourceStateText(snapshot, QStringLiteral("blendMask"), QStringLiteral("Blend / Mask Contract")));
    }

    static QString ramPreviewText(ArtifactPlaybackService* playbackSvc)
    {
        if (!playbackSvc) {
            return QStringLiteral("ramPreview=<no service>");
        }
        const auto summary = playbackSvc->ramPreviewSummary();
        const auto currentFrame = playbackSvc->currentFrame().framePosition();
        const auto currentState = playbackSvc->ramPreviewFrameState(currentFrame);
        const auto currentPriority = playbackSvc->ramPreviewPriorityState(currentFrame);
        const bool currentPlayable = currentState.playable;
        const bool currentPending =
            playbackSvc->isRamPreviewFramePendingBuild(currentFrame);
        const QString currentNote = ramPreviewStatusNote(currentState);
        const QString currentPriorityBand = summary.currentPriorityBand.trimmed().isEmpty()
                                                ? QStringLiteral("unknown")
                                                : summary.currentPriorityBand.trimmed();
        const QString currentPriorityReason = summary.currentPriorityReason;
        return QStringLiteral(
                   "ramPreview playable=%1/%2 requested=%3 pending=%4 failed=%5 inRam=%6 onDisk=%7 readyMissingImage=%8 queue=%9 next=%10 active=%11 rangeReady=%12 progress=%13 playFallback=%14 gen=%15 reason=%16 hit=%17%% range=%18-%19 current=%20 playable=%21 pendingBuild=%22 currentState={requested:%23 ready:%24 image:%25 playable:%26 inRam:%27 onDisk:%28 failed:%29} note=%30 band=%31 reasonText=%32 priorityState={inComp:%33 inWork:%34 current:%35 next:%36 reverse:%37 dist:%38}")
                .arg(summary.playableFrames)
                .arg(summary.rangeFrames)
                .arg(summary.requestedFrames)
                .arg(summary.buildQueuePendingFrames)
                .arg(summary.failedFrames)
                .arg(summary.inRamFrames)
                .arg(summary.onDiskFrames)
                .arg(summary.readyMissingImageFrames)
                .arg(summary.buildQueuePendingFrames)
                .arg(summary.buildQueueNextFrame)
                .arg(summary.buildQueueActive ? 1 : 0)
                .arg(summary.buildRangeReady ? 1 : 0)
                .arg(QString::number(summary.buildRangeProgress * 100.0f, 'f', 0) + QStringLiteral("%"))
                .arg(summary.playbackFallbackWhilePlaying ? 1 : 0)
                .arg(QString::number(static_cast<qulonglong>(summary.buildQueueGeneration)))
                .arg(summary.buildQueueReason)
                .arg(QString::number(summary.hitRate * 100.0f, 'f', 1))
                .arg(summary.range.start())
                .arg(summary.range.end())
                .arg(currentFrame)
                .arg(currentPlayable ? 1 : 0)
                .arg(currentPending ? 1 : 0)
                .arg(currentState.requested ? 1 : 0)
                .arg(currentState.ready ? 1 : 0)
                .arg(currentState.imageAvailable ? 1 : 0)
                .arg(currentState.playable ? 1 : 0)
                .arg(currentState.inRam ? 1 : 0)
                .arg(currentState.onDisk ? 1 : 0)
                .arg(currentState.failed ? 1 : 0)
                .arg(currentNote)
                .arg(currentPriorityBand)
                .arg(currentPriorityReason)
                .arg(currentPriority.inCompositionRange ? 1 : 0)
                .arg(currentPriority.inWorkArea ? 1 : 0)
                .arg(currentPriority.currentFrame ? 1 : 0)
                .arg(currentPriority.nextQueued ? 1 : 0)
                .arg(currentPriority.reverse ? 1 : 0)
                .arg(currentPriority.distanceFromCurrent);
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

    static QString perfBaselineText(const ArtifactCore::FrameDebugSnapshot& snapshot,
                                    CompositionRenderController* controller)
    {
        const double lastMs = controller ? controller->lastFrameTimeMs()
                                         : snapshot.renderLastFrameMs;
        const double avgMs = controller ? controller->averageFrameTimeMs()
                                        : snapshot.renderAverageFrameMs;
        const int visibleLayers = std::max(0, snapshot.visibleLayerCount);
        const double drawPerVisibleLayer =
            visibleLayers > 0
                ? static_cast<double>(snapshot.renderCost.drawCalls) /
                      static_cast<double>(visibleLayers)
                : 0.0;
        const double bufferUpdatesPerVisibleLayer =
            visibleLayers > 0
                ? static_cast<double>(snapshot.renderCost.bufferUpdates) /
                      static_cast<double>(visibleLayers)
                : 0.0;
        const bool gpuTimerLooksActive =
            snapshot.renderGpuFrameMs > 0.001 || snapshot.renderCost.drawCalls == 0;

        QStringList lines;
        lines << QStringLiteral("Composition Editor Perf Baseline");
        lines << QStringLiteral("  frame: %1").arg(snapshot.frame.framePosition());
        lines << QStringLiteral("  composition: %1")
                     .arg(snapshot.compositionName.isEmpty()
                              ? QStringLiteral("<none>")
                              : snapshot.compositionName);
        lines << QStringLiteral("  backend: %1")
                     .arg(snapshot.renderBackend.isEmpty()
                              ? QStringLiteral("<none>")
                              : snapshot.renderBackend);
        lines << QStringLiteral("  layers: total=%1 visible=%2 text=%3")
                     .arg(snapshot.totalLayerCount)
                     .arg(snapshot.visibleLayerCount)
                     .arg(snapshot.textLayerCount);
        lines << QStringLiteral("  timingMs: last=%1 avg=%2 gpu=%3")
                     .arg(QString::number(lastMs, 'f', 2))
                     .arg(QString::number(avgMs, 'f', 2))
                     .arg(QString::number(snapshot.renderGpuFrameMs, 'f', 2));
        lines << QStringLiteral("  renderCost: draw=%1 indexed=%2 pso=%3 srb=%4 bufferUpdates=%5")
                     .arg(QString::number(static_cast<qulonglong>(snapshot.renderCost.drawCalls)))
                     .arg(QString::number(static_cast<qulonglong>(snapshot.renderCost.indexedDrawCalls)))
                     .arg(QString::number(static_cast<qulonglong>(snapshot.renderCost.psoSwitches)))
                     .arg(QString::number(static_cast<qulonglong>(snapshot.renderCost.srbCommits)))
                     .arg(QString::number(static_cast<qulonglong>(snapshot.renderCost.bufferUpdates)));
        lines << QStringLiteral("  perVisibleLayer: draw=%1 bufferUpdates=%2")
                     .arg(QString::number(drawPerVisibleLayer, 'f', 2))
                     .arg(QString::number(bufferUpdatesPerVisibleLayer, 'f', 2));
        lines << QStringLiteral("  gpuTimer: %1")
                     .arg(gpuTimerLooksActive ? QStringLiteral("active-or-idle")
                                              : QStringLiteral("not-updating"));
        lines << QStringLiteral("  nPlusOneSignal: %1")
                     .arg(visibleLayers > 0 &&
                                  snapshot.renderCost.bufferUpdates >
                                      static_cast<std::uint64_t>(visibleLayers)
                              ? QStringLiteral("bufferUpdates exceed visible layer count")
                              : QStringLiteral("not-obvious-from-snapshot"));
        return lines.join(QStringLiteral("\n"));
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

    static QString visualDensityMonitorText(const ArtifactCore::FrameDebugSnapshot& snapshot)
    {
        return QStringLiteral("level=%1 visual=%2 info=%3 luminance=%4 motion=%5")
                .arg(snapshot.densityLabel.isEmpty() ? QStringLiteral("low")
                                                    : snapshot.densityLabel)
                .arg(QString::number(snapshot.visualDensityScore, 'f', 2))
                .arg(QString::number(snapshot.informationDensityScore, 'f', 2))
                .arg(QString::number(snapshot.luminanceDensityScore, 'f', 2))
                .arg(QString::number(snapshot.motionDensityScore, 'f', 2));
    }

    static QString densityWarningText(const ArtifactCore::FrameDebugSnapshot& snapshot)
    {
        if (!snapshot.densityWarning.isEmpty()) {
            return snapshot.densityWarning;
        }
        if (snapshot.densityLabel.isEmpty()) {
            return QStringLiteral("density is readable");
        }
        if (snapshot.densityLabel == QStringLiteral("high")) {
            return QStringLiteral("density is high");
        }
        if (snapshot.densityLabel == QStringLiteral("medium")) {
            return QStringLiteral("density is moderate");
        }
        return QStringLiteral("density is readable");
    }

    static QString captureEntryLabel(const ArtifactCore::FrameDebugCapture& capture, bool isCurrent)
    {
        const auto& snapshot = capture.snapshot;
        return QStringLiteral("%1 frame=%2  comp=%3  layer=%4  backend=%5  visualDensityMonitor=%6  passes=%7  res=%8  att=%9")
                .arg(isCurrent ? QStringLiteral("[current]") : (capture.pinned ? QStringLiteral("[pinned]") : QStringLiteral("[history]")))
                .arg(snapshot.frame.framePosition())
                .arg(snapshot.compositionName.isEmpty() ? QStringLiteral("<none>") : snapshot.compositionName)
                .arg(snapshot.selectedLayerName.isEmpty() ? QStringLiteral("<none>") : snapshot.selectedLayerName)
                .arg(snapshot.renderBackend.isEmpty() ? QStringLiteral("<none>") : snapshot.renderBackend)
                .arg(snapshot.densityLabel.isEmpty() ? QStringLiteral("low") : snapshot.densityLabel)
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
                ArtifactCore::FrameDebugSnapshot emptySnapshot;
                captureDetailView_->setFrameDebugSnapshot(emptySnapshot);
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
                              .arg(pass.name.isEmpty() ? QStringLiteral("<unnamed>") : pass.name)
                              .arg(ArtifactCore::toString(pass.kind))
                              .arg(ArtifactCore::toString(pass.status))
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
                              .arg(resource.label.isEmpty() ? QStringLiteral("<unnamed>") : resource.label)
                              .arg(resource.type.isEmpty() ? QStringLiteral("<type?>") : resource.type)
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
                              .arg(attachment.name.isEmpty() ? QStringLiteral("<unnamed>") : attachment.name)
                              .arg(attachment.role.isEmpty() ? QStringLiteral("<none>") : attachment.role)
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
        if (refreshing_) {
            return;
        }
        QScopedValueRollback<bool> refreshGuard(refreshing_, true);
        refreshCaptureBundleFromDisk();
        const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
        const auto projectSvc = ArtifactProjectService::instance();
        const auto playbackSvc = ArtifactPlaybackService::instance();
        const auto queueSvc = ArtifactRenderQueueService::instance();
        const QJsonObject debugMcpState = readDebugMcpStateObject();
        const QJsonObject debugMcpSession = debugMcpState.value(QStringLiteral("session")).toObject();
        const QJsonArray debugMcpBreakConditions =
            debugMcpState.value(QStringLiteral("breakConditions")).toArray();
        const QJsonArray debugMcpProperties =
            debugMcpState.value(QStringLiteral("properties")).toArray();
        const QJsonArray debugMcpHistory =
            debugMcpState.value(QStringLiteral("history")).toArray();
        const QJsonArray debugMcpHistorySummary =
            debugMcpState.value(QStringLiteral("summary")).toArray();
        const QJsonObject debugMcpSessionSummary =
            debugMcpState.value(QStringLiteral("sessionSummary")).toObject();
        const QString debugMcpStatus = debugMcpStatusSummaryText(debugMcpState);
        const QString debugMcpHit = debugMcpLastHitText(debugMcpState);
        const QString debugMcpConditionPreview =
            debugMcpBreakConditionsPreviewText(debugMcpBreakConditions);
        const QString debugMcpPropertyPreview =
            debugMcpPropertyPreviewText(debugMcpProperties);
        const QString debugMcpSummaryPreview =
            debugMcpSummaryPreviewText(debugMcpHistory, debugMcpHistorySummary);

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
            lines << QStringLiteral("Pseudo Breakpoints");
            lines << QStringLiteral("mode: %1")
                          .arg(debugMcpSessionSummary.value(QStringLiteral("mode")).toString().isEmpty()
                                   ? (debugMcpSessionSummary.value(QStringLiteral("source")).toString() == QStringLiteral("bridge")
                                          ? QStringLiteral("live")
                                          : QStringLiteral("mock"))
                                   : debugMcpSessionSummary.value(QStringLiteral("mode")).toString());
            if (debugMcpSessionSummary.value(QStringLiteral("mode")).toString() == QStringLiteral("live") ||
                debugMcpSessionSummary.value(QStringLiteral("source")).toString() == QStringLiteral("bridge")) {
                lines << QStringLiteral("bridgePath: %1")
                              .arg(debugMcpSessionSummary.value(QStringLiteral("bridgePath")).toString().isEmpty()
                                       ? QStringLiteral("<none>")
                                       : debugMcpSessionSummary.value(QStringLiteral("bridgePath")).toString());
            }
            lines << QStringLiteral("lastHit: %1").arg(debugMcpLastHitText(debugMcpState));
            lines << QStringLiteral("lastAction: %1")
                          .arg(debugMcpSessionSummary.value(QStringLiteral("lastAction")).toString().isEmpty()
                                   ? QStringLiteral("<none>")
                                   : debugMcpSessionSummary.value(QStringLiteral("lastAction")).toString());
            lines << QStringLiteral("stateFile: %1").arg(debugMcpStateFilePath());
            lines << QStringLiteral("counts: breakConditions=%1  history=%2")
                          .arg(debugMcpBreakConditions.size())
                          .arg(debugMcpHistory.size());
            lines << QStringLiteral("conditionsPreview:");
            lines << debugMcpConditionPreview;
            lines << QStringLiteral("propertySnapshot: %1").arg(debugMcpProperties.size());
            lines << QStringLiteral("propertiesPreview:");
            lines << debugMcpPropertyPreview;
            lines << QStringLiteral("summaryPreview:");
            lines << debugMcpSummaryPreview;
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
            lines << QStringLiteral("visualDensityMonitor: %1").arg(visualDensityMonitorText(controllerSnapshot));
            lines << QStringLiteral("densityWarning: %1").arg(densityWarningText(controllerSnapshot));
            lines << QStringLiteral("densityNext: %1")
                          .arg(controllerSnapshot.densityNextAction.isEmpty()
                                   ? QStringLiteral("<none>")
                                   : controllerSnapshot.densityNextAction);
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
            const QString recentActionText = debugMcpSessionSummary.value(QStringLiteral("recentAction")).toString().isEmpty()
                                                 ? debugMcpSessionSummary.value(QStringLiteral("lastAction")).toString()
                                                 : debugMcpSessionSummary.value(QStringLiteral("recentAction")).toString();
            const QString priorRecentActionText = debugMcpSessionSummary.value(QStringLiteral("priorRecentAction")).toString().trimmed();
            const QString breakHistorySummaryText = debugMcpSessionSummary.value(QStringLiteral("breakHistorySummary")).toString().trimmed();
            const QString densityWarning = densityWarningText(controllerSnapshot);
            const QString nextText = controllerSnapshot.densityNextAction.isEmpty()
                                         ? QStringLiteral("Capture a frame when behavior changes.")
                                         : controllerSnapshot.densityNextAction;
            stateSummary_->setText(QStringLiteral("NOW  %1 / %2 / Frame %3 / %4\nWARNING  %5    NEXT  %6")
                                       .arg(compositionText, layerText)
                                       .arg(controllerSnapshot.frame.framePosition())
                                       .arg(playbackText)
                                       .arg(densityWarning == QStringLiteral("none") ? QStringLiteral("None") : densityWarning)
                                       .arg(nextText));
            if (stateContextValue_) {
                stateContextValue_->setText(QStringLiteral("%1 / Frame %2")
                                                 .arg(compositionText)
                                                 .arg(controllerSnapshot.frame.framePosition()));
                stateContextDetail_->setText(QStringLiteral("Project: %1\nLayer: %2")
                                                  .arg(projectText, layerText));
            }
            if (statePlaybackValue_) {
                statePlaybackValue_->setText(playbackText.toUpper());
                statePlaybackDetail_->setText(QStringLiteral("Quality: %1\nDropped frames: %2")
                                                   .arg(qualityText)
                                                   .arg(playbackSvc ? playbackSvc->droppedFrameCount() : 0));
            }
            if (stateRenderValue_) {
                stateRenderValue_->setText(QStringLiteral("%1 ms average")
                                                .arg(QString::number(controllerSnapshot.renderAverageFrameMs, 'f', 1)));
                stateRenderDetail_->setText(QStringLiteral("Last: %1 ms  •  Backend: %2\nQueue: %3 job(s)")
                                                 .arg(QString::number(controllerSnapshot.renderLastFrameMs, 'f', 1),
                                                      backendText, queueText));
            }
            if (stateAttentionValue_) {
                const bool hasWarning = densityWarning != QStringLiteral("none");
                stateAttentionValue_->setText(hasWarning ? QStringLiteral("ATTENTION") : QStringLiteral("READY"));
                stateAttentionDetail_->setText(hasWarning ? densityWarning + QStringLiteral("\nNext: ") + nextText
                                                           : QStringLiteral("No current diagnostic warning.\nNext: ") + nextText);
                QPalette attentionPalette = stateAttentionValue_->palette();
                attentionPalette.setColor(QPalette::WindowText,
                                          hasWarning ? QColor::fromRgb(255, 196, 92)
                                                     : QColor::fromRgb(106, 218, 148));
                stateAttentionValue_->setPalette(attentionPalette);
            }
            if (!breakHistorySummaryText.isEmpty()) {
                stateSummary_->setToolTip(QStringLiteral("breakHistorySummary=%1\nvisualDensityMonitor=%2  warning=%3  next=%4  status=%5  lastHit=%6  act=%7")
                                             .arg(breakHistorySummaryText)
                                             .arg(visualDensityMonitorText(controllerSnapshot))
                                             .arg(densityWarningText(controllerSnapshot))
                                             .arg(controllerSnapshot.densityNextAction.isEmpty()
                                                      ? QStringLiteral("<none>")
                                                      : controllerSnapshot.densityNextAction)
                                             .arg(debugMcpStatus, debugMcpHit, recentActionText));
            } else if (!controllerSnapshot.densityLabel.isEmpty() ||
                       !controllerSnapshot.densityWarning.isEmpty()) {
                stateSummary_->setToolTip(QStringLiteral("visualDensityMonitor=%1  warning=%2  next=%3  status=%4  lastHit=%5  act=%6")
                                             .arg(visualDensityMonitorText(controllerSnapshot))
                                             .arg(densityWarningText(controllerSnapshot))
                                             .arg(controllerSnapshot.densityNextAction.isEmpty()
                                                      ? QStringLiteral("<none>")
                                                      : controllerSnapshot.densityNextAction)
                                             .arg(debugMcpStatus, debugMcpHit, recentActionText));
            } else {
                stateSummary_->setToolTip(QStringLiteral("status=%1  lastHit=%2  act=%3")
                                              .arg(debugMcpStatus,
                                                   debugMcpHit,
                                                   recentActionText));
            }
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
                playbackSummary_->setText(QStringLiteral("NOW  %1 at Frame %2    •    %3\nUse the cards for health; use the raw snapshot only when diagnosing a regression.")
                                              .arg(playbackText)
                                              .arg(controllerSnapshot.frame.framePosition())
                                              .arg(renderTiming));
                playbackSummary_->setToolTip(QStringLiteral("%1  %2")
                                                 .arg(playbackQuality, playbackDiagnosticsText(playbackSvc, controllerSnapshot, controller_)));
            }
            if (playbackStateValue_) {
                playbackStateValue_->setText(playbackText.toUpper());
                playbackStateDetail_->setText(QStringLiteral("Frame: %1\nAudio offset: %2 s  •  Dropped: %3")
                                                   .arg(controllerSnapshot.frame.framePosition())
                                                   .arg(playbackSvc ? QString::number(playbackSvc->audioOffsetSeconds(), 'f', 3)
                                                                    : QStringLiteral("-"))
                                                   .arg(playbackSvc ? playbackSvc->droppedFrameCount() : 0));
            }
            if (playbackCacheValue_) {
                playbackCacheValue_->setText(cacheHealthText(controllerSnapshot));
                playbackCacheDetail_->setText(QStringLiteral("%1\nQuality: %2")
                                                   .arg(ramPreviewText(playbackSvc), playbackQuality));
            }
            if (playbackRenderValue_) {
                playbackRenderValue_->setText(QStringLiteral("%1 ms")
                                                   .arg(QString::number(controllerSnapshot.renderLastFrameMs, 'f', 1)));
                playbackRenderDetail_->setText(QStringLiteral("Average: %1 ms\nBackend: %2")
                                                    .arg(QString::number(controllerSnapshot.renderAverageFrameMs, 'f', 1),
                                                         controllerSnapshot.renderBackend.isEmpty()
                                                             ? QStringLiteral("<none>")
                                                             : controllerSnapshot.renderBackend));
            }
            if (playbackQueueValue_) {
                playbackQueueValue_->setText(QStringLiteral("%1 job(s)")
                                                 .arg(queueSvc ? queueSvc->jobCount() : 0));
                playbackQueueDetail_->setText(QStringLiteral("%1")
                                                  .arg(poolText));
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
            QString lastCrashText = QStringLiteral("<none>");
            if (!trace.crashes.empty()) {
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
            const QString projectHealthText = projectSvc
                                                  ? projectSvc->currentProjectHealthStateToken()
                                                  : QStringLiteral("<no service>");
            QString warningText = QStringLiteral("none");
            if (controllerSnapshot.failed) {
                warningText = QStringLiteral("frame failed");
            } else if (failedPasses > 0) {
                warningText = QStringLiteral("%1 failed passes").arg(failedPasses);
            } else if (projectHealthText == QStringLiteral("error")) {
                warningText = QStringLiteral("project health error");
            } else if (projectHealthText == QStringLiteral("warning")) {
                warningText = QStringLiteral("project health warning");
            } else if (!trace.crashes.empty()) {
                warningText = QStringLiteral("recent crash: %1").arg(lastCrashText);
            } else if (!controllerSnapshot.densityLabel.isEmpty()) {
                warningText = densityWarningText(controllerSnapshot);
            }
            const QString nextText = warningText == QStringLiteral("none")
                                         ? QStringLiteral("capture frame when behavior changes")
                                         : QStringLiteral("open the relevant diagnostic tab");
            overviewSummary_->setText(QStringLiteral("NOW  %1 / %2 / Frame %3 / %4 / %5\nWARNING  %6    NEXT  %7")
                                          .arg(compositionText, layerText)
                                          .arg(controllerSnapshot.frame.framePosition())
                                          .arg(playbackText, backendText)
                                          .arg(warningText == QStringLiteral("none") ? QStringLiteral("None") : warningText)
                                          .arg(nextText));
            overviewSummary_->setToolTip(QStringLiteral("failedPasses=%1 totalPassUs=%2 queueJobs=%3 traceThreads=%4 bundle=%5")
                                             .arg(failedPasses)
                                             .arg(totalPassUs)
                                             .arg(queueSvc ? queueSvc->jobCount() : 0)
                                             .arg(static_cast<int>(trace.threads.size()))
                                             .arg(hasCaptureBundle_ ? captureBundle_.bundleId : QStringLiteral("<none>")));
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
            const QString compareText = controllerSnapshot.compareMode == ArtifactCore::FrameDebugCompareMode::Disabled
                                            ? QStringLiteral("off")
                                            : QStringLiteral("%1%2")
                                                  .arg(ArtifactCore::toString(controllerSnapshot.compareMode))
                                                  .arg(controllerSnapshot.compareTargetId.isEmpty()
                                                           ? QString()
                                                           : QStringLiteral(" -> %1").arg(controllerSnapshot.compareTargetId));
            captureSummary_->setText(QStringLiteral("frame=%1  composition=%2  layer=%3  backend=%4  compare=%5  passes=%6  resources=%7  attachments=%8  traceEvents=%9  bundle=%10")
                                         .arg(controllerSnapshot.frame.framePosition())
                                         .arg(compositionText)
                                         .arg(layerText)
                                         .arg(backendText)
                                         .arg(QStringLiteral("%1  visualDensityMonitor=%2")
                                                  .arg(compareText, visualDensityMonitorText(controllerSnapshot)))
                                         .arg(static_cast<int>(controllerSnapshot.passes.size()))
                                         .arg(static_cast<int>(controllerSnapshot.resources.size()))
                                         .arg(static_cast<int>(controllerSnapshot.attachments.size()))
                                         .arg(static_cast<int>(trace.events.size()))
                                         .arg(hasCaptureBundle_ ? captureBundle_.bundleId : QStringLiteral("<none>")));
            captureSummary_->setToolTip(QStringLiteral("history=%1  currentCapture=%2  bundlePath=%3  updated=%4")
                                            .arg(hasCaptureBundle_ ? static_cast<int>(captureBundle_.history.size()) : 0)
                                            .arg(hasCaptureBundle_ ? captureBundle_.capture.captureId : QStringLiteral("<none>"))
                                            .arg(captureBundlePath_.isEmpty() ? QStringLiteral("<unset>") : captureBundlePath_)
                                            .arg(captureBundleStampMs_ > 0 ? QString::number(captureBundleStampMs_) : QStringLiteral("<unset>")));
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
            persistCaptureBundle();
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

            if (!trace.threads.empty()) {
                auto hotThreads = trace.threads;
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

            if (!trace.locks.empty()) {
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

            if (!trace.crashes.empty()) {
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
                              .arg(pass.name)
                              .arg(ArtifactCore::toString(pass.kind))
                              .arg(ArtifactCore::toString(pass.status))
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

            frameSummary_->setText(QStringLiteral("Frame %1 | %2 | %3 | visualDensityMonitor=%4 | passes=%5 resources=%6 attachments=%7 totalPassUs=%8")
                                       .arg(frameIndex)
                                       .arg(controllerSnapshot.compositionName.isEmpty()
                                                ? QStringLiteral("<no composition>")
                                                : controllerSnapshot.compositionName)
                                       .arg(compareText)
                                       .arg(visualDensityMonitorText(controllerSnapshot))
                                       .arg(passCount)
                                       .arg(resourceCount)
                                       .arg(attachmentCount)
                                       .arg(totalPassUs));
            frameSummary_->setToolTip(QStringLiteral("%1\nfailedPasses=%2\n%3\nnext=%4")
                                          .arg(hint)
                                          .arg(failedPasses)
                                          .arg(mediaHealthText(controllerSnapshot))
                                          .arg(controllerSnapshot.densityNextAction.isEmpty()
                                                   ? QStringLiteral("<none>")
                                                   : controllerSnapshot.densityNextAction));
        }

        if (harnessWidget_) {
            harnessWidget_->setFrameDebugSnapshot(controllerSnapshot);
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
            if (!trace.threads.empty()) {
                auto hotThreads = trace.threads;
                std::sort(hotThreads.begin(), hotThreads.end(), [](const auto& a, const auto& b) {
                    if (a.lockDepth == b.lockDepth) {
                        return a.lockCount > b.lockCount;
                    }
                    return a.lockDepth > b.lockDepth;
                });
                focusThreadId = hotThreads.front().threadId;
            }
            if (!trace.locks.empty()) {
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
                    focusMutexName = rows.front().name;
                }
            }
            captureTraceTimelineView_->setFocusedThreadId(focusThreadId);
            captureTraceTimelineView_->setFocusedMutexName(focusMutexName);
            captureTraceTimelineView_->setTraceSnapshot(trace);
        }

        if (traceTimelineView_) {
            std::uint64_t focusThreadId = 0;
            QString focusMutexName;
            if (!trace.threads.empty()) {
                auto hotThreads = trace.threads;
                std::sort(hotThreads.begin(), hotThreads.end(), [](const auto& a, const auto& b) {
                    if (a.lockDepth == b.lockDepth) {
                        return a.lockCount > b.lockCount;
                    }
                    return a.lockDepth > b.lockDepth;
                });
                focusThreadId = hotThreads.front().threadId;
            }
            if (!trace.locks.empty()) {
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
                    focusMutexName = rows.front().name;
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
            lines << motionTrackerSummaryText();
            if (projectSvc) {
                const auto projectHealth = projectSvc->currentProjectDiagnostics();
                const int projectErrorCount = static_cast<int>(std::count_if(
                    projectHealth.begin(), projectHealth.end(),
                    [](const auto& diagnostic) { return diagnostic.isError(); }));
                const int projectWarningCount = static_cast<int>(std::count_if(
                    projectHealth.begin(), projectHealth.end(),
                    [](const auto& diagnostic) { return diagnostic.isWarning(); }));
                lines << QStringLiteral("projectHealth: %1/%2")
                              .arg((projectErrorCount > 0 || projectWarningCount > 0)
                                       ? QStringLiteral("issues")
                                       : QStringLiteral("healthy"))
                              .arg(projectErrorCount + projectWarningCount);
            } else {
                lines << QStringLiteral("projectHealth: <no service>");
            }
            if (controllerSnapshot.failed && !controllerSnapshot.failureReason.isEmpty()) {
                lines << QStringLiteral("failureReason: %1").arg(controllerSnapshot.failureReason);
            }
            diagnosticsText_->setPlainText(lines.join(QStringLiteral("\n")));
        }

        if (diagnosticsSummary_) {
            QString lastCrashText = QStringLiteral("<none>");
            if (!trace.crashes.empty()) {
                const auto& crash = trace.crashes.back();
                lastCrashText = crash.summary.isEmpty() ? QStringLiteral("<no-summary>") : crash.summary.left(48);
            }
            QString summaryText = QStringLiteral("traceFrames=%1  traceEvents=%2  crashes=%3  openLocks=%4  queueJobs=%5  playback=%6")
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
                                      .arg(playbackSvc ? playbackStateText(playbackSvc->state()) : QStringLiteral("<no service>"));
            summaryText += QStringLiteral("  tracker=%1")
                               .arg(ArtifactCore::TrackerManager::instance().trackerCount());
            diagnosticsSummary_->setText(summaryText);
            diagnosticsSummary_->setToolTip(QStringLiteral("lastCrash=%1\n%2\nprojectHealth=%3\n%4")
                                                .arg(lastCrashText)
                                                .arg(mediaHealthText(controllerSnapshot))
                                                .arg(projectSvc ? projectSvc->currentProjectHealthSummaryText()
                                                               : QStringLiteral("Status: <no service>"))
                                                .arg(motionTrackerSummaryText()));
        }

        if (exportText_) {
            QStringList lines;
            lines << QStringLiteral("App Debug Export");
            lines << QStringLiteral("shareableSummary:");
            lines << QStringLiteral("  frame: %1").arg(controllerSnapshot.frame.framePosition());
            lines << QStringLiteral("  composition: %1")
                          .arg(controllerSnapshot.compositionName.isEmpty() ? QStringLiteral("<none>")
                                                                            : controllerSnapshot.compositionName);
            lines << QStringLiteral("  playback: %1")
                          .arg(playbackSvc ? playbackStateText(playbackSvc->state()) : QStringLiteral("<no service>"));
            lines << QStringLiteral("  compare: %1")
                          .arg(ArtifactCore::toString(controllerSnapshot.compareMode));
            lines << QStringLiteral("  compareTarget: %1")
                          .arg(controllerSnapshot.compareTargetId.isEmpty()
                                   ? QStringLiteral("<none>")
                                   : controllerSnapshot.compareTargetId);
            lines << QStringLiteral("  traceFrames: %1").arg(static_cast<int>(trace.frames.size()));
            lines << QStringLiteral("  traceEvents: %1").arg(static_cast<int>(trace.events.size()));
            lines << QStringLiteral("  crashes: %1").arg(static_cast<int>(trace.crashes.size()));
            lines << QStringLiteral("  captureBundle: %1")
                          .arg(hasCaptureBundle_ ? captureBundle_.bundleId : QStringLiteral("<none>"));
            lines << QStringLiteral("  captureHistory: %1")
                          .arg(hasCaptureBundle_ ? static_cast<int>(captureBundle_.history.size()) : 0);
            lines << QStringLiteral("  particleState: %1")
                          .arg(resourceStateText(controllerSnapshot, QStringLiteral("particle"), QStringLiteral("Particle Draw")));
            lines << QStringLiteral("  particleDetail: %1")
                          .arg(resourceNoteText(controllerSnapshot, QStringLiteral("particle"), QStringLiteral("Particle Draw")));
            lines << QStringLiteral("  textState: %1")
                          .arg(resourceStateText(controllerSnapshot, QStringLiteral("text"), QStringLiteral("Glyph Atlas")));
            lines << QStringLiteral("  videoState: %1")
                          .arg(resourceStateText(controllerSnapshot, QStringLiteral("video"), QStringLiteral("Video Decode")));
            const QString blendState =
                resourceStateText(controllerSnapshot, QStringLiteral("blendMask"), QStringLiteral("Blend / Mask Contract"));
            lines << QStringLiteral("  blendState: %1")
                          .arg(blendState != QStringLiteral("none")
                                   ? blendState
                                   : resourceStateText(controllerSnapshot, QStringLiteral("composition"), QStringLiteral("Render Path")));
            lines << QStringLiteral("  blendMaskContract: %1")
                          .arg(resourceStateText(controllerSnapshot,
                                                 QStringLiteral("blendMask"),
                                                 QStringLiteral("Blend / Mask Contract")));
            lines << QStringLiteral("  glyphState: %1")
                          .arg(resourceStateText(controllerSnapshot, QStringLiteral("glyphAtlas"), QStringLiteral("Glyph Atlas")));
            lines << QStringLiteral("  bundlePath: %1")
                          .arg(captureBundlePath_.isEmpty() ? QStringLiteral("<unset>") : captureBundlePath_);
            lines << QStringLiteral("  bundlePresent: %1")
                          .arg(hasCaptureBundle_ ? QStringLiteral("true") : QStringLiteral("false"));
            lines << QStringLiteral("  bundleHistory: %1")
                          .arg(hasCaptureBundle_ ? static_cast<int>(captureBundle_.history.size()) : 0);
            lines << QStringLiteral("  bundleId: %1")
                          .arg(hasCaptureBundle_ ? captureBundle_.bundleId : QStringLiteral("<none>"));
            lines << QString();
            lines << QStringLiteral("sharingNotes:");
            lines << QStringLiteral("  - bundlePath is the file to hand off for reproduction");
            lines << QStringLiteral("  - captureHistory is newest-last and selectedRow is reflected in the UI");
            lines << QStringLiteral("  - the JSON blocks below are machine-readable, but the summary above is the preferred paste target");
            lines << QString();
            lines << perfBaselineText(controllerSnapshot, controller_);
            lines << QString();
            if (hasCaptureBundle_) {
                lines << QStringLiteral("CaptureBundle JSON:");
                lines << QString::fromUtf8(QJsonDocument(captureBundle_.toJson()).toJson(QJsonDocument::Indented));
                lines << QString();
            }
            lines << QStringLiteral("FrameDebugSnapshot JSON:");
            lines << QString::fromUtf8(QJsonDocument(controllerSnapshot.toJson()).toJson(QJsonDocument::Indented));
            lines << QString();
            lines << QStringLiteral("TraceSnapshot JSON:");
            lines << QString::fromUtf8(QJsonDocument(ArtifactCore::toJson(trace)).toJson(QJsonDocument::Indented));
            exportText_->setPlainText(lines.join(QStringLiteral("\n")));
        }

        if (exportSummary_) {
            QString crashText = QStringLiteral("<none>");
            if (!trace.crashes.empty()) {
                crashText = trace.crashes.back().summary.isEmpty() ? QStringLiteral("<no-summary>")
                                                                  : trace.crashes.back().summary.left(48);
            }
            exportSummary_->setText(QStringLiteral("ready to copy: frame=%1  traceEvents=%2  crashes=%3  compare=%4  bundle=%5")
                                        .arg(controllerSnapshot.frame.framePosition())
                                        .arg(static_cast<int>(trace.events.size()))
                                        .arg(static_cast<int>(trace.crashes.size()))
                                        .arg(ArtifactCore::toString(controllerSnapshot.compareMode))
                                        .arg(hasCaptureBundle_ ? captureBundle_.bundleId : QStringLiteral("<none>")));
            exportSummary_->setToolTip(QStringLiteral("latestCrash=%1  history=%2  path=%3  updated=%4")
                                           .arg(crashText)
                                           .arg(hasCaptureBundle_ ? static_cast<int>(captureBundle_.history.size()) : 0)
                                           .arg(captureBundlePath_.isEmpty() ? QStringLiteral("<unset>") : captureBundlePath_)
                                           .arg(captureBundleStampMs_ > 0 ? QString::number(captureBundleStampMs_) : QStringLiteral("<unset>")));
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

void AppDebuggerWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));
}

void AppDebuggerWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    applyDebuggerSurfacePalette(this, palette());
    if (impl_) {
        impl_->refresh();
    }
}

void AppDebuggerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

} // namespace Artifact
