module;
#include <QObject>
#include <wobjectimpl.h>
module Artifact.Render.Queue.Service;

import std;
//import Container.MultiIndex;

namespace Artifact
{

 class ArtifactRenderQueueService::Impl
 {
 public:
  Impl();
  ~Impl()=default;
 };

 ArtifactRenderQueueService::Impl::Impl()
 {

 }
 W_OBJECT_IMPL(ArtifactRenderQueueService)

 ArtifactRenderQueueService::ArtifactRenderQueueService(QObject* parent/*=nullptr*/):QObject(parent),impl_(new Impl)
 {

 }

 ArtifactRenderQueueService::~ArtifactRenderQueueService()
 {
  delete impl_;
 }

};