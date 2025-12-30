module;
#include <QObject>
#include <wobjectimpl.h>
module Artifact.Render.Queue.Service;

import std;
//import Container.MultiIndex;
//import Artifact.Render.Queue;
import Render.Queue.Manager;


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

 void ArtifactRenderQueueService::addRenderQueue()
 {

 }

 void ArtifactRenderQueueService::removeRenderQueue()
 {

 }

 void ArtifactRenderQueueService::removeAllRenderQueues()
 {

 }



};