module;
#include <cmath>
#include <algorithm>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QLabel>
#include <QHBoxLayout>
#include <QListWidget>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QDir>
#include <QFileInfo>
#include <QPoint>
#include <QRect>
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
    QLabel* reportMeta_ = nullptr;
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
        auto* layout = new QVBoxLayout(owner_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        summary_ = new QLabel(owner_);
        summary_->setTextFormat(Qt::PlainText);
        summary_->setWordWrap(true);
        summary_->setMinimumHeight(48);
        layout->addWidget(summary_);

        auto* reportBar = new QWidget(owner_);
        auto* reportBarLayout = new QHBoxLayout(reportBar);
        reportBarLayout->setContentsMargins(8, 4, 8, 4);
        reportBarLayout->setSpacing(8);

        reportMeta_ = new QLabel(reportBar);
        reportMeta_->setTextFormat(Qt::PlainText);
        reportMeta_->setWordWrap(false);
        reportMeta_->setMinimumHeight(24);
        reportBarLayout->addWidget(reportMeta_);
        reportBarLayout->addStretch();

        copyReportButton_ = new QToolButton(reportBar);
        copyReportButton_->setText(QStringLiteral("Copy Report"));
        copyReportButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        copyReportButton_->setToolTip(QStringLiteral("Copy the current smoke report to the clipboard"));
        copyReportButton_->setFixedWidth(120);
        reportBarLayout->addWidget(copyReportButton_);

        saveReportButton_ = new QToolButton(reportBar);
        saveReportButton_->setText(QStringLiteral("Save Report"));
        saveReportButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        saveReportButton_->setToolTip(QStringLiteral("Save the current smoke report to a text file"));
        saveReportButton_->setFixedWidth(120);
        reportBarLayout->addWidget(saveReportButton_);

        layout->addWidget(reportBar);

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

        auto* rightSplitter = new QSplitter(Qt::Vertical, splitter);
        rightSplitter->setChildrenCollapsible(false);

        preview_ = new QLabel(rightSplitter);
        preview_->setAlignment(Qt::AlignCenter);
        preview_->setFrameShape(QFrame::StyledPanel);
        preview_->setFrameShadow(QFrame::Sunken);
        preview_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
        preview_->setMinimumHeight(260);
        preview_->setText(QStringLiteral("preview pending"));
        preview_->setWordWrap(true);

        report_ = new QPlainTextEdit(rightSplitter);
        report_->setReadOnly(true);
        report_->setLineWrapMode(QPlainTextEdit::NoWrap);
        report_->setFocusPolicy(Qt::StrongFocus);

        splitter->addWidget(presetList_);
        splitter->addWidget(rightSplitter);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 3);
        rightSplitter->setStretchFactor(0, 2);
        rightSplitter->setStretchFactor(1, 3);
        layout->addWidget(splitter);

        QObject::connect(copyReportButton_, &QToolButton::clicked, owner_, [this]() {
            copyReportToClipboard();
        });

        QObject::connect(saveReportButton_, &QToolButton::clicked, owner_, [this]() {
            saveReportToFile();
        });

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

    QImage renderParticleFixturePreview() const
    {
        ArtifactParticleLayer particleLayer;
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
                    *fixtureNote = QStringLiteral("fixture=video-only missing source");
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
            painter.drawText(canvas.rect().adjusted(18, 18, -18, -18),
                             Qt::AlignCenter | Qt::TextWordWrap,
                             QStringLiteral("Preview not available for this preset yet.\nSelect particle-only to inspect the live fixture."));
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
            preview_->setText(previewImage.isNull() ? fixtureNote : QString());
        }
        if (reportMeta_) {
            reportMeta_->setText(QStringLiteral("reportId=%1  status=%2  viewport=%3x%4")
                                     .arg(reportId)
                                     .arg(statusText)
                                     .arg(previewSize().width())
                                     .arg(previewSize().height()));
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

        summary_->setText(QStringLiteral("preset=%1  fixture=%2  frame=%3  comp=%4  layer=%5  particle=%6  video=%7  blend=%8")
                              .arg(scenePreset_)
                              .arg(previewImage.isNull() ? QStringLiteral("pending") : QStringLiteral("ready"))
                              .arg(hasSnapshot_ ? snapshot_.frame.framePosition() : -1)
                              .arg(hasSnapshot_ && !snapshot_.compositionName.isEmpty() ? snapshot_.compositionName : QStringLiteral("<none>"))
                              .arg(hasSnapshot_ && !snapshot_.selectedLayerName.isEmpty() ? snapshot_.selectedLayerName : QStringLiteral("<none>"))
                              .arg(particleState)
                              .arg(videoState)
                              .arg(blendState));

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
        lines << QStringLiteral("selectedLayer: %1").arg(hasSnapshot_ && !snapshot_.selectedLayerName.isEmpty()
                                                        ? snapshot_.selectedLayerName
                                                        : QStringLiteral("<none>"));
        lines << QStringLiteral("renderBackend: %1").arg(hasSnapshot_ && !snapshot_.renderBackend.isEmpty()
                                                       ? snapshot_.renderBackend
                                                       : QStringLiteral("<none>"));

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
        lines << QStringLiteral("particle: %1").arg(hasSnapshot_ ? particleStateText() : QStringLiteral("none"));
        lines << QStringLiteral("video: %1").arg(hasSnapshot_ ? videoStateText() : QStringLiteral("none"));
        lines << QStringLiteral("blend: %1").arg(hasSnapshot_ ? blendStateText() : QStringLiteral("none"));
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
        return mediaStateFromResource(snapshot_, QStringLiteral("video"), QStringLiteral("Video Decode"));
    }

    QString blendStateText() const
    {
        return mediaStateFromResource(snapshot_, QStringLiteral("composition"), QStringLiteral("Render Path"));
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
        return QStringLiteral("cache=%1/%2 hit (%3%%) stale=%4")
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

} // namespace Artifact
