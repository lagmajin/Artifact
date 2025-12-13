module;
#include <QObject>
module Artifact.Render.Queue.Service;


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


 ArtifactRenderQueueService::ArtifactRenderQueueService(QObject* parent/*=nullptr*/):QObject(parent),impl_(new Impl)
 {

 }

 ArtifactRenderQueueService::~ArtifactRenderQueueService()
 {
  delete impl_;
 }

};