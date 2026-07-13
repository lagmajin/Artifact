module;
#include <cmath>
#include <algorithm>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QAbstractScrollArea>
#include <QHBoxLayout>
#include <QListWidget>
#include <QClipboard>
#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPixmap>
#include <QDir>
#include <QFileInfo>
#include <QPoint>
#include <QRect>
#include <QPalette>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QStandardPaths>
#include <QToolButton>
#include <QTextStream>
#include <QStringList>
#include <memory>
#include <wobjectimpl.h>

module Artifact.Widgets.DebugRenderHarnessWidget;

import Frame.Debug;
import Artifact.Layer.Particle;
import Artifact.Layer.Video;
import Layer.BlendModeInfo;

namespace Artifact {

namespace {
void applyHarnessSurfacePalette(QWidget* root, const QPalette& palette)
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
            const QString note = resource.note.trimmed();
            if (note.startsWith(QStringLiteral("state="))) {
                return note.section(QChar::Space, 0, 0);
            }
            if (note.contains(QStringLiteral("skipped="))) {
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

QString resourceNoteFromResource(const ArtifactCore::FrameDebugSnapshot& snapshot,
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

QString diagnosticField(const QString& note, const QString& key,
                        const QString& fallback = QStringLiteral("-"))
{
    const QString prefix = key + QLatin1Char('=');
    const QStringList fields = note.split(QChar::Space, Qt::SkipEmptyParts);
    for (const QString& field : fields) {
        if (field.startsWith(prefix)) {
            return field.mid(prefix.size());
        }
    }
    return fallback;
}

QString perfBaselineText(const ArtifactCore::FrameDebugSnapshot& snapshot)
{
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
    lines << QStringLiteral("Perf Baseline");
    lines << QStringLiteral("composition: %1")
                 .arg(snapshot.compositionName.isEmpty()
                          ? QStringLiteral("<none>")
                          : snapshot.compositionName);
    lines << QStringLiteral("backend: %1")
                 .arg(snapshot.renderBackend.isEmpty()
                          ? QStringLiteral("<none>")
                          : snapshot.renderBackend);
    lines << QStringLiteral("layers: total=%1 visible=%2 text=%3")
                 .arg(snapshot.totalLayerCount)
                 .arg(snapshot.visibleLayerCount)
                 .arg(snapshot.textLayerCount);
    lines << QStringLiteral("timingMs: last=%1 avg=%2 gpu=%3")
                 .arg(QString::number(snapshot.renderLastFrameMs, 'f', 2))
                 .arg(QString::number(snapshot.renderAverageFrameMs, 'f', 2))
                 .arg(QString::number(snapshot.renderGpuFrameMs, 'f', 2));
    lines << QStringLiteral("renderCost: draw=%1 indexed=%2 pso=%3 srb=%4 bufferUpdates=%5")
                 .arg(static_cast<qulonglong>(snapshot.renderCost.drawCalls))
                 .arg(static_cast<qulonglong>(snapshot.renderCost.indexedDrawCalls))
                 .arg(static_cast<qulonglong>(snapshot.renderCost.psoSwitches))
                 .arg(static_cast<qulonglong>(snapshot.renderCost.srbCommits))
                 .arg(static_cast<qulonglong>(snapshot.renderCost.bufferUpdates));
    lines << QStringLiteral("perVisibleLayer: draw=%1 bufferUpdates=%2")
                 .arg(QString::number(drawPerVisibleLayer, 'f', 2))
                 .arg(QString::number(bufferUpdatesPerVisibleLayer, 'f', 2));
    lines << QStringLiteral("gpuTimer: %1")
                 .arg(gpuTimerLooksActive ? QStringLiteral("active-or-idle")
                                          : QStringLiteral("not-updating"));
    return lines.join(QStringLiteral("\n"));
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
    QLabel* reportMeta_ = nullptr;
    QLabel* healthState_ = nullptr;
    QLabel* healthReason_ = nullptr;
    QLabel* failureSummary_ = nullptr;
    QLabel* failureDetails_ = nullptr;
    QPlainTextEdit* overview_ = nullptr;
    QToolButton* copyReportButton_ = nullptr;
    QToolButton* saveReportButton_ = nullptr;
    PresetListWidget* presetList_ = nullptr;
    QLabel* preview_ = nullptr;
    QPlainTextEdit* report_ = nullptr;
    ArtifactCore::FrameDebugSnapshot snapshot_;
    QString scenePreset_ = QStringLiteral("particle-only");
    QString reportId_;
    QDateTime reportCreatedAt_;
    mutable std::unique_ptr<ArtifactVideoLayer> videoFixtureLayer_;
    mutable QString videoFixturePath_;
    bool hasSnapshot_ = false;

    explicit Impl(DebugRenderHarnessWidget* owner) : owner_(owner) {}

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
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        auto* header = new QFrame(owner_);
        header->setFrameShape(QFrame::StyledPanel);
        header->setFrameShadow(QFrame::Plain);
        header->setAutoFillBackground(true);
        QPalette headerPalette = palette;
        headerPalette.setColor(QPalette::Window, QColor::fromRgb(34, 37, 44));
        header->setPalette(headerPalette);
        auto* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(12, 8, 10, 8);
        headerLayout->setSpacing(10);

        auto* headerText = new QVBoxLayout();
        headerText->setContentsMargins(0, 0, 0, 0);
        headerText->setSpacing(2);
        auto* headerTitle = new QLabel(QStringLiteral("DEBUG RENDER HARNESS"), header);
        headerTitle->setPalette(headerPalette);
        QFont headerTitleFont = headerTitle->font();
        headerTitleFont.setBold(true);
        headerTitleFont.setPointSize(std::max(11, headerTitleFont.pointSize() + 2));
        headerTitle->setFont(headerTitleFont);
        headerText->addWidget(headerTitle);

        summary_ = new QLabel(header);
        summary_->setPalette(headerPalette);
        summary_->setTextFormat(Qt::PlainText);
        summary_->setWordWrap(false);
        summary_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        QFont summaryFont = summary_->font();
        summaryFont.setBold(true);
        summaryFont.setPointSize(std::max(10, summaryFont.pointSize() + 1));
        summary_->setFont(summaryFont);
        headerText->addWidget(summary_);

        reportMeta_ = new QLabel(header);
        reportMeta_->setPalette(palette);
        reportMeta_->setTextFormat(Qt::PlainText);
        reportMeta_->setWordWrap(false);
        reportMeta_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        headerText->addWidget(reportMeta_);
        headerLayout->addLayout(headerText, 1);

        copyReportButton_ = new QToolButton(header);
        copyReportButton_->setText(QStringLiteral("Copy Report"));
        copyReportButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        copyReportButton_->setToolTip(QStringLiteral("Copy the current smoke report to the clipboard"));
        copyReportButton_->setFixedWidth(108);
        headerLayout->addWidget(copyReportButton_);

        saveReportButton_ = new QToolButton(header);
        saveReportButton_->setText(QStringLiteral("Save Report"));
        saveReportButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        saveReportButton_->setToolTip(QStringLiteral("Save the current smoke report to a text file"));
        saveReportButton_->setFixedWidth(108);
        headerLayout->addWidget(saveReportButton_);

        layout->addWidget(header);

        auto* splitter = new QSplitter(Qt::Horizontal, owner_);
        splitter->setChildrenCollapsible(false);

        auto* presetPanel = new QFrame(splitter);
        presetPanel->setFrameShape(QFrame::StyledPanel);
        presetPanel->setFrameShadow(QFrame::Plain);
        auto* presetLayout = new QVBoxLayout(presetPanel);
        presetLayout->setContentsMargins(8, 8, 8, 8);
        presetLayout->setSpacing(6);
        auto* presetTitle = new QLabel(QStringLiteral("TEST SCENES"), presetPanel);
        QFont sectionFont = presetTitle->font();
        sectionFont.setBold(true);
        presetTitle->setFont(sectionFont);
        presetLayout->addWidget(presetTitle);

        presetList_ = new PresetListWidget(this, presetPanel);
        presetList_->addItems(QStringList{
            QStringLiteral("particle-only"),
            QStringLiteral("video-only"),
            QStringLiteral("blend-only"),
            QStringLiteral("overlay-only"),
            QStringLiteral("mixed-media")
        });
        presetList_->setMinimumWidth(150);
        presetList_->setCurrentRow(0);
        presetLayout->addWidget(presetList_, 1);

        auto* contentSplitter = new QSplitter(Qt::Vertical, splitter);
        contentSplitter->setChildrenCollapsible(true);

        auto* previewPanel = new QFrame(contentSplitter);
        previewPanel->setFrameShape(QFrame::StyledPanel);
        previewPanel->setFrameShadow(QFrame::Plain);
        auto* previewLayout = new QVBoxLayout(previewPanel);
        previewLayout->setContentsMargins(8, 8, 8, 8);
        previewLayout->setSpacing(6);
        auto* previewTitle = new QLabel(QStringLiteral("FRAME PREVIEW"), previewPanel);
        previewTitle->setFont(sectionFont);
        previewLayout->addWidget(previewTitle);

        preview_ = new QLabel(previewPanel);
        preview_->setAutoFillBackground(true);
        preview_->setAttribute(Qt::WA_StyledBackground, true);
        preview_->setPalette(palette);
        preview_->setAlignment(Qt::AlignCenter);
        preview_->setFrameShape(QFrame::StyledPanel);
        preview_->setFrameShadow(QFrame::Sunken);
        preview_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
        preview_->setMinimumSize(360, 240);
        preview_->setMinimumHeight(260);
        preview_->setText(QString());
        preview_->setWordWrap(true);
        previewLayout->addWidget(preview_, 1);

        auto* reportPanel = new QFrame(contentSplitter);
        reportPanel->setFrameShape(QFrame::StyledPanel);
        reportPanel->setFrameShadow(QFrame::Plain);
        auto* reportLayout = new QVBoxLayout(reportPanel);
        reportLayout->setContentsMargins(8, 8, 8, 8);
        reportLayout->setSpacing(6);
        auto* failureTitle = new QLabel(QStringLiteral("FAILURE SUMMARY"), reportPanel);
        failureTitle->setFont(sectionFont);
        reportLayout->addWidget(failureTitle);

        failureSummary_ = new QLabel(reportPanel);
        QFont failureFont = failureSummary_->font();
        failureFont.setBold(true);
        failureFont.setPointSize(std::max(11, failureFont.pointSize() + 2));
        failureSummary_->setFont(failureFont);
        failureSummary_->setWordWrap(true);
        reportLayout->addWidget(failureSummary_);

        failureDetails_ = new QLabel(reportPanel);
        failureDetails_->setWordWrap(true);
        failureDetails_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        reportLayout->addWidget(failureDetails_);

        auto* reportTitle = new QLabel(QStringLiteral("REPORT DETAILS"), reportPanel);
        reportTitle->setFont(sectionFont);
        reportLayout->addWidget(reportTitle);

        report_ = new QPlainTextEdit(reportPanel);
        report_->setPalette(palette);
        if (auto* viewport = report_->viewport()) {
            viewport->setAutoFillBackground(true);
            viewport->setPalette(palette);
        }
        report_->setReadOnly(true);
        report_->setLineWrapMode(QPlainTextEdit::NoWrap);
        report_->setFocusPolicy(Qt::StrongFocus);
        reportLayout->addWidget(report_, 1);

        auto* healthPanel = new QFrame(splitter);
        healthPanel->setFrameShape(QFrame::StyledPanel);
        healthPanel->setFrameShadow(QFrame::Plain);
        auto* healthLayout = new QVBoxLayout(healthPanel);
        healthLayout->setContentsMargins(10, 10, 10, 10);
        healthLayout->setSpacing(6);
        auto* healthTitle = new QLabel(QStringLiteral("FRAME HEALTH"), healthPanel);
        healthTitle->setFont(sectionFont);
        healthLayout->addWidget(healthTitle);

        healthState_ = new QLabel(QStringLiteral("WAITING"), healthPanel);
        QFont healthFont = healthState_->font();
        healthFont.setBold(true);
        healthFont.setPointSize(std::max(12, healthFont.pointSize() + 3));
        healthState_->setFont(healthFont);
        healthLayout->addWidget(healthState_);

        healthReason_ = new QLabel(healthPanel);
        healthReason_->setWordWrap(true);
        healthReason_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        healthLayout->addWidget(healthReason_);

        auto* overviewTitle = new QLabel(QStringLiteral("AT A GLANCE"), healthPanel);
        overviewTitle->setFont(sectionFont);
        healthLayout->addWidget(overviewTitle);
        overview_ = new QPlainTextEdit(healthPanel);
        overview_->setReadOnly(true);
        overview_->setFrameShape(QFrame::NoFrame);
        overview_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        overview_->setFocusPolicy(Qt::NoFocus);
        overview_->setPalette(palette);
        healthLayout->addWidget(overview_, 1);

        splitter->addWidget(presetPanel);
        splitter->addWidget(contentSplitter);
        splitter->addWidget(healthPanel);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setStretchFactor(2, 0);
        splitter->setSizes({170, 760, 300});
        contentSplitter->setStretchFactor(0, 3);
        contentSplitter->setStretchFactor(1, 2);
        contentSplitter->setSizes({470, 300});
        layout->addWidget(splitter, 1);

        QObject::connect(copyReportButton_, &QToolButton::clicked, owner_, [this]() {
            copyReportToClipboard();
        });

        QObject::connect(saveReportButton_, &QToolButton::clicked, owner_, [this]() {
            saveReportToFile();
        });

        owner_->ensurePolished();
        layout->activate();
        applyHarnessSurfacePalette(owner_, palette);
        refresh();
    }

    void updateReportIdentity()
    {
        const QString preset = scenePreset_.trimmed().isEmpty() ? QStringLiteral("particle-only") : scenePreset_.trimmed();
        reportCreatedAt_ = QDateTime::currentDateTime();
        reportId_ = QStringLiteral("%1-%2")
                        .arg(preset, reportCreatedAt_.toString(QStringLiteral("yyyyMMddHHmmss")));
    }

    QSize previewSize() const
    {
        if (preview_) {
            const QSize contentSize = preview_->contentsRect().size();
            if (contentSize.isValid() && contentSize.width() > 0 && contentSize.height() > 0) {
                return contentSize.expandedTo(QSize(320, 180)).boundedTo(QSize(1280, 720));
            }
        }
        return QSize(640, 360);
    }

    QString resolveVideoFixturePath() const
    {
        const QString overridePath = qEnvironmentVariable("ARTIFACT_DEBUG_VIDEO_FIXTURE");
        if (!overridePath.trimmed().isEmpty()) {
            const QFileInfo overrideInfo(overridePath);
            if (overrideInfo.exists() && overrideInfo.isFile()) {
                return overrideInfo.absoluteFilePath();
            }
        }

        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            QDir(appDir).filePath(QStringLiteral("comp1.mov")),
            QDir(appDir).filePath(QStringLiteral("Artifact/App/comp1.mov")),
            QDir(appDir).filePath(QStringLiteral("../Artifact/App/comp1.mov")),
            QDir(appDir).filePath(QStringLiteral("../App/comp1.mov")),
            QDir::current().filePath(QStringLiteral("Artifact/App/comp1.mov")),
            QDir::current().filePath(QStringLiteral("App/comp1.mov")),
            QDir::current().filePath(QStringLiteral("comp1.mov"))
        };
        for (const QString& candidate : candidates) {
            const QFileInfo info(candidate);
            if (info.exists() && info.isFile()) {
                return info.absoluteFilePath();
            }
        }
        return QString();
    }

    ArtifactVideoLayer* ensureVideoFixtureLayer() const
    {
        const QString path = resolveVideoFixturePath();
        if (path.isEmpty()) {
            videoFixtureLayer_.reset();
            videoFixturePath_.clear();
            return nullptr;
        }

        const bool needsReload = !videoFixtureLayer_ || videoFixturePath_ != path;
        if (needsReload) {
            videoFixtureLayer_ = std::make_unique<ArtifactVideoLayer>();
            videoFixturePath_ = path;
            videoFixtureLayer_->setSourceFile(path);
            videoFixtureLayer_->setInPoint(0);
        }
        return videoFixtureLayer_.get();
    }

    bool usesVideoFixturePreset() const
    {
        const QString preset = scenePreset_.trimmed();
        return preset == QStringLiteral("video-only") ||
               preset == QStringLiteral("mixed-media");
    }

    const ArtifactCore::FrameDebugResourceRecord* capturedVideoResource() const
    {
        if (!hasSnapshot_) {
            return nullptr;
        }
        for (const auto& resource : snapshot_.resources) {
            if (resource.type == QStringLiteral("video") ||
                resource.label == QStringLiteral("Video Decode")) {
                return &resource;
            }
        }
        return nullptr;
    }

    QString effectiveHarnessLayerName() const
    {
        if (!usesVideoFixturePreset()) {
            if (hasSnapshot_ && !snapshot_.selectedLayerName.isEmpty()) {
                return snapshot_.selectedLayerName;
            }
            return QStringLiteral("<none>");
        }

        const QString source = videoFixturePath_.isEmpty()
                                   ? resolveVideoFixturePath()
                                   : videoFixturePath_;
        auto* fixture = ensureVideoFixtureLayer();
        const QString fixtureSource =
            !source.isEmpty()
                ? source
                : (fixture ? fixture->sourceFile() : QString());
        if (fixtureSource.isEmpty()) {
            if (const auto* captured = capturedVideoResource()) {
                return captured->label.trimmed().isEmpty()
                           ? QStringLiteral("captured composition video")
                           : QStringLiteral("captured composition video (%1)")
                                 .arg(captured->label.trimmed());
            }
            return QStringLiteral("video fixture (<missing source>)");
        }

        const QString baseName = QFileInfo(fixtureSource).completeBaseName();
        return baseName.isEmpty()
                   ? QStringLiteral("video fixture")
                   : QStringLiteral("video fixture (%1)").arg(baseName);
    }

    QString videoFixtureDebugText() const
    {
        if (!usesVideoFixturePreset()) {
            return QStringLiteral("not-requested");
        }

        const QString source = videoFixturePath_.isEmpty()
                                   ? resolveVideoFixturePath()
                                   : videoFixturePath_;
        auto* fixture = ensureVideoFixtureLayer();
        if (!fixture) {
            if (const auto* captured = capturedVideoResource()) {
                const QString note = captured->note.trimmed();
                return QStringLiteral("state=captured-composition-video label=%1 %2")
                    .arg(captured->label.trimmed().isEmpty()
                             ? QStringLiteral("<unnamed>")
                             : captured->label.trimmed(),
                         note.isEmpty() ? QStringLiteral("note=<none>") : note);
            }
            return QStringLiteral("state=missing-source source=%1")
                .arg(source.isEmpty() ? QStringLiteral("<none>") : source);
        }

        const QString decodeState = fixture->decodeState().trimmed().isEmpty()
                                        ? QStringLiteral("unknown")
                                        : fixture->decodeState().trimmed();
        return QStringLiteral("state=%1 loaded=%2 source=%3 currentFrame=%4")
            .arg(decodeState)
            .arg(fixture->isLoaded() ? 1 : 0)
            .arg(fixture->sourceFile().isEmpty() ? QStringLiteral("<none>")
                                                 : fixture->sourceFile())
            .arg(fixture->currentFrame());
    }

    QImage renderParticleFixturePreview() const
    {
        ArtifactParticleDebugLayer particleLayer;
        particleLayer.loadPreset(QStringLiteral("fire"));
        particleLayer.setTimeScale(1.0f);

        const int frameSeed = hasSnapshot_ ? std::max(0, static_cast<int>(snapshot_.frame.framePosition())) : 30;
        const float warmupSeconds = std::clamp(static_cast<float>(frameSeed) / 30.0f, 0.35f, 1.75f);
        particleLayer.preWarm(warmupSeconds);
        QImage preview = particleLayer.renderFrame(640, 360, warmupSeconds);
        if (!preview.isNull()) {
            preview = preview.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
        return preview;
    }

    QImage renderBlendFixturePreview() const
    {
        QImage canvas(previewSize(), QImage::Format_ARGB32_Premultiplied);
        canvas.fill(QColor::fromRgb(18, 20, 25));

        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRect fullRect = canvas.rect().adjusted(20, 20, -20, -20);
        const QRect leftRect(fullRect.left() + 30, fullRect.top() + 40,
                             fullRect.width() / 2, fullRect.height() / 2);
        const QRect rightRect(fullRect.left() + fullRect.width() / 3,
                              fullRect.top() + fullRect.height() / 3,
                              fullRect.width() / 2, fullRect.height() / 2);

        painter.fillRect(canvas.rect(), QColor::fromRgb(34, 36, 42));

        for (int y = 0; y < canvas.height(); y += 18) {
            for (int x = 0; x < canvas.width(); x += 18) {
                const bool dark = ((x / 18) + (y / 18)) % 2 == 0;
                painter.fillRect(x, y, 18, 18, dark ? QColor::fromRgb(44, 48, 56) : QColor::fromRgb(56, 60, 70));
            }
        }

        painter.setBrush(QColor(100, 180, 255, 120));
        painter.setPen(QPen(QColor(180, 230, 255, 220), 3.0));
        painter.drawRoundedRect(leftRect, 16, 16);

        painter.setBrush(QColor(255, 118, 90, 110));
        painter.setPen(QPen(QColor(255, 184, 160, 220), 3.0));
        painter.drawRoundedRect(rightRect, 16, 16);

        painter.setCompositionMode(QPainter::CompositionMode_Screen);
        painter.fillRect(fullRect.adjusted(60, 60, -120, -120), QColor(255, 255, 255, 80));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        painter.setPen(QColor::fromRgb(232, 240, 248));
        painter.drawText(canvas.rect().adjusted(16, 16, -16, -16),
                         Qt::AlignTop | Qt::AlignLeft,
                         QStringLiteral("blend-only\ncheckerboard + overlap + screen composite"));

        painter.setPen(QColor::fromRgb(210, 220, 235));
        painter.drawText(canvas.rect().adjusted(16, 16, -16, -16),
                         Qt::AlignBottom | Qt::AlignRight,
                         QStringLiteral("visible vs transparent output"));
        return canvas;
    }

    QImage renderOverlayFixturePreview() const
    {
        QImage canvas(previewSize(), QImage::Format_ARGB32_Premultiplied);
        canvas.fill(QColor::fromRgb(20, 22, 27));

        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRect rect = canvas.rect().adjusted(20, 20, -20, -20);
        painter.fillRect(canvas.rect(), QColor::fromRgb(28, 30, 36));

        const int gridStep = 32;
        for (int x = rect.left(); x <= rect.right(); x += gridStep) {
            const bool major = (x - rect.left()) % (gridStep * 3) == 0;
            painter.setPen(QPen(major ? QColor(120, 185, 255, 220) : QColor(120, 185, 255, 110),
                                major ? 2.0 : 1.0));
            painter.drawLine(x, rect.top(), x, rect.bottom());
        }
        for (int y = rect.top(); y <= rect.bottom(); y += gridStep) {
            const bool major = (y - rect.top()) % (gridStep * 3) == 0;
            painter.setPen(QPen(major ? QColor(120, 185, 255, 220) : QColor(120, 185, 255, 110),
                                major ? 2.0 : 1.0));
            painter.drawLine(rect.left(), y, rect.right(), y);
        }

        const QPoint center = rect.center();
        painter.setPen(QPen(QColor(255, 255, 255, 240), 2.0));
        painter.drawLine(center.x() - 40, center.y(), center.x() + 40, center.y());
        painter.drawLine(center.x(), center.y() - 40, center.x(), center.y() + 40);
        painter.setBrush(QColor(255, 255, 255, 230));
        painter.drawEllipse(center, 7, 7);

        const QPoint anchor(center.x() + 88, center.y() + 42);
        painter.setPen(QPen(QColor(255, 230, 110, 240), 2.0));
        painter.drawLine(center, anchor);
        painter.setBrush(QColor(255, 230, 110, 210));
        painter.drawEllipse(anchor, 8, 8);

        painter.setPen(QColor::fromRgb(232, 240, 248));
        painter.drawText(canvas.rect().adjusted(16, 16, -16, -16),
                         Qt::AlignTop | Qt::AlignLeft,
                         QStringLiteral("overlay-only\nrule-of-thirds + anchor + bounds"));

        painter.setPen(QColor::fromRgb(210, 220, 235));
        painter.drawText(canvas.rect().adjusted(16, 16, -16, -16),
                         Qt::AlignBottom | Qt::AlignRight,
                         QStringLiteral("guidance visible without media"));
        return canvas;
    }

    QImage renderPreviewImage(QString* fixtureNote = nullptr) const
    {
        const QString preset = scenePreset_.trimmed();
        if (preset == QStringLiteral("particle-only")) {
            if (fixtureNote) {
                *fixtureNote = QStringLiteral("fixture=particle-only builtin=fire warmup=prewarm");
            }
            return renderParticleFixturePreview();
        }

        if (preset == QStringLiteral("video-only")) {
            auto* videoLayer = ensureVideoFixtureLayer();
            if (!videoLayer) {
                if (fixtureNote) {
                    *fixtureNote = capturedVideoResource()
                                       ? QStringLiteral("fixture=video-only source=captured-composition")
                                       : QStringLiteral("fixture=video-only missing source");
                }
                return QImage();
            }

            const QString source = videoFixturePath_.isEmpty() ? resolveVideoFixturePath() : videoFixturePath_;
            if (fixtureNote) {
                *fixtureNote = QStringLiteral("fixture=video-only source=%1").arg(source);
            }
            if (!videoLayer->isLoaded()) {
                return QImage();
            }

            QImage frame = videoLayer->decodeFrameToQImage(0);
            if (frame.isNull()) {
                frame = videoLayer->currentFrameToQImage();
            }
            if (!frame.isNull()) {
                return frame.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            }
            return QImage();
        }

        if (preset == QStringLiteral("blend-only")) {
            if (fixtureNote) {
                *fixtureNote = QStringLiteral("fixture=blend-only synthetic=composite");
            }
            return renderBlendFixturePreview();
        }

        if (preset == QStringLiteral("overlay-only")) {
            if (fixtureNote) {
                *fixtureNote = QStringLiteral("fixture=overlay-only synthetic=guides");
            }
            return renderOverlayFixturePreview();
        }

        if (preset == QStringLiteral("mixed-media")) {
            if (fixtureNote) {
                *fixtureNote = QStringLiteral("fixture=mixed-media synthetic=particle+video+overlay");
            }
            QImage canvas(previewSize(), QImage::Format_ARGB32_Premultiplied);
            canvas.fill(QColor::fromRgb(16, 18, 23));
            QPainter painter(&canvas);
            painter.drawImage(QRect(16, 16, canvas.width() / 2 - 24, canvas.height() / 2 - 24),
                              renderParticleFixturePreview());
            painter.drawImage(QRect(canvas.width() / 2 + 8, 16, canvas.width() / 2 - 24, canvas.height() / 2 - 24),
                              renderBlendFixturePreview());
            painter.drawImage(QRect(16, canvas.height() / 2 + 8, canvas.width() - 32, canvas.height() / 2 - 24),
                              renderOverlayFixturePreview());
            painter.setPen(QColor::fromRgb(232, 240, 248));
            painter.drawText(canvas.rect().adjusted(16, 16, -16, -16),
                             Qt::AlignTop | Qt::AlignLeft,
                             QStringLiteral("mixed-media"));
            return canvas;
        }

        if (fixtureNote) {
            *fixtureNote = QStringLiteral("fixture=%1 pending").arg(preset.isEmpty() ? QStringLiteral("<none>") : preset);
        }
        return QImage();
    }

    QImage composePreviewCanvas(const QImage& sourceImage, const QSize& targetSize) const
    {
        const QSize canvasSize = targetSize.isValid() && targetSize.width() > 0 && targetSize.height() > 0
            ? targetSize
            : QSize(640, 360);
        QImage canvas(canvasSize, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(QColor::fromRgb(24, 26, 31));

        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const int checker = 18;
        for (int y = 0; y < canvas.height(); y += checker) {
            for (int x = 0; x < canvas.width(); x += checker) {
                const bool dark = ((x / checker) + (y / checker)) % 2 == 0;
                painter.fillRect(x, y, checker, checker,
                                 dark ? QColor::fromRgb(34, 37, 43) : QColor::fromRgb(44, 48, 56));
            }
        }

        if (!sourceImage.isNull()) {
            const QPixmap pixmap = QPixmap::fromImage(sourceImage);
            const QSize scaledSize = pixmap.size().scaled(canvas.size() - QSize(24, 24), Qt::KeepAspectRatio);
            const QRect targetRect(QPoint((canvas.width() - scaledSize.width()) / 2,
                                          (canvas.height() - scaledSize.height()) / 2),
                                   scaledSize);
            painter.fillRect(targetRect.adjusted(-1, -1, 1, 1), QColor::fromRgb(12, 12, 12, 180));
            painter.drawPixmap(targetRect, pixmap);
            painter.setPen(QColor::fromRgb(210, 220, 235));
            painter.drawRect(targetRect.adjusted(-1, -1, 0, 0));
        } else {
            painter.setPen(QColor::fromRgb(220, 228, 236));
            const bool capturedVideoAvailable =
                usesVideoFixturePreset() && capturedVideoResource();
            painter.drawText(canvas.rect().adjusted(18, 18, -18, -18),
                             Qt::AlignCenter | Qt::TextWordWrap,
                             capturedVideoAvailable
                                 ? QStringLiteral("Video diagnostics captured.\nNo frame preview attachment is available.")
                                 : QStringLiteral("Preview unavailable.\nProvide a fixture source or capture a frame attachment."));
        }

        return canvas;
    }

    void setScenePreset(const QString& presetName)
    {
        const QString normalized = presetReportName(presetName);
        const bool presetChanged = normalized != scenePreset_;
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
        if (presetChanged || reportId_.isEmpty()) {
            updateReportIdentity();
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
        const QString normalized = presetList_->item(row)->text();
        const bool presetChanged = normalized != scenePreset_;
        scenePreset_ = normalized;
        if (presetChanged || reportId_.isEmpty()) {
            updateReportIdentity();
        }
        refresh();
    }

    void setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot)
    {
        snapshot_ = snapshot;
        hasSnapshot_ = true;
        if (reportId_.isEmpty()) {
            updateReportIdentity();
        }
        refresh();
    }

    void refresh()
    {
        if (!summary_ || !report_) {
            return;
        }

        QString fixtureNote;
        const QImage previewImage = renderPreviewImage(&fixtureNote);
        if (reportId_.isEmpty()) {
            updateReportIdentity();
        }
        const QString reportId = reportId_;
        const QString statusText = previewImage.isNull()
                                       ? QStringLiteral("pending")
                                       : (hasSnapshot_ && snapshot_.failed ? QStringLiteral("failed")
                                                                          : QStringLiteral("ok"));
        const QString reportText = buildReportText(reportId, statusText, fixtureNote, previewImage);
        if (preview_) {
            const QImage canvas = composePreviewCanvas(previewImage, previewSize());
            preview_->setPixmap(QPixmap::fromImage(canvas));
            preview_->setText(QString());
            preview_->update();
        }
        if (reportMeta_) {
            reportMeta_->setText(QStringLiteral("Preset %1  |  %2  |  %3  |  viewport %4x%5")
                                     .arg(scenePreset_)
                                     .arg(statusText)
                                     .arg(reportId)
                                     .arg(previewSize().width())
                                     .arg(previewSize().height()));
        }

        const QString particleState = hasSnapshot_
            ? mediaStateFromResource(snapshot_, QStringLiteral("particle"), QStringLiteral("Particle Draw"))
            : QStringLiteral("none");
        const QString textState = hasSnapshot_
            ? mediaStateFromResource(snapshot_, QStringLiteral("text"), QStringLiteral("Glyph Atlas"))
            : QStringLiteral("none");
        const QString videoState = hasSnapshot_
            ? videoStateText()
            : QStringLiteral("none");
        const QString blendState = hasSnapshot_
            ? mediaStateFromResource(snapshot_, QStringLiteral("composition"), QStringLiteral("Render Path"))
            : QStringLiteral("none");
        const QString glyphState = hasSnapshot_
            ? mediaStateFromResource(snapshot_, QStringLiteral("glyphAtlas"), QStringLiteral("Glyph Atlas"))
            : QStringLiteral("none");
        const QString effectiveLayerName = effectiveHarnessLayerName();

        const QString compositionName =
            hasSnapshot_ && !snapshot_.compositionName.isEmpty()
                ? snapshot_.compositionName
                : QStringLiteral("<no composition>");
        summary_->setText(QStringLiteral("%1  |  Frame %2  |  %3")
                              .arg(compositionName)
                              .arg(hasSnapshot_ ? snapshot_.frame.framePosition() : -1)
                              .arg(effectiveLayerName));

        if (healthState_) {
            const QString healthText =
                hasSnapshot_ && snapshot_.failed
                    ? QStringLiteral("FAILED")
                    : (previewImage.isNull() ? QStringLiteral("NEEDS ATTENTION")
                                             : QStringLiteral("READY"));
            healthState_->setText(healthText);
            QPalette healthPalette = healthState_->palette();
            healthPalette.setColor(
                QPalette::WindowText,
                hasSnapshot_ && snapshot_.failed
                    ? QColor::fromRgb(255, 105, 105)
                    : (previewImage.isNull() ? QColor::fromRgb(255, 196, 92)
                                             : QColor::fromRgb(106, 218, 148)));
            healthState_->setPalette(healthPalette);
        }
        if (healthReason_) {
            healthReason_->setText(
                hasSnapshot_ && snapshot_.failed
                    ? (snapshot_.failureReason.isEmpty()
                           ? QStringLiteral("The captured frame reported a failure.")
                           : snapshot_.failureReason)
                    : (previewImage.isNull()
                           ? QStringLiteral("Diagnostics are available, but the preview image is missing.")
                    : QStringLiteral("Preview and frame diagnostics are available.")));
        }
        if (failureSummary_ && failureDetails_) {
            const bool failed = hasSnapshot_ && snapshot_.failed;
            const bool missingPreview = previewImage.isNull();
            const QString frameText = hasSnapshot_
                ? QString::number(snapshot_.frame.framePosition())
                : QStringLiteral("-");
            const QString backendText = hasSnapshot_ && !snapshot_.renderBackend.isEmpty()
                ? snapshot_.renderBackend
                : QStringLiteral("<none>");
            const QString cacheText = hasSnapshot_ ? cacheHealthText() : QStringLiteral("unknown");
            const QString videoText = hasSnapshot_ ? videoStateText() : QStringLiteral("none");
            const QString headline = failed
                ? QStringLiteral("FAILED — %1")
                      .arg(snapshot_.failureReason.isEmpty()
                               ? QStringLiteral("Captured frame reported a failure")
                               : snapshot_.failureReason)
                : (missingPreview
                       ? QStringLiteral("NEEDS ATTENTION — Preview unavailable")
                       : QStringLiteral("READY — Preview and diagnostics captured"));
            failureSummary_->setText(headline);
            failureDetails_->setText(
                failed
                    ? QStringLiteral("Frame %1  |  %2  |  videoState=%3  |  cacheHealth=%4\nThe frame was captured, but one or more render resources did not complete. Use Report Details to identify the first failing state.")
                          .arg(frameText, backendText, videoText, cacheText)
                    : (missingPreview
                           ? QStringLiteral("Diagnostics are available, but the preview image is missing.")
                           : QStringLiteral("Frame %1  |  %2\nUse Copy Report or Save Report to share this capture with the frame debugger.")
                                 .arg(frameText, backendText)));
            QPalette failurePalette = failureSummary_->palette();
            failurePalette.setColor(QPalette::WindowText,
                                    failed ? QColor::fromRgb(255, 105, 105)
                                           : (missingPreview ? QColor::fromRgb(255, 196, 92)
                                                             : QColor::fromRgb(106, 218, 148)));
            failureSummary_->setPalette(failurePalette);
        }
        if (overview_) {
            const auto* videoResource = capturedVideoResource();
            const QString videoNote =
                videoResource ? videoResource->note.trimmed() : QString();
            QStringList overviewLines;
            overviewLines
                << QStringLiteral("SCENE")
                << QStringLiteral("Preset      %1").arg(scenePreset_)
                << QStringLiteral("Backend     %1")
                       .arg(hasSnapshot_ && !snapshot_.renderBackend.isEmpty()
                                ? snapshot_.renderBackend
                                : QStringLiteral("<none>"))
                << QString()
                << QStringLiteral("MEDIA")
                << QStringLiteral("Video       %1").arg(videoState)
                << QStringLiteral("Decoder     %1")
                       .arg(diagnosticField(videoNote, QStringLiteral("backend")))
                << QStringLiteral("Frames      %1 -> %2")
                       .arg(diagnosticField(videoNote, QStringLiteral("source")),
                            diagnosticField(videoNote, QStringLiteral("target")))
                << QStringLiteral("Decode      %1 ms")
                       .arg(diagnosticField(videoNote, QStringLiteral("decodeMs")))
                << QStringLiteral("Late by     %1 ms")
                       .arg(diagnosticField(videoNote, QStringLiteral("lateByMs")))
                << QStringLiteral("On time     %1%")
                       .arg(diagnosticField(videoNote, QStringLiteral("onTimePct")))
                << QStringLiteral("Blend       %1").arg(blendState)
                << QStringLiteral("Text        %1").arg(textState)
                << QStringLiteral("Particles   %1").arg(particleState)
                << QStringLiteral("Glyphs      %1").arg(glyphState)
                << QString()
                << QStringLiteral("PERFORMANCE")
                << QStringLiteral("Last        %1 ms")
                       .arg(hasSnapshot_
                                ? QString::number(snapshot_.renderLastFrameMs, 'f', 1)
                                : QStringLiteral("-"))
                << QStringLiteral("Average     %1 ms")
                       .arg(hasSnapshot_
                                ? QString::number(snapshot_.renderAverageFrameMs, 'f', 1)
                                : QStringLiteral("-"))
                << QStringLiteral("GPU         %1 ms")
                       .arg(hasSnapshot_
                                ? QString::number(snapshot_.renderGpuFrameMs, 'f', 1)
                                : QStringLiteral("-"))
                << QString()
                << QStringLiteral("CAPTURE")
                << QStringLiteral("Resources   %1")
                       .arg(hasSnapshot_
                                ? static_cast<int>(snapshot_.resources.size())
                                : 0)
                << QStringLiteral("Passes      %1")
                       .arg(hasSnapshot_
                                ? static_cast<int>(snapshot_.passes.size())
                                : 0)
                << QStringLiteral("Preview     %1")
                       .arg(previewImage.isNull() ? QStringLiteral("missing")
                                                  : QStringLiteral("ready"));
            overview_->setPlainText(overviewLines.join(QChar::LineFeed));
        }

        report_->setPlainText(reportText);
    }

    QString reportText() const
    {
        return report_ ? report_->toPlainText() : QString();
    }

    QString buildReportText(const QString& reportId,
                            const QString& statusText,
                            const QString& fixtureNote,
                            const QImage& previewImage) const
    {
        const QString preset = scenePreset_.trimmed().isEmpty() ? QStringLiteral("particle-only") : scenePreset_.trimmed();
        const QString viewport = QStringLiteral("%1x%2").arg(previewSize().width()).arg(previewSize().height());
        const QString createdAt = reportCreatedAt_.isValid()
                                      ? reportCreatedAt_.toString(Qt::ISODateWithMs)
                                      : QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        const QString cacheHealth = hasSnapshot_ ? cacheHealthText() : QStringLiteral("cache=unknown");
        const QString resourceNotes = hasSnapshot_ ? resourceNotesText() : QStringLiteral("resourceNotes=<no snapshot>");
        const QString skippedReasons = hasSnapshot_ ? skippedReasonsText() : QStringLiteral("skippedReasons=<no snapshot>");
        const QString particleDetail = hasSnapshot_
            ? resourceNoteFromResource(snapshot_, QStringLiteral("particle"), QStringLiteral("Particle Draw"))
            : QStringLiteral("none");
        const QString effectiveLayerName = effectiveHarnessLayerName();
        const QString videoFixtureDebug = videoFixtureDebugText();
        const QString shortReason = hasSnapshot_ && snapshot_.failed && !snapshot_.failureReason.isEmpty()
                                        ? snapshot_.failureReason
                                        : QStringLiteral("none");

        QStringList lines;
        lines << QStringLiteral("Debug Render Harness");
        lines << QStringLiteral("reportId: %1").arg(reportId);
        lines << QStringLiteral("createdAt: %1").arg(createdAt);
        lines << QStringLiteral("preset: %1").arg(preset);
        lines << QStringLiteral("frame: %1").arg(hasSnapshot_ ? snapshot_.frame.framePosition() : -1);
        lines << QStringLiteral("composition: %1").arg(hasSnapshot_ && !snapshot_.compositionName.isEmpty()
                                                       ? snapshot_.compositionName
                                                       : QStringLiteral("<none>"));
        lines << QStringLiteral("selectedLayer: %1").arg(effectiveLayerName);
        lines << QStringLiteral("renderBackend: %1").arg(hasSnapshot_ && !snapshot_.renderBackend.isEmpty()
                                                       ? snapshot_.renderBackend
                                                       : QStringLiteral("<none>"));
        lines << QStringLiteral("textState: %1").arg(hasSnapshot_ ? textStateText() : QStringLiteral("none"));
        lines << QStringLiteral("videoState: %1").arg(hasSnapshot_ ? videoStateText() : QStringLiteral("none"));
        lines << QStringLiteral("particleState: %1").arg(hasSnapshot_ ? particleStateText() : QStringLiteral("none"));
        lines << QStringLiteral("particleDetail: %1").arg(particleDetail);
        lines << QStringLiteral("blendState: %1").arg(hasSnapshot_ ? blendStateText() : QStringLiteral("none"));
        lines << QStringLiteral("blendMaskContract: %1").arg(hasSnapshot_ ? blendMaskContractText() : QStringLiteral("none"));
        lines << QStringLiteral("glyphState: %1").arg(hasSnapshot_ ? glyphStateText() : QStringLiteral("none"));
        if (usesVideoFixturePreset()) {
            lines << QStringLiteral("videoFixture: %1").arg(videoFixtureDebug);
        }

        lines << QString();
        lines << QStringLiteral("Summary");
        lines << QStringLiteral("status: %1").arg(statusText);
        lines << QStringLiteral("shortReason: %1").arg(shortReason);
        lines << QStringLiteral("viewport: %1").arg(viewport);
        lines << QStringLiteral("rtvState: %1").arg(previewImage.isNull() ? QStringLiteral("missing")
                                                                           : QStringLiteral("ready"));
        lines << QStringLiteral("cacheHealth: %1").arg(cacheHealth);

        lines << QString();
        lines << QStringLiteral("Media States");
        lines << QStringLiteral("particleState: %1").arg(hasSnapshot_ ? particleStateText() : QStringLiteral("none"));
        lines << QStringLiteral("particleDetail: %1").arg(particleDetail);
        lines << QStringLiteral("textState: %1").arg(hasSnapshot_ ? textStateText() : QStringLiteral("none"));
        lines << QStringLiteral("videoState: %1").arg(hasSnapshot_ ? videoStateText() : QStringLiteral("none"));
        lines << QStringLiteral("blendState: %1").arg(hasSnapshot_ ? blendStateText() : QStringLiteral("none"));
        lines << QStringLiteral("blendMaskContract: %1").arg(hasSnapshot_ ? blendMaskContractText() : QStringLiteral("none"));
        lines << QStringLiteral("glyphState: %1").arg(hasSnapshot_ ? glyphStateText() : QStringLiteral("none"));
        if (usesVideoFixturePreset()) {
            lines << QStringLiteral("videoFixture: %1").arg(videoFixtureDebug);
        }
        lines << QStringLiteral("viewportNotes: %1").arg(previewImage.isNull()
                                                            ? QStringLiteral("preview missing or not yet rendered")
                                                            : QStringLiteral("preview available"));

        lines << QString();
        lines << QStringLiteral("Diagnostics");
        if (hasSnapshot_) {
            lines << QStringLiteral("traceFrames: %1").arg(static_cast<int>(snapshot_.passes.size()));
            lines << QStringLiteral("resourceNotes: %1").arg(resourceNotes);
            lines << QStringLiteral("skippedReasons: %1").arg(skippedReasons);
            lines << QStringLiteral("compareMode: %1").arg(ArtifactCore::toString(snapshot_.compareMode));
            if (!snapshot_.compareTargetId.isEmpty()) {
                lines << QStringLiteral("compareTarget: %1").arg(snapshot_.compareTargetId);
            }
            lines << QStringLiteral("resources: %1").arg(static_cast<int>(snapshot_.resources.size()));
            lines << QStringLiteral("attachments: %1").arg(static_cast<int>(snapshot_.attachments.size()));
        } else {
            lines << QStringLiteral("resourceNotes: <no snapshot>");
            lines << QStringLiteral("skippedReasons: <no snapshot>");
        }

        const QString blendCoverage = BlendModeCatalog::coverageReport().trimmed();
        lines << QStringLiteral("blendCoverage: %1 lines")
                     .arg(blendCoverage.isEmpty() ? 0 : blendCoverage.count(QChar::LineFeed) + 1);
        if (!blendCoverage.isEmpty()) {
            lines << QStringLiteral("blendCoveragePreview:");
            const QStringList blendLines = blendCoverage.split(QChar::LineFeed, Qt::SkipEmptyParts);
            const int previewLines = std::min(6, static_cast<int>(blendLines.size()));
            for (int i = 0; i < previewLines; ++i) {
                lines << QStringLiteral("  %1").arg(blendLines.at(i));
            }
        }

        lines << QString();
        lines << QStringLiteral("Scene Contract");
        lines << QStringLiteral("particle-only: one emitter over dark/light backgrounds");
        lines << QStringLiteral("video-only: short MP4, frame 0 and mid-frame seek");
        lines << QStringLiteral("blend-only: visible vs transparent output");
        lines << QStringLiteral("overlay-only: grid / anchor / guidance visible without media");
        lines << QStringLiteral("mixed-media: all paths present in one report");
        lines << QStringLiteral("fixture: %1").arg(fixtureNote.isEmpty() ? QStringLiteral("pending") : fixtureNote);

        lines << QString();
        lines << QStringLiteral("Capture Notes");
        if (hasSnapshot_) {
            lines << QStringLiteral("timestampMs: %1").arg(snapshot_.timestampMs);
            lines << QStringLiteral("playbackState: %1").arg(snapshot_.playbackState.isEmpty() ? QStringLiteral("<none>") : snapshot_.playbackState);
            lines << QStringLiteral("renderLastFrameMs: %1").arg(QString::number(snapshot_.renderLastFrameMs, 'f', 1));
            lines << QStringLiteral("renderAverageFrameMs: %1").arg(QString::number(snapshot_.renderAverageFrameMs, 'f', 1));
            lines << QStringLiteral("renderGpuFrameMs: %1").arg(QString::number(snapshot_.renderGpuFrameMs, 'f', 1));
            lines << QStringLiteral("renderCost: draw=%1 indexed=%2 pso=%3 srb=%4 buf=%5")
                          .arg(static_cast<qulonglong>(snapshot_.renderCost.drawCalls))
                          .arg(static_cast<qulonglong>(snapshot_.renderCost.indexedDrawCalls))
                          .arg(static_cast<qulonglong>(snapshot_.renderCost.psoSwitches))
                          .arg(static_cast<qulonglong>(snapshot_.renderCost.srbCommits))
                          .arg(static_cast<qulonglong>(snapshot_.renderCost.bufferUpdates));
            if (!snapshot_.failureReason.isEmpty()) {
                lines << QStringLiteral("failureReason: %1").arg(snapshot_.failureReason);
            }
            lines << QString();
            lines << perfBaselineText(snapshot_);
        } else {
            lines << QStringLiteral("<no frame snapshot>");
        }

        return lines.join(QStringLiteral("\n"));
    }

    QString particleStateText() const
    {
        return mediaStateFromResource(snapshot_, QStringLiteral("particle"), QStringLiteral("Particle Draw"));
    }

    QString videoStateText() const
    {
        const QString snapshotState =
            mediaStateFromResource(snapshot_, QStringLiteral("video"),
                                   QStringLiteral("Video Decode"));
        if (snapshotState != QStringLiteral("none")) {
            return snapshotState;
        }

        const QString preset = scenePreset_.trimmed();
        if (preset != QStringLiteral("video-only") &&
            preset != QStringLiteral("mixed-media")) {
            return QStringLiteral("none");
        }

        auto* fixture = ensureVideoFixtureLayer();
        if (!fixture) {
            return QStringLiteral("fixture-missing source=unresolved");
        }
        const QString state = fixture->decodeState().trimmed();
        return state.isEmpty() ? QStringLiteral("fixture-present state=unknown")
                               : QStringLiteral("fixture %1").arg(state);
    }

    QString textStateText() const
    {
        return mediaStateFromResource(snapshot_, QStringLiteral("text"), QStringLiteral("Glyph Atlas"));
    }

    QString blendStateText() const
    {
        const QString blendMaskState =
            mediaStateFromResource(snapshot_, QStringLiteral("blendMask"),
                                   QStringLiteral("Blend / Mask Contract"));
        if (blendMaskState != QStringLiteral("none")) {
            return blendMaskState;
        }
        return mediaStateFromResource(snapshot_, QStringLiteral("composition"),
                                      QStringLiteral("Render Path"));
    }

    QString blendMaskContractText() const
    {
        for (const auto& resource : snapshot_.resources) {
            if (resource.type == QStringLiteral("blendMask") ||
                resource.label == QStringLiteral("Blend / Mask Contract")) {
                return resource.note.trimmed().isEmpty()
                           ? QStringLiteral("present")
                           : resource.note.trimmed();
            }
        }
        return QStringLiteral("none");
    }

    QString glyphStateText() const
    {
        return mediaStateFromResource(snapshot_, QStringLiteral("glyphAtlas"), QStringLiteral("Glyph Atlas"));
    }

    QString cacheHealthText() const
    {
        if (snapshot_.resources.empty()) {
            return QStringLiteral("cache=unknown");
        }
        int hitCount = 0;
        int staleCount = 0;
        for (const auto& resource : snapshot_.resources) {
            if (resource.cacheHit) {
                ++hitCount;
            }
            if (resource.stale) {
                ++staleCount;
            }
        }
        const int total = static_cast<int>(snapshot_.resources.size());
        const int hitPercent = total > 0 ? static_cast<int>(std::lround((hitCount * 100.0) / total)) : 0;
        return QStringLiteral("cache=%1/%2 hit (%3%) stale=%4")
                .arg(hitCount)
                .arg(total)
                .arg(hitPercent)
                .arg(staleCount);
    }

    QString resourceNotesText() const
    {
        QStringList notes;
        for (const auto& resource : snapshot_.resources) {
            const QString note = resource.note.trimmed();
            if (!note.isEmpty()) {
                notes << QStringLiteral("%1:%2").arg(resource.type.isEmpty() ? resource.label : resource.type, note);
            }
        }
        return notes.isEmpty() ? QStringLiteral("none") : notes.join(QStringLiteral(" | "));
    }

    QString skippedReasonsText() const
    {
        QStringList reasons;
        for (const auto& pass : snapshot_.passes) {
            if (pass.status == ArtifactCore::FrameDebugPassStatus::Skipped && !pass.note.trimmed().isEmpty()) {
                reasons << QStringLiteral("%1:%2").arg(pass.name, pass.note.trimmed());
            }
        }
        for (const auto& resource : snapshot_.resources) {
            if (resource.note.contains(QStringLiteral("skipped="))) {
                reasons << QStringLiteral("%1:%2").arg(resource.label.isEmpty() ? resource.type : resource.label, resource.note.trimmed());
            }
        }
        return reasons.isEmpty() ? QStringLiteral("none") : reasons.join(QStringLiteral(" | "));
    }

    void copyReportToClipboard() const
    {
        const QString text = reportText();
        if (!text.isEmpty()) {
            QGuiApplication::clipboard()->setText(text);
        }
    }

    bool saveReportToFile() const
    {
        const QString text = reportText();
        if (text.isEmpty()) {
            return false;
        }

        const QString preset = scenePreset_.trimmed().isEmpty() ? QStringLiteral("particle-only") : scenePreset_.trimmed();
        const QString defaultName = QStringLiteral("debug_render_%1_%2.txt")
                                        .arg(preset,
                                             QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
        const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        const QString initialPath = QDir(defaultDir.isEmpty() ? QDir::currentPath() : defaultDir).filePath(defaultName);
        const QString path = QFileDialog::getSaveFileName(
            owner_, QStringLiteral("Save Debug Render Report"), initialPath,
            QStringLiteral("Text Files (*.txt);;All Files (*.*)"));
        if (path.trimmed().isEmpty()) {
            return false;
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }

        QTextStream stream(&file);
        stream << text;
        return true;
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
            impl_->copyReportToClipboard();
            event->accept();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void DebugRenderHarnessWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));
}

void DebugRenderHarnessWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    applyHarnessSurfacePalette(this, palette());
    if (impl_) {
        impl_->refresh();
    }
}

void DebugRenderHarnessWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (impl_) {
        impl_->refresh();
    }
}

} // namespace Artifact
