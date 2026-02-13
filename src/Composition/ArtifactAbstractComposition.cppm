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
#include <QImage>

module Artifact.Composition.Abstract;

import std;
import Container;
import Frame.Position;
import Composition.Settings;
import Artifact.Composition.Result;
import Artifact.Layers;
//import Playback.Clock;

namespace Artifact {
 using namespace ArtifactCore;
	


 W_OBJECT_IMPL(ArtifactAbstractComposition)

 class ArtifactAbstractComposition::Impl {
 private:
  

 public:
  Impl();
  ~Impl();
  MultiIndexLayerContainer layerMultiIndex_;
  CompositionSettings settings_;
  FramePosition position_;
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

  bool isPlaying_ = false;
 };

 ArtifactAbstractComposition::Impl::Impl()
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
  layerMultiIndex_.removeById(id);
}

 bool ArtifactAbstractComposition::Impl::containsLayerById(const LayerID& id) const
 {
  return layerMultiIndex_.containsId(id);
 }

 void ArtifactAbstractComposition::Impl::goToStartFrame()
 {
  goToFrame(0);
 }

 void ArtifactAbstractComposition::Impl::goToEndFrame()
 {
  // TODO: settings_ does not have frameRange() method
  // if (!settings_.frameRange().isNil()) {
  //  goToFrame(settings_.frameRange().end());
  // }
  goToFrame(0); // Temporary placeholder
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
 }

 QVector<ArtifactAbstractLayerPtr> ArtifactAbstractComposition::Impl::allLayer() const
 {
  return layerMultiIndex_.all();
 }

 AppendLayerToCompositionResult ArtifactAbstractComposition::Impl::appendLayerBottom(ArtifactAbstractLayerPtr layer)
 {
     AppendLayerToCompositionResult result;


	 return result;
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

 ArtifactAbstractComposition::ArtifactAbstractComposition(const CompositionID& id, const ArtifactCompositionInitParams& params) :impl_(new Impl())
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
  if (impl_) {
   // TODO: settings_ does not have setBackgroundColor() method
   // impl_->settings_.setBackgroundColor(color);
  }
 }
 
 void ArtifactAbstractComposition::setFramePosition(const FramePosition& position)
 {
  impl_->setFramePosition(position);
 }

 void ArtifactAbstractComposition::goToStartFrame()
 {
  impl_->goToStartFrame();
 }

 void ArtifactAbstractComposition::goToEndFrame()
 {
  impl_->goToEndFrame();
 }

 void ArtifactAbstractComposition::goToFrame(int64_t frameNumber /*= 0*/)
 {
  impl_->goToFrame(frameNumber);
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
    // TODO: insertAt not yet implemented in MultiIndexContainer
    // For now, just append to top
    if (!layer) return;
    appendLayerTop(layer);
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

CompositionID ArtifactAbstractComposition::id() const
{
    return impl_->id_;
}

bool ArtifactAbstractComposition::isAudioOnly() const
{
 return false;
}

bool ArtifactAbstractComposition::isVisual() const
{
 return true;
}

bool ArtifactAbstractComposition::isPlaying() const
{
 return impl_->isPlaying_;
}

QJsonDocument ArtifactAbstractComposition::toJson() const
{
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

// TODO: fromJson is not declared in the interface - commenting out for now
/*
std::shared_ptr<ArtifactAbstractComposition> ArtifactAbstractComposition::fromJson(const QJsonDocument& doc)
{
    if (!doc.isObject()) return nullptr;
    QJsonObject obj = doc.object();
    // ID取得
    CompositionID compId;
    if (obj.contains("id")) {
        compId = CompositionID(obj["id"].toString());
    }
    // 仮: デフォルトパラメータで生成
    ArtifactCompositionInitParams params;
    auto comp = std::make_shared<ArtifactAbstractComposition>(compId, params);
    // レイヤー復元
    if (obj.contains("layers") && obj["layers"].isArray()) {
        QJsonArray arr = obj["layers"].toArray();
        for (const auto& v : arr) {
            if (v.isObject()) {
                auto layer = ArtifactAbstractLayer::fromJson(v.toObject());
                if (layer) comp->appendLayerTop(layer);
            }
        }
    }
    // 必要に応じて他のプロパティも復元
    return comp;
 }
 */

 QImage ArtifactAbstractComposition::getThumbnail(int width, int height) const
 {
  // サムネイル用に黒いイメージを作成（プレースホルダー実装）
  QImage thumbnail(width, height, QImage::Format_ARGB32);
  thumbnail.fill(QColor(0, 0, 0, 255));  // 黒で塗りつぶし

  // TODO: 実際のコンポジションコンテンツをサムネイルにレンダリング
  // 例：レイヤーをすべてレンダリングしてサムネイルに合成
  qDebug() << "[Composition Thumbnail] Generated placeholder thumbnail:" 
           << width << "x" << height << "layers:" << impl_->layerMultiIndex_.all().size();

  return thumbnail;
 }

};
