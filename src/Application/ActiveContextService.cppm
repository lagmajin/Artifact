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

 void ArtifactActiveContextService::sendStopToActiveContext()
 {
  if (!impl_->pointer_)
   return;

  QMetaObject::invokeMethod(impl_->pointer_, "stop",
   Qt::QueuedConnection);
 }

 void ArtifactActiveContextService::sendNextFrameToActiveContext()
 {
  if (!impl_->pointer_)
   return;

  QMetaObject::invokeMethod(impl_->pointer_, "nextFrame",
   Qt::QueuedConnection);
 }

 void ArtifactActiveContextService::sendPreviousFrameToActiveContext()
 {
  if (!impl_->pointer_)
   return;

  QMetaObject::invokeMethod(impl_->pointer_, "previousFrame",
   Qt::QueuedConnection);
 }

 void ArtifactActiveContextService::sendGoToStartToActiveContext()
 {
  if (!impl_->pointer_)
   return;

  QMetaObject::invokeMethod(impl_->pointer_, "goToStart",
   Qt::QueuedConnection);
 }

 void ArtifactActiveContextService::sendGoToEndToActiveContext()
 {
  if (!impl_->pointer_)
   return;

  QMetaObject::invokeMethod(impl_->pointer_, "goToEnd",
   Qt::QueuedConnection);
 }

 void ArtifactActiveContextService::sendSeekToFrameToActiveContext(int frame)
 {
  if (!impl_->pointer_)
   return;

  QMetaObject::invokeMethod(impl_->pointer_, "seekToFrame",
   Qt::QueuedConnection,
   Q_ARG(int, frame));
 }

};