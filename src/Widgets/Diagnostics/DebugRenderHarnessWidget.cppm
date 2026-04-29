module;
#include <algorithm>
#include <QLabel>
#include <QListWidget>
#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QVBoxLayout>
#include <QStringList>
#include <wobjectimpl.h>

module Artifact.Widgets.DebugRenderHarnessWidget;

import Frame.Debug;

namespace Artifact {

namespace {
QString presetReportName(const QString& preset)
{
    const QString trimmed = preset.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("particle-only") : trimmed;
}

QString mediaStateFromResource(const ArtifactCore::FrameDebugSnapshot& snapshot,
                               const QString& typeName,
                               const QString& labelName)
{
    for (const auto& resource : snapshot.resources) {
        if (resource.type == typeName || resource.label == labelName) {
            if (resource.note.contains(QStringLiteral("skipped="))) {
                return QStringLiteral("skipped");
            }
            if (!resource.cacheHit || resource.stale) {
                return QStringLiteral("degraded");
            }
            return QStringLiteral("ok");
        }
    }
    return QStringLiteral("none");
}
}

W_OBJECT_IMPL(DebugRenderHarnessWidget)

class DebugRenderHarnessWidget::Impl {
public:
    class PresetListWidget : public QListWidget {
    public:
        explicit PresetListWidget(Impl* impl, QWidget* parent = nullptr)
            : QListWidget(parent), impl_(impl)
        {}

    protected:
        void currentChanged(const QModelIndex& current, const QModelIndex& previous) override
        {
            QListWidget::currentChanged(current, previous);
            if (impl_) {
                impl_->setScenePresetIndex(current.row());
            }
        }

    private:
        Impl* impl_ = nullptr;
    };

    DebugRenderHarnessWidget* owner_ = nullptr;
    QLabel* summary_ = nullptr;
    PresetListWidget* presetList_ = nullptr;
    QPlainTextEdit* report_ = nullptr;
    ArtifactCore::FrameDebugSnapshot snapshot_;
    QString scenePreset_ = QStringLiteral("particle-only");
    bool hasSnapshot_ = false;

    explicit Impl(DebugRenderHarnessWidget* owner) : owner_(owner) {}

    void setupUI()
    {
        auto* layout = new QVBoxLayout(owner_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        summary_ = new QLabel(owner_);
        summary_->setTextFormat(Qt::PlainText);
        summary_->setWordWrap(true);
        summary_->setMinimumHeight(48);
        layout->addWidget(summary_);

        auto* splitter = new QSplitter(Qt::Horizontal, owner_);
        presetList_ = new PresetListWidget(this, splitter);
        presetList_->addItems(QStringList{
            QStringLiteral("particle-only"),
            QStringLiteral("video-only"),
            QStringLiteral("blend-only"),
            QStringLiteral("overlay-only"),
            QStringLiteral("mixed-media")
        });
        presetList_->setMinimumWidth(220);
        presetList_->setCurrentRow(0);

        report_ = new QPlainTextEdit(splitter);
        report_->setReadOnly(true);
        report_->setLineWrapMode(QPlainTextEdit::NoWrap);
        report_->setFocusPolicy(Qt::StrongFocus);

        splitter->addWidget(presetList_);
        splitter->addWidget(report_);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 3);
        layout->addWidget(splitter);

        refresh();
    }

    void setScenePreset(const QString& presetName)
    {
        const QString normalized = presetReportName(presetName);
        scenePreset_ = normalized;
        if (presetList_) {
            const auto matches = presetList_->findItems(normalized, Qt::MatchExactly);
            if (!matches.isEmpty()) {
                const int row = presetList_->row(matches.front());
                if (row != presetList_->currentRow()) {
                    presetList_->setCurrentRow(row);
                }
            }
        }
        refresh();
    }

    void setScenePresetIndex(int row)
    {
        if (!presetList_) {
            return;
        }
        if (row < 0 || row >= presetList_->count()) {
            return;
        }
        scenePreset_ = presetList_->item(row)->text();
        refresh();
    }

    void setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot)
    {
        snapshot_ = snapshot;
        hasSnapshot_ = true;
        refresh();
    }

