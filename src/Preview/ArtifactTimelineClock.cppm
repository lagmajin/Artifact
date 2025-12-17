module;
#include <wobjectimpl.h>
#include <QElapsedTimer>
#include <QObject>
module Artifact.Preview.Clock;

import std;
import Frame.Rate;
import Frame.Range;
import Frame.Position;

namespace Artifact
{
 using namespace ArtifactCore;
	
 

  class ArtifactTimelineClock::Impl
 {
 private:
  FrameRange range_;
 public:
  Impl(ArtifactTimelineClock* parent);
  ~Impl();
  void startTimer();
  void tick();
  QElapsedTimer clock;
  bool running=false;
  double fps=0.0f;
  double currentTimeSec = 0.0;
  ArtifactTimelineClock* parent_ = nullptr;
 };

 ArtifactTimelineClock::Impl::Impl(ArtifactTimelineClock* parent):parent_(parent)
{

 }

 void ArtifactTimelineClock::Impl::tick()
 {

 }
	
 W_OBJECT_IMPL(ArtifactTimelineClock)
	
 ArtifactTimelineClock::ArtifactTimelineClock(QObject* parent/*=nullptr*/) :QObject(parent), impl_(new Impl(this))
 {

 }

 ArtifactTimelineClock::~ArtifactTimelineClock()
 {
  delete impl_;
 }

 void ArtifactTimelineClock::start()
 {
  impl_->running = true;
 }

 void ArtifactTimelineClock::stop()
 {
  impl_->running = false;
 }

 void ArtifactTimelineClock::startClockRange(int startFrame, int endFrame)
 {

 }

};
