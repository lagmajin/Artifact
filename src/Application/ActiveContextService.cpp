module;

#include <wobjectimpl.h>
#include <QPointer>
module Artifact.Service.ActiveContext;

import std;

namespace Artifact
{
 class ArtifactActiveContextService::Impl
 {
 private:
  
 public:
  Impl();
  QPointer<QObject> pointer_;
 };

 ArtifactActiveContextService::Impl::Impl()
 {

 }

 W_OBJECT_IMPL(ArtifactActiveContextService)

  ArtifactActiveContextService::ArtifactActiveContextService(QObject* parent/*=nullptr*/) :QObject(parent), impl_(new Impl())
 {

 }

 ArtifactActiveContextService::~ArtifactActiveContextService()
 {
  delete impl_;
 }

 void ArtifactActiveContextService::setHandler(QObject* obj)
 {
  impl_->pointer_ = obj;
 	
 	
 }

 void ArtifactActiveContextService::sendPlayToActiveContext()
 {
  if (!impl_->pointer_)
   return;

  QMetaObject::invokeMethod(impl_->pointer_, "play",
   Qt::QueuedConnection);
 }

 void ArtifactActiveContextService::sendPauseToActiveContext()
 {
  if (!impl_->pointer_)
   return;

  QMetaObject::invokeMethod(impl_->pointer_, "pause",
   Qt::QueuedConnection);
 }

};