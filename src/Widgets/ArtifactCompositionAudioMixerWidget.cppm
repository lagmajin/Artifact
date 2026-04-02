module;

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLinearGradient>
#include <QPushButton>
#include <QPainter>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionAudioMixer;

import Artifact.Widgets.CompositionAudioMixer;
import Artifact.Audio.Mixer;
import Artifact.Composition.Abstract;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import std;

namespace Artifact
{

namespace
{
float sliderValueToVolume(const int value)
{
    return std::clamp(static_cast<float>(value) / 100.0f, 0.0f, 2.0f);
}

int volumeToPercent(const float volume)
{
    return static_cast<int>(std::lround(std::clamp(volume, 0.0f, 2.0f) * 100.0f));
}

float volumeToMeterDb(const float volume)
{
    if (volume <= 0.0001f) {
        return -60.0f;
    }
    return std::clamp(20.0f * std::log10(volume), -60.0f, 0.0f);
}

class AudioLevelMeterWidget final : public QWidget
{
public:
    explicit AudioLevelMeterWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(26, 224);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void setLevels(const float left, const float right)
    {
        const float clampedLeft = std::clamp(left, -60.0f, 0.0f);
        const float clampedRight = std::clamp(right, -60.0f, 0.0f);
        if (qFuzzyCompare(left_ + 61.0f, clampedLeft + 61.0f) &&
            qFuzzyCompare(right_ + 61.0f, clampedRight + 61.0f)) {
            return;
        }
        left_ = clampedLeft;
        right_ = clampedRight;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF bounds = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QColor(51, 60, 69));
        painter.setBrush(QColor(15, 18, 22));
        painter.drawRoundedRect(bounds, 4.0, 4.0);

        const qreal outerMargin = 1.0;
        const qreal gap = 1.0;
        const qreal laneWidth = (width() - (outerMargin * 2.0) - gap) / 2.0;
        drawLane(&painter, QRectF(outerMargin, outerMargin, laneWidth, height() - (outerMargin * 2.0)), left_);
        drawLane(&painter, QRectF(outerMargin + laneWidth + gap, outerMargin, laneWidth, height() - (outerMargin * 2.0)), right_);
    }

private:
    static float meterFraction(const float db)
    {
        return std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
    }

    static void drawLane(QPainter* painter, const QRectF& rect, const float db)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(22, 28, 34));
        painter->drawRoundedRect(rect, 3.0, 3.0);

        QRectF fillRect = rect;
        fillRect.setTop(rect.bottom() - rect.height() * meterFraction(db));
        if (fillRect.height() <= 0.0f) {
            return;
        }

        QLinearGradient gradient(fillRect.bottomLeft(), fillRect.topLeft());
        gradient.setColorAt(0.0, QColor(53, 176, 111));
        gradient.setColorAt(0.68, QColor(218, 187, 74));
        gradient.setColorAt(1.0, QColor(212, 89, 89));
        painter->setBrush(gradient);
        painter->drawRoundedRect(fillRect, 3.0, 3.0);
    }

    float left_ = -60.0f;
    float right_ = -60.0f;
};

class AudioMixerStripRow final : public QFrame
{
public:
    explicit AudioMixerStripRow(AudioMixerChannelStrip* strip, QWidget* parent = nullptr)
        : QFrame(parent)
        , strip_(strip)
    {
        setObjectName(QStringLiteral("AudioMixerStripCard"));
        setFrameShape(QFrame::NoFrame);
        setAttribute(Qt::WA_StyledBackground, true);
        setFixedWidth(148);
        setMinimumHeight(374);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 10, 8, 10);
        layout->setSpacing(8);

        nameLabel_ = new QLabel(this);
        nameLabel_->setAlignment(Qt::AlignCenter);
        nameLabel_->setWordWrap(true);
        nameLabel_->setFixedHeight(36);
        nameLabel_->setStyleSheet(QStringLiteral("color: #e6e6e6; font-weight: 600;"));

        meterWidget_ = new AudioLevelMeterWidget(this);

        volumeSlider_ = new QSlider(Qt::Vertical, this);
        volumeSlider_->setRange(0, 200);
        volumeSlider_->setSingleStep(1);
        volumeSlider_->setFixedHeight(224);
        volumeSlider_->setPageStep(10);
        volumeSlider_->setTracking(true);

        volumeValueLabel_ = new QLabel(this);
        volumeValueLabel_->setAlignment(Qt::AlignCenter);
        volumeValueLabel_->setStyleSheet(QStringLiteral("color: #8fa6bf; font-weight: 600;"));

