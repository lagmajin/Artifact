module;
#include <algorithm>
#include <QColor>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPalette>
#include <QVBoxLayout>
#include <QStringList>
#include <wobjectimpl.h>

module Artifact.Widgets.FrameDebugViewWidget;

import Core.Diagnostics.Trace;
import Frame.Debug;

namespace Artifact {

W_OBJECT_IMPL(FrameDebugViewWidget)

class FrameDebugViewWidget::Impl {
public:
    FrameDebugViewWidget* owner_;
    QLabel* summary_ = nullptr;
    QPlainTextEdit* text_ = nullptr;

    explicit Impl(FrameDebugViewWidget* owner) : owner_(owner) {}

    void setupUI() {
        auto* layout = new QVBoxLayout(owner_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        summary_ = new QLabel(owner_);
        summary_->setWordWrap(true);
        summary_->setMargin(8);
        summary_->setAutoFillBackground(true);
        layout->addWidget(summary_);

        text_ = new QPlainTextEdit(owner_);
        text_->setReadOnly(true);
        text_->setLineWrapMode(QPlainTextEdit::NoWrap);
        layout->addWidget(text_);
    }

    void showFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot) {
        if (!text_) {
            return;
        }

        const int failedPassCount = static_cast<int>(std::count_if(
            snapshot.passes.begin(), snapshot.passes.end(),
            [](const auto& pass) { return pass.status == ArtifactCore::FrameDebugPassStatus::Failed; }));
        const int warningResourceCount = static_cast<int>(std::count_if(
            snapshot.resources.begin(), snapshot.resources.end(),
            [](const auto& resource) { return resource.stale || !resource.cacheHit; }));
        const QString statusText =
            snapshot.failed
                ? QStringLiteral("Failed")
                : (failedPassCount > 0 ? QStringLiteral("Degraded") : QStringLiteral("Healthy"));
        if (summary_) {
            const QString summaryText = QStringLiteral("%1 | frame %2 | comp %3 | layer %4 | backend %5 | passes %6 | resources %7")
                                            .arg(statusText)
                                            .arg(snapshot.frame.framePosition())
                                            .arg(snapshot.compositionName.isEmpty() ? QStringLiteral("<none>") : snapshot.compositionName)
                                            .arg(snapshot.selectedLayerName.isEmpty() ? QStringLiteral("<none>") : snapshot.selectedLayerName)
                                            .arg(snapshot.renderBackend.isEmpty() ? QStringLiteral("<none>") : snapshot.renderBackend)
                                            .arg(static_cast<int>(snapshot.passes.size()))
                                            .arg(static_cast<int>(snapshot.resources.size()));
            summary_->setText(summaryText);
            QPalette pal = summary_->palette();
            pal.setColor(QPalette::Window,
                         snapshot.failed ? QColor(54, 18, 18)
                                         : (failedPassCount > 0 ? QColor(50, 40, 18)
                                                                : QColor(24, 30, 36)));
            pal.setColor(QPalette::WindowText, snapshot.failed ? QColor(255, 210, 210)
                                                               : QColor(232, 238, 244));
            summary_->setPalette(pal);
        }

        QStringList lines;
        lines << QStringLiteral("Frame Debug View");
        lines << QStringLiteral("frame: %1").arg(snapshot.frame.framePosition());
        lines << QStringLiteral("timestampMs: %1").arg(snapshot.timestampMs);
        lines << QStringLiteral("composition: %1").arg(snapshot.compositionName);
        lines << QStringLiteral("selectedLayer: %1").arg(snapshot.selectedLayerName);
        lines << QStringLiteral("playbackState: %1").arg(snapshot.playbackState);
        lines << QStringLiteral("renderBackend: %1").arg(snapshot.renderBackend);
        lines << QStringLiteral("renderLastFrameMs: %1").arg(QString::number(snapshot.renderLastFrameMs, 'f', 1));
        lines << QStringLiteral("renderAverageFrameMs: %1").arg(QString::number(snapshot.renderAverageFrameMs, 'f', 1));
        lines << QStringLiteral("renderGpuFrameMs: %1").arg(QString::number(snapshot.renderGpuFrameMs, 'f', 1));
        lines << QStringLiteral("renderCost: draw=%1 indexed=%2 pso=%3 srb=%4 buf=%5")
                      .arg(static_cast<qulonglong>(snapshot.renderCost.drawCalls))
                      .arg(static_cast<qulonglong>(snapshot.renderCost.indexedDrawCalls))
                      .arg(static_cast<qulonglong>(snapshot.renderCost.psoSwitches))
                      .arg(static_cast<qulonglong>(snapshot.renderCost.srbCommits))
                      .arg(static_cast<qulonglong>(snapshot.renderCost.bufferUpdates));
        lines << QStringLiteral("failed: %1").arg(snapshot.failed ? QStringLiteral("true") : QStringLiteral("false"));
        if (!snapshot.failureReason.isEmpty()) {
            lines << QStringLiteral("failureReason: %1").arg(snapshot.failureReason);
        }
        lines << QStringLiteral("compareMode: %1").arg(ArtifactCore::toString(snapshot.compareMode));
        if (!snapshot.compareTargetId.isEmpty()) {
            lines << QStringLiteral("compareTarget: %1").arg(snapshot.compareTargetId);
        }

        lines << QString();
        lines << QStringLiteral("Highlights:");
        if (snapshot.failed) {
            lines << QStringLiteral("  - snapshot failed");
            if (!snapshot.failureReason.isEmpty()) {
                lines << QStringLiteral("    reason: %1").arg(snapshot.failureReason);
            }
        }
        if (failedPassCount > 0) {
            lines << QStringLiteral("  - failed passes: %1").arg(failedPassCount);
        }
        if (warningResourceCount > 0) {
            lines << QStringLiteral("  - warning resources: %1").arg(warningResourceCount);
        }

        lines << QString();
        lines << QStringLiteral("Passes:");
        if (snapshot.passes.empty()) {
            lines << QStringLiteral("  <none>");
        } else {
            auto passOrder = snapshot.passes;
            std::stable_sort(passOrder.begin(), passOrder.end(), [](const auto& a, const auto& b) {
                if (a.status == ArtifactCore::FrameDebugPassStatus::Failed &&
                    b.status != ArtifactCore::FrameDebugPassStatus::Failed) {
                    return true;
                }
                if (a.status != ArtifactCore::FrameDebugPassStatus::Failed &&
                    b.status == ArtifactCore::FrameDebugPassStatus::Failed) {
                    return false;
                }
                return static_cast<int>(a.kind) < static_cast<int>(b.kind);
            });
            for (const auto& pass : passOrder) {
                lines << QStringLiteral("  - %1 [%2/%3] inputs=%4 outputs=%5 durationUs=%6")
                              .arg(pass.name,
                                   ArtifactCore::toString(pass.kind),
                                   ArtifactCore::toString(pass.status))
                              .arg(pass.inputs.size())
                              .arg(pass.outputs.size())
                              .arg(pass.durationUs);
                if (!pass.note.isEmpty()) {
                    lines << QStringLiteral("    note: %1").arg(pass.note);
                }
            }
        }

        lines << QString();
        lines << QStringLiteral("Resources:");
        if (snapshot.resources.empty()) {
            lines << QStringLiteral("  <none>");
        } else {
            auto resourceOrder = snapshot.resources;
            std::stable_sort(resourceOrder.begin(), resourceOrder.end(), [](const auto& a, const auto& b) {
                const bool aHot = a.stale || !a.cacheHit;
                const bool bHot = b.stale || !b.cacheHit;
                if (aHot != bHot) {
                    return aHot;
                }
                return a.label < b.label;
            });
            for (const auto& resource : resourceOrder) {
                const bool hot = resource.stale || !resource.cacheHit;
                lines << QStringLiteral("  - %1 [%2] relation=%3 cacheHit=%4 stale=%5")
                              .arg(resource.label.isEmpty() ? QStringLiteral("<unnamed>") : resource.label,
                                   resource.type.isEmpty() ? QStringLiteral("<type?>") : resource.type,
                                   resource.relation.isEmpty() ? QStringLiteral("<none>") : resource.relation)
                              .arg(resource.cacheHit ? QStringLiteral("true") : QStringLiteral("false"))
                              .arg(resource.stale ? QStringLiteral("true") : QStringLiteral("false"));
                if (hot && !resource.note.isEmpty()) {
                    lines << QStringLiteral("    note: %1").arg(resource.note);
                }
                if (!resource.note.isEmpty()) {
                    if (!hot) {
                        lines << QStringLiteral("    note: %1").arg(resource.note);
                    }
                }
            }
        }

        lines << QString();
        lines << QStringLiteral("Attachments:");
        if (snapshot.attachments.empty()) {
            lines << QStringLiteral("  <none>");
        } else {
            for (const auto& attachment : snapshot.attachments) {
                lines << QStringLiteral("  - %1 [%2] readOnly=%3 texture=%4x%5")
                              .arg(attachment.name, attachment.role)
                              .arg(attachment.readOnly ? QStringLiteral("true") : QStringLiteral("false"))
                              .arg(attachment.texture.width)
                              .arg(attachment.texture.height);
            }
        }

        const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
        lines << QString();
        lines << QStringLiteral("Trace:");
        lines << QStringLiteral("  frames=%1 scopes=%2 locks=%3 crashes=%4")
                      .arg(static_cast<int>(trace.frames.size()))
                      .arg(static_cast<int>(trace.scopes.size()))
                      .arg(static_cast<int>(trace.locks.size()))
                      .arg(static_cast<int>(trace.crashes.size()));
        if (!trace.frames.empty()) {
            const auto& frame = trace.frames.back();
            lines << QStringLiteral("  lastFrame=%1 lanes=%2 spanNs=%3")
                          .arg(frame.frameIndex)
                          .arg(static_cast<int>(frame.lanes.size()))
                          .arg(frame.frameEndNs - frame.frameStartNs);
        }
        lines << QStringLiteral("  threads=%1").arg(static_cast<int>(trace.threads.size()));
        const int threadRows = std::min(static_cast<int>(trace.threads.size()), 4);
        for (int i = 0; i < threadRows; ++i) {
            const auto& thread = trace.threads[i];
            lines << QStringLiteral("    - %1 [0x%2] sc=%3 lk=%4 cr=%5")
                          .arg(thread.threadName)
                          .arg(QString::number(static_cast<unsigned long long>(thread.threadId), 16))
                          .arg(thread.scopeCount)
                          .arg(thread.lockCount)
                          .arg(thread.crashCount);
        }

        text_->setPlainText(lines.join(QStringLiteral("\n")));
    }
};

FrameDebugViewWidget::FrameDebugViewWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this)) {
    impl_->setupUI();
}

FrameDebugViewWidget::~FrameDebugViewWidget() {
    delete impl_;
}

void FrameDebugViewWidget::setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot) {
    if (!impl_) {
        return;
    }
    impl_->showFrameDebugSnapshot(snapshot);
}

} // namespace Artifact
