module;
#include <algorithm>
#include <cstddef>
#include <QHash>
#include <QPainter>
#include <QPaintEvent>
#include <QColor>
#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QStringList>
#include <wobjectimpl.h>

module Artifact.Widgets.FramePipelineViewWidget;

namespace Artifact {

W_OBJECT_IMPL(FramePipelineViewWidget)

class PipelineTimelineCanvas : public QWidget {
public:
    PipelineTimelineCanvas(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(180);
    }

    void setData(const ArtifactCore::FrameDebugSnapshot& snapshot,
                 const ArtifactCore::TraceSnapshot& trace)
    {
        snapshot_ = snapshot;
        trace_ = trace;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.fillRect(rect(), palette().base());
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRect content = rect().adjusted(8, 8, -8, -8);
        painter.setPen(palette().mid().color());
        painter.drawRect(content);

        if (trace_.frames.empty()) {
            painter.setPen(palette().text().color());
            painter.drawText(content, Qt::AlignCenter, QStringLiteral("No trace frame available"));
            return;
        }

        const auto& frame = trace_.frames.back();
        const qint64 spanNs = std::max<qint64>(1, frame.frameEndNs - frame.frameStartNs);
        const int laneCount = std::max(1, static_cast<int>(frame.lanes.size()));
        const int laneHeight = std::max(22, content.height() / std::max(1, laneCount));
        const int labelWidth = 92;
        const int barLeft = content.left() + labelWidth;
        const int barWidth = std::max(1, content.width() - labelWidth - 8);

        painter.setPen(palette().text().color());
        painter.drawText(QRect(content.left(), content.top(), labelWidth - 6, 18),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("frame %1").arg(frame.frameIndex));

        for (int i = 0; i < static_cast<int>(frame.lanes.size()); ++i) {
            const auto& lane = frame.lanes[static_cast<std::size_t>(i)];
            const int y = content.top() + i * laneHeight;
            const QRect laneRect(content.left(), y, content.width(), laneHeight - 4);

            painter.fillRect(laneRect, QColor(30, 30, 34));
            painter.setPen(QColor(70, 70, 80));
            painter.drawRect(laneRect.adjusted(0, 0, -1, -1));

            const QString laneName = lane.laneName.isEmpty() ? QStringLiteral("Lane") : lane.laneName;
            painter.setPen(QColor(220, 220, 220));
            painter.drawText(QRect(content.left() + 4, y, labelWidth - 10, laneHeight - 4),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             laneName);

            for (const auto& scope : lane.scopes) {
                const qreal startT = qreal(scope.startNs - frame.frameStartNs) / qreal(spanNs);
                const qreal endT = qreal(scope.endNs - frame.frameStartNs) / qreal(spanNs);
                const int x = barLeft + static_cast<int>(startT * barWidth);
                const int w = std::max(2, static_cast<int>((endT - startT) * barWidth));
                const QRect barRect(x, y + 6, w, laneHeight - 12);

                QColor barColor;
                switch (scope.domain) {
                case ArtifactCore::TraceDomain::Render: barColor = QColor(110, 150, 255); break;
                case ArtifactCore::TraceDomain::Decode: barColor = QColor(110, 220, 150); break;
                case ArtifactCore::TraceDomain::UI: barColor = QColor(255, 170, 80); break;
                case ArtifactCore::TraceDomain::Event: barColor = QColor(235, 120, 165); break;
                case ArtifactCore::TraceDomain::Thread: barColor = QColor(150, 150, 150); break;
                case ArtifactCore::TraceDomain::Crash: barColor = QColor(255, 90, 90); break;
                case ArtifactCore::TraceDomain::Timeline: barColor = QColor(180, 120, 255); break;
                case ArtifactCore::TraceDomain::Scope:
                default: barColor = QColor(180, 180, 180); break;
                }
                if (scope.frameIndex >= 0 && scope.frameIndex != frame.frameIndex) {
                    barColor = barColor.darker(130);
                }

                painter.fillRect(barRect, barColor);
                painter.setPen(barColor.lighter(140));
                painter.drawRect(barRect.adjusted(0, 0, -1, -1));

                const QString label = scope.name.isEmpty() ? QStringLiteral("<scope>") : scope.name;
                painter.setPen(Qt::black);
                painter.drawText(barRect.adjusted(4, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter, label);
            }
        }

        // Frame summary ruler
        painter.setPen(QColor(100, 100, 110));
        painter.drawLine(barLeft, content.bottom() - 4, barLeft + barWidth, content.bottom() - 4);
        for (int tick = 0; tick <= 4; ++tick) {
            const int x = barLeft + (barWidth * tick) / 4;
            painter.drawLine(x, content.bottom() - 8, x, content.bottom() - 1);
            painter.drawText(QRect(x - 20, content.bottom() - 22, 40, 14),
                             Qt::AlignCenter,
                             QString::number(tick * 25) + QStringLiteral("%"));
        }
    }

private:
    ArtifactCore::FrameDebugSnapshot snapshot_;
    ArtifactCore::TraceSnapshot trace_;
};

class FramePipelineViewWidget::Impl {
public:
    FramePipelineViewWidget* owner_ = nullptr;
    QLabel* summary_ = nullptr;
    PipelineTimelineCanvas* canvas_ = nullptr;
    QPlainTextEdit* text_ = nullptr;
    ArtifactCore::FrameDebugSnapshot snapshot_;
    ArtifactCore::TraceSnapshot trace_;
    bool hasSnapshot_ = false;

