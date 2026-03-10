// ReSharper disable All
module;

#include <QJsonDocument>
#include <QList>
#include <qforeach.h>
#include <wobjectimpl.h>
#include <QHash>
#include <QVector>
#include <QMultiMap>
#include <typeindex>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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
module Artifact.Composition.Abstract;




import Container;
import Frame.Position;
import Frame.Range;
import Frame.Rate;
import Composition.Settings;
import Artifact.Composition.Result;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
//import Playback.Clock;

namespace Artifact {
 using namespace ArtifactCore;
	


 W_OBJECT_IMPL(ArtifactAbstractComposition)

 class ArtifactAbstractComposition::Impl {
 private:
  

 public:
  Impl(ArtifactAbstractComposition* owner);
  ~Impl();
  ArtifactAbstractComposition* owner_;
  MultiIndexLayerContainer layerMultiIndex_;
  CompositionSettings settings_;
  FramePosition position_;
  FrameRange frameRange_ = FrameRange(0, 300);
  FrameRate frameRate_;
  bool looping_ = false;
  float playbackSpeed_ = 1.0f;
  CompositionID id_;
  //PlaybackClock playbackClock_;  // 高精度再生クロック
  
  AppendLayerToCompositionResult appendLayerTop(ArtifactAbstractLayerPtr layer);
  AppendLayerToCompositionResult appendLayerBottom(ArtifactAbstractLayerPtr layer);
  bool containsLayerById(const LayerID& id) const;
  void removeAllLayers();
  void removeLayer(const LayerID& id);
  const FramePosition framePosition() const;
  void setFramePosition(const FramePosition& position);
  void goToStartFrame();
  void goToEndFrame();
  void goToFrame(int64_t frame=0);
  QVector<ArtifactAbstractLayerPtr> allLayer() const;
  QVector<ArtifactAbstractLayerPtr> allLayerBackToFront() const;

   ArtifactAbstractLayerPtr frontMostLayer() const;
   ArtifactAbstractLayerPtr backMostLayer() const;
   bool hasVideo() const;
   bool hasAudio() const;
   void moveLayerToIndex(const LayerID& id, int newIndex);
   void bringToFront(const LayerID& id);
   void sendToBack(const LayerID& id);

   bool isPlaying_ = false;
 };

 ArtifactAbstractComposition::Impl::Impl(ArtifactAbstractComposition* owner) : owner_(owner)
 {

 }

 ArtifactAbstractComposition::Impl::~Impl()
 {

 }

 AppendLayerToCompositionResult ArtifactAbstractComposition::Impl::appendLayerTop(ArtifactAbstractLayerPtr layer)
 {
  AppendLayerToCompositionResult result;

  if (!layer) {
   result.success = false;
   result.error = AppendLayerToCompositionError::LayerNotFound;
   result.message = QString("Layer not found");
   return result;
  }

  auto id = layer->id();

  layerMultiIndex_.add(layer,id,layer->type_index());

  result.success = true;
  result.error = AppendLayerToCompositionError::None;
  result.message = QString("Layer added successfully");
  return result;
 }

 void ArtifactAbstractComposition::Impl::removeAllLayers()
 {
  layerMultiIndex_.clear();
 }

void ArtifactAbstractComposition::Impl::removeLayer(const LayerID& id)
{
   // Safe Detachment: Clear parent link of any layer that refers to this ID
   for (auto& layer : layerMultiIndex_) {
       if (layer->parentLayerId() == id) {
           layer->clearParent();
       }
   }
   layerMultiIndex_.removeById(id);
}

 bool ArtifactAbstractComposition::Impl::containsLayerById(const LayerID& id) const
 {
  return layerMultiIndex_.containsId(id);
 }

 void ArtifactAbstractComposition::Impl::goToStartFrame()
 {
  //goToFrame(0);
 }

 void ArtifactAbstractComposition::Impl::goToEndFrame()
 {

 }

 void ArtifactAbstractComposition::Impl::setFramePosition(const FramePosition& position)
 {

 	
 }

 const FramePosition ArtifactAbstractComposition::Impl::framePosition() const
 {
  return position_;
 }

  void ArtifactAbstractComposition::Impl::goToFrame(int64_t frame/*=0*/)
  {
    position_ = FramePosition(frame);
    for (auto& layer : layerMultiIndex_) {
        if (layer) layer->goToFrame(frame);
    }
  }

 QVector<ArtifactAbstractLayerPtr> ArtifactAbstractComposition::Impl::allLayer() const
 {
  return layerMultiIndex_.all();
 }

  AppendLayerToCompositionResult ArtifactAbstractComposition::Impl::appendLayerBottom(ArtifactAbstractLayerPtr layer)
  {
      AppendLayerToCompositionResult result;
      if (!layer) {
          result.success = false;
          result.error = AppendLayerToCompositionError::LayerNotFound;
          return result;
      }
      layerMultiIndex_.insertAt(0, layer, layer->id(), layer->type_index());
      result.success = true;
      result.error = AppendLayerToCompositionError::None;
      return result;
  }

