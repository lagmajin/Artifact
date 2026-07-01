module;
#include <utility>
#include <QWidget>
#include <QSizePolicy>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFont>
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
#include <QMetaObject>
#include <QTime>
#include <QMessageBox>
#include <QFile>
#include <QThread>
#include <QElapsedTimer>
#include <QPalette>
#include <QColor>
#include <wobjectimpl.h>
#include <cmath>
#include <algorithm>

module Artifact.Widgets.AudioPreview;

import Artifact.Audio.Waveform;
import Media.Encoder.FFmpegAudioDecoder;
import Audio.Segment;
import AudioRenderer;

namespace Artifact {

namespace {
constexpr float kAudioPreviewMinDb = -60.0f;
constexpr float kAudioPreviewMaxDb = 0.0f;
}

W_OBJECT_IMPL(AudioLevelBarWidget)
W_OBJECT_IMPL(AudioWaveformWidget)
W_OBJECT_IMPL(ArtifactAudioPreviewWidget)

// ─────────────────────────────────────────────────────────
// AudioLevelBarWidget
// ─────────────────────────────────────────────────────────

AudioLevelBarWidget::AudioLevelBarWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(24);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    peakHoldTimer_.start();
}

void AudioLevelBarWidget::setLevels(float leftPeak, float leftRms, float rightPeak, float rightRms)
{
    leftPeak_ = std::max(-96.0f, leftPeak);
    leftRms_ = std::max(-96.0f, leftRms);
    rightPeak_ = std::max(-96.0f, rightPeak);
    rightRms_ = std::max(-96.0f, rightRms);

    if (leftPeak_ > leftPeakHold_) {
        leftPeakHold_ = leftPeak_;
    }
    if (rightPeak_ > rightPeakHold_) {
        rightPeakHold_ = rightPeak_;
    }

    // Release peak hold after 1.5s
    if (peakHoldTimer_.hasExpired(1500)) {
        leftPeakHold_ = std::max(leftPeakHold_ - 0.5f, leftPeak_);
        rightPeakHold_ = std::max(rightPeakHold_ - 0.5f, rightPeak_);
        peakHoldTimer_.restart();
    }

    update();
}

static float dbToRatio(float db)
{
    if (db <= kAudioPreviewMinDb) return 0.0f;
    return (db - kAudioPreviewMinDb) / (kAudioPreviewMaxDb - kAudioPreviewMinDb);
}

void AudioLevelBarWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const int w = width();
    const int h = height();
    if (w <= 0 || h <= 0) return;

    // Background
    painter.fillRect(rect(), QColor(20, 20, 30));

    const int barHeight = (h - 4) / 2;
    const int barWidth = w - 8;

    auto paintBar = [&](int y, float rms, float peak, float peakHold) {
        const int x = 4;
        const int rmsW = static_cast<int>(dbToRatio(rms) * barWidth);
        const int peakW = static_cast<int>(dbToRatio(peak) * barWidth);
        const int holdW = static_cast<int>(dbToRatio(peakHold) * barWidth);

        // Background fill
        painter.fillRect(x, y, barWidth, barHeight, QColor(10, 10, 18));

        // Gradient for RMS
        if (rmsW > 0) {
            QLinearGradient grad(x, 0, x + barWidth, 0);
            grad.setColorAt(0.0, QColor(0, 200, 80));
            grad.setColorAt(0.7, QColor(240, 220, 0));
            grad.setColorAt(0.9, QColor(240, 80, 0));
            grad.setColorAt(1.0, QColor(220, 30, 30));
            painter.fillRect(x, y, rmsW, barHeight, grad);
        }

        // Peak line
        if (peakW > 0) {
            painter.setPen(QPen(QColor(255, 255, 255, 220), 1));
            painter.drawLine(x + peakW, y, x + peakW, y + barHeight);
        }

        // Peak hold line
        if (holdW > 0) {
            painter.setPen(QPen(QColor(255, 255, 255, 80), 1));
            painter.drawLine(x + holdW, y, x + holdW, y + barHeight);
        }
    };

    // Left channel (top)
    paintBar(2, leftRms_, leftPeak_, leftPeakHold_);

    // Right channel (bottom)
    paintBar(2 + barHeight + 2, rightRms_, rightPeak_, rightPeakHold_);
}

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
    peaks_ = samples;
    rms_.clear();
    sampleRate_ = sampleRate;
    totalSamples_ = samples.size();
    currentPosition_ = 0;
    update();
}

void AudioWaveformWidget::setWaveformData(const WaveformData& data) {
    peaks_ = data.peaks;
    rms_ = data.rms;
    sampleRate_ = data.sampleRate;
    totalSamples_ = peaks_.size();
    currentPosition_ = 0;
    update();
}

