module;
#include <algorithm>
#include <array>
#include <QColor>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPalette>
#include <QVBoxLayout>
#include <QStringList>
#include <wobjectimpl.h>
#include <QShowEvent>

module Artifact.Widgets.FrameDebugViewWidget;

import Core.Diagnostics.Trace;
import Frame.Debug;
import Widgets.Utils.CSS;
import Widgets.StyleSurface;

namespace Artifact {

W_OBJECT_IMPL(FrameDebugViewWidget)

class FrameDebugViewWidget::Impl {
public:
    FrameDebugViewWidget* owner_;
    QLabel* summary_ = nullptr;
    QPlainTextEdit* text_ = nullptr;

    explicit Impl(FrameDebugViewWidget* owner) : owner_(owner) {}

    static const ArtifactCore::FrameDebugResourceRecord* findResource(
        const ArtifactCore::FrameDebugSnapshot& snapshot, const QString& label)
    {
        for (const auto& resource : snapshot.resources) {
            if (resource.label == label) {
                return &resource;
            }
        }
        return nullptr;
    }

    static QStringList buildDifferenceLeakageNotes(
        const ArtifactCore::FrameDebugSnapshot& snapshot)
    {
        QStringList notes;
        notes << QStringLiteral("differenceMode=%1")
                     .arg(ArtifactCore::toString(snapshot.compareMode));
        if (!snapshot.compareTargetId.isEmpty()) {
            notes << QStringLiteral("differenceTarget=%1")
                         .arg(snapshot.compareTargetId);
        } else if (snapshot.compareMode == ArtifactCore::FrameDebugCompareMode::Disabled) {
            notes << QStringLiteral("differenceTarget=<none>");
        }

        if (const auto* visibility = findResource(
                snapshot, QStringLiteral("Composition Visibility"))) {
            if (visibility->note.contains(QStringLiteral("state=allSkipped"))) {
                notes << QStringLiteral("differenceAlert=all layers skipped");
            } else if (visibility->note.contains(QStringLiteral("state=noLayers"))) {
                notes << QStringLiteral("differenceAlert=no visible layers");
            } else if (visibility->note.contains(QStringLiteral("frameOutOfRange=1"))) {
                notes << QStringLiteral("differenceAlert=frame out of range");
            }
        }

        if (const auto* contract = findResource(
                snapshot, QStringLiteral("Blend / Mask Contract"))) {
            if (contract->note.contains(QStringLiteral("failed=1")) ||
                (contract->note.contains(QStringLiteral("directFallback=")) &&
                 !contract->note.contains(QStringLiteral("directFallback=0")))) {
                notes << QStringLiteral("alphaLeakageRisk=blend fallback or failure");
            } else if (contract->note.contains(QStringLiteral("maskContract=pending"))) {
                notes << QStringLiteral("alphaLeakageRisk=mask contract pending");
            } else {
                notes << QStringLiteral("alphaLeakageRisk=none detected");
            }
        }

        const int staleResourceCount = static_cast<int>(std::count_if(
            snapshot.resources.begin(), snapshot.resources.end(),
            [](const auto& resource) { return resource.stale; }));
        const int cacheMissCount = static_cast<int>(std::count_if(
            snapshot.resources.begin(), snapshot.resources.end(),
            [](const auto& resource) { return !resource.cacheHit; }));
        const int textureMismatchCount = static_cast<int>(std::count_if(
            snapshot.resources.begin(), snapshot.resources.end(),
            [](const auto& resource) {
                return resource.texture.valid &&
                       (resource.texture.width <= 0 || resource.texture.height <= 0);
            }));
        notes << QStringLiteral("leakageSignals=stale:%1 cacheMiss:%2 invalidTexture:%3")
                     .arg(staleResourceCount)
                     .arg(cacheMissCount)
                     .arg(textureMismatchCount);

        const auto* selected = findResource(snapshot, snapshot.selectedLayerName);
        if (selected && selected->texture.valid) {
            notes << QStringLiteral("selectedProxy=%1x%2")
                         .arg(selected->texture.width)
                         .arg(selected->texture.height);
        }

        return notes;
    }

    static QString densitySummaryText(const ArtifactCore::FrameDebugSnapshot& snapshot)
    {
        return QStringLiteral("density=%1 visual=%2 info=%3 luminance=%4 motion=%5")
                .arg(snapshot.densityLabel.isEmpty() ? QStringLiteral("low")
                                                    : snapshot.densityLabel)
                .arg(QString::number(snapshot.visualDensityScore, 'f', 2))
                .arg(QString::number(snapshot.informationDensityScore, 'f', 2))
                .arg(QString::number(snapshot.luminanceDensityScore, 'f', 2))
                .arg(QString::number(snapshot.motionDensityScore, 'f', 2));
    }

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

