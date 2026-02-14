module;
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Widgets.SeekBar;

import Frame.Position;

W_REGISTER_ARGTYPE(ArtifactCore::FramePosition)

export namespace Artifact
{
 using namespace ArtifactCore;

 class ArtifactSeekBar: public QWidget
 {
  W_OBJECT(ArtifactSeekBar)
 private:
  class Impl;
  Impl* impl_;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 public:
  explicit ArtifactSeekBar(QWidget* parent = nullptr);
  ~ArtifactSeekBar();

  // フレーム位置の取得/設定
  FramePosition currentFrame() const;
  void setCurrentFrame(const FramePosition& frame);

  // 総フレーム数の設定
  void setTotalFrames(int totalFrames);
  int totalFrames() const;

  // 再生中のシークロック設定
  void setSeekLockDuringPlayback(bool lock);
  bool isSeekLockedDuringPlayback() const;

  // 再生状態設定（シークロック判定用）
  void setIsPlaying(bool playing);
  bool isPlaying() const;

  // シグナル
  void frameChanged(const FramePosition& frame) W_SIGNAL(frameChanged, frame);
  void frameDragStarted() W_SIGNAL(frameDragStarted);
  void frameDragFinished() W_SIGNAL(frameDragFinished);
 };

};