void AudioWaveformWidget::setPosition(int sampleIndex) {
    if (totalSamples_ <= 0) {
        currentPosition_ = 0;
        return;
    }
    currentPosition_ = std::clamp(sampleIndex, 0, totalSamples_ - 1);
    update();
}

void AudioWaveformWidget::clear() {
    peaks_.clear();
    rms_.clear();
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
    if (w <= 0 || h <= 0) {
        return;
    }
    const int centerY = h / 2;

    // Background
    painter.fillRect(rect(), bgColor_);

    if (peaks_.isEmpty()) {
        painter.setPen(QColor(128, 128, 128));
        painter.drawText(rect(), Qt::AlignCenter, "No audio data");
        return;
    }

    // Draw center line
    painter.setPen(QPen(QColor(60, 60, 80), 1.0));
    painter.drawLine(0, centerY, w, centerY);

    // Calculate samples per pixel
    const int samplesPerPixel = std::max(1, totalSamples_ / w);

    // Draw RMS body first so the peak line can sit on top.
    for (int x = 0; x < w; ++x) {
        const int startSample = x * samplesPerPixel;
        const int endSample = std::min(startSample + samplesPerPixel, totalSamples_);

        float peakMax = 0.0f;
        for (int s = startSample; s < endSample; ++s) {
            if (peaks_[s] > peakMax) peakMax = peaks_[s];
        }

        const int yMin = centerY - static_cast<int>(peakMax * centerY * 0.9f);
        const int yMax = centerY + static_cast<int>(peakMax * centerY * 0.9f);

        if (!rms_.isEmpty()) {
            float rmsPeak = 0.0f;
            for (int s = startSample; s < endSample && s < rms_.size(); ++s) {
                rmsPeak = std::max(rmsPeak, rms_[s]);
            }
            const int rmsTop = centerY - static_cast<int>(rmsPeak * centerY * 0.75f);
            const int rmsBottom = centerY + static_cast<int>(rmsPeak * centerY * 0.75f);
            painter.setPen(QPen(QColor(82, 130, 220, 120), 1.0));
            painter.drawLine(x, rmsTop, x, rmsBottom);
        }

        painter.setPen(QPen(waveColor_, 1.5));
        painter.drawLine(x, yMin, x, yMax);
    }

    // Draw playhead
    if (totalSamples_ > 0) {
        const int playheadX = static_cast<int>(
            static_cast<double>(currentPosition_) / std::max(totalSamples_, 1) * w);
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
    if (width() <= 0) return;
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
    W_OBJECT(AudioPlaybackEngine)
public:
    explicit AudioPlaybackEngine(QObject* parent = nullptr)
        : QObject(parent)
    {
        decoder_ = std::make_unique<ArtifactCore::FFmpegAudioDecoder>();
        renderer_ = std::make_unique<ArtifactCore::AudioRenderer>();

        renderer_->setLevelCallback([this](const ArtifactCore::AudioLevelData& data) {
            levelTickCounter_++;
            if (levelTickCounter_ % 4 == 0) {
                QMetaObject::invokeMethod(this, [this, data]() {
                    emit levelUpdated(data.leftPeak, data.leftRms, data.rightPeak, data.rightRms);
                }, Qt::QueuedConnection);
            }
        });

        timer_ = new QTimer(this);
        timer_->setInterval(50);
        connect(timer_, &QTimer::timeout, this, &AudioPlaybackEngine::onTimerTick);
    }

    bool loadFile(const QString& filePath) {
        stop();
        if (!decoder_->openFile(filePath)) {
            return false;
        }
        sampleRate_ = decoder_->sampleRate();
        numChannels_ = decoder_->channelCount();
        totalSamples_ = 0;
        preloadedSegments_.clear();

        ArtifactCore::AudioSegment seg;
        while (decoder_->decodeNextSegment(seg)) {
            totalSamples_ += seg.frameCount();
            preloadedSegments_.push_back(seg);
        }

        if (preloadedSegments_.empty()) {
            decoder_->closeFile();
            return false;
        }

        currentSample_ = 0;
        currentSegmentIndex_ = 0;
        currentSegmentOffset_ = 0;
        eosReached_ = false;

        levelTickCounter_ = 0;
        emit levelUpdated(-96.0f, -96.0f, -96.0f, -96.0f);

        generateWaveformSamples();
        return true;
    }

    void play() {
        if (!decoder_ || preloadedSegments_.empty()) return;

        if (!renderer_->isDeviceOpen()) {
            renderer_->openDevice(ArtifactCore::AudioBackendType::WASAPI);
            renderer_->start();
        }
        renderer_->setMasterVolume(volumeToDb(volume_));
        eosReached_ = false;
        isPlaying_ = true;
        timer_->start();
        emit playbackStarted();
    }

    void pause() {
        if (!decoder_) return;
        isPlaying_ = false;
        timer_->stop();
        renderer_->clearBuffer();
        renderer_->stop();
        emit levelUpdated(-96.0f, -96.0f, -96.0f, -96.0f);
    }

    void stop() {
        if (!decoder_) return;
        isPlaying_ = false;
        timer_->stop();
        renderer_->stop();
        renderer_->closeDevice();
        decoder_->closeFile();
        currentSegmentIndex_ = 0;
        currentSegmentOffset_ = 0;
        currentSample_ = 0;
        eosReached_ = false;
        emit positionChanged(0);
        emit levelUpdated(-96.0f, -96.0f, -96.0f, -96.0f);
    }

    ~AudioPlaybackEngine() {
        stop();
    }

    bool isPlaying() const { return isPlaying_; }
    int currentPosition() const { return currentSample_; }
    int totalSamples() const { return totalSamples_; }
    int sampleRate() const { return sampleRate_; }

    void setPosition(int sampleIndex) {
        currentSample_ = std::clamp(sampleIndex, 0, totalSamples_ - 1);
        int pos = 0;
        for (size_t i = 0; i < preloadedSegments_.size(); ++i) {
            int frames = preloadedSegments_[i].frameCount();
            if (pos + frames > currentSample_) {
                currentSegmentIndex_ = i;
                currentSegmentOffset_ = currentSample_ - pos;
                emit positionChanged(currentSample_);
                return;
            }
            pos += frames;
        }
        emit positionChanged(currentSample_);
    }

    void setVolume(float volume) {
        volume_ = std::clamp(volume, 0.0f, 1.0f);
        if (renderer_ && renderer_->isDeviceOpen()) {
            renderer_->setMasterVolume(volumeToDb(volume_));
        }
    }

    float volume() const { return volume_; }

    const QVector<float>& waveformSamples() const { return waveformSamples_; }

signals:
    void playbackStarted() W_SIGNAL(playbackStarted);
    void playbackStopped() W_SIGNAL(playbackStopped);
    void positionChanged(int sampleIndex) W_SIGNAL(positionChanged, sampleIndex);
    void levelUpdated(float leftPeak, float leftRms, float rightPeak, float rightRms)
        W_SIGNAL(levelUpdated, leftPeak, leftRms, rightPeak, rightRms);

private slots:
    void onTimerTick() {
        if (!isPlaying_ || preloadedSegments_.empty()) return;
        if (eosReached_) {
            stop();
            emit playbackStopped();
            return;
        }

        if (currentSegmentIndex_ >= preloadedSegments_.size()) {
            eosReached_ = true;
            stop();
            emit playbackStopped();
            return;
        }

        // Feed segments to AudioRenderer at half the buffer rate (~10ms chunks)
        static int feedCounter = 0;
        feedCounter++;
        if (feedCounter % 4 == 0 || renderer_->bufferedFrames() < 1024) {
            const auto& seg = preloadedSegments_[currentSegmentIndex_];
            if (currentSegmentOffset_ < seg.frameCount()) {
                ArtifactCore::AudioSegment chunk;
                chunk.sampleRate = seg.sampleRate;
                chunk.layout = seg.layout;
                chunk.startFrame = seg.startFrame + currentSegmentOffset_;
                int available = seg.frameCount() - currentSegmentOffset_;
                int chunkSize = std::min(available, 512);
                chunk.channelData.resize(seg.channelData.size());
                for (int ch = 0; ch < static_cast<int>(seg.channelData.size()); ++ch) {
                    const auto& srcVec = seg.channelData[ch];
                chunk.channelData[ch] = QVector<float>(srcVec.constData() + currentSegmentOffset_, chunkSize);
                }
                renderer_->enqueue(chunk);
            }
        }

        // Advance position based on actual hardware playback
        size_t bufferedBefore = renderer_->bufferedFrames();
        if (bufferedBefore < 4096) {
            const auto& seg = preloadedSegments_[currentSegmentIndex_];
            int remaining = seg.frameCount() - currentSegmentOffset_;
            int advance = std::min(remaining, 256);
            currentSegmentOffset_ += advance;
            currentSample_ += advance;
            if (currentSegmentOffset_ >= seg.frameCount()) {
                currentSegmentOffset_ = 0;
                ++currentSegmentIndex_;
            }
        }

        emit positionChanged(currentSample_);

        if (currentSample_ >= totalSamples_) {
            eosReached_ = true;
            stop();
            emit playbackStopped();
        }
    }

private:
    static float volumeToDb(float linear) {
        if (linear < 1e-6f) return -144.0f;
        return 20.0f * std::log10(linear);
    }

    void generateWaveformSamples() {
        if (preloadedSegments_.empty()) return;
        const int targetSamples = 4000;
        const int samplesPerPixel = std::max(1, totalSamples_ / targetSamples);
        waveformSamples_.resize(targetSamples);

        int segIdx = 0;
        int segOffset = 0;
        for (int i = 0; i < targetSamples; ++i) {
            float maxVal = 0.0f;
            for (int s = 0; s < samplesPerPixel; ++s) {
                if (segIdx >= static_cast<int>(preloadedSegments_.size())) break;
                const auto& seg = preloadedSegments_[segIdx];
                if (seg.channelData.isEmpty()) break;
                const auto& ch0 = seg.channelData[0];
                if (segOffset < static_cast<int>(ch0.size())) {
                    float v = std::abs(ch0[segOffset]);
                    if (v > maxVal) maxVal = v;
                }
                ++segOffset;
                if (segOffset >= seg.frameCount()) {
                    segOffset = 0;
                    ++segIdx;
                }
            }
            waveformSamples_[i] = maxVal;
        }
    }

    std::unique_ptr<ArtifactCore::FFmpegAudioDecoder> decoder_;
    std::unique_ptr<ArtifactCore::AudioRenderer> renderer_;
    QTimer* timer_ = nullptr;
    std::vector<ArtifactCore::AudioSegment> preloadedSegments_;
    QVector<float> waveformSamples_;
    int sampleRate_ = 44100;
    int numChannels_ = 2;
    int totalSamples_ = 0;
    int currentSample_ = 0;
    size_t currentSegmentIndex_ = 0;
    int currentSegmentOffset_ = 0;
    bool isPlaying_ = false;
    bool eosReached_ = false;
    float volume_ = 1.0f;
    int levelTickCounter_ = 0;
};

W_OBJECT_IMPL(AudioPlaybackEngine)

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
    AudioLevelBarWidget* levelBar_ = nullptr;
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
    mainLayout->setSpacing(6);

    // Title
    impl_->titleLabel_ = new QLabel("Audio Preview");
    QFont titleFont = impl_->titleLabel_->font();
    titleFont.setBold(true);
    if (titleFont.pointSize() > 0) {
        titleFont.setPointSize(titleFont.pointSize() + 2);
    } else {
        titleFont.setPointSize(14);
    }
    impl_->titleLabel_->setFont(titleFont);

    // Level meter
    impl_->levelBar_ = new AudioLevelBarWidget();
    impl_->levelBar_->setFixedHeight(28);

    // Waveform
    impl_->waveformWidget_ = new AudioWaveformWidget();
    impl_->waveformWidget_->setMinimumHeight(120);

    // Duration
    impl_->durationLabel_ = new QLabel("00:00 / 00:00");
    QFont durationFont = impl_->durationLabel_->font();
    if (durationFont.pointSize() > 0) {
        durationFont.setPointSize(durationFont.pointSize() - 1);
    } else {
        durationFont.setPointSize(11);
    }
    impl_->durationLabel_->setFont(durationFont);

    // Controls
    auto* controlsLayout = new QHBoxLayout();

    impl_->playButton_ = new QPushButton("▶ Play");
    impl_->playButton_->setDefault(false);

    impl_->stopButton_ = new QPushButton("⏹ Stop");
    impl_->stopButton_->setDefault(false);

    controlsLayout->addWidget(impl_->playButton_);
    controlsLayout->addWidget(impl_->stopButton_);
    controlsLayout->addStretch();

    // Volume
    auto* volumeLabel = new QLabel("Vol:");
    impl_->volumeSlider_ = new QSlider(Qt::Horizontal);
    impl_->volumeSlider_->setRange(0, 100);
    impl_->volumeSlider_->setValue(80);
    impl_->volumeSlider_->setFixedWidth(100);
    controlsLayout->addWidget(volumeLabel);
    controlsLayout->addWidget(impl_->volumeSlider_);

    mainLayout->addWidget(impl_->titleLabel_);
    mainLayout->addWidget(impl_->levelBar_);
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

    connect(impl_->engine_, &AudioPlaybackEngine::levelUpdated, this,
        [this](float leftPeak, float leftRms, float rightPeak, float rightRms) {
            impl_->levelBar_->setLevels(leftPeak, leftRms, rightPeak, rightRms);
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
            QString("Failed to load audio file:\n%1").arg(filePath));
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

    const QTime currentTime = QTime(0, 0, 0, 0).addMSecs(currentMs);
    const QTime totalTime = QTime(0, 0, 0, 0).addMSecs(totalMs);

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