    explicit Impl(FramePipelineViewWidget* owner)
        : owner_(owner)
    {}

    void setupUI()
    {
        auto* layout = new QVBoxLayout(owner_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        summary_ = new QLabel(owner_);
        summary_->setTextFormat(Qt::PlainText);
        summary_->setWordWrap(false);
        summary_->setMinimumHeight(56);
        summary_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        layout->addWidget(summary_);

        canvas_ = new PipelineTimelineCanvas(owner_);
        layout->addWidget(canvas_);

        text_ = new QPlainTextEdit(owner_);
        text_->setReadOnly(true);
        text_->setLineWrapMode(QPlainTextEdit::NoWrap);
        text_->setMinimumHeight(180);
        layout->addWidget(text_);
    }

    static QString laneNameForDomain(ArtifactCore::TraceDomain domain)
    {
        switch (domain) {
        case ArtifactCore::TraceDomain::Render: return QStringLiteral("Render");
        case ArtifactCore::TraceDomain::Decode: return QStringLiteral("Decode");
        case ArtifactCore::TraceDomain::UI: return QStringLiteral("UI");
        case ArtifactCore::TraceDomain::Event: return QStringLiteral("Event");
        case ArtifactCore::TraceDomain::Thread: return QStringLiteral("Thread");
        case ArtifactCore::TraceDomain::Crash: return QStringLiteral("Crash");
        case ArtifactCore::TraceDomain::Timeline: return QStringLiteral("Timeline");
        case ArtifactCore::TraceDomain::Scope:
        default: return QStringLiteral("Scope");
        }
    }

    void showFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot,
                                const ArtifactCore::TraceSnapshot& trace)
    {
        snapshot_ = snapshot;
        trace_ = trace;
        hasSnapshot_ = true;

        updateSummary();
        updateDetailText();
    }

    void updateSummary()
    {
        if (!summary_) {
            return;
        }

        const auto frameIndex = snapshot_.frame.framePosition();
        const int passCount = static_cast<int>(snapshot_.passes.size());
        const int resourceCount = static_cast<int>(snapshot_.resources.size());
        const int attachmentCount = static_cast<int>(snapshot_.attachments.size());
        const int laneCount = trace_.frames.empty() ? 0 : static_cast<int>(trace_.frames.back().lanes.size());
        const qint64 spanNs = trace_.frames.empty() ? 0 : (trace_.frames.back().frameEndNs - trace_.frames.back().frameStartNs);

        summary_->setText(QStringLiteral("frame=%1  passes=%2  resources=%3  attachments=%4  lanes=%5  traceSpanNs=%6")
                              .arg(QString::number(frameIndex))
                              .arg(passCount)
                              .arg(resourceCount)
                              .arg(attachmentCount)
                              .arg(laneCount)
                              .arg(spanNs));
        if (canvas_) {
            canvas_->setData(snapshot_, trace_);
        }
    }