        muteButton_ = new QPushButton(QStringLiteral("M"), this);
        muteButton_->setCheckable(true);
        muteButton_->setFixedSize(30, 24);

        soloButton_ = new QPushButton(QStringLiteral("S"), this);
        soloButton_->setCheckable(true);
        soloButton_->setFixedSize(30, 24);

        auto* faderLayout = new QHBoxLayout();
        faderLayout->setContentsMargins(2, 0, 2, 0);
        faderLayout->setSpacing(8);
        faderLayout->addStretch(1);
        faderLayout->addWidget(meterWidget_, 0, Qt::AlignBottom);
        faderLayout->addWidget(volumeSlider_, 0, Qt::AlignBottom);
        faderLayout->addStretch(1);

        auto* buttonLayout = new QHBoxLayout();
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        buttonLayout->setSpacing(6);
        buttonLayout->addWidget(muteButton_, 1);
        buttonLayout->addWidget(soloButton_, 1);

        layout->addWidget(nameLabel_, 0);
        layout->addLayout(faderLayout, 1);
        layout->addWidget(volumeValueLabel_, 0);
        layout->addLayout(buttonLayout, 0);

        setStyleSheet(QStringLiteral(R"(
            QFrame#AudioMixerStripCard {
                background: #23272b;
                border: 1px solid #3a4046;
                border-radius: 8px;
            }
            QSlider::groove:vertical {
                width: 8px;
                background: #171a1e;
                border: 1px solid #4a4f55;
                border-radius: 4px;
            }
            QSlider::sub-page:vertical {
                background: #41464c;
                border-radius: 4px;
            }
            QSlider::add-page:vertical {
                background: #a88d5d;
                border-radius: 4px;
            }
            QSlider::handle:vertical {
                width: 28px;
                height: 12px;
                margin: 0 -10px;
                background: #f0f0f0;
                border: 1px solid #b9b9b9;
                border-radius: 4px;
            }
            QPushButton {
                background: #2b3035;
                color: #dde2e7;
                border: 1px solid #444b52;
                border-radius: 4px;
                font-weight: 700;
            }
            QPushButton:checked {
                color: #081018;
            }
            QPushButton:checked[muteButton="true"] {
                background: #c55b5b;
                border-color: #d07171;
            }
            QPushButton:checked[soloButton="true"] {
                background: #d8b64f;
                border-color: #e4c66a;
            }
        )"));
        muteButton_->setProperty("muteButton", true);
        soloButton_->setProperty("soloButton", true);

        volumeCommitTimer_ = new QTimer(this);
        volumeCommitTimer_->setSingleShot(true);
        volumeCommitTimer_->setInterval(16);
        QObject::connect(volumeCommitTimer_, &QTimer::timeout, this, [this]() {
            commitPendingVolume();
        });

        QObject::connect(volumeSlider_, &QSlider::sliderPressed, this, [this]() {
            draggingVolume_ = true;
            pendingVolume_ = sliderValueToVolume(volumeSlider_->value());
        });
        QObject::connect(volumeSlider_, &QSlider::sliderReleased, this, [this]() {
            draggingVolume_ = false;
            pendingVolume_ = sliderValueToVolume(volumeSlider_->value());
            commitPendingVolume();
            syncFromStrip();
        });
        QObject::connect(volumeSlider_, &QSlider::valueChanged, this, [this](const int value) {
            pendingVolume_ = sliderValueToVolume(value);
            updateVolumePresentation(pendingVolume_);
            if (!draggingVolume_) {
                commitPendingVolume();
                return;
            }
            volumeCommitTimer_->start();
        });
        QObject::connect(muteButton_, &QPushButton::toggled, this, [this](const bool checked) {
            if (strip_) {
                strip_->setMuted(checked);
            }
        });
        QObject::connect(soloButton_, &QPushButton::toggled, this, [this](const bool checked) {
            if (strip_) {
                strip_->setSolo(checked);
            }
        });

        QObject::connect(strip_, &AudioMixerChannelStrip::volumeChanged, this, [this](const float) { syncFromStrip(); });
        QObject::connect(strip_, &AudioMixerChannelStrip::muteChanged, this, [this](const bool) { syncFromStrip(); });
        QObject::connect(strip_, &AudioMixerChannelStrip::soloChanged, this, [this](const bool) { syncFromStrip(); });
        QObject::connect(strip_, &AudioMixerChannelStrip::levelChanged, this, [this](const float left, const float right) {
            if (meterWidget_) {
                meterWidget_->setLevels(left, right);
            }
        });

