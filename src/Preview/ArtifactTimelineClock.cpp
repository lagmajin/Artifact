module;
#include <wobjectimpl.h>
#include <QElapsedTimer>
module Artifact.Preview.Controller;



namespace Artifact
{
	W_OBJECT_IMPL(ArtifactTimelineClock)
	
 class ArtifactTimelineClock::Impl
 {
 public:
  Impl();
  ~Impl();
  void tick();
  QElapsedTimer clock;
 };

 ArtifactTimelineClock::Impl::Impl()
 {

 }

 void ArtifactTimelineClock::Impl::tick()
 {

 }

 ArtifactTimelineClock::ArtifactTimelineClock(QObject* parent/*=nullptr*/) :QObject(parent),impl_(new Impl())
 {

 }

 ArtifactTimelineClock::~ArtifactTimelineClock()
 {
  delete impl_;
 }

};