  void ArtifactAbstractComposition::Impl::moveLayerToIndex(const LayerID& id, int newIndex)
  {
      auto layer = layerMultiIndex_.findById(id);
      if (!layer) return;
      int oldIndex = layerMultiIndex_.indexOf(layer);
      if (oldIndex == -1) return;
      layerMultiIndex_.move(oldIndex, newIndex);
  }

  void ArtifactAbstractComposition::Impl::bringToFront(const LayerID& id)
  {
      auto layer = layerMultiIndex_.findById(id);
      if (!layer) return;
      int oldIndex = layerMultiIndex_.indexOf(layer);
      if (oldIndex == -1) return;
      layerMultiIndex_.move(oldIndex, layerMultiIndex_.all().size() - 1);
  }

  void ArtifactAbstractComposition::Impl::sendToBack(const LayerID& id)
  {
      auto layer = layerMultiIndex_.findById(id);
      if (!layer) return;
      int oldIndex = layerMultiIndex_.indexOf(layer);
      if (oldIndex == -1) return;
      layerMultiIndex_.move(oldIndex, 0);
  }

 bool ArtifactAbstractComposition::Impl::hasVideo() const
 {
  for (const auto& layer : layerMultiIndex_) {
   if (layer->hasVideo())
   {
	return true;
   }
  }
 	
  return false;
 }

 bool ArtifactAbstractComposition::Impl::hasAudio() const
 {
  for (const auto& layer : layerMultiIndex_) {
   if (layer->hasAudio())
   {
	return true;
   }
  }
  return false;
 }

ArtifactAbstractLayerPtr ArtifactAbstractComposition::Impl::frontMostLayer() const
{
    auto all = layerMultiIndex_.all();
    if (!all.isEmpty()) return all.last();
    return ArtifactAbstractLayerPtr();
}

ArtifactAbstractLayerPtr ArtifactAbstractComposition::Impl::backMostLayer() const
{
    auto all = layerMultiIndex_.all();
    if (!all.isEmpty()) return all.first();
    return ArtifactAbstractLayerPtr();
}

 QVector<ArtifactAbstractLayerPtr> ArtifactAbstractComposition::Impl::allLayerBackToFront() const
 {
  auto v = layerMultiIndex_.all();
  std::reverse(v.begin(), v.end());
  return v;
 }

 ArtifactAbstractComposition::ArtifactAbstractComposition(const CompositionID& id, const ArtifactCompositionInitParams& params) :impl_(new Impl(this))
 {
  impl_->id_ = id;
 	
 	
 }

 ArtifactAbstractComposition::~ArtifactAbstractComposition()
 {
  delete impl_;
 }

 bool ArtifactAbstractComposition::containsLayerById(const LayerID& id)
 {
  return impl_->containsLayerById(id);
 }

ArtifactAbstractLayerPtr ArtifactAbstractComposition::layerById(const LayerID& id)
{
  return impl_->layerMultiIndex_.findById(id);
}


 void ArtifactAbstractComposition::setBackGroundColor(const FloatColor& color)
 {

 }
 
 void ArtifactAbstractComposition::setFramePosition(const FramePosition& position)
 {
  impl_->setFramePosition(position);
 }

  FramePosition ArtifactAbstractComposition::framePosition() const
  {
   return impl_->framePosition();
  }

 void ArtifactAbstractComposition::goToStartFrame()
 {
  impl_->goToEndFrame();
 }

 void ArtifactAbstractComposition::goToEndFrame()
 {
  impl_->goToEndFrame();
 }

 void ArtifactAbstractComposition::goToFrame(int64_t frameNumber /*= 0*/)
 {
  impl_->goToFrame(frameNumber);
 }

  FrameRange ArtifactAbstractComposition::frameRange() const
  {
   return impl_->frameRange_;
  }

  FrameRate ArtifactAbstractComposition::frameRate() const
  {
   return impl_->frameRate_;
  }

 bool ArtifactAbstractComposition::hasVideo() const
 {
  return true;
 }

 bool ArtifactAbstractComposition::hasAudio() const
 {
  return true;
 }

 QVector<Artifact::ArtifactAbstractLayerPtr> ArtifactAbstractComposition::allLayer()
 {
  return impl_->layerMultiIndex_.all();
 }

 AppendLayerToCompositionResult ArtifactAbstractComposition::appendLayerTop(ArtifactAbstractLayerPtr layer)
 {
  return impl_->appendLayerTop(layer);
 }

