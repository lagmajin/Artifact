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
#include <QFont>
#include <QPalette>
#include <QColor>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionAudioMixer;

import Artifact.Widgets.CompositionAudioMixer;
import Artifact.Audio.Mixer;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Event.Bus;
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
        setAutoFillBackground(true);
        setFixedWidth(148);
        setMinimumHeight(374);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 10, 8, 10);
        layout->setSpacing(8);

        nameLabel_ = new QLabel(this);
        nameLabel_->setAlignment(Qt::AlignCenter);
        nameLabel_->setWordWrap(true);
        nameLabel_->setFixedHeight(36);
        QFont nameFont = nameLabel_->font();
        nameFont.setBold(true);
        nameFont.setPointSize(nameFont.pointSize() > 0 ? nameFont.pointSize() : 10);
        nameLabel_->setFont(nameFont);
        QPalette namePalette = nameLabel_->palette();
        namePalette.setColor(QPalette::WindowText, QColor(230, 230, 230));
        nameLabel_->setPalette(namePalette);

        meterWidget_ = new AudioLevelMeterWidget(this);

        volumeSlider_ = new QSlider(Qt::Vertical, this);
        volumeSlider_->setRange(0, 200);
        volumeSlider_->setSingleStep(1);
        volumeSlider_->setFixedHeight(224);
        volumeSlider_->setPageStep(10);
        volumeSlider_->setTracking(true);

        volumeValueLabel_ = new QLabel(this);
        volumeValueLabel_->setAlignment(Qt::AlignCenter);
        QFont valueFont = volumeValueLabel_->font();
        valueFont.setBold(true);
        volumeValueLabel_->setFont(valueFont);
        QPalette valuePalette = volumeValueLabel_->palette();
        valuePalette.setColor(QPalette::WindowText, QColor(143, 166, 191));
        volumeValueLabel_->setPalette(valuePalette);

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

        QPalette cardPalette = palette();
        cardPalette.setColor(QPalette::Window, QColor(35, 39, 43));
        setPalette(cardPalette);
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
        setAutoFillBackground(true);
        setFixedWidth(160);
        setMinimumHeight(374);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 10, 8, 10);
        layout->setSpacing(8);

        nameLabel_ = new QLabel(QStringLiteral("Master"), this);
        nameLabel_->setAlignment(Qt::AlignCenter);
        nameLabel_->setWordWrap(true);
        nameLabel_->setFixedHeight(36);
        QFont nameFont = nameLabel_->font();
        nameFont.setBold(true);
        nameFont.setPointSize(nameFont.pointSize() > 0 ? nameFont.pointSize() : 10);
        nameLabel_->setFont(nameFont);
        QPalette namePalette = nameLabel_->palette();
        namePalette.setColor(QPalette::WindowText, QColor(240, 243, 246));
        nameLabel_->setPalette(namePalette);

        meterWidget_ = new AudioLevelMeterWidget(this);

        volumeSlider_ = new QSlider(Qt::Vertical, this);
        volumeSlider_->setRange(0, 200);
        volumeSlider_->setSingleStep(1);
        volumeSlider_->setFixedHeight(224);
        volumeSlider_->setPageStep(10);
        volumeSlider_->setTracking(true);

        volumeValueLabel_ = new QLabel(this);
        volumeValueLabel_->setAlignment(Qt::AlignCenter);
        QFont valueFont = volumeValueLabel_->font();
        valueFont.setBold(true);
        volumeValueLabel_->setFont(valueFont);
        QPalette valuePalette = volumeValueLabel_->palette();
        valuePalette.setColor(QPalette::WindowText, QColor(155, 192, 227));
        volumeValueLabel_->setPalette(valuePalette);

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

        QPalette cardPalette = palette();
        cardPalette.setColor(QPalette::Window, QColor(38, 41, 45));
        setPalette(cardPalette);

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
    ArtifactCore::EventBus eventBus_;
    std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

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
    setAutoFillBackground(true);

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
                Q_UNUSED(leftPeak);
                Q_UNUSED(rightPeak);
                impl_->mixer_->updatePlaybackLevels(leftRms, rightRms);
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
    {
        QPalette titlePalette = titleLabel->palette();
        titlePalette.setColor(QPalette::WindowText, QColor(240, 243, 246));
        titleLabel->setPalette(titlePalette);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() > 0 ? titleFont.pointSize() + 1 : 11);
        titleLabel->setFont(titleFont);

        QPalette subtitlePalette = subtitleLabel->palette();
        subtitlePalette.setColor(QPalette::WindowText, QColor(166, 179, 195));
        subtitleLabel->setPalette(subtitlePalette);
    }

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subtitleLabel);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    impl_->contentWidget_ = new QWidget(scrollArea);
    impl_->contentWidget_->setAutoFillBackground(true);
    impl_->contentLayout_ = new QHBoxLayout(impl_->contentWidget_);
    impl_->contentLayout_->setContentsMargins(12, 12, 12, 12);
    impl_->contentLayout_->setSpacing(10);
    impl_->contentLayout_->addStretch();
    scrollArea->setWidget(impl_->contentWidget_);

    rootLayout->addWidget(header);
    rootLayout->addWidget(scrollArea, 1);

    if (auto* service = ArtifactProjectService::instance()) {
        QObject::connect(service, &ArtifactProjectService::projectChanged, this, [this]() {
            impl_->eventBus_.post<ProjectChangedEvent>(ProjectChangedEvent{QString(), QString()});
        });
        QObject::connect(service, &ArtifactProjectService::currentCompositionChanged, this,
            [this](const CompositionID& compositionId) {
                impl_->eventBus_.post<CurrentCompositionChangedEvent>(CurrentCompositionChangedEvent{compositionId.toString()});
            });
        QObject::connect(service, &ArtifactProjectService::layerCreated, this,
            [this](const CompositionID& compositionId, const LayerID& layerId) {
                impl_->eventBus_.post<LayerChangedEvent>(LayerChangedEvent{
                    compositionId.toString(),
                    layerId.toString(),
                    LayerChangedEvent::ChangeType::Created});
            });
        QObject::connect(service, &ArtifactProjectService::layerRemoved, this,
            [this](const CompositionID& compositionId, const LayerID& layerId) {
                impl_->eventBus_.post<LayerChangedEvent>(LayerChangedEvent{
                    compositionId.toString(),
                    layerId.toString(),
                    LayerChangedEvent::ChangeType::Removed});
            });

        impl_->eventBusSubscriptions_.push_back(
            impl_->eventBus_.subscribe<ProjectChangedEvent>([this](const ProjectChangedEvent&) {
                refreshFromCurrentComposition();
            }));
        impl_->eventBusSubscriptions_.push_back(
            impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>([this](const CurrentCompositionChangedEvent&) {
                refreshFromCurrentComposition();
            }));
        impl_->eventBusSubscriptions_.push_back(
            impl_->eventBus_.subscribe<LayerChangedEvent>([this](const LayerChangedEvent&) {
                refreshFromCurrentComposition();
            }));
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
        impl_->emptyLabel_->setAutoFillBackground(true);
        QPalette emptyPalette = impl_->emptyLabel_->palette();
        emptyPalette.setColor(QPalette::WindowText, QColor(125, 139, 153));
        emptyPalette.setColor(QPalette::Window, QColor(26, 32, 39));
        impl_->emptyLabel_->setPalette(emptyPalette);
        impl_->contentLayout_->addWidget(impl_->emptyLabel_);
    } else {
        for (auto* strip : strips) {
            impl_->contentLayout_->addWidget(new AudioMixerStripRow(strip, impl_->contentWidget_));
        }
    }
    impl_->contentLayout_->addStretch();
}

}
