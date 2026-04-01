module;
#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QAudioOutput>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QVector>
#include <QTimer>
#include <QFileInfo>
#include <QTime>
#include <QMessageBox>
#include <QFile>
#include <QThread>
#include <QElapsedTimer>
#include <cmath>

module Artifact.Widgets.AudioPreview;

namespace Artifact {

// ─────────────────────────────────────────────────────────
// AudioWaveformWidget
// ─────────────────────────────────────────────────────────

AudioWaveformWidget::AudioWaveformWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void AudioWaveformWidget::setSamples(const QVector<float>& samples, int sampleRate) {
    samples_ = samples;
    sampleRate_ = sampleRate;
    totalSamples_ = samples.size();
    currentPosition_ = 0;
    update();
}

void AudioWaveformWidget::setPosition(int sampleIndex) {
    currentPosition_ = std::clamp(sampleIndex, 0, totalSamples_ - 1);
    update();
}

void AudioWaveformWidget::clear() {
    samples_.clear();
    currentPosition_ = 0;
    totalSamples_ = 0;
    update();
}

void AudioWaveformWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int centerY = h / 2;

    // Background
    painter.fillRect(rect(), bgColor_);

    if (samples_.isEmpty()) {
        painter.setPen(QColor(128, 128, 128));
        painter.drawText(rect(), Qt::AlignCenter, "No audio data");
        return;
    }

    // Draw center line
    painter.setPen(QColor(60, 60, 80));
    painter.drawLine(0, centerY, w, centerY);

    // Calculate samples per pixel
    const int samplesPerPixel = std::max(1, totalSamples_ / w);

    // Draw waveform
    painter.setPen(QPen(waveColor_, 1.5));
    for (int x = 0; x < w; ++x) {
        const int startSample = x * samplesPerPixel;
        const int endSample = std::min(startSample + samplesPerPixel, totalSamples_);

        float minVal = 0.0f, maxVal = 0.0f;
        for (int s = startSample; s < endSample; ++s) {
            if (samples_[s] < minVal) minVal = samples_[s];
            if (samples_[s] > maxVal) maxVal = samples_[s];
        }

        const int yMin = centerY - static_cast<int>(maxVal * centerY * 0.9f);
        const int yMax = centerY - static_cast<int>(minVal * centerY * 0.9f);

        painter.drawLine(x, yMin, x, yMax);
    }

    // Draw playhead
    if (totalSamples_ > 0) {
        const int playheadX = static_cast<int>(
            static_cast<double>(currentPosition_) / totalSamples_ * w);
        painter.setPen(QPen(playheadColor_, 2));
        painter.drawLine(playheadX, 0, playheadX, h);
    }
}

void AudioWaveformWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isDragging_ = true;
        updatePositionFromMouse(event->pos());
    }
}

void AudioWaveformWidget::mouseMoveEvent(QMouseEvent* event) {
    if (isDragging_) {
        updatePositionFromMouse(event->pos());
    }
}

void AudioWaveformWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isDragging_ = false;
    }
}

void AudioWaveformWidget::updatePositionFromMouse(const QPoint& pos) {
    if (totalSamples_ <= 0) return;
    const int x = std::clamp(pos.x(), 0, width());
    const int sampleIndex = static_cast<int>(
        static_cast<double>(x) / width() * totalSamples_);
    currentPosition_ = std::clamp(sampleIndex, 0, totalSamples_ - 1);
    emit positionChanged(currentPosition_);
    update();
}

// ─────────────────────────────────────────────────────────
// Audio Playback Engine (simple QAudioSink based)
// ─────────────────────────────────────────────────────────

class AudioPlaybackEngine : public QObject {
    Q_OBJECT
public:
    explicit AudioPlaybackEngine(QObject* parent = nullptr)
        : QObject(parent)
    {
        timer_ = new QTimer(this);
        timer_->setInterval(50);
        connect(timer_, &QTimer::timeout, this, &AudioPlaybackEngine::onTimerTick);
    }

    bool loadFile(const QString& filePath) {
        stop();
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }

        // Simple WAV parser
        QByteArray data = file.readAll();
        file.close();

        if (data.size() < 44) return false;

        // Check RIFF header
        if (data.mid(0, 4) != "RIFF" || data.mid(8, 4) != "WAVE") {
            return false;
        }