    void refresh()
    {
        if (!summary_ || !report_) {
            return;
        }

        const QString particleState = hasSnapshot_
            ? mediaStateFromResource(snapshot_, QStringLiteral("particle"), QStringLiteral("Particle Draw"))
            : QStringLiteral("none");
        const QString videoState = hasSnapshot_
            ? mediaStateFromResource(snapshot_, QStringLiteral("video"), QStringLiteral("Video Decode"))
            : QStringLiteral("none");
        const QString blendState = hasSnapshot_
            ? mediaStateFromResource(snapshot_, QStringLiteral("composition"), QStringLiteral("Render Path"))
            : QStringLiteral("none");

        summary_->setText(QStringLiteral("preset=%1  frame=%2  comp=%3  layer=%4  particle=%5  video=%6  blend=%7")
                              .arg(scenePreset_)
                              .arg(hasSnapshot_ ? snapshot_.frame.framePosition() : -1)
                              .arg(hasSnapshot_ && !snapshot_.compositionName.isEmpty() ? snapshot_.compositionName : QStringLiteral("<none>"))
                              .arg(hasSnapshot_ && !snapshot_.selectedLayerName.isEmpty() ? snapshot_.selectedLayerName : QStringLiteral("<none>"))
                              .arg(particleState)
                              .arg(videoState)
                              .arg(blendState));

        QStringList lines;
        lines << QStringLiteral("Debug Render Harness");
        lines << QStringLiteral("preset: %1").arg(scenePreset_);
        lines << QStringLiteral("frame: %1").arg(hasSnapshot_ ? snapshot_.frame.framePosition() : -1);
        lines << QStringLiteral("composition: %1").arg(hasSnapshot_ && !snapshot_.compositionName.isEmpty()
                                                       ? snapshot_.compositionName
                                                       : QStringLiteral("<none>"));
        lines << QStringLiteral("selectedLayer: %1").arg(hasSnapshot_ && !snapshot_.selectedLayerName.isEmpty()
                                                        ? snapshot_.selectedLayerName
                                                        : QStringLiteral("<none>"));
        lines << QStringLiteral("renderBackend: %1").arg(hasSnapshot_ && !snapshot_.renderBackend.isEmpty()
                                                       ? snapshot_.renderBackend
                                                       : QStringLiteral("<none>"));
        lines << QStringLiteral("viewport: <from scene>");
        lines << QStringLiteral("status: %1")
                     .arg(hasSnapshot_ && snapshot_.failed ? QStringLiteral("failed")
                                                           : QStringLiteral("ok"));
        if (hasSnapshot_ && snapshot_.failed && !snapshot_.failureReason.isEmpty()) {
            lines << QStringLiteral("failureReason: %1").arg(snapshot_.failureReason);
        }

        lines << QString();
        lines << QStringLiteral("Media:");
        lines << QStringLiteral("  particle=%1").arg(particleState);
        lines << QStringLiteral("  video=%1").arg(videoState);
        lines << QStringLiteral("  blend=%1").arg(blendState);

        lines << QString();
        lines << QStringLiteral("Scene Contract:");
        lines << QStringLiteral("  - particle-only: one emitter over dark/light backgrounds");
        lines << QStringLiteral("  - video-only: short MP4, frame 0 and mid-frame seek");
        lines << QStringLiteral("  - blend-only: visible vs transparent output");
        lines << QStringLiteral("  - overlay-only: grid / anchor / guidance visible without media");
        lines << QStringLiteral("  - mixed-media: all paths present in one report");

        lines << QString();
        lines << QStringLiteral("Capture Notes:");
        if (hasSnapshot_) {
            lines << QStringLiteral("  traceFrames=%1").arg(static_cast<int>(snapshot_.passes.size()));
            lines << QStringLiteral("  resources=%1").arg(static_cast<int>(snapshot_.resources.size()));
            lines << QStringLiteral("  attachments=%1").arg(static_cast<int>(snapshot_.attachments.size()));
            lines << QStringLiteral("  compareMode=%1").arg(ArtifactCore::toString(snapshot_.compareMode));
        } else {
            lines << QStringLiteral("  <no frame snapshot>");
        }

        report_->setPlainText(lines.join(QStringLiteral("\n")));
    }

    QString reportText() const
    {
        return report_ ? report_->toPlainText() : QString();
    }
};

DebugRenderHarnessWidget::DebugRenderHarnessWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    impl_->setupUI();
}

DebugRenderHarnessWidget::~DebugRenderHarnessWidget()
{
    delete impl_;
}

void DebugRenderHarnessWidget::setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot)
{
    if (!impl_) {
        return;
    }
    impl_->setFrameDebugSnapshot(snapshot);
}

void DebugRenderHarnessWidget::setScenePreset(const QString& presetName)
{
    if (!impl_) {
        return;
    }
    impl_->setScenePreset(presetName);
}

void DebugRenderHarnessWidget::keyPressEvent(QKeyEvent* event)
{
    if (!impl_ || !event) {
        return;
    }

    if (event->matches(QKeySequence::Copy)) {
        if (!impl_->reportText().isEmpty()) {
            QGuiApplication::clipboard()->setText(impl_->reportText());
            event->accept();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

} // namespace Artifact
