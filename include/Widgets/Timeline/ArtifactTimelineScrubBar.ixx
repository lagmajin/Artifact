module;
#include <QMouseEvent>
#include <QPaintEvent>
#include <QSize>
#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Timeline.ScrubBar;

import Frame.Position;

W_REGISTER_ARGTYPE(ArtifactCore::FramePosition)

export namespace Artifact
{
 using namespace ArtifactCore;

 class ArtifactTimelineScrubBar : public QWidget
 {
  W_OBJECT(ArtifactTimelineScrubBar)

 private:
  class Impl;
  Impl* impl_;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 public:
  explicit ArtifactTimelineScrubBar(QWidget* parent = nullptr);
  ~ArtifactTimelineScrubBar();

  FramePosition currentFrame() const;
  void setCurrentFrame(const FramePosition& frame);

  void setTotalFrames(int totalFrames);
  int totalFrames() const;

  // タイムラインズーム座標に合わせたルーラーのための設定
  void setRulerPixelsPerFrame(double ppf);
  void setRulerHorizontalOffset(double offset);

  void setSeekLockDuringPlayback(bool lock);
  bool isSeekLockedDuringPlayback() const;

  void setIsPlaying(bool playing);
  bool isPlaying() const;

  void setFps(int fps);
  int fps() const;

  void frameChanged(const FramePosition& frame) W_SIGNAL(frameChanged, frame);
  void frameDragStarted() W_SIGNAL(frameDragStarted);
  void frameDragFinished() W_SIGNAL(frameDragFinished);
 };
}
