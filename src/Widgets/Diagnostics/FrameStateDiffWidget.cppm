module;
#include <algorithm>
#include <cstddef>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QStringList>
#include <wobjectimpl.h>

module Artifact.Widgets.FrameStateDiffWidget;

namespace Artifact {

W_OBJECT_IMPL(FrameStateDiffWidget)

class FrameStateDiffWidget::Impl {
public:
    FrameStateDiffWidget* owner_ = nullptr;
    QPlainTextEdit* text_ = nullptr;
    ArtifactCore::FrameDebugSnapshot previous_;
    ArtifactCore::TraceSnapshot previousTrace_;
    bool hasPrevious_ = false;
    bool hasPreviousTrace_ = false;

    explicit Impl(FrameStateDiffWidget* owner)
        : owner_(owner)
    {}

    void setupUI()
    {
        auto* layout = new QVBoxLayout(owner_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        text_ = new QPlainTextEdit(owner_);
        text_->setReadOnly(true);
        text_->setLineWrapMode(QPlainTextEdit::NoWrap);
        layout->addWidget(text_);
    }

    static QString snapshotKey(const ArtifactCore::FrameDebugSnapshot& snapshot)
    {
        return QStringLiteral("%1|%2|%3|%4|%5|%6")
                .arg(snapshot.compositionName,
                     snapshot.renderBackend,
                     snapshot.playbackState,
                     snapshot.selectedLayerName,
                     QString::number(snapshot.renderLastFrameMs, 'f', 1),
                     QString::number(snapshot.renderAverageFrameMs, 'f', 1));
    }

    void showFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot,
                                const ArtifactCore::TraceSnapshot& trace)
    {
        if (!text_) {
            return;
        }

        QStringList lines;
        lines << QStringLiteral("State Diff Tracker");
        lines << QStringLiteral("frame: %1").arg(snapshot.frame.framePosition());
        lines << QStringLiteral("currentKey: %1").arg(snapshotKey(snapshot));

        if (!hasPrevious_) {
            lines << QStringLiteral("baseline: <none>");
        } else {
            lines << QStringLiteral("baselineFrame: %1").arg(previous_.frame.framePosition());
            lines << QStringLiteral("frameDelta: %1").arg(snapshot.frame.framePosition() - previous_.frame.framePosition());

            if (snapshot.compositionName != previous_.compositionName) {
                lines << QStringLiteral("compositionChanged: %1 -> %2")
                              .arg(previous_.compositionName.isEmpty() ? QStringLiteral("<none>") : previous_.compositionName,
                                   snapshot.compositionName.isEmpty() ? QStringLiteral("<none>") : snapshot.compositionName);
            }
            if (snapshot.renderBackend != previous_.renderBackend) {
                lines << QStringLiteral("renderBackendChanged: %1 -> %2").arg(previous_.renderBackend, snapshot.renderBackend);
            }
            if (snapshot.playbackState != previous_.playbackState) {
                lines << QStringLiteral("playbackStateChanged: %1 -> %2").arg(previous_.playbackState, snapshot.playbackState);
            }
            if (snapshot.selectedLayerName != previous_.selectedLayerName) {
                lines << QStringLiteral("selectedLayerChanged: %1 -> %2")
                              .arg(previous_.selectedLayerName.isEmpty() ? QStringLiteral("<none>") : previous_.selectedLayerName,
                                   snapshot.selectedLayerName.isEmpty() ? QStringLiteral("<none>") : snapshot.selectedLayerName);
            }
            if (snapshot.renderLastFrameMs != previous_.renderLastFrameMs ||
                snapshot.renderAverageFrameMs != previous_.renderAverageFrameMs) {
                lines << QStringLiteral("renderTimingChanged: last %1ms/%2ms avg %3ms/%4ms")
                              .arg(QString::number(previous_.renderLastFrameMs, 'f', 1))
                              .arg(QString::number(snapshot.renderLastFrameMs, 'f', 1))
                              .arg(QString::number(previous_.renderAverageFrameMs, 'f', 1))
                              .arg(QString::number(snapshot.renderAverageFrameMs, 'f', 1));
            }
            if (snapshot.compareMode != previous_.compareMode) {
                lines << QStringLiteral("compareModeChanged: %1 -> %2")
                              .arg(ArtifactCore::toString(previous_.compareMode),
                                   ArtifactCore::toString(snapshot.compareMode));
            }
            if (snapshot.compareTargetId != previous_.compareTargetId) {
                lines << QStringLiteral("compareTargetChanged: %1 -> %2")
                              .arg(previous_.compareTargetId.isEmpty() ? QStringLiteral("<none>") : previous_.compareTargetId,
                                   snapshot.compareTargetId.isEmpty() ? QStringLiteral("<none>") : snapshot.compareTargetId);
            }
            if (snapshot.failed != previous_.failed) {
                lines << QStringLiteral("failedChanged: %1 -> %2")
                              .arg(previous_.failed ? QStringLiteral("true") : QStringLiteral("false"),
                                   snapshot.failed ? QStringLiteral("true") : QStringLiteral("false"));
            }
        }

        lines << QString();
        lines << QStringLiteral("Counts:");
        lines << QStringLiteral("  passes: %1 -> %2")
                      .arg(hasPrevious_ ? static_cast<int>(previous_.passes.size()) : 0)
                      .arg(static_cast<int>(snapshot.passes.size()));
        lines << QStringLiteral("  resources: %1 -> %2")
                      .arg(hasPrevious_ ? static_cast<int>(previous_.resources.size()) : 0)
                      .arg(static_cast<int>(snapshot.resources.size()));
        lines << QStringLiteral("  attachments: %1 -> %2")
                      .arg(hasPrevious_ ? static_cast<int>(previous_.attachments.size()) : 0)
                      .arg(static_cast<int>(snapshot.attachments.size()));
        lines << QStringLiteral("  traceEvents: %1 -> %2")
                      .arg(hasPreviousTrace_ ? static_cast<int>(previousTrace_.events.size()) : 0)
                      .arg(static_cast<int>(trace.events.size()));
        lines << QStringLiteral("  locks: %1 -> %2")
                      .arg(hasPreviousTrace_ ? static_cast<int>(previousTrace_.locks.size()) : 0)
                      .arg(static_cast<int>(trace.locks.size()));
        if (hasPreviousTrace_ && !previousTrace_.threads.isEmpty() && !trace.threads.isEmpty()) {
            int prevHotDepth = 0;
            QString prevHotThread;
            for (const auto& thread : previousTrace_.threads) {
                if (thread.lockDepth > prevHotDepth) {
                    prevHotDepth = thread.lockDepth;
                    prevHotThread = thread.threadName;
                }
            }
            int hotDepth = 0;
            QString hotThread;
            for (const auto& thread : trace.threads) {
                if (thread.lockDepth > hotDepth) {
                    hotDepth = thread.lockDepth;
                    hotThread = thread.threadName;
                }
            }
            if (hotDepth != prevHotDepth || hotThread != prevHotThread) {
                lines << QStringLiteral("  hotThread: %1(%2) -> %3(%4)")
                              .arg(prevHotThread.isEmpty() ? QStringLiteral("<unnamed>") : prevHotThread)
                              .arg(prevHotDepth)
                              .arg(hotThread.isEmpty() ? QStringLiteral("<unnamed>") : hotThread)
                              .arg(hotDepth);
            }
        }

        lines << QString();
        lines << QStringLiteral("Pass changes:");
        const int passRows = std::min(static_cast<int>(snapshot.passes.size()),
                                      hasPrevious_ ? static_cast<int>(previous_.passes.size()) : 0);
        if (passRows == 0 && snapshot.passes.empty()) {
            lines << QStringLiteral("  <none>");
        } else {
            for (int i = 0; i < passRows; ++i) {
                const auto& now = snapshot.passes[static_cast<std::size_t>(i)];
                const auto& prev = previous_.passes[static_cast<std::size_t>(i)];
                if (now.name != prev.name || now.kind != prev.kind || now.status != prev.status || now.durationUs != prev.durationUs) {
                    lines << QStringLiteral("  #%1 %2/%3 -> %4/%5 duration %6 -> %7")
                                  .arg(i)
                                  .arg(prev.name, ArtifactCore::toString(prev.kind), ArtifactCore::toString(prev.status))
                                  .arg(now.name, ArtifactCore::toString(now.kind), ArtifactCore::toString(now.status))
                                  .arg(prev.durationUs)
                                  .arg(now.durationUs);
                }
            }
            if (snapshot.passes.size() != previous_.passes.size()) {
                lines << QStringLiteral("  sizeChanged: %1 -> %2")
                              .arg(hasPrevious_ ? static_cast<int>(previous_.passes.size()) : 0)
                              .arg(static_cast<int>(snapshot.passes.size()));
            }
        }

        lines << QString();
        lines << QStringLiteral("Resource changes:");
        const int resourceRows = std::min(static_cast<int>(snapshot.resources.size()),
                                          hasPrevious_ ? static_cast<int>(previous_.resources.size()) : 0);
        if (resourceRows == 0 && snapshot.resources.empty()) {
            lines << QStringLiteral("  <none>");
        } else {
            for (int i = 0; i < resourceRows; ++i) {
                const auto& now = snapshot.resources[static_cast<std::size_t>(i)];
                const auto& prev = previous_.resources[static_cast<std::size_t>(i)];
                if (now.label != prev.label || now.relation != prev.relation ||
                    now.cacheHit != prev.cacheHit || now.stale != prev.stale) {
                    lines << QStringLiteral("  #%1 %2 -> %3 hit %4 -> %5 stale %6 -> %7")
                                  .arg(i)
                                  .arg(prev.label.isEmpty() ? QStringLiteral("<unnamed>") : prev.label,
                                       now.label.isEmpty() ? QStringLiteral("<unnamed>") : now.label)
                                  .arg(prev.relation.isEmpty() ? QStringLiteral("<none>") : prev.relation,
                                       now.relation.isEmpty() ? QStringLiteral("<none>") : now.relation)
                                  .arg(prev.cacheHit ? QStringLiteral("true") : QStringLiteral("false"),
                                       now.cacheHit ? QStringLiteral("true") : QStringLiteral("false"))
                                  .arg(prev.stale ? QStringLiteral("true") : QStringLiteral("false"),
                                       now.stale ? QStringLiteral("true") : QStringLiteral("false"));
                    if (now.texture.valid || prev.texture.valid) {
                        lines << QStringLiteral("    texture: %1x%2 -> %3x%4 mip %5/%6 -> %7/%8 array %9 -> %10")
                                      .arg(prev.texture.width)
                                      .arg(prev.texture.height)
                                      .arg(now.texture.width)
                                      .arg(now.texture.height)
                                      .arg(prev.texture.mipLevel)
                                      .arg(prev.texture.mipLevels)
                                      .arg(now.texture.mipLevel)
                                      .arg(now.texture.mipLevels)
                                      .arg(prev.texture.arrayLayers)
                                      .arg(now.texture.arrayLayers);
                        if (prev.texture.sliceIndex != now.texture.sliceIndex ||
                            prev.texture.sampleCount != now.texture.sampleCount ||
                            prev.texture.srgb != now.texture.srgb) {
                            lines << QStringLiteral("    viewMeta: slice %1 -> %2 samples %3 -> %4 srgb %5 -> %6")
                                          .arg(prev.texture.sliceIndex)
                                          .arg(now.texture.sliceIndex)
                                          .arg(prev.texture.sampleCount)
                                          .arg(now.texture.sampleCount)
                                          .arg(prev.texture.srgb ? QStringLiteral("true") : QStringLiteral("false"))
                                          .arg(now.texture.srgb ? QStringLiteral("true") : QStringLiteral("false"));
                        }
                    }
                }
            }
            if (snapshot.resources.size() != previous_.resources.size()) {
                lines << QStringLiteral("  sizeChanged: %1 -> %2")
                              .arg(hasPrevious_ ? static_cast<int>(previous_.resources.size()) : 0)
                              .arg(static_cast<int>(snapshot.resources.size()));
            }
        }

        lines << QString();
        lines << QStringLiteral("Attachment changes:");
        const int attachmentRows = std::min(static_cast<int>(snapshot.attachments.size()),
                                            hasPrevious_ ? static_cast<int>(previous_.attachments.size()) : 0);
        if (attachmentRows == 0 && snapshot.attachments.empty()) {
            lines << QStringLiteral("  <none>");
        } else {
            for (int i = 0; i < attachmentRows; ++i) {
                const auto& now = snapshot.attachments[static_cast<std::size_t>(i)];
                const auto& prev = previous_.attachments[static_cast<std::size_t>(i)];
                if (now.name != prev.name || now.role != prev.role || now.readOnly != prev.readOnly ||
                    now.texture.valid != prev.texture.valid || now.buffer.valid != prev.buffer.valid) {
                    lines << QStringLiteral("  #%1 %2/%3 -> %4/%5 readOnly %6 -> %7")
                                  .arg(i)
                                  .arg(prev.name.isEmpty() ? QStringLiteral("<unnamed>") : prev.name,
                                       prev.role.isEmpty() ? QStringLiteral("<none>") : prev.role)
                                  .arg(now.name.isEmpty() ? QStringLiteral("<unnamed>") : now.name,
                                       now.role.isEmpty() ? QStringLiteral("<none>") : now.role)
                                  .arg(prev.readOnly ? QStringLiteral("true") : QStringLiteral("false"),
                                       now.readOnly ? QStringLiteral("true") : QStringLiteral("false"));
                    if (now.texture.valid || prev.texture.valid) {
                        lines << QStringLiteral("    texture: %1x%2 -> %3x%4 mip %5/%6 -> %7/%8")
                                      .arg(prev.texture.width)
                                      .arg(prev.texture.height)
                                      .arg(now.texture.width)
                                      .arg(now.texture.height)
                                      .arg(prev.texture.mipLevel)
                                      .arg(prev.texture.mipLevels)
                                      .arg(now.texture.mipLevel)
                                      .arg(now.texture.mipLevels);
                    }
                }
            }
            if (snapshot.attachments.size() != previous_.attachments.size()) {
                lines << QStringLiteral("  sizeChanged: %1 -> %2")
                              .arg(hasPrevious_ ? static_cast<int>(previous_.attachments.size()) : 0)
                              .arg(static_cast<int>(snapshot.attachments.size()));
            }
        }

        lines << QString();
        lines << QStringLiteral("Lifetime changes:");
        if (!hasPrevious_) {
            lines << QStringLiteral("  <baseline only>");
        } else {
            int changedLifetimeRows = 0;
            for (const auto& current : snapshot.resources) {
                for (const auto& oldResource : previous_.resources) {
                    if (current.label == oldResource.label && current.relation != oldResource.relation) {
                        lines << QStringLiteral("  %1 relation %2 -> %3")
                                      .arg(current.label.isEmpty() ? QStringLiteral("<unnamed>") : current.label)
                                      .arg(oldResource.relation.isEmpty() ? QStringLiteral("<none>") : oldResource.relation)
                                      .arg(current.relation.isEmpty() ? QStringLiteral("<none>") : current.relation);
                        ++changedLifetimeRows;
                        break;
                    }
                }
                if (changedLifetimeRows >= 6) {
                    break;
                }
            }
            if (changedLifetimeRows == 0) {
                lines << QStringLiteral("  <none>");
            }
        }

        lines << QString();
        lines << QStringLiteral("Trace cross-check:");
        lines << QStringLiteral("  frames=%1 scopes=%2 threads=%3")
                      .arg(static_cast<int>(trace.frames.size()))
                      .arg(static_cast<int>(trace.scopes.size()))
                      .arg(static_cast<int>(trace.threads.size()));
        if (!trace.frames.empty()) {
            const auto& frame = trace.frames.back();
            qint64 totalFrameSpan = 0;
            for (const auto& lane : frame.lanes) {
                for (const auto& scope : lane.scopes) {
                    totalFrameSpan += std::max<qint64>(0, scope.endNs - scope.startNs);
                }
            }
            lines << QStringLiteral("  traceFrameSpanNs=%1 accumulatedLaneNs=%2")
                          .arg(frame.frameEndNs - frame.frameStartNs)
                          .arg(totalFrameSpan);
        }
        if (!trace.frames.empty()) {
            const auto& frame = trace.frames.back();
            lines << QStringLiteral("  lastTraceFrame=%1 lanes=%2")
                          .arg(frame.frameIndex)
                          .arg(static_cast<int>(frame.lanes.size()));
        }
        if (!trace.locks.empty()) {
            const auto& lock = trace.locks.back();
            lines << QStringLiteral("  lastLock=%1 %2 [0x%3]")
                          .arg(lock.acquired ? QStringLiteral("acquire") : QStringLiteral("release"))
                          .arg(lock.mutexName.isEmpty() ? QStringLiteral("<unnamed-mutex>") : lock.mutexName)
                          .arg(QString::number(static_cast<unsigned long long>(lock.threadId), 16));
        }
        if (!trace.threads.empty()) {
            int hottestDepth = 0;
            QString hottestThread;
            for (const auto& thread : trace.threads) {
                if (thread.lockDepth > hottestDepth) {
                    hottestDepth = thread.lockDepth;
                    hottestThread = thread.threadName;
                }
            }
            if (hottestDepth > 0) {
                lines << QStringLiteral("  hottestLockDepth=%1 thread=%2")
                              .arg(hottestDepth)
                              .arg(hottestThread.isEmpty() ? QStringLiteral("<unnamed>") : hottestThread);
            }
        }

        text_->setPlainText(lines.join(QStringLiteral("\n")));
        previous_ = snapshot;
        previousTrace_ = trace;
        hasPrevious_ = true;
        hasPreviousTrace_ = true;
    }
};

FrameStateDiffWidget::FrameStateDiffWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    impl_->setupUI();
}

FrameStateDiffWidget::~FrameStateDiffWidget()
{
    delete impl_;
}

void FrameStateDiffWidget::setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot,
                                                 const ArtifactCore::TraceSnapshot& trace)
{
    if (!impl_) {
        return;
    }
    impl_->showFrameDebugSnapshot(snapshot, trace);
}

} // namespace Artifact
