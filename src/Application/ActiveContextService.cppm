module;
#include <wobjectimpl.h>
#include <QMetaObject>
#include <qlogging.h>
#include <QDebug>
module Artifact.Service.ActiveContext;

import std;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;

namespace Artifact
{

 class ArtifactActiveContextService::Impl
 {
 public:
  QObject* handler_ = nullptr;
  ArtifactCompositionPtr activeComp_ = nullptr;
 };

 W_OBJECT_IMPL(ArtifactActiveContextService)

 ArtifactActiveContextService::ArtifactActiveContextService(QObject* parent) 
  : QObject(parent), impl_(new Impl())
 {
 }

 ArtifactActiveContextService::~ArtifactActiveContextService() {
  delete impl_;
 }

 void ArtifactActiveContextService::setHandler(QObject* obj) {
  impl_->handler_ = obj;
 }

 void ArtifactActiveContextService::setActiveComposition(ArtifactCompositionPtr comp) {
  if (impl_->activeComp_ == comp) return;
  impl_->activeComp_ = comp;
  activeCompositionChanged(comp);
 }

 ArtifactCompositionPtr ArtifactActiveContextService::activeComposition() const {
  return impl_->activeComp_;
 }

 // --- Playback Actions ---
 void ArtifactActiveContextService::play() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "play");
  else if (impl_->activeComp_) impl_->activeComp_->play();
 }

 void ArtifactActiveContextService::pause() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "pause");
  else if (impl_->activeComp_) impl_->activeComp_->pause();
 }

 void ArtifactActiveContextService::togglePlayPause() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "togglePlayPause");
  else if (impl_->activeComp_) impl_->activeComp_->togglePlayPause();
 }

 void ArtifactActiveContextService::stop() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "stop");
  else if (impl_->activeComp_) impl_->activeComp_->stop();
 }

 void ArtifactActiveContextService::nextFrame() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "nextFrame");
  else if (impl_->activeComp_) impl_->activeComp_->goToFrame(impl_->activeComp_->framePosition().framePosition() + 1);
 }

 void ArtifactActiveContextService::prevFrame() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "prevFrame");
  else if (impl_->activeComp_) impl_->activeComp_->goToFrame(impl_->activeComp_->framePosition().framePosition() - 1);
 }

 void ArtifactActiveContextService::goToStart() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "goToStart");
  else if (impl_->activeComp_) impl_->activeComp_->goToStartFrame();
 }

 void ArtifactActiveContextService::goToEnd() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "goToEnd");
  else if (impl_->activeComp_) impl_->activeComp_->goToEndFrame();
 }

 void ArtifactActiveContextService::seekToFrame(int64_t frame) {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "seekToFrame", Q_ARG(int64_t, frame));
  else if (impl_->activeComp_) impl_->activeComp_->goToFrame(frame);
 }

 // --- Layer Actions ---
 void ArtifactActiveContextService::setLayerInAtCurrentTime() {
  auto l = ArtifactApplicationManager::instance()->layerSelectionManager()->currentLayer();
  if (l && impl_->activeComp_) {
   l->setInPoint(impl_->activeComp_->framePosition());
   qDebug() << "[ActiveContext] Set In for" << l->layerName() << "to" << impl_->activeComp_->framePosition().framePosition();
  }
 }

 void ArtifactActiveContextService::setLayerOutAtCurrentTime() {
  auto l = ArtifactApplicationManager::instance()->layerSelectionManager()->currentLayer();
  if (l && impl_->activeComp_) {
   l->setOutPoint(impl_->activeComp_->framePosition());
  }
 }

 void ArtifactActiveContextService::trimLayerInAtCurrentTime() {
  auto l = ArtifactApplicationManager::instance()->layerSelectionManager()->currentLayer();
  if (l && impl_->activeComp_) {
   auto now = impl_->activeComp_->framePosition();
   l->setInPoint(now);
   l->setStartTime(now);
  }
 }

 void ArtifactActiveContextService::trimLayerOutAtCurrentTime() {
  // Trim Out logic
 }

};