        // Parse format
        quint16 audioFormat = *reinterpret_cast<const quint16*>(data.constData() + 20);
        quint16 numChannels = *reinterpret_cast<const quint16*>(data.constData() + 22);
        quint32 sampleRate = *reinterpret_cast<const quint32*>(data.constData() + 24);
        quint16 bitsPerSample = *reinterpret_cast<const quint16*>(data.constData() + 34);

        if (audioFormat != 1) return false; // Only PCM

        // Find data chunk
        int dataPos = 36;
        while (dataPos < data.size() - 8) {
            if (data.mid(dataPos, 4) == "data") {
                break;
            }
            quint32 chunkSize = *reinterpret_cast<const quint32*>(data.constData() + dataPos + 4);
            dataPos += 8 + chunkSize;
        }

        if (dataPos >= data.size() - 8) return false;

        quint32 dataSize = *reinterpret_cast<const quint32*>(data.constData() + dataPos + 4);
        audioData_ = data.mid(dataPos + 8, dataSize);

        // Setup format
        QAudioFormat format;
        format.setSampleRate(sampleRate);
        format.setChannelCount(numChannels);
        format.setSampleFormat(bitsPerSample == 16 ? QAudioFormat::Int16 : QAudioFormat::UInt8);

        audioSink_ = new QAudioSink(format, this);
        audioDevice_ = audioSink_->start();

        sampleRate_ = sampleRate;
        numChannels_ = numChannels;
        bitsPerSample_ = bitsPerSample;
        totalSamples_ = (dataSize / (bitsPerSample / 8)) / numChannels;
        currentSample_ = 0;
        isPlaying_ = false;

        // Generate downsampled samples for waveform display
        generateWaveformSamples();

        return true;
    }

    void play() {
        if (!audioSink_ || audioData_.isEmpty()) return;
        isPlaying_ = true;
        timer_->start();
        emit playbackStarted();
    }

    void pause() {
        if (!audioSink_) return;
        isPlaying_ = false;
        timer_->stop();
        audioSink_->suspend();
    }

    void stop() {
        if (!audioSink_) return;
        isPlaying_ = false;
        timer_->stop();
        audioSink_->stop();
        currentSample_ = 0;
        emit positionChanged(0);
    }

    bool isPlaying() const { return isPlaying_; }

    int currentPosition() const { return currentSample_; }
    int totalSamples() const { return totalSamples_; }
    int sampleRate() const { return sampleRate_; }

    void setPosition(int sampleIndex) {
        currentSample_ = std::clamp(sampleIndex, 0, totalSamples_ - 1);
        emit positionChanged(currentSample_);
    }

    void setVolume(float volume) {
        if (audioSink_) {
            audioSink_->setVolume(std::clamp(volume, 0.0f, 1.0f));
        }
    }

    float volume() const {
        return audioSink_ ? audioSink_->volume() : 1.0f;
    }

    const QVector<float>& waveformSamples() const { return waveformSamples_; }

signals:
    void playbackStarted();
    void playbackStopped();
    void positionChanged(int sampleIndex);

private slots:
    void onTimerTick() {
        if (!isPlaying_ || !audioDevice_ || audioData_.isEmpty()) return;

        const int bytesPerSample = bitsPerSample_ / 8;
        const int bytesToWrite = 4096;
        const int samplesToWrite = bytesToWrite / bytesPerSample / numChannels_;

        if (currentSample_ + samplesToWrite >= totalSamples_) {
            // End of audio
            stop();
            emit playbackStopped();
            return;
        }

        const int byteOffset = currentSample_ * numChannels_ * bytesPerSample;
        const int writeBytes = std::min(bytesToWrite, audioData_.size() - byteOffset);

        if (writeBytes > 0) {
            qint64 written = audioDevice_->write(audioData_.constData() + byteOffset, writeBytes);
            if (written > 0) {
                currentSample_ += (written / bytesPerSample) / numChannels_;
                emit positionChanged(currentSample_);
            }
        }
    }

