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
#include <QEvent>
#include <QShowEvent>
export module Artifact.Widgets.PlaybackControlTestWidget;

export namespace Artifact
{

/// 再生コントロールウィジェット
/// メインの再生操作 UI を提供する
class ArtifactPlaybackControlTestWidget : public QWidget
{
    W_OBJECT(ArtifactPlaybackControlTestWidget)
private:
    class Impl;
    Impl* impl_;
    void refreshSurfaceAfterDockLifecycle();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

public:
    explicit ArtifactPlaybackControlTestWidget(QWidget* parent = nullptr);
    ~ArtifactPlaybackControlTestWidget() override;

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

} // namespace Artifact

