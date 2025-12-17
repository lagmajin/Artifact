module;
#include <wobjectimpl.h>
#include <QObject>

export module Artifact.Preview.Clock;

import std;
import Frame.Rate;
import Frame.Range;
import Frame.Position;


export namespace Artifact
{
 using namespace ArtifactCore;
	
 class ArtifactTimelineClock:public QObject
 {
 	W_OBJECT(ArtifactTimelineClock)
 private:
  class Impl;
  Impl* impl_;
 public:
 	ArtifactTimelineClock(QObject*parent=nullptr);
    ~ArtifactTimelineClock();
    
 public/*signals*/:
  void tick() W_SIGNAL(tick);
  void tickFrame(const FramePosition& position) W_SIGNAL(tickFrame, position);
 public /*slots*/:
  void start(); W_SLOT(start);
  void stop(); W_SLOT(stop);
  void startClockRange(int startFrame, int endFrame); W_SLOT(startClockRange);
 
 };


};

W_REGISTER_ARGTYPE(ArtifactCore::FramePosition)