private:
    void generateWaveformSamples() {
        if (audioData_.isEmpty() || totalSamples_ <= 0) return;

        // Downsample to ~4000 samples for display
        const int targetSamples = 4000;
        const int samplesPerPixel = std::max(1, totalSamples_ / targetSamples);
        const int bytesPerSample = bitsPerSample_ / 8;

        waveformSamples_.resize(targetSamples);

        for (int i = 0; i < targetSamples; ++i) {
            const int startSample = i * samplesPerPixel;
            const int endSample = std::min(startSample + samplesPerPixel, totalSamples_);

            float maxVal = 0.0f;
            for (int s = startSample; s < endSample; ++s) {
                const int byteOffset = s * numChannels_ * bytesPerSample;
                float sampleVal = 0.0f;

                if (bitsPerSample_ == 16) {
                    const qint16* ptr = reinterpret_cast<const qint16*>(audioData_.constData() + byteOffset);
                    sampleVal = static_cast<float>(*ptr) / 32768.0f;
                } else {
                    const quint8* ptr = reinterpret_cast<const quint8*>(audioData_.constData() + byteOffset);
                    sampleVal = (static_cast<float>(*ptr) / 255.0f) * 2.0f - 1.0f;
                }

                if (std::abs(sampleVal) > std::abs(maxVal)) {
                    maxVal = sampleVal;
                }
            }

            waveformSamples_[i] = maxVal;
        }
    }

    QAudioSink* audioSink_ = nullptr;
    QIODevice* audioDevice_ = nullptr;
    QTimer* timer_ = nullptr;
    QByteArray audioData_;
    QVector<float> waveformSamples_;
    int sampleRate_ = 44100;
    int numChannels_ = 2;
    int bitsPerSample_ = 16;
    int totalSamples_ = 0;
    int currentSample_ = 0;
    bool isPlaying_ = false;
};

// ─────────────────────────────────────────────────────────
// ArtifactAudioPreviewWidget
// ─────────────────────────────────────────────────────────

class ArtifactAudioPreviewWidget::Impl {
public:
    Impl(ArtifactAudioPreviewWidget* widget) : widget_(widget) {
        engine_ = new AudioPlaybackEngine(widget_);
    }
    ~Impl() = default;

    ArtifactAudioPreviewWidget* widget_ = nullptr;
    AudioPlaybackEngine* engine_ = nullptr;

    AudioWaveformWidget* waveformWidget_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* durationLabel_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QString currentFilePath_;
};

ArtifactAudioPreviewWidget::ArtifactAudioPreviewWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // Title
    impl_->titleLabel_ = new QLabel("Audio Preview");
    impl_->titleLabel_->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");

    // Waveform
    impl_->waveformWidget_ = new AudioWaveformWidget();
    impl_->waveformWidget_->setMinimumHeight(120);

    // Duration
    impl_->durationLabel_ = new QLabel("00:00 / 00:00");
    impl_->durationLabel_->setStyleSheet("color: #aaaaaa; font-size: 11px;");

    // Controls
    auto* controlsLayout = new QHBoxLayout();

    impl_->playButton_ = new QPushButton("▶ Play");
    impl_->playButton_->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background-color: #2ecc71; }"
        "QPushButton:disabled { background-color: #555; color: #888; }"
    );

    impl_->stopButton_ = new QPushButton("⏹ Stop");
    impl_->stopButton_->setStyleSheet(
        "QPushButton { background-color: #c0392b; color: white; border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background-color: #e74c3c; }"
        "QPushButton:disabled { background-color: #555; color: #888; }"
    );

    controlsLayout->addWidget(impl_->playButton_);
    controlsLayout->addWidget(impl_->stopButton_);
    controlsLayout->addStretch();

    // Volume
    controlsLayout->addWidget(new QLabel("Vol:"));
    impl_->volumeSlider_ = new QSlider(Qt::Horizontal);
    impl_->volumeSlider_->setRange(0, 100);
    impl_->volumeSlider_->setValue(80);
    impl_->volumeSlider_->setFixedWidth(100);
    controlsLayout->addWidget(impl_->volumeSlider_);

    mainLayout->addWidget(impl_->titleLabel_);
    mainLayout->addWidget(impl_->waveformWidget_, 1);
    mainLayout->addWidget(impl_->durationLabel_);
    mainLayout->addLayout(controlsLayout);

    // Connections
    connect(impl_->playButton_, &QPushButton::clicked, this, [this]() {
        if (impl_->engine_->isPlaying()) {
            impl_->engine_->pause();
            impl_->playButton_->setText("▶ Play");
        } else {
            impl_->engine_->play();
            impl_->playButton_->setText("⏸ Pause");
            emit playbackStarted();
        }
    });

    connect(impl_->stopButton_, &QPushButton::clicked, this, [this]() {
        impl_->engine_->stop();
        impl_->playButton_->setText("▶ Play");
        impl_->waveformWidget_->setPosition(0);
        updateDurationLabel();
        emit playbackStopped();
    });

    connect(impl_->volumeSlider_, &QSlider::valueChanged, this, [this](int value) {
        impl_->engine_->setVolume(value / 100.0f);
    });

    connect(impl_->engine_, &AudioPlaybackEngine::positionChanged, this, [this](int sampleIndex) {
        impl_->waveformWidget_->setPosition(sampleIndex);
        updateDurationLabel();
        emit positionChanged(sampleIndexToMs(sampleIndex));
    });

    connect(impl_->engine_, &AudioPlaybackEngine::playbackStopped, this, [this]() {
        impl_->playButton_->setText("▶ Play");
        impl_->waveformWidget_->setPosition(0);
        updateDurationLabel();
        emit playbackStopped();
    });

    connect(impl_->waveformWidget_, &AudioWaveformWidget::positionChanged, this, [this](int sampleIndex) {
        impl_->engine_->setPosition(sampleIndex);
    });

    // Initial state
    impl_->playButton_->setEnabled(false);
    impl_->stopButton_->setEnabled(false);
}