        syncFromStrip();
    }

private:
    void syncFromStrip()
    {
        if (!strip_) {
            return;
        }

        const QSignalBlocker volumeBlocker(volumeSlider_);
        const QSignalBlocker muteBlocker(muteButton_);
        const QSignalBlocker soloBlocker(soloButton_);

        nameLabel_->setText(strip_->layerName().trimmed().isEmpty() ? QStringLiteral("Audio Layer") : strip_->layerName());
        const float stripVolume = strip_->volume();
        if (!volumeSlider_->isSliderDown()) {
            volumeSlider_->setValue(volumeToPercent(stripVolume));
        }
        updateVolumePresentation(stripVolume);
        muteButton_->setChecked(strip_->isMuted());
        soloButton_->setChecked(strip_->isSolo());
        meterWidget_->setLevels(strip_->leftLevel(), strip_->rightLevel());
    }

    void updateVolumePresentation(const float volume)
    {
        volumeValueLabel_->setText(QStringLiteral("%1%").arg(volumeToPercent(volume)));
        if (draggingVolume_ && meterWidget_) {
            const float db = volumeToMeterDb(volume);
            meterWidget_->setLevels(db, db);
        }
    }

    void commitPendingVolume()
    {
        if (!strip_) {
            return;
        }
        strip_->setVolume(pendingVolume_);
    }

    AudioMixerChannelStrip* strip_ = nullptr;
    QLabel* nameLabel_ = nullptr;
    AudioLevelMeterWidget* meterWidget_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* volumeValueLabel_ = nullptr;
    QPushButton* muteButton_ = nullptr;
    QPushButton* soloButton_ = nullptr;
    QTimer* volumeCommitTimer_ = nullptr;
    float pendingVolume_ = 1.0f;
    bool draggingVolume_ = false;
};

class AudioMixerMasterRow final : public QFrame
{
public:
    explicit AudioMixerMasterRow(AudioMixerMasterBus* masterBus, QWidget* parent = nullptr)
        : QFrame(parent)
        , masterBus_(masterBus)
    {
        setObjectName(QStringLiteral("AudioMixerMasterCard"));
        setFrameShape(QFrame::NoFrame);
        setAttribute(Qt::WA_StyledBackground, true);
        setFixedWidth(160);
        setMinimumHeight(374);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 10, 8, 10);
        layout->setSpacing(8);

        nameLabel_ = new QLabel(QStringLiteral("Master"), this);
        nameLabel_->setAlignment(Qt::AlignCenter);
        nameLabel_->setWordWrap(true);
        nameLabel_->setFixedHeight(36);
        nameLabel_->setStyleSheet(QStringLiteral("color: #f0f3f6; font-weight: 700;"));

        meterWidget_ = new AudioLevelMeterWidget(this);

        volumeSlider_ = new QSlider(Qt::Vertical, this);
        volumeSlider_->setRange(0, 200);
        volumeSlider_->setSingleStep(1);
        volumeSlider_->setFixedHeight(224);
        volumeSlider_->setPageStep(10);
        volumeSlider_->setTracking(true);

        volumeValueLabel_ = new QLabel(this);
        volumeValueLabel_->setAlignment(Qt::AlignCenter);
        volumeValueLabel_->setStyleSheet(QStringLiteral("color: #9bc0e3; font-weight: 700;"));

        muteButton_ = new QPushButton(QStringLiteral("M"), this);
        muteButton_->setCheckable(true);
        muteButton_->setFixedSize(32, 24);
        muteButton_->setProperty("muteButton", true);

        auto* faderLayout = new QHBoxLayout();
        faderLayout->setContentsMargins(2, 0, 2, 0);
        faderLayout->setSpacing(8);
        faderLayout->addStretch(1);
        faderLayout->addWidget(meterWidget_, 0, Qt::AlignBottom);
        faderLayout->addWidget(volumeSlider_, 0, Qt::AlignBottom);
        faderLayout->addStretch(1);

        layout->addWidget(nameLabel_, 0);
        layout->addLayout(faderLayout, 1);
        layout->addWidget(volumeValueLabel_, 0);
        layout->addWidget(muteButton_, 0, Qt::AlignCenter);

