module;
#include <wobjectimpl.h>
#include <QObject>
#include <QTimer>
#include <QString>

module Artifact.Preview.Clock;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>



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
  TimelineClock timelineClock_;  // xNbN
  QTimer* uiUpdateTimer_;        // UIXVp^C}[i60fpsj
  ArtifactTimelineClock* parent_;
  
  QString lastTimecode_;
  bool lastPlayingState_ = false;
  
  Impl(ArtifactTimelineClock* parent)
   : parent_(parent)
   , uiUpdateTimer_(new QTimer(parent))
  {
   // UIXV60fpsi16msԊuj
   uiUpdateTimer_->setInterval(16);
   
   QObject::connect(uiUpdateTimer_, &QTimer::timeout, parent, [this]() {
    updateUI();
   });
  }
  
  ~Impl() = default;
  
  void updateUI() {
   // ĐԂςʒm
   bool isPlaying = timelineClock_.isPlaying();
   if (isPlaying != lastPlayingState_) {
    lastPlayingState_ = isPlaying;
    parent_->playbackStateChanged(isPlaying);
   }
   
   // ^CR[hςʒm
   QString timecode = timelineClock_.timecode();
   if (timecode != lastTimecode_) {
    lastTimecode_ = timecode;
    parent_->timecodeChanged(timecode);
   }
   
   // t[ʒuʒm
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
  
  // ~ɍŏIԂʒm
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