 AppendLayerToCompositionResult ArtifactAbstractComposition::appendLayerBottom(ArtifactAbstractLayerPtr layer)
 {
   return impl_->appendLayerBottom(layer);
 }

void ArtifactAbstractComposition::insertLayerAt(ArtifactAbstractLayerPtr layer, int index/*=0*/)
{
    if (!layer) return;
    impl_->layerMultiIndex_.insertAt(index, layer, layer->id(), layer->type_index());
}

void ArtifactAbstractComposition::moveLayerToIndex(const LayerID& id, int newIndex)
{
    impl_->moveLayerToIndex(id, newIndex);
}

 void ArtifactAbstractComposition::removeLayer(const LayerID& id)
 {
  impl_->removeLayer(id);
 }

void ArtifactAbstractComposition::removeAllLayers()
{
    impl_->removeAllLayers();
}

ArtifactAbstractLayerPtr ArtifactAbstractComposition::frontMostLayer() const
{
    return impl_->frontMostLayer();
}

ArtifactAbstractLayerPtr ArtifactAbstractComposition::backMostLayer() const
{
    return impl_->backMostLayer();
}

void ArtifactAbstractComposition::bringToFront(const LayerID& id)
{
    impl_->bringToFront(id);
}

void ArtifactAbstractComposition::sendToBack(const LayerID& id)
{
    impl_->sendToBack(id);
}

CompositionID ArtifactAbstractComposition::id() const
{
    return impl_->id_;
}

int ArtifactAbstractComposition::layerCount() const
{
    return impl_->layerMultiIndex_.all().size();
}

bool ArtifactAbstractComposition::isAudioOnly() const
{
 return false;
}

CompositionSettings ArtifactAbstractComposition::settings() const
{
  return impl_->settings_;
}

bool ArtifactAbstractComposition::isVisual() const
{
 return true;
}

bool ArtifactAbstractComposition::isPlaying() const
{
 return impl_->isPlaying_;
}

void ArtifactAbstractComposition::play()
{
    impl_->isPlaying_ = true;
}

void ArtifactAbstractComposition::pause()
{
    impl_->isPlaying_ = false;
}

void ArtifactAbstractComposition::stop()
{
    impl_->isPlaying_ = false;
    goToStartFrame();
}

void ArtifactAbstractComposition::togglePlayPause()
{
    impl_->isPlaying_ = !impl_->isPlaying_;
}

float ArtifactAbstractComposition::playbackSpeed() const
{
    return impl_->playbackSpeed_;
}

void ArtifactAbstractComposition::setPlaybackSpeed(float speed)
{
    impl_->playbackSpeed_ = speed;
}

bool ArtifactAbstractComposition::isLooping() const
{
    return impl_->looping_;
}

void ArtifactAbstractComposition::setLooping(bool loop)
{
    impl_->looping_ = loop;
}

void ArtifactAbstractComposition::setFrameRange(const FrameRange& range)
{
    impl_->frameRange_ = range;
}

void ArtifactAbstractComposition::setFrameRate(const FrameRate& rate)
{
    impl_->frameRate_ = rate;
}

QJsonDocument ArtifactAbstractComposition::toJson() const{
    QJsonObject obj;
    obj["id"] = id().toString();
    QJsonArray layersArray;
    for (const auto& layer : impl_->layerMultiIndex_.all()) {
        if (layer) {
            layersArray.append(layer->toJson());
        }
    }
    obj["layers"] = layersArray;
    // 必要に応じて他のプロパティも追加可能
    return QJsonDocument(obj);
}

void ArtifactAbstractComposition::removeLayerById(const ArtifactCore::LayerID& id)
{
    // Placeholder for removing layer by ID
}

std::shared_ptr<ArtifactAbstractComposition> ArtifactAbstractComposition::fromJson(const QJsonDocument& doc){
    if (!doc.isObject()) return nullptr;
    QJsonObject obj = doc.object();
    
    CompositionID compId;
    if (obj.contains("id")) {
        compId = CompositionID(obj["id"].toString());
    }
    
    ArtifactCompositionInitParams params;
    auto comp = std::make_shared<ArtifactAbstractComposition>(compId, params);
    
    if (obj.contains("layers") && obj["layers"].isArray()) {
        QJsonArray arr = obj["layers"].toArray();
        QVector<ArtifactAbstractLayerPtr> loadedLayers;
        for (const auto& v : arr) {
            if (v.isObject()) {
                auto layer = ArtifactLayerFactory::createFromJson(v.toObject());
                if (layer) {
                    comp->appendLayerTop(layer);
                    loadedLayers.append(layer);
                }
            }
        }
        
        // Parent resolution pass
        for (const auto& layer : loadedLayers) {
            QJsonObject lobj = arr.at(loadedLayers.indexOf(layer)).toObject();
            if (lobj.contains("parentId")) {
                LayerID pid(lobj["parentId"].toString());
                layer->setParentById(pid);
            }
        }
    }
    return comp;
}

};