        setStyleSheet(QStringLiteral(R"(
            QFrame#AudioMixerMasterCard {
                background: #26292d;
                border: 1px solid #474b51;
                border-radius: 8px;
            }
            QSlider::groove:vertical {
                width: 8px;
                background: #171a1e;
                border: 1px solid #4a4f55;
                border-radius: 4px;
            }
            QSlider::sub-page:vertical {
                background: #4a4f56;
                border-radius: 4px;
            }
            QSlider::add-page:vertical {
                background: #ad8d59;
                border-radius: 4px;
            }
            QSlider::handle:vertical {
                width: 30px;
                height: 12px;
                margin: 0 -11px;
                background: #f0f0f0;
                border: 1px solid #b9b9b9;
                border-radius: 4px;
            }
            QPushButton {
                background: #2b3035;
                color: #dde2e7;
                border: 1px solid #444b52;
                border-radius: 4px;
                font-weight: 700;
            }
            QPushButton:checked {
                color: #081018;
                background: #c55b5b;
                border-color: #d07171;
            }
        )"));

        volumeCommitTimer_ = new QTimer(this);
        volumeCommitTimer_->setSingleShot(true);
        volumeCommitTimer_->setInterval(16);
        QObject::connect(volumeCommitTimer_, &QTimer::timeout, this, [this]() {
            commitPendingVolume();
        });

        QObject::connect(volumeSlider_, &QSlider::sliderPressed, this, [this]() {
            draggingVolume_ = true;
            pendingVolume_ = sliderValueToVolume(volumeSlider_->value());
        });
        QObject::connect(volumeSlider_, &QSlider::sliderReleased, this, [this]() {
            draggingVolume_ = false;
            pendingVolume_ = sliderValueToVolume(volumeSlider_->value());
            commitPendingVolume();
            syncFromMaster();
        });
        QObject::connect(volumeSlider_, &QSlider::valueChanged, this, [this](const int value) {
            pendingVolume_ = sliderValueToVolume(value);
            updateVolumePresentation(pendingVolume_);
            if (!draggingVolume_) {
                commitPendingVolume();
                return;
            }
            volumeCommitTimer_->start();
        });
        QObject::connect(muteButton_, &QPushButton::toggled, this, [this](const bool checked) {
            if (masterBus_) {
                masterBus_->setMuted(checked);
            }
        });
        QObject::connect(masterBus_, &AudioMixerMasterBus::volumeChanged, this, [this](const float) { syncFromMaster(); });
        QObject::connect(masterBus_, &AudioMixerMasterBus::muteChanged, this, [this](const bool) { syncFromMaster(); });
        QObject::connect(masterBus_, &AudioMixerMasterBus::levelChanged, this, [this](const float left, const float right) {
            if (meterWidget_) {
                meterWidget_->setLevels(left, right);
            }
        });

        syncFromMaster();
    }

private:
    void syncFromMaster()
    {
        if (!masterBus_) {
            return;
        }

        const QSignalBlocker volumeBlocker(volumeSlider_);
        const QSignalBlocker muteBlocker(muteButton_);

        const float masterVolume = masterBus_->volume();
        if (!volumeSlider_->isSliderDown()) {
            volumeSlider_->setValue(volumeToPercent(masterVolume));
        }
        updateVolumePresentation(masterVolume);
        muteButton_->setChecked(masterBus_->isMuted());
        meterWidget_->setLevels(masterBus_->leftLevel(), masterBus_->rightLevel());
    }

    void updateVolumePresentation(const float volume)
    {
        volumeValueLabel_->setText(QStringLiteral("%1%").arg(volumeToPercent(volume)));
        if (draggingVolume_ && meterWidget_) {
            const float db = volumeToMeterDb(volume);
            meterWidget_->setLevels(db, db);
        }
    }

    void commitPendingVolume()
    {
        if (!masterBus_) {
            return;
        }
        masterBus_->setVolume(pendingVolume_);
    }

    AudioMixerMasterBus* masterBus_ = nullptr;
    QLabel* nameLabel_ = nullptr;
    AudioLevelMeterWidget* meterWidget_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* volumeValueLabel_ = nullptr;
    QPushButton* muteButton_ = nullptr;
    QTimer* volumeCommitTimer_ = nullptr;
    float pendingVolume_ = 1.0f;
    bool draggingVolume_ = false;
};
}

W_OBJECT_IMPL(ArtifactCompositionAudioMixerWidget)

class ArtifactCompositionAudioMixerWidget::Impl
{
public:
    AudioMixer* mixer_ = nullptr;
    QWidget* contentWidget_ = nullptr;
    QHBoxLayout* contentLayout_ = nullptr;
    QLabel* emptyLabel_ = nullptr;

