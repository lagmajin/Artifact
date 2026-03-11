module;

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
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
import std;

namespace Artifact
{

namespace
{
class AudioMixerStripRow final : public QFrame
{
public:
    explicit AudioMixerStripRow(AudioMixerChannelStrip* strip, QWidget* parent = nullptr)
        : QFrame(parent)
        , strip_(strip)
    {
        setObjectName(QStringLiteral("AudioMixerStripRow"));
        setFrameShape(QFrame::NoFrame);
        setAttribute(Qt::WA_StyledBackground, true);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(10);

        nameLabel_ = new QLabel(this);
        nameLabel_->setMinimumWidth(120);
        nameLabel_->setStyleSheet(QStringLiteral("color: #e6e6e6; font-weight: 600;"));

        volumeSlider_ = new QSlider(Qt::Horizontal, this);
        volumeSlider_->setRange(0, 200);
        volumeSlider_->setSingleStep(1);

        volumeValueLabel_ = new QLabel(this);
        volumeValueLabel_->setFixedWidth(46);
        volumeValueLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        volumeValueLabel_->setStyleSheet(QStringLiteral("color: #8fa6bf;"));

        muteButton_ = new QPushButton(QStringLiteral("M"), this);
        muteButton_->setCheckable(true);
        muteButton_->setFixedSize(28, 24);

        soloButton_ = new QPushButton(QStringLiteral("S"), this);
        soloButton_->setCheckable(true);
        soloButton_->setFixedSize(28, 24);

        layout->addWidget(nameLabel_, 0);
        layout->addWidget(volumeSlider_, 1);
        layout->addWidget(volumeValueLabel_, 0);
        layout->addWidget(muteButton_, 0);
        layout->addWidget(soloButton_, 0);

        setStyleSheet(QStringLiteral(R"(
            QFrame#AudioMixerStripRow {
                background: #1f2328;
                border: 1px solid #2c333b;
                border-radius: 5px;
            }
            QSlider::groove:horizontal {
                height: 5px;
                background: #101418;
                border-radius: 2px;
            }
            QSlider::sub-page:horizontal {
                background: #2d74b8;
                border-radius: 2px;
            }
            QSlider::handle:horizontal {
                width: 12px;
                margin: -4px 0;
                background: #d9e3ec;
                border-radius: 6px;
            }
            QPushButton {
                background: #2a3138;
                color: #d6dde5;
                border: 1px solid #3a4652;
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

        QObject::connect(volumeSlider_, &QSlider::valueChanged, this, [this](const int value) {
            if (strip_) {
                strip_->setVolume(static_cast<float>(value) / 100.0f);
            }
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
        volumeSlider_->setValue(static_cast<int>(std::lround(strip_->volume() * 100.0f)));
        volumeValueLabel_->setText(QStringLiteral("%1%").arg(static_cast<int>(std::lround(strip_->volume() * 100.0f))));
        muteButton_->setChecked(strip_->isMuted());
        soloButton_->setChecked(strip_->isSolo());
    }

    AudioMixerChannelStrip* strip_ = nullptr;
    QLabel* nameLabel_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* volumeValueLabel_ = nullptr;
    QPushButton* muteButton_ = nullptr;
    QPushButton* soloButton_ = nullptr;
};
}

W_OBJECT_IMPL(ArtifactCompositionAudioMixerWidget)

class ArtifactCompositionAudioMixerWidget::Impl
{
public:
    AudioMixer* mixer_ = nullptr;
    QWidget* contentWidget_ = nullptr;
    QVBoxLayout* contentLayout_ = nullptr;
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
    setStyleSheet(QStringLiteral("background: #15191d;"));

    impl_->mixer_ = new AudioMixer(this);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* header = new QWidget(this);
    header->setStyleSheet(QStringLiteral("background: #1b2026; border-bottom: 1px solid #2b333b;"));
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(12, 10, 12, 10);
    headerLayout->setSpacing(2);

    auto* titleLabel = new QLabel(QStringLiteral("Audio Mixer"), header);
    titleLabel->setStyleSheet(QStringLiteral("color: #f0f3f6; font-size: 13px; font-weight: 700;"));
    auto* subtitleLabel = new QLabel(QStringLiteral("Current composition audio layers"), header);
    subtitleLabel->setStyleSheet(QStringLiteral("color: #7d8b99; font-size: 11px;"));

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subtitleLabel);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background: #15191d; border: none; }"));

    impl_->contentWidget_ = new QWidget(scrollArea);
    impl_->contentLayout_ = new QVBoxLayout(impl_->contentWidget_);
    impl_->contentLayout_->setContentsMargins(8, 8, 8, 8);
    impl_->contentLayout_->setSpacing(8);
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

    const auto strips = impl_->mixer_->allChannelStrips();
    if (strips.isEmpty()) {
        impl_->emptyLabel_ = new QLabel(QStringLiteral("No audio layers in the current composition"), impl_->contentWidget_);
        impl_->emptyLabel_->setAlignment(Qt::AlignCenter);
        impl_->emptyLabel_->setStyleSheet(QStringLiteral("color: #7d8b99; padding: 28px 0;"));
        impl_->contentLayout_->addWidget(impl_->emptyLabel_);
    } else {
        for (auto* strip : strips) {
            impl_->contentLayout_->addWidget(new AudioMixerStripRow(strip, impl_->contentWidget_));
        }
    }
    impl_->contentLayout_->addStretch();
}

}
