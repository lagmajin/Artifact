module;
#include <wobjectimpl.h>
#include <QObject>
#include <QString>

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
export module Artifact.Preview.Clock;




import Frame.Rate;
import Frame.Range;
import Frame.Position;

export namespace Artifact
{
 using namespace ArtifactCore;
 
 // UIw̃^CCNbNʒmNX
 // 
 // ݌vj:
 // - x^C~O ArtifactCore::TimelineClock ŏi}CNbxj
 // - UIւ̒ʒm̂ Signal/Slot gpi60fps = 16msԊuj
 // - Composition/Layer clock() ŒTimelineClockQƂčx^C~O擾
 // 
 // Signal/SlotgȂR:
 // - Qt ̃CxgL[COxi~bjɂ荂x
 // - XbhԒʐMł͂ɒx
 // - UIXV60fpsi16msjŏ\
 class ArtifactTimelineClock : public QObject
 {
  W_OBJECT(ArtifactTimelineClock)
 public:
  class Impl;
  Impl* impl_;

 public:
  ArtifactTimelineClock(QObject* parent = nullptr);
  ~ArtifactTimelineClock();
  
  // xNbNւ̒ڃANZXiComposition/LayerŎgpj
  // : ̃\bh̓XbhZ[tł
  class TimelineClock* clock();
  const class TimelineClock* clock() const;
  
 public /*signals*/:
  // UIXVp̃VOii60fpsxŔsj
  // : ͍x^C~Oɂ͎gpȂ
  void tick() W_SIGNAL(tick);
  void tickFrame(const FramePosition& position) W_SIGNAL(tickFrame, position);
  void playbackStateChanged(bool isPlaying) W_SIGNAL(playbackStateChanged, isPlaying);
  void timecodeChanged(const QString& timecode) W_SIGNAL(timecodeChanged, timecode);
  
 public /*slots*/:
  void start(); W_SLOT(start);
  void stop(); W_SLOT(stop);
  void pause(); W_SLOT(pause);
  void resume(); W_SLOT(resume);
  void startClockRange(int startFrame, int endFrame); W_SLOT(startClockRange);
  void setPlaybackSpeed(double speed); W_SLOT(setPlaybackSpeed);
 };

};

W_REGISTER_ARGTYPE(ArtifactCore::FramePosition)