    void clearRows()
    {
        if (!contentLayout_) {
            return;
        }
        while (QLayoutItem* item = contentLayout_->takeAt(0)) {
            if (QWidget* widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }
    }
};

ArtifactCompositionAudioMixerWidget::ArtifactCompositionAudioMixerWidget(QWidget* parent)
    : QWidget(parent)
    , impl_(new Impl())
{
    setAttribute(Qt::WA_StyledBackground, true);

    impl_->mixer_ = new AudioMixer(this);

    if (auto* playbackService = ArtifactPlaybackService::instance()) {
        if (auto* masterBus = impl_->mixer_->masterBus()) {
            QObject::connect(masterBus, &AudioMixerMasterBus::volumeChanged, this,
                [playbackService](const float volume) {
                    playbackService->setAudioMasterVolume(volume);
                });
            QObject::connect(masterBus, &AudioMixerMasterBus::muteChanged, this,
                [playbackService](const bool muted) {
                    playbackService->setAudioMasterMuted(muted);
                });
            playbackService->setAudioMasterVolume(masterBus->volume());
            playbackService->setAudioMasterMuted(masterBus->isMuted());
        }

        QObject::connect(playbackService, &ArtifactPlaybackService::audioLevelChanged, this,
            [this](float leftRms, float rightRms, float leftPeak, float rightPeak) {
                if (auto* masterBus = impl_->mixer_->masterBus()) {
                    masterBus->updateLevels(leftRms, rightRms);
                }
            });
    }

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* header = new QWidget(this);
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(12, 10, 12, 10);
    headerLayout->setSpacing(2);

    auto* titleLabel = new QLabel(QStringLiteral("Audio Mixer"), header);
    auto* subtitleLabel = new QLabel(QStringLiteral("Master bus and current composition audio layers"), header);

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subtitleLabel);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    impl_->contentWidget_ = new QWidget(scrollArea);
    impl_->contentLayout_ = new QHBoxLayout(impl_->contentWidget_);
    impl_->contentLayout_->setContentsMargins(12, 12, 12, 12);
    impl_->contentLayout_->setSpacing(10);
    impl_->contentLayout_->addStretch();
    scrollArea->setWidget(impl_->contentWidget_);

    rootLayout->addWidget(header);
    rootLayout->addWidget(scrollArea, 1);

    auto scheduleRefresh = [this]() {
        QTimer::singleShot(0, this, &ArtifactCompositionAudioMixerWidget::refreshFromCurrentComposition);
    };

    if (auto* service = ArtifactProjectService::instance()) {
        QObject::connect(service, &ArtifactProjectService::projectChanged, this, scheduleRefresh);
        QObject::connect(service, &ArtifactProjectService::currentCompositionChanged, this,
            [scheduleRefresh](const CompositionID&) {
                scheduleRefresh();
            });
        QObject::connect(service, &ArtifactProjectService::layerCreated, this,
            [scheduleRefresh](const CompositionID&, const LayerID&) {
                scheduleRefresh();
            });
        QObject::connect(service, &ArtifactProjectService::layerRemoved, this,
            [scheduleRefresh](const CompositionID&, const LayerID&) {
                scheduleRefresh();
            });
    }

    refreshFromCurrentComposition();
}

ArtifactCompositionAudioMixerWidget::~ArtifactCompositionAudioMixerWidget()
{
    delete impl_;
}

void ArtifactCompositionAudioMixerWidget::refreshFromCurrentComposition()
{
    ArtifactCompositionPtr composition;
    if (auto* service = ArtifactProjectService::instance()) {
        composition = service->currentComposition().lock();
    }

    impl_->mixer_->syncFromComposition(composition);
    impl_->clearRows();
    if (auto* masterBus = impl_->mixer_->masterBus()) {
        impl_->contentLayout_->addWidget(new AudioMixerMasterRow(masterBus, impl_->contentWidget_));
    }

    const auto strips = impl_->mixer_->allChannelStrips();
    if (strips.isEmpty()) {
        impl_->emptyLabel_ = new QLabel(QStringLiteral("No audio layers in the current composition"), impl_->contentWidget_);
        impl_->emptyLabel_->setAlignment(Qt::AlignCenter);
        impl_->emptyLabel_->setFixedWidth(180);
        impl_->emptyLabel_->setStyleSheet(QStringLiteral("color: #7d8b99; padding: 28px 12px; background: #1a2027; border: 1px dashed #314050; border-radius: 8px;"));
        impl_->contentLayout_->addWidget(impl_->emptyLabel_);
    } else {
        for (auto* strip : strips) {
            impl_->contentLayout_->addWidget(new AudioMixerStripRow(strip, impl_->contentWidget_));
        }
    }
    impl_->contentLayout_->addStretch();
}

}
