module;
#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Widgets.PlaybackControlWidget;

export namespace Artifact
{
 class ArtifactPlaybackControlWidget :public QWidget 
 {
 	W_OBJECT(ArtifactPlaybackControlWidget)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactPlaybackControlWidget(QWidget* parent = nullptr);
  virtual ~ArtifactPlaybackControlWidget();
  void play();
  void stop();
  void seekStart();     // 最初のフレームへ
  void seekEnd();       // 最後のフレームへ（任意）
  void stepForward();   // 1フレーム進む
  void stepBackward();
  void setLoopEnabled(bool);
  void setPreviewRange(int start, int end);
 public/*signals*/:
  void playButtonClicked() W_SIGNAL(playButtonClicked)
   void stopButtonClicked() W_SIGNAL(stopButtonClicked)
 public/*slots*/:

  W_SLOT(play)
   W_SLOT(stop)
  };
	
	
};;
