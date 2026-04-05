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
#include <wobjectdefs.h>

export module Artifact.Widgets.AudioPreview;

export namespace Artifact {

// 音声波形表示ウィジェット
class AudioWaveformWidget : public QWidget {
    W_OBJECT(AudioWaveformWidget)
private:
    QVector<float> samples_;
    int sampleRate_ = 44100;
    int currentPosition_ = 0;
    int totalSamples_ = 0;
    QColor waveColor_ = QColor(52, 152, 219);
    QColor bgColor_ = QColor(26, 26, 46);
    QColor playheadColor_ = QColor(231, 76, 60);
    bool isDragging_ = false;

public:
    explicit AudioWaveformWidget(QWidget* parent = nullptr);

    void setSamples(const QVector<float>& samples, int sampleRate);
    void setPosition(int sampleIndex);
    void clear();

signals:
    void positionChanged(int sampleIndex) W_SIGNAL(positionChanged, sampleIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void updatePositionFromMouse(const QPoint& pos);
};

// 音声プレビューウィジェット
class ArtifactAudioPreviewWidget : public QWidget {
    W_OBJECT(ArtifactAudioPreviewWidget)
private:
    class Impl;
    Impl* impl_;

public:
    explicit ArtifactAudioPreviewWidget(QWidget* parent = nullptr);
    ~ArtifactAudioPreviewWidget();

    void loadAudioFile(const QString& filePath);
    void play();
    void pause();
    void stop();
    bool isPlaying() const;

    void setVolume(float volume);
    float volume() const;

    void updateDurationLabel();
    int sampleIndexToMs(int sampleIndex) const;

signals:
    void playbackStarted() W_SIGNAL(playbackStarted);
    void playbackStopped() W_SIGNAL(playbackStopped);
    void positionChanged(int ms) W_SIGNAL(positionChanged, ms);

protected:
    void closeEvent(QCloseEvent* event) override;
};

} // namespace Artifact
