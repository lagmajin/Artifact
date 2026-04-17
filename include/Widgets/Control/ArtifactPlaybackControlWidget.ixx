module;
#include <utility>

#include <wobjectdefs.h>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QTime>
#include <QCheckBox>
export module Artifact.Widgets.PlaybackControlWidget;

export namespace Artifact
{

/// 再生コントロールウィジェット
/// メインの再生操作 UI を提供する
class ArtifactPlaybackControlWidget : public QWidget
{
    W_OBJECT(ArtifactPlaybackControlWidget)
private:
    class Impl;
    Impl* impl_;

protected:
    void paintEvent(QPaintEvent* event) override;

public:
    explicit ArtifactPlaybackControlWidget(QWidget* parent = nullptr);
    ~ArtifactPlaybackControlWidget() override;

    // 再生制御
    void play();
    void pause();
    void stop();
    void togglePlayPause();

    // シーク操作
    void seekStart();      // 先頭へ
    void seekEnd();        // 末尾へ
    void seekPrevious();   // 前へ（マーカー/チャプター）
    void seekNext();       // 次へ（マーカー/チャプター）

    // フレーム操作
    void stepForward();    // 1 フレーム進む
    void stepBackward();   // 1 フレーム戻る

    // 設定
    void setLoopEnabled(bool enabled);
    bool isLoopEnabled() const;

    void setPlaybackSpeed(float speed);
    float playbackSpeed() const;

    // 状態取得
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;

    // In/Out Points
    void setInPoint();
    void setOutPoint();
    void clearInOutPoints();

public: // slots
    W_SLOT(play)
    W_SLOT(pause)
    W_SLOT(stop)
    W_SLOT(togglePlayPause)
    W_SLOT(stepForward)
    W_SLOT(stepBackward)
    W_SLOT(seekStart)
    W_SLOT(seekEnd)
    W_SLOT(setLoopEnabled)
    W_SLOT(setPlaybackSpeed)
};


/// 再生情報表示ウィジェット
/// 現在のフレーム、再生速度、ドロップフレーム数などを表示
class ArtifactPlaybackInfoWidget : public QWidget
{
    W_OBJECT(ArtifactPlaybackInfoWidget)
private:
    class Impl;
    Impl* impl_;

protected:
    void paintEvent(QPaintEvent* event) override;

public:
    explicit ArtifactPlaybackInfoWidget(QWidget* parent = nullptr);
    ~ArtifactPlaybackInfoWidget() override;

    // 表示更新
    void setCurrentFrame(int64_t frame);
    void setTotalFrames(int64_t frames);
    void setFrameRate(float fps);
    void setPlaybackSpeed(float speed);
    void setDroppedFrames(int64_t count);

    // フレーム入力
    void setEditable(bool editable);

public: // signals
    void frameChanged(int64_t frame) W_SIGNAL(frameChanged, frame)
    void frameInput(int64_t frame) W_SIGNAL(frameInput, frame)

public: // slots
    W_SLOT(setCurrentFrame)
    W_SLOT(setTotalFrames)
    W_SLOT(setFrameRate)
};


/// スピードコントロールウィジェット
/// 再生速度をスライダーまたは数値で設定
class ArtifactPlaybackSpeedWidget : public QWidget
{
    W_OBJECT(ArtifactPlaybackSpeedWidget)
private:
    class Impl;
    Impl* impl_;

protected:
    void paintEvent(QPaintEvent* event) override;

public:
    explicit ArtifactPlaybackSpeedWidget(QWidget* parent = nullptr);
    ~ArtifactPlaybackSpeedWidget() override;

    float playbackSpeed() const;
    void setPlaybackSpeed(float speed);

    // プリセット
    void setSpeedPreset(float speed);  // 0.25, 0.5, 1.0, 2.0, etc.

public: // signals
    void speedChanged(float speed) W_SIGNAL(speedChanged, speed)

public: // slots
    W_SLOT(setPlaybackSpeed)
};

} // namespace Artifact