    void updateDetailText()
    {
        if (!text_) {
            return;
        }

        QStringList lines;
        lines << QStringLiteral("Frame Graph / Pipeline");
        lines << QStringLiteral("frame: %1").arg(QString::number(snapshot_.frame.framePosition()));
        lines << QStringLiteral("composition: %1")
                     .arg(snapshot_.compositionName.isEmpty() ? QStringLiteral("<none>") : snapshot_.compositionName);
        lines << QStringLiteral("renderBackend: %1")
                     .arg(snapshot_.renderBackend.isEmpty() ? QStringLiteral("<none>") : snapshot_.renderBackend);
        lines << QStringLiteral("playback: %1")
                     .arg(snapshot_.playbackState.isEmpty() ? QStringLiteral("<none>") : snapshot_.playbackState);
        lines << QStringLiteral("selectedLayer: %1")
                     .arg(snapshot_.selectedLayerName.isEmpty() ? QStringLiteral("<none>") : snapshot_.selectedLayerName);
        lines << QStringLiteral("compareMode: %1").arg(ArtifactCore::toString(snapshot_.compareMode));
        lines << QStringLiteral("compareTarget: %1")
                     .arg(snapshot_.compareTargetId.isEmpty() ? QStringLiteral("<none>") : snapshot_.compareTargetId);
        lines << QStringLiteral("failed: %1")
                     .arg(snapshot_.failed ? QStringLiteral("true") : QStringLiteral("false"));
        if (!snapshot_.failureReason.isEmpty()) {
            lines << QStringLiteral("failureReason: %1").arg(snapshot_.failureReason);
        }

        lines << QString();
        lines << QStringLiteral("Pass DAG:");
        if (snapshot_.passes.empty()) {
            lines << QStringLiteral("  <none>");
        } else {
            QStringList lastOutputs;
            QHash<QString, int> lastWritePass;
            for (int i = 0; i < static_cast<int>(snapshot_.passes.size()); ++i) {
                const auto& pass = snapshot_.passes[static_cast<std::size_t>(i)];
                QStringList inputs;
                for (const auto& input : pass.inputs) {
                    inputs << input.name;
                }
                QStringList outputs;
                for (const auto& output : pass.outputs) {
                    outputs << output.name;
                }

                QString hazards;
                if (!pass.outputs.empty()) {
                    for (const auto& output : pass.outputs) {
                        const QString outputName = output.name.isEmpty() ? QStringLiteral("<unnamed>") : output.name;
                        if (inputs.contains(output.name)) {
                            hazards += QStringLiteral(" rw-same;");
                        }
                        if (lastOutputs.contains(output.name)) {
                            hazards += QStringLiteral(" write-repeat;");
                        }
                        if (lastWritePass.contains(outputName) && lastWritePass.value(outputName) != i) {
                            hazards += QStringLiteral(" w-after-w;");
                        }
                        lastWritePass.insert(outputName, i);
                    }
                }
                if (pass.status == ArtifactCore::FrameDebugPassStatus::Failed) {
                    hazards += QStringLiteral(" failed;");
                }
                if (hazards.isEmpty()) {
                    hazards = QStringLiteral(" ok");
                }

                lines << QStringLiteral("  %1. %2 [%3/%4] %5 -> %6%7")
                              .arg(i + 1)
                              .arg(pass.name)
                              .arg(ArtifactCore::toString(pass.kind))
                              .arg(ArtifactCore::toString(pass.status))
                              .arg(inputs.isEmpty() ? QStringLiteral("<none>") : inputs.join(QStringLiteral(", ")))
                              .arg(outputs.isEmpty() ? QStringLiteral("<none>") : outputs.join(QStringLiteral(", ")))
                              .arg(hazards);

                lastOutputs = outputs;
            }
        }

        lines << QString();
        lines << QStringLiteral("Barrier Hints:");
        if (snapshot_.passes.empty()) {
            lines << QStringLiteral("  <none>");
        } else {
            QStringList barrierHints;
            QHash<QString, int> producerIndex;
            for (int i = 0; i < static_cast<int>(snapshot_.passes.size()); ++i) {
                const auto& pass = snapshot_.passes[static_cast<std::size_t>(i)];
                for (const auto& output : pass.outputs) {
                    const QString key = output.name.isEmpty() ? QStringLiteral("<unnamed>") : output.name;
                    producerIndex.insert(key, i);
                }
            }

            for (int i = 0; i < static_cast<int>(snapshot_.passes.size()); ++i) {
                const auto& pass = snapshot_.passes[static_cast<std::size_t>(i)];
                for (const auto& input : pass.inputs) {
                    const QString key = input.name.isEmpty() ? QStringLiteral("<unnamed>") : input.name;
                    if (producerIndex.contains(key) && producerIndex.value(key) < i) {
                        barrierHints << QStringLiteral("  %1 reads %2 after pass #%3")
                                            .arg(pass.name)
                                            .arg(key)
                                            .arg(producerIndex.value(key));
                    }
                }
            }
            const int barrierRows = std::min(static_cast<int>(barrierHints.size()), 8);
            if (barrierRows == 0) {
                lines << QStringLiteral("  <none>");
            } else {
                for (int i = 0; i < barrierRows; ++i) {
                    lines << barrierHints[static_cast<std::size_t>(i)];
                }
                if (barrierHints.size() > barrierRows) {
                    lines << QStringLiteral("  ... %1 more").arg(barrierHints.size() - barrierRows);
                }
            }
        }

        lines << QString();
        lines << QStringLiteral("Resource Lifetime:");
        if (snapshot_.passes.empty() && snapshot_.resources.empty()) {
            lines << QStringLiteral("  <none>");
        } else {
            struct ResourceUseSummary {
                int firstPass = -1;
                int lastPass = -1;
                int readCount = 0;
                int writeCount = 0;
                QString producer;
                QStringList readers;
                QString lastWriter;
                bool samePassReadWrite = false;
            };

            QHash<QString, ResourceUseSummary> resourceUse;
            for (int i = 0; i < static_cast<int>(snapshot_.passes.size()); ++i) {
                const auto& pass = snapshot_.passes[static_cast<std::size_t>(i)];
                QHash<QString, bool> passReads;
                QHash<QString, bool> passWrites;
                for (const auto& input : pass.inputs) {
                    const QString key = input.name.isEmpty() ? QStringLiteral("<unnamed>") : input.name;
                    auto& use = resourceUse[key];
                    if (use.firstPass < 0) {
                        use.firstPass = i;
                    }
                    use.lastPass = i;
                    ++use.readCount;
                    if (use.readers.size() < 3 && !use.readers.contains(pass.name)) {
                        use.readers << pass.name;
                    }
                    passReads.insert(key, true);
                }
                for (const auto& output : pass.outputs) {
                    const QString key = output.name.isEmpty() ? QStringLiteral("<unnamed>") : output.name;
                    auto& use = resourceUse[key];
                    if (use.firstPass < 0) {
                        use.firstPass = i;
                    }
                    use.lastPass = i;
                    ++use.writeCount;
                    use.producer = pass.name;
                    use.lastWriter = pass.name;
                    passWrites.insert(key, true);
                }
                for (auto it = passReads.cbegin(); it != passReads.cend(); ++it) {
                    if (passWrites.contains(it.key())) {
                        resourceUse[it.key()].samePassReadWrite = true;
                    }
                }
            }

            QStringList resourceNames = resourceUse.keys();
            std::sort(resourceNames.begin(), resourceNames.end());
            const int resourceRows = std::min(static_cast<int>(resourceNames.size()), 10);
            for (int i = 0; i < resourceRows; ++i) {
                const auto& name = resourceNames[static_cast<std::size_t>(i)];
                const auto& use = resourceUse.value(name);
                QString hazard;
                if (use.samePassReadWrite) {
                    hazard += QStringLiteral(" rw-same");
                }
                if (use.writeCount > 1) {
                    hazard += QStringLiteral(" multi-write");
                }
                if (use.readCount > 0 && use.writeCount > 0 && use.lastPass > use.firstPass) {
                    hazard += QStringLiteral(" lifetime");
                }
                if (hazard.isEmpty()) {
                    hazard = QStringLiteral(" ok");
                }

                lines << QStringLiteral("  - %1 life=%2..%3 reads=%4 writes=%5 producer=%6 readers=%7%8")
                              .arg(name)
                              .arg(use.firstPass)
                              .arg(use.lastPass)
                              .arg(use.readCount)
                              .arg(use.writeCount)
                              .arg(use.producer.isEmpty() ? QStringLiteral("<none>") : use.producer)
                              .arg(use.readers.isEmpty() ? QStringLiteral("<none>") : use.readers.join(QStringLiteral(", ")))
                              .arg(hazard);
            }
            if (resourceRows == 0) {
                lines << QStringLiteral("  <none>");
            } else if (resourceNames.size() > resourceRows) {
                lines << QStringLiteral("  ... %1 more").arg(resourceNames.size() - resourceRows);
            }
        }

        lines << QString();
        lines << QStringLiteral("Lane Summary:");
        if (trace_.frames.empty()) {
            lines << QStringLiteral("  <no trace frames>");
        } else {
            const auto& lastFrame = trace_.frames.back();
            lines << QStringLiteral("  frameIndex: %1").arg(lastFrame.frameIndex);
            lines << QStringLiteral("  lanes: %1").arg(static_cast<int>(lastFrame.lanes.size()));
            for (const auto& lane : lastFrame.lanes) {
                lines << QStringLiteral("  - %1 scopes=%2")
                              .arg(laneNameForDomain(lane.scopes.isEmpty()
                                                         ? ArtifactCore::TraceDomain::Timeline
                                                         : lane.scopes.first().domain))
                              .arg(static_cast<int>(lane.scopes.size()));
            }
        }

        lines << QString();
        lines << QStringLiteral("Trace Lane Breakdown:");
        if (trace_.frames.empty()) {
            lines << QStringLiteral("  <no trace frames>");
        } else {
            const auto& lastFrame = trace_.frames.back();
            for (const auto& lane : lastFrame.lanes) {
                QStringList scopeNames;
                qint64 totalDurationNs = 0;
                for (const auto& scope : lane.scopes) {
                    totalDurationNs += std::max<qint64>(0, scope.endNs - scope.startNs);
                    if (scopeNames.size() < 4) {
                        scopeNames << (scope.name.isEmpty() ? QStringLiteral("<scope>") : scope.name);
                    }
                }
                const ArtifactCore::TraceDomain laneDomain = lane.scopes.isEmpty()
                    ? ArtifactCore::TraceDomain::Timeline
                    : lane.scopes.first().domain;
                lines << QStringLiteral("  - %1 scopes=%2 totalNs=%3 samples=%4")
                              .arg(laneNameForDomain(laneDomain))
                              .arg(static_cast<int>(lane.scopes.size()))
                              .arg(totalDurationNs)
                              .arg(scopeNames.isEmpty() ? QStringLiteral("<none>") : scopeNames.join(QStringLiteral(", ")));
            }
        }

        lines << QString();
        lines << QStringLiteral("Lifetime Heuristic:");
        if (snapshot_.resources.empty()) {
            lines << QStringLiteral("  <no resources>");
        } else {
            for (const auto& resource : snapshot_.resources) {
                const QString resourceLabel = resource.label.isEmpty() ? QStringLiteral("<unnamed>") : resource.label;
                const QString resourceType = resource.type.isEmpty() ? QStringLiteral("<type?>") : resource.type;
                const QString relation = resource.relation.isEmpty() ? QStringLiteral("<none>") : resource.relation;
                QString resourceView;
                if (resource.texture.valid) {
                    resourceView = QStringLiteral(" view=mip%1/%2 slice%3 array%4 srgb=%5 samples=%6")
                                       .arg(resource.texture.mipLevel)
                                       .arg(resource.texture.mipLevels)
                                       .arg(resource.texture.sliceIndex)
                                       .arg(resource.texture.arrayLayers);
                    resourceView = resourceView.arg(resource.texture.srgb ? QStringLiteral("true") : QStringLiteral("false"))
                                                   .arg(resource.texture.sampleCount);
                }
                lines << QStringLiteral("  - %1 [%2] relation=%3 cacheHit=%4 stale=%5%6")
                              .arg(resourceLabel, resourceType, relation)
                              .arg(resource.cacheHit ? QStringLiteral("true") : QStringLiteral("false"))
                              .arg(resource.stale ? QStringLiteral("true") : QStringLiteral("false"))
                              .arg(resourceView);
            }
        }

        text_->setPlainText(lines.join(QStringLiteral("\n")));
    }
};

FramePipelineViewWidget::FramePipelineViewWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    impl_->setupUI();
}

FramePipelineViewWidget::~FramePipelineViewWidget()
{
    delete impl_;
}

void FramePipelineViewWidget::setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot,
                                                    const ArtifactCore::TraceSnapshot& trace)
{
    if (!impl_) {
        return;
    }
    impl_->showFrameDebugSnapshot(snapshot, trace);
}

} // namespace Artifact
