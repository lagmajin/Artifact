module;
#include <wobjectimpl.h>
#include <QObject>
#include <QTimer>
#include <QString>

module Artifact.Preview.Clock;

import std;
import Frame.Rate;
import Frame.Range;
import Frame.Position;
import Timeline.Clock;

namespace Artifact
{
 using namespace ArtifactCore;

 class ArtifactTimelineClock::Impl
 {
 public:
  TimelineClock timelineClock_;  // 高精度クロック
  QTimer* uiUpdateTimer_;        // UI更新用タイマー（60fps）
  ArtifactTimelineClock* parent_;
  
  QString lastTimecode_;
  bool lastPlayingState_ = false;
  
  Impl(ArtifactTimelineClock* parent)
   : parent_(parent)
   , uiUpdateTimer_(new QTimer(parent))
  {
   // UI更新は60fps（16ms間隔）
   uiUpdateTimer_->setInterval(16);
   
   QObject::connect(uiUpdateTimer_, &QTimer::timeout, parent, [this]() {
    updateUI();
   });
  }
  
  ~Impl() = default;
  
  void updateUI() {
   // 再生状態が変わったら通知
   bool isPlaying = timelineClock_.isPlaying();
   if (isPlaying != lastPlayingState_) {
    lastPlayingState_ = isPlaying;
    parent_->playbackStateChanged(isPlaying);
   }
   
   // タイムコードが変わったら通知
   QString timecode = timelineClock_.timecode();
   if (timecode != lastTimecode_) {
    lastTimecode_ = timecode;
    parent_->timecodeChanged(timecode);
   }
   
   // フレーム位置を通知
   auto position = timelineClock_.currentPosition();
   parent_->tickFrame(position);
   parent_->tick();
  }
 };

 W_OBJECT_IMPL(ArtifactTimelineClock)
 
 ArtifactTimelineClock::ArtifactTimelineClock(QObject* parent)
  : QObject(parent)
  , impl_(new Impl(this))
 {
 }

 ArtifactTimelineClock::~ArtifactTimelineClock()
 {
  delete impl_;
 }

 TimelineClock* ArtifactTimelineClock::clock()
 {
  return &impl_->timelineClock_;
 }

 const TimelineClock* ArtifactTimelineClock::clock() const
 {
  return &impl_->timelineClock_;
 }

 void ArtifactTimelineClock::start()
 {
  impl_->timelineClock_.start();
  impl_->uiUpdateTimer_->start();
  impl_->lastPlayingState_ = true;
  playbackStateChanged(true);
 }

 void ArtifactTimelineClock::stop()
 {
  impl_->timelineClock_.stop();
  impl_->uiUpdateTimer_->stop();
  impl_->lastPlayingState_ = false;
  playbackStateChanged(false);
  
  // 停止時に最終状態を通知
  impl_->updateUI();
 }

 void ArtifactTimelineClock::pause()
 {
  impl_->timelineClock_.pause();
  impl_->lastPlayingState_ = false;
  playbackStateChanged(false);
 }

 void ArtifactTimelineClock::resume()
 {
  impl_->timelineClock_.resume();
  impl_->lastPlayingState_ = true;
  playbackStateChanged(true);
 }

 void ArtifactTimelineClock::startClockRange(int startFrame, int endFrame)
 {
  impl_->timelineClock_.setLoopRange(startFrame, endFrame);
  impl_->timelineClock_.setFrame(startFrame);
  start();
 }

 void ArtifactTimelineClock::setPlaybackSpeed(double speed)
 {
  impl_->timelineClock_.setPlaybackSpeed(speed);
 }

};