        auto resourceState = [&snapshot](const QString& typeName, const QString& labelName) {
            for (const auto& resource : snapshot.resources) {
                if (resource.type == typeName || resource.label == labelName) {
                    const bool skipped = resource.note.contains(QStringLiteral("skipped=")) ||
                                         resource.stale || !resource.cacheHit;
                    return skipped ? QStringLiteral("skipped") : QStringLiteral("ok");
                }
            }
            return QStringLiteral("none");
        };

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
        const QStringList diffLeakageNotes = buildDifferenceLeakageNotes(snapshot);
        if (summary_) {
            const QString diffModeText =
                snapshot.compareMode == ArtifactCore::FrameDebugCompareMode::Disabled
                    ? QStringLiteral("diff off")
                    : QStringLiteral("diff %1")
                          .arg(ArtifactCore::toString(snapshot.compareMode));
            const QString summaryText = QStringLiteral("%1 | frame %2 | comp %3 | layer %4 | backend %5 | passes %6 | resources %7 | %8 | video %9 | particle %10")
                                            .arg(statusText)
                                            .arg(snapshot.frame.framePosition())
                                            .arg(snapshot.compositionName.isEmpty() ? QStringLiteral("<none>") : snapshot.compositionName)
                                            .arg(snapshot.selectedLayerName.isEmpty() ? QStringLiteral("<none>") : snapshot.selectedLayerName)
                                            .arg(snapshot.renderBackend.isEmpty() ? QStringLiteral("<none>") : snapshot.renderBackend)
                                            .arg(static_cast<int>(snapshot.passes.size()))
                                            .arg(static_cast<int>(snapshot.resources.size()))
                                            .arg(QStringLiteral("%1 | %2").arg(diffModeText, densitySummaryText(snapshot)))
                                            .arg(resourceState(QStringLiteral("video"), QStringLiteral("Video Decode")))
                                            .arg(resourceState(QStringLiteral("particle"), QStringLiteral("Particle Draw")));
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
        lines << QStringLiteral("density: %1").arg(densitySummaryText(snapshot));
        if (snapshot.totalLayerCount > 0 || snapshot.visibleLayerCount > 0 ||
            snapshot.selectedLayerMaskCount > 0 || snapshot.selectedLayerEffectCount > 0 ||
            snapshot.selectedLayerMatteCount > 0) {
            lines << QStringLiteral("densityCounts: layers=%1 visible=%2 text=%3 mask=%4 effects=%5 mattes=%6")
                          .arg(snapshot.totalLayerCount)
                          .arg(snapshot.visibleLayerCount)
                          .arg(snapshot.textLayerCount)
                          .arg(snapshot.selectedLayerMaskCount)
                          .arg(snapshot.selectedLayerEffectCount)
                          .arg(snapshot.selectedLayerMatteCount);
        }
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
        if (!snapshot.densityWarning.isEmpty()) {
            lines << QStringLiteral("  - density warning: %1").arg(snapshot.densityWarning);
        }
        if (!snapshot.densityNextAction.isEmpty()) {
            lines << QStringLiteral("  - next: %1").arg(snapshot.densityNextAction);
        }
        lines << QString();
        lines << QStringLiteral("Difference / Leakage:");
        for (const auto& note : diffLeakageNotes) {
            lines << QStringLiteral("  - %1").arg(note);
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
                              .arg(pass.name)
                              .arg(ArtifactCore::toString(pass.kind))
                              .arg(ArtifactCore::toString(pass.status))
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
                const bool isMediaResource =
                    resource.label == QStringLiteral("Video Decode") ||
                    resource.label == QStringLiteral("Particle Draw");
                lines << QStringLiteral("  - %1 [%2] relation=%3 cacheHit=%4 stale=%5")
                              .arg(resource.label.isEmpty() ? QStringLiteral("<unnamed>") : resource.label)
                              .arg(resource.type.isEmpty() ? QStringLiteral("<type?>") : resource.type)
                              .arg(resource.relation.isEmpty() ? QStringLiteral("<none>") : resource.relation)
                              .arg(resource.cacheHit ? QStringLiteral("true") : QStringLiteral("false"))
                              .arg(resource.stale ? QStringLiteral("true") : QStringLiteral("false"));
                if (!resource.note.isEmpty() && (hot || isMediaResource)) {
                    lines << QStringLiteral("    note: %1").arg(resource.note);
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

void FrameDebugViewWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (impl_) {
        const auto& theme = ArtifactCore::currentDCCTheme();
        const QColor background(theme.backgroundColor);
        const QColor surface(theme.secondaryBackgroundColor);
        const QColor text(theme.textColor);

        setAutoFillBackground(true);
        QPalette ownerPalette = palette();
        ownerPalette.setColor(QPalette::Window, background);
        ownerPalette.setColor(QPalette::WindowText, text);
        setPalette(ownerPalette);

        if (impl_->text_) {
            QPalette textPalette = impl_->text_->palette();
            textPalette.setColor(QPalette::Base, surface);
            textPalette.setColor(QPalette::Text, text);
            impl_->text_->setPalette(textPalette);
        }
    }
    update();
}

} // namespace Artifact