ArtifactAudioPreviewWidget::~ArtifactAudioPreviewWidget() {
    delete impl_;
}

void ArtifactAudioPreviewWidget::loadAudioFile(const QString& filePath) {
    impl_->currentFilePath_ = filePath;
    QFileInfo fi(filePath);
    impl_->titleLabel_->setText(fi.fileName());

    if (impl_->engine_->loadFile(filePath)) {
        impl_->waveformWidget_->setSamples(
            impl_->engine_->waveformSamples(),
            impl_->engine_->sampleRate());
        impl_->playButton_->setEnabled(true);
        impl_->stopButton_->setEnabled(true);
        updateDurationLabel();
    } else {
        QMessageBox::warning(this, "Audio Preview",
            QString("Failed to load audio file:\n%1\n\nOnly WAV files are supported.").arg(filePath));
    }
}

void ArtifactAudioPreviewWidget::play() {
    if (!impl_->engine_->isPlaying()) {
        impl_->engine_->play();
        impl_->playButton_->setText("⏸ Pause");
        emit playbackStarted();
    }
}

void ArtifactAudioPreviewWidget::pause() {
    if (impl_->engine_->isPlaying()) {
        impl_->engine_->pause();
        impl_->playButton_->setText("▶ Play");
    }
}

void ArtifactAudioPreviewWidget::stop() {
    impl_->engine_->stop();
    impl_->playButton_->setText("▶ Play");
    impl_->waveformWidget_->setPosition(0);
    updateDurationLabel();
    emit playbackStopped();
}

bool ArtifactAudioPreviewWidget::isPlaying() const {
    return impl_->engine_->isPlaying();
}

void ArtifactAudioPreviewWidget::setVolume(float volume) {
    impl_->engine_->setVolume(volume);
    impl_->volumeSlider_->setValue(static_cast<int>(volume * 100));
}

float ArtifactAudioPreviewWidget::volume() const {
    return impl_->engine_->volume();
}

void ArtifactAudioPreviewWidget::closeEvent(QCloseEvent* event) {
    stop();
    QWidget::closeEvent(event);
}

void ArtifactAudioPreviewWidget::updateDurationLabel() {
    const int currentMs = sampleIndexToMs(impl_->engine_->currentPosition());
    const int totalMs = sampleIndexToMs(impl_->engine_->totalSamples());

    const QTime currentTime(0, 0, 0, 0).addMSecs(currentMs);
    const QTime totalTime(0, 0, 0, 0).addMSecs(totalMs);

    impl_->durationLabel_->setText(
        QString("%1 / %2")
            .arg(currentTime.toString("mm:ss"))
            .arg(totalTime.toString("mm:ss")));
}

int ArtifactAudioPreviewWidget::sampleIndexToMs(int sampleIndex) const {
    if (impl_->engine_->sampleRate() <= 0) return 0;
    return static_cast<int>(
        static_cast<double>(sampleIndex) / impl_->engine_->sampleRate() * 1000.0);
}

} // namespace Artifact

#include "AudioPreviewWidget.moc"
