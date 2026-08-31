module;
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
#include <wobjectimpl.h>
#include <QMetaObject>
#include <qlogging.h>
#include <QDebug>
#include <QVariant>

module Artifact.Service.ActiveContext;
import Artifact.Service.Project;




import Artifact.Layers.Selection.Manager;
import Artifact.Service.Playback;
import Undo.UndoManager;

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

 ArtifactActiveContextService* ArtifactActiveContextService::instance() {
  static ArtifactActiveContextService service;
  return &service;
 }

 void ArtifactActiveContextService::setHandler(QObject* obj) {
  impl_->handler_ = obj;
 }

 void ArtifactActiveContextService::setActiveComposition(ArtifactCompositionPtr comp) {
  if (impl_->activeComp_ == comp) return;
  impl_->activeComp_ = comp;
  if (auto* playback = ArtifactPlaybackService::instance()) {
   playback->setCurrentComposition(comp);
  }
}

 ArtifactCompositionPtr ArtifactActiveContextService::activeComposition() const {
  return impl_->activeComp_;
 }

 // --- Playback Actions ---
 void ArtifactActiveContextService::play() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "play");
  else if (auto* playback = ArtifactPlaybackService::instance()) playback->play();
  else if (impl_->activeComp_) impl_->activeComp_->play();
 }

 void ArtifactActiveContextService::pause() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "pause");
  else if (auto* playback = ArtifactPlaybackService::instance()) playback->pause();
  else if (impl_->activeComp_) impl_->activeComp_->pause();
 }

 void ArtifactActiveContextService::togglePlayPause() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "togglePlayPause");
  else if (auto* playback = ArtifactPlaybackService::instance()) playback->togglePlayPause();
  else if (impl_->activeComp_) impl_->activeComp_->togglePlayPause();
 }

 void ArtifactActiveContextService::stop() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "stop");
  else if (auto* playback = ArtifactPlaybackService::instance()) playback->stop();
  else if (impl_->activeComp_) impl_->activeComp_->stop();
 }

 void ArtifactActiveContextService::nextFrame() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "nextFrame");
  else if (auto* playback = ArtifactPlaybackService::instance()) playback->goToNextFrame();
  else if (impl_->activeComp_) impl_->activeComp_->goToFrame(impl_->activeComp_->framePosition().framePosition() + 1);
 }

 void ArtifactActiveContextService::prevFrame() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "prevFrame");
  else if (auto* playback = ArtifactPlaybackService::instance()) playback->goToPreviousFrame();
  else if (impl_->activeComp_) impl_->activeComp_->goToFrame(impl_->activeComp_->framePosition().framePosition() - 1);
 }

 void ArtifactActiveContextService::goToStart() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "goToStart");
  else if (auto* playback = ArtifactPlaybackService::instance()) playback->goToStartFrame();
  else if (impl_->activeComp_) impl_->activeComp_->goToStartFrame();
 }

 void ArtifactActiveContextService::goToEnd() {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "goToEnd");
  else if (auto* playback = ArtifactPlaybackService::instance()) playback->goToEndFrame();
  else if (impl_->activeComp_) impl_->activeComp_->goToEndFrame();
 }

 void ArtifactActiveContextService::seekToFrame(int64_t frame) {
  if (impl_->handler_) QMetaObject::invokeMethod(impl_->handler_, "seekToFrame", Q_ARG(int64_t, frame));
  else if (auto* playback = ArtifactPlaybackService::instance()) playback->goToFrame(FramePosition(static_cast<int>(frame)));
  else if (impl_->activeComp_) impl_->activeComp_->goToFrame(frame);
 }

 // --- Layer Actions ---
 void ArtifactActiveContextService::setLayerInAtCurrentTime() {
  auto l = ArtifactLayerSelectionManager::instance()->currentLayer();
  if (l && impl_->activeComp_) {
   const auto now = impl_->activeComp_->framePosition();
   if (l->isTimingLocked() ||
       l->inPoint().framePosition() == now.framePosition()) {
    return;
   }
    const qint64 oldIn = l->inPoint().framePosition();
    const qint64 newIn = now.framePosition();
    if (auto* undo = UndoManager::instance()) {
     if (!undo->push(std::make_unique<SetLayerPropertyValueCommand>(
             l, QStringLiteral("time.inPoint"),
             QVariant::fromValue(oldIn), QVariant::fromValue(newIn),
             QStringLiteral("Set Layer In")))) {
      return;
     }
    } else {
     l->setInPoint(now);
     if (l->inPoint().framePosition() != newIn) {
      return;
     }
   }
   qDebug() << "[ActiveContext] Set In for" << l->layerName() << "to" << newIn;
  }
 }

 void ArtifactActiveContextService::setLayerOutAtCurrentTime() {
  auto l = ArtifactLayerSelectionManager::instance()->currentLayer();
  if (l && impl_->activeComp_) {
   const auto now = impl_->activeComp_->framePosition();
   if (l->isTimingLocked() ||
       l->outPoint().framePosition() == now.framePosition()) {
    return;
   }
    const qint64 oldOut = l->outPoint().framePosition();
    const qint64 newOut = now.framePosition();
    if (auto* undo = UndoManager::instance()) {
     if (!undo->push(std::make_unique<SetLayerPropertyValueCommand>(
             l, QStringLiteral("time.outPoint"),
             QVariant::fromValue(oldOut), QVariant::fromValue(newOut),
             QStringLiteral("Set Layer Out")))) {
      return;
     }
    } else {
     l->setOutPoint(now);
     if (l->outPoint().framePosition() != newOut) {
      return;
     }
   }
  }
 }

 void ArtifactActiveContextService::trimLayerInAtCurrentTime() {
  auto l = ArtifactLayerSelectionManager::instance()->currentLayer();
  if (l && impl_->activeComp_) {
   auto now = impl_->activeComp_->framePosition();
   if (l->isTimingLocked()) {
    return;
   }
   const qint64 oldIn = l->inPoint().framePosition();
   const qint64 oldStart = l->startTime().framePosition();
   const qint64 newFrame = now.framePosition();
   if (oldIn == newFrame && oldStart == newFrame) {
    return;
   }
   if (auto* undo = UndoManager::instance()) {
    auto macro = std::make_unique<MacroUndoCommand>(
        QStringLiteral("Trim Layer In"));
    if (oldIn != newFrame) {
     macro->addChild(std::make_unique<SetLayerPropertyValueCommand>(
         l, QStringLiteral("time.inPoint"),
         QVariant::fromValue(oldIn), QVariant::fromValue(newFrame),
         QStringLiteral("Trim Layer In Point")));
    }
    if (oldStart != newFrame) {
     macro->addChild(std::make_unique<SetLayerPropertyValueCommand>(
         l, QStringLiteral("time.startTime"),
         QVariant::fromValue(oldStart), QVariant::fromValue(newFrame),
         QStringLiteral("Trim Layer Source Start")));
    }
     if (!undo->push(std::move(macro))) {
      return;
     }
    } else {
     l->setInPoint(now);
     l->setStartTime(now);
     if (l->inPoint().framePosition() != newFrame ||
         l->startTime().framePosition() != newFrame) {
      l->setInPoint(FramePosition(oldIn));
      l->setStartTime(FramePosition(oldStart));
      return;
     }
   }
   qDebug() << "[ActiveContext] Trim In for" << l->layerName() << "to" << now.framePosition();
  }
 }

 void ArtifactActiveContextService::trimLayerOutAtCurrentTime() {
  auto l = ArtifactLayerSelectionManager::instance()->currentLayer();
  if (l && impl_->activeComp_) {
   auto now = impl_->activeComp_->framePosition();
   if (l->isTimingLocked() ||
       l->outPoint().framePosition() == now.framePosition()) {
    return;
   }
    const qint64 oldOut = l->outPoint().framePosition();
    const qint64 newOut = now.framePosition();
    if (auto* undo = UndoManager::instance()) {
     if (!undo->push(std::make_unique<SetLayerPropertyValueCommand>(
             l, QStringLiteral("time.outPoint"),
             QVariant::fromValue(oldOut), QVariant::fromValue(newOut),
             QStringLiteral("Trim Layer Out")))) {
      return;
     }
    } else {
     l->setOutPoint(now);
     if (l->outPoint().framePosition() != newOut) {
      return;
     }
   }
   qDebug() << "[ActiveContext] Trim Out for" << l->layerName() << "to" << now.framePosition();
  }
 }

 void ArtifactActiveContextService::splitLayerAtCurrentTime() {
  auto l = ArtifactLayerSelectionManager::instance()->currentLayer();
  auto comp = impl_->activeComp_;
  if (l && comp) {
   if (auto* svc = ArtifactProjectService::instance()) {
    svc->splitLayerWithUndo(comp->id(), l->id());
   }
  }
 }

};
