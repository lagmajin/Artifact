// ReSharper disable All
module;
#include <utility>
#include <QJsonDocument>
#include <QList>
#include <qforeach.h>
#include <wobjectimpl.h>
#include <QHash>
#include <QVector>
#include <QMultiMap>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>
#include <QSet>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <QPointF>
#include <QRectF>
#include "../../include/Layer/ArtifactLayerJsonFactory.ixx"

module Artifact.Composition.Abstract;

import std;

import Container;
import Frame.Position;
import Frame.Range;
import Frame.Rate;
import Composition.Context;
import Composition.Settings;
import Artifact.Composition.Result;
import Artifact.Layer.Abstract;
import Artifact.Event.Types;
import Event.Bus;
//import Playback.Clock;

namespace Artifact {
 using namespace ArtifactCore;

namespace {

void collectAssetSourcePaths(const QJsonValue& value, QSet<QString>& out)
{
  if (value.isObject()) {
    const QJsonObject obj = value.toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      const QString key = it.key();
      const QString loweredKey = key.toLower();
      if ((loweredKey == QStringLiteral("sourcepath")) ||
          (loweredKey == QStringLiteral("filepath")) ||
          loweredKey.endsWith(QStringLiteral(".sourcepath")) ||
          loweredKey.endsWith(QStringLiteral(".filepath")) ||
          loweredKey.contains(QStringLiteral("sourcepath")) ||
          loweredKey.contains(QStringLiteral("filepath"))) {
        const QString candidate = it.value().toString().trimmed();
        if (!candidate.isEmpty()) {
          out.insert(candidate);
        }
      }
      collectAssetSourcePaths(it.value(), out);
    }
    return;
  }

  if (value.isArray()) {
    const QJsonArray array = value.toArray();
    for (const auto& child : array) {
      collectAssetSourcePaths(child, out);
    }
  }
}

ArtifactCore::AssetID assetIdForPath(const QString& path)
{
  const QFileInfo info(path);
  QString normalized = info.canonicalFilePath();
  if (normalized.isEmpty()) {
    normalized = QDir::cleanPath(path);
  }
  return ArtifactCore::AssetID(QCryptographicHash::hash(normalized.toUtf8(), QCryptographicHash::Sha256).toHex().constData());
}

Artifact::ResponsiveLayoutVariant makeDefaultResponsiveLayoutVariant(const QSize& size)
 {
   Artifact::ResponsiveLayoutVariant variant;
   variant.variantId = QStringLiteral("default");
   variant.displayName = QStringLiteral("Default");
   variant.baseSize = size.isValid() ? size : QSize(1920, 1080);
   variant.aspectRatio = variant.baseSize.height() > 0
       ? static_cast<qreal>(variant.baseSize.width()) / static_cast<qreal>(variant.baseSize.height())
       : 0.0;
   variant.safeArea = QRectF(0.0, 0.0, 1.0, 1.0);
   variant.contentAnchor = QPointF(0.5, 0.5);
   variant.layoutRules.insert(QStringLiteral("scaleMode"), QStringLiteral("fit"));
   variant.layoutRules.insert(QStringLiteral("cropMode"), QStringLiteral("none"));
   variant.layoutRules.insert(QStringLiteral("guidePreset"), QStringLiteral("default"));
   variant.enabled = true;
   return variant;
 }

 Artifact::ResponsiveLayoutSet makeDefaultResponsiveLayoutSet(const QSize& size)
 {
   Artifact::ResponsiveLayoutSet layout;
   layout.activeVariantId = QStringLiteral("default");
   layout.defaultPolicy = QStringLiteral("manual");
   layout.variants.append(makeDefaultResponsiveLayoutVariant(size));
   return layout;
 }

 float limiterGainForSegment(const AudioSegment& segment)
 {
   constexpr float ceiling = 0.995f;
   float peak = 0.0f;
   for (const auto& channel : segment.channelData) {
     for (const float sample : channel) {
       peak = std::max(peak, std::abs(sample));
       if (peak >= ceiling) {
         break;
       }
     }
   }
   return peak > ceiling ? ceiling / peak : 1.0f;
 }

 } // namespace

Artifact::ResponsiveLayoutSet makeDefaultResponsiveLayoutSet(const QSize& size)
{
  Artifact::ResponsiveLayoutSet layout;
  layout.activeVariantId = QStringLiteral("default");
  layout.defaultPolicy = QStringLiteral("manual");
  layout.variants.append(makeDefaultResponsiveLayoutVariant(size));
  return layout;
=======
float limiterGainForSegment(const AudioSegment& segment)
{
  constexpr float ceiling = 0.995f;
  float peak = 0.0f;
  for (const auto& channel : segment.channelData) {
    for (const float sample : channel) {
      peak = std::max(peak, std::abs(sample));
      if (peak >= ceiling) {
        break;
      }
    }
  }
  return peak > ceiling ? ceiling / peak : 1.0f;
>>>>>>> 7115214a (Audio mixer: add pan support, extend volume range to 0-2.0, add brickwall limiter for overflow protection)
}

} // namespace
	


 W_OBJECT_IMPL(ArtifactAbstractComposition)

 class ArtifactAbstractComposition::Impl {
 private:
  

 public:
  Impl(ArtifactAbstractComposition* owner);
  ~Impl();
  ArtifactAbstractComposition* owner_;
  MultiIndexLayerContainer layerMultiIndex_;
  CompositionSettings settings_;
  CompositionContext context_;
  FramePosition position_;
  FrameRange frameRange_ = FrameRange(0, 300);
  FrameRange workAreaRange_ = FrameRange(0, 300);
  FrameRate frameRate_;
  ResponsiveLayoutSet responsiveLayout_;
  bool looping_ = false;
  float playbackSpeed_ = 1.0f;
  CompositionID id_;
  QString compositionNote_;
  FloatColor backgroundColor_ = { 0.47f, 0.47f, 0.47f, 1.0f };
  mutable QImage thumbnailCache_;
  mutable QSize thumbnailCacheSize_;
  mutable bool thumbnailCacheValid_ = false;
  //PlaybackClock playbackClock_;  // 高精度再生クロック
  
  AppendLayerToCompositionResult appendLayerTop(ArtifactAbstractLayerPtr layer);
  AppendLayerToCompositionResult appendLayerBottom(ArtifactAbstractLayerPtr layer);
  void invalidateThumbnailCache();
  bool containsLayerById(const LayerID& id) const;
  void removeAllLayers();
  void recalculateFrameRange();
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

    // Asset usage tracking
    QVector<ArtifactCore::AssetID> getUsedAssets() const;
  };

 ArtifactAbstractComposition::Impl::Impl(ArtifactAbstractComposition* owner) : owner_(owner)
 {

 }

 ArtifactAbstractComposition::Impl::~Impl()
 {

 }

void ArtifactAbstractComposition::Impl::invalidateThumbnailCache()
{
  thumbnailCacheValid_ = false;
  thumbnailCache_.fill(Qt::transparent);
  thumbnailCacheSize_ = QSize();
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

  layer->setComposition(owner_);
  
  // Newly created layers often have a default duration. 
  // If it's a solid/plane layer and hasn't been configured yet, match composition duration.
  const int64_t currentOut = layer->outPoint().framePosition();
  const int64_t currentIn = layer->inPoint().framePosition();
  const int64_t currentStart = layer->startTime().framePosition();
  const int64_t compEnd = frameRange_.end();
  
  if (currentOut == 300 && currentIn == 0 && currentStart == 0 && compEnd > 0) {
    layer->setOutPoint(FramePosition(compEnd));
  }
  
  layerMultiIndex_.add(layer,id,layer->type_index());
  invalidateThumbnailCache();
  recalculateFrameRange();

  result.success = true;
  result.error = AppendLayerToCompositionError::None;
  result.message = QString("Layer added successfully");

  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      owner_->id().toString(), id.toString(),
      LayerChangedEvent::ChangeType::Created});
  return result;
  }

  void ArtifactAbstractComposition::Impl::recalculateFrameRange()
  {
   int64_t maxOut = 0;
   for (const auto& layer : layerMultiIndex_) {
    if (layer) {
     maxOut = std::max(maxOut, layer->outPoint().framePosition());
    }
   }
   if (maxOut > 0) {
    frameRange_ = FrameRange(0, maxOut);
   }
  }

  void ArtifactAbstractComposition::Impl::removeAllLayers()
 {
  for (auto& layer : layerMultiIndex_) {
   if (layer) {
    layer->setComposition(static_cast<ArtifactAbstractComposition *>(nullptr));
   }
  }
  layerMultiIndex_.clear();
  invalidateThumbnailCache();
  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      owner_->id().toString(), QString{},
      LayerChangedEvent::ChangeType::Removed});
 }

void ArtifactAbstractComposition::Impl::removeLayer(const LayerID& id)
{
   auto removedLayer = layerMultiIndex_.findById(id);
   // Safe Detachment: Clear parent link of any layer that refers to this ID
    for (auto& layer : layerMultiIndex_) {
       if (layer->parentLayerId() == id) {
           layer->clearParent();
       }
   }
    layerMultiIndex_.removeById(id);
    if (removedLayer) {
     removedLayer->setComposition(static_cast<ArtifactAbstractComposition *>(nullptr));
     invalidateThumbnailCache();
     ArtifactCore::globalEventBus().publish(LayerChangedEvent{
         owner_->id().toString(), id.toString(),
         LayerChangedEvent::ChangeType::Removed});
    }
}

 bool ArtifactAbstractComposition::Impl::containsLayerById(const LayerID& id) const
 {
  return layerMultiIndex_.containsId(id);
 }

 void ArtifactAbstractComposition::Impl::goToStartFrame()
 {
  goToFrame(frameRange_.start());
 }

 void ArtifactAbstractComposition::Impl::goToEndFrame()
 {
  goToFrame(frameRange_.end());
 }

 void ArtifactAbstractComposition::Impl::setFramePosition(const FramePosition& position)
 {
    if (position_ == position) {
        return;
    }
    // Playback updates only need the composition's current frame state.
    // Layer propagation is handled by goToFrame() for explicit timeline edits/seeks.
    position_ = position;
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
      layer->setComposition(owner_);
      
      const int64_t currentOut = layer->outPoint().framePosition();
      const int64_t currentIn = layer->inPoint().framePosition();
      const int64_t currentStart = layer->startTime().framePosition();
      const int64_t compEnd = frameRange_.end();
      
      if (currentOut == 300 && currentIn == 0 && currentStart == 0 && compEnd > 0) {
          layer->setOutPoint(FramePosition(compEnd));
      }
      
      layerMultiIndex_.insertAt(0, layer, layer->id(), layer->type_index());
      invalidateThumbnailCache();
      recalculateFrameRange();
      result.success = true;
      result.error = AppendLayerToCompositionError::None;
      ArtifactCore::globalEventBus().publish(LayerChangedEvent{
          owner_->id().toString(), layer->id().toString(),
          LayerChangedEvent::ChangeType::Created});
      return result;
  }

  void ArtifactAbstractComposition::Impl::moveLayerToIndex(const LayerID& id, int newIndex)
  {
      auto layer = layerMultiIndex_.findById(id);
      if (!layer) return;
      int oldIndex = layerMultiIndex_.indexOf(layer);
      if (oldIndex == -1) return;
      layerMultiIndex_.move(oldIndex, newIndex);
      invalidateThumbnailCache();
      ArtifactCore::globalEventBus().publish(LayerChangedEvent{
          owner_->id().toString(), id.toString(),
          LayerChangedEvent::ChangeType::Modified});
  }

  void ArtifactAbstractComposition::Impl::bringToFront(const LayerID& id)
  {
      auto layer = layerMultiIndex_.findById(id);
      if (!layer) return;
      int oldIndex = layerMultiIndex_.indexOf(layer);
      if (oldIndex == -1) return;
      layerMultiIndex_.move(oldIndex, layerMultiIndex_.all().size() - 1);
      invalidateThumbnailCache();
      ArtifactCore::globalEventBus().publish(LayerChangedEvent{
          owner_->id().toString(), id.toString(),
          LayerChangedEvent::ChangeType::Modified});
  }

  void ArtifactAbstractComposition::Impl::sendToBack(const LayerID& id)
  {
      auto layer = layerMultiIndex_.findById(id);
      if (!layer) return;
      int oldIndex = layerMultiIndex_.indexOf(layer);
      if (oldIndex == -1) return;
      layerMultiIndex_.move(oldIndex, 0);
      invalidateThumbnailCache();
      ArtifactCore::globalEventBus().publish(LayerChangedEvent{
          owner_->id().toString(), id.toString(),
          LayerChangedEvent::ChangeType::Modified});
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

  QVector<ArtifactCore::AssetID> ArtifactAbstractComposition::Impl::getUsedAssets() const
  {
    QVector<ArtifactCore::AssetID> usedAssets;
    QSet<QString> sourcePaths;

    // Collect assets from all layers
    for (const auto& layer : layerMultiIndex_.all()) {
      if (!layer) continue;
      collectAssetSourcePaths(layer->toJson(), sourcePaths);
    }

    for (const auto& path : sourcePaths) {
      usedAssets.append(assetIdForPath(path));
    }

    return usedAssets;
  }

QJsonObject ResponsiveLayoutVariant::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("variantId"), variantId);
    obj.insert(QStringLiteral("displayName"), displayName);
    obj.insert(QStringLiteral("width"), baseSize.width());
    obj.insert(QStringLiteral("height"), baseSize.height());
    obj.insert(QStringLiteral("aspectRatio"), aspectRatio);
    obj.insert(QStringLiteral("safeArea"), QJsonObject{
        {QStringLiteral("x"), safeArea.x()},
        {QStringLiteral("y"), safeArea.y()},
        {QStringLiteral("width"), safeArea.width()},
        {QStringLiteral("height"), safeArea.height()},
    });
    obj.insert(QStringLiteral("contentAnchor"), QJsonObject{
        {QStringLiteral("x"), contentAnchor.x()},
        {QStringLiteral("y"), contentAnchor.y()},
    });
    obj.insert(QStringLiteral("layoutRules"), layoutRules);
    obj.insert(QStringLiteral("enabled"), enabled);
    return obj;
}

ResponsiveLayoutVariant ResponsiveLayoutVariant::fromJson(const QJsonObject& obj)
{
    ResponsiveLayoutVariant variant;
    variant.variantId = obj.value(QStringLiteral("variantId")).toString();
    variant.displayName = obj.value(QStringLiteral("displayName")).toString();
    const int width = obj.value(QStringLiteral("width")).toInt(0);
    const int height = obj.value(QStringLiteral("height")).toInt(0);
    variant.baseSize = QSize(width, height);
    variant.aspectRatio = obj.value(QStringLiteral("aspectRatio")).toDouble(
        height > 0 ? static_cast<double>(width) / static_cast<double>(height) : 0.0);
    const QJsonObject safeAreaObj = obj.value(QStringLiteral("safeArea")).toObject();
    variant.safeArea = QRectF(
        safeAreaObj.value(QStringLiteral("x")).toDouble(0.0),
        safeAreaObj.value(QStringLiteral("y")).toDouble(0.0),
        safeAreaObj.value(QStringLiteral("width")).toDouble(1.0),
        safeAreaObj.value(QStringLiteral("height")).toDouble(1.0));
    const QJsonObject anchorObj = obj.value(QStringLiteral("contentAnchor")).toObject();
    variant.contentAnchor = QPointF(
        anchorObj.value(QStringLiteral("x")).toDouble(0.5),
        anchorObj.value(QStringLiteral("y")).toDouble(0.5));
    variant.layoutRules = obj.value(QStringLiteral("layoutRules")).toObject();
    variant.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    if (variant.variantId.isEmpty()) {
        variant.variantId = QStringLiteral("default");
    }
    if (variant.displayName.isEmpty()) {
        variant.displayName = variant.variantId;
    }
    if (!variant.baseSize.isValid()) {
        variant.baseSize = QSize(1920, 1080);
    }
    if (variant.aspectRatio <= 0.0 && variant.baseSize.height() > 0) {
        variant.aspectRatio = static_cast<qreal>(variant.baseSize.width()) / static_cast<qreal>(variant.baseSize.height());
    }
    return variant;
}

QJsonObject ResponsiveLayoutSet::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("activeVariantId"), activeVariantId);
    obj.insert(QStringLiteral("defaultPolicy"), defaultPolicy);
    QJsonArray variantArray;
    for (const auto& variant : variants) {
        variantArray.append(variant.toJson());
    }
    obj.insert(QStringLiteral("variants"), variantArray);
    return obj;
}

ResponsiveLayoutSet ResponsiveLayoutSet::fromJson(const QJsonObject& obj)
{
    ResponsiveLayoutSet layout;
    layout.activeVariantId = obj.value(QStringLiteral("activeVariantId")).toString();
    layout.defaultPolicy = obj.value(QStringLiteral("defaultPolicy")).toString(QStringLiteral("manual"));
    const QJsonArray variantArray = obj.value(QStringLiteral("variants")).toArray();
    for (const auto& value : variantArray) {
        if (value.isObject()) {
            layout.variants.append(ResponsiveLayoutVariant::fromJson(value.toObject()));
        }
    }
    if (layout.variants.isEmpty()) {
        layout.variants.append(makeDefaultResponsiveLayoutVariant(QSize(1920, 1080)));
    }
    if (layout.activeVariantId.isEmpty() || !layout.hasVariant(layout.activeVariantId)) {
        layout.activeVariantId = layout.variants.front().variantId;
    }
    return layout;
}

bool ResponsiveLayoutSet::hasVariant(const QString& variantId) const
{
    for (const auto& variant : variants) {
        if (variant.variantId == variantId) {
            return true;
        }
    }
    return false;
}

 ArtifactAbstractComposition::ArtifactAbstractComposition(const CompositionID& id, const ArtifactCompositionInitParams& params) :impl_(new Impl(this))
 {
  impl_->id_ = id;

  impl_->settings_.setCompositionName(params.compositionName());
  impl_->settings_.setCompositionSize(QSize(params.width(), params.height()));
  // Keep init params and live composition state consistent for viewers.
  impl_->backgroundColor_ = params.backgroundColor();
  impl_->frameRate_ = params.frameRate();

  const int64_t totalFrames = std::max<int64_t>(1, params.durationFrames());
  impl_->frameRange_ = FrameRange(0, totalFrames);

  const auto workArea = params.workArea();
  if (workArea.enabled) {
   const int64_t workStart = std::clamp<int64_t>(workArea.inPoint.rescaledTo(static_cast<int64_t>(std::round(impl_->frameRate_.framerate()))), 0, totalFrames);
   const int64_t workEnd = std::clamp<int64_t>(workArea.outPoint.rescaledTo(static_cast<int64_t>(std::round(impl_->frameRate_.framerate()))), workStart, totalFrames);
   impl_->workAreaRange_ = FrameRange(workStart, workEnd);
  } else {
   impl_->workAreaRange_ = impl_->frameRange_;
  }
  impl_->responsiveLayout_ = makeDefaultResponsiveLayoutSet(impl_->settings_.compositionSize());
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
  impl_->backgroundColor_ = FloatColor(color.r(), color.g(), color.b(), 1.0f);
  Q_EMIT changed();
 }

 FloatColor ArtifactAbstractComposition::backgroundColor() const
 {
  return impl_->backgroundColor_;
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
  return impl_->hasVideo();
}

bool ArtifactAbstractComposition::hasAudio() const
{
  return impl_->hasAudio();
}

bool ArtifactAbstractComposition::getAudio(AudioSegment &outSegment, const FramePosition &start,
                                            int frameCount, int sampleRate)
{
    bool hasAnyAudio = false;
    int activeAudioLayerCount = 0;
    int producedAudioLayerCount = 0;
    
    // Prepare output segment
    if (outSegment.channelCount() < 2) {
        outSegment.channelData.resize(2);
    }
    outSegment.sampleRate = sampleRate;
    outSegment.setFrameCount(frameCount);
    outSegment.zero();

    AudioSegment layerSegment;
    for (auto &layer : impl_->layerMultiIndex_) {
        if (layer && layer->isActiveAt(start) && layer->hasAudio()) {
            ++activeAudioLayerCount;
            if (layer->getAudio(layerSegment, start, frameCount, sampleRate)) {
                // Simple mix (Addition)
                int chCount = std::min(outSegment.channelCount(), layerSegment.channelCount());
                int fCount = std::min(outSegment.frameCount(), layerSegment.frameCount());
                
                for (int ch = 0; ch < chCount; ++ch) {
                    float* outData = outSegment.channelData[ch].data();
                    const float* layerData = layerSegment.channelData[ch].constData();
                    for (int i = 0; i < fCount; ++i) {
                        outData[i] += layerData[i];
                    }
                }
                ++producedAudioLayerCount;
                hasAnyAudio = true;
            }
        }
    }
    if (activeAudioLayerCount > 0 && !hasAnyAudio) {
        qWarning() << "[Composition][Audio] active layers produced no audio"
                   << "startFrame=" << start.framePosition()
                   << "frameCount=" << frameCount
                   << "sampleRate=" << sampleRate
                   << "activeAudioLayers=" << activeAudioLayerCount
                   << "producedAudioLayers=" << producedAudioLayerCount;
    }

    if (hasAnyAudio) {
        const float gain = limiterGainForSegment(outSegment);
        if (gain < 1.0f) {
            for (auto& channel : outSegment.channelData) {
                for (float& sample : channel) {
                    sample *= gain;
                }
            }
        }
    }

    return hasAnyAudio;
}

QVector<Artifact::ArtifactAbstractLayerPtr> ArtifactAbstractComposition::allLayer()
{
  return impl_->layerMultiIndex_.all();
}

const QVector<Artifact::ArtifactAbstractLayerPtr>&
ArtifactAbstractComposition::allLayerRef() const
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
    layer->setComposition(this);
    
    const int64_t currentOut = layer->outPoint().framePosition();
    const int64_t currentIn = layer->inPoint().framePosition();
    const int64_t compEnd = impl_->frameRange_.end();
    
    if (currentOut == 300 && currentIn == 0 && compEnd > 300) {
        layer->setOutPoint(FramePosition(compEnd));
    }
    
    impl_->layerMultiIndex_.insertAt(index, layer, layer->id(), layer->type_index());
    impl_->recalculateFrameRange();
    Q_EMIT changed();
}

void ArtifactAbstractComposition::moveLayerToIndex(const LayerID& id, int newIndex)
{
    impl_->moveLayerToIndex(id, newIndex);
}

void ArtifactAbstractComposition::removeLayer(const LayerID& id)
{
  impl_->removeLayer(id);
  impl_->recalculateFrameRange();
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

CompositionContext& ArtifactAbstractComposition::compositionContext()
{
  return impl_->context_;
}

const CompositionContext& ArtifactAbstractComposition::compositionContext() const
{
  return impl_->context_;
}

void ArtifactAbstractComposition::setCompositionContext(const CompositionContext& context)
{
  impl_->context_ = context;
  Q_EMIT changed();
}

void ArtifactAbstractComposition::setCompositionName(const UniString& name)
{
    if (impl_->settings_.compositionName() == name) {
        return;
    }
    impl_->settings_.setCompositionName(name);
    Q_EMIT changed();
}

QString ArtifactAbstractComposition::compositionNote() const
{
    return impl_->compositionNote_;
}

void ArtifactAbstractComposition::setCompositionNote(const QString& note)
{
    if (impl_->compositionNote_ == note) {
        return;
    }
    impl_->compositionNote_ = note;
    Q_EMIT compositionNoteChanged(note);
    Q_EMIT changed();
}

void ArtifactAbstractComposition::setCompositionSize(const QSize& size)
{
    if (impl_->settings_.compositionSize() == size) {
        return;
    }
    impl_->settings_.setCompositionSize(size);
    for (auto& variant : impl_->responsiveLayout_.variants) {
        if (variant.variantId == impl_->responsiveLayout_.activeVariantId) {
            variant.baseSize = size;
            variant.aspectRatio = size.height() > 0
                ? static_cast<qreal>(size.width()) / static_cast<qreal>(size.height())
                : 0.0;
            break;
        }
    }
    impl_->invalidateThumbnailCache();
    Q_EMIT changed();
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
    const FrameRange normalized = range.normalized();
    if (impl_->frameRange_ == normalized) {
        return;
    }
    impl_->frameRange_ = normalized;
    impl_->workAreaRange_.clip(impl_->frameRange_);
    if (!impl_->workAreaRange_.isValid() || impl_->workAreaRange_.isEmpty()) {
        impl_->workAreaRange_ = impl_->frameRange_;
    }
    Q_EMIT changed();
}

FrameRange ArtifactAbstractComposition::workAreaRange() const
{
    return impl_->workAreaRange_;
}

void ArtifactAbstractComposition::setWorkAreaRange(const FrameRange& range)
{
    FrameRange normalized = range.normalized();
    normalized.clip(impl_->frameRange_);
    if (!normalized.isValid() || normalized.isEmpty()) {
        normalized = impl_->frameRange_;
    }
    if (impl_->workAreaRange_ == normalized) {
        return;
    }
    impl_->workAreaRange_ = normalized;
    Q_EMIT changed();
}

void ArtifactAbstractComposition::setFrameRate(const FrameRate& rate)
{
    if (impl_->frameRate_ == rate) {
        return;
    }
    impl_->frameRate_ = rate;
    Q_EMIT changed();
}

ResponsiveLayoutSet ArtifactAbstractComposition::responsiveLayout() const
{
    return impl_->responsiveLayout_;
}

void ArtifactAbstractComposition::setResponsiveLayout(const ResponsiveLayoutSet& layout)
{
    ResponsiveLayoutSet normalized = layout;
    if (normalized.variants.isEmpty()) {
        normalized = makeDefaultResponsiveLayoutSet(impl_->settings_.compositionSize());
    }
    if (normalized.activeVariantId.isEmpty() || !normalized.hasVariant(normalized.activeVariantId)) {
        normalized.activeVariantId = normalized.variants.front().variantId;
    }
    impl_->responsiveLayout_ = normalized;

    for (const auto& variant : impl_->responsiveLayout_.variants) {
        if (variant.variantId == impl_->responsiveLayout_.activeVariantId && variant.baseSize.isValid()) {
            impl_->settings_.setCompositionSize(variant.baseSize);
            break;
        }
    }
    impl_->invalidateThumbnailCache();
    Q_EMIT changed();
}

QString ArtifactAbstractComposition::activeResponsiveLayoutVariantId() const
{
    return impl_->responsiveLayout_.activeVariantId;
}

void ArtifactAbstractComposition::setActiveResponsiveLayoutVariantId(const QString& variantId)
{
    if (variantId.isEmpty() || impl_->responsiveLayout_.activeVariantId == variantId) {
        return;
    }
    if (!impl_->responsiveLayout_.hasVariant(variantId)) {
        return;
    }
    impl_->responsiveLayout_.activeVariantId = variantId;
    for (const auto& variant : impl_->responsiveLayout_.variants) {
        if (variant.variantId == variantId && variant.baseSize.isValid()) {
            impl_->settings_.setCompositionSize(variant.baseSize);
            break;
        }
    }
    impl_->invalidateThumbnailCache();
    Q_EMIT changed();
}

QVector<ResponsiveLayoutVariant> ArtifactAbstractComposition::responsiveLayoutVariants() const
{
    return impl_->responsiveLayout_.variants;
}

QSize ArtifactAbstractComposition::effectiveCompositionSize() const
{
    for (const auto& variant : impl_->responsiveLayout_.variants) {
        if (variant.variantId == impl_->responsiveLayout_.activeVariantId && variant.baseSize.isValid()) {
            return variant.baseSize;
        }
    }
    return impl_->settings_.compositionSize();
}

QJsonDocument ArtifactAbstractComposition::toJson() const{
    QJsonObject obj;
    obj["id"] = id().toString();
    obj["frameRange"] = impl_->frameRange_.toJson();
    obj["workAreaRange"] = impl_->workAreaRange_.toJson();
    obj["currentFrame"] = impl_->position_.framePosition();
    obj["playbackSpeed"] = impl_->playbackSpeed_;
    obj["looping"] = impl_->looping_;
    obj["isPlaying"] = impl_->isPlaying_;
    obj["name"] = impl_->settings_.compositionName().toQString();
    obj["compositionNote"] = impl_->compositionNote_;
    obj["width"] = impl_->settings_.compositionSize().width();
    obj["height"] = impl_->settings_.compositionSize().height();
    QJsonObject backgroundColorObj;
    backgroundColorObj["r"] = impl_->backgroundColor_.r();
    backgroundColorObj["g"] = impl_->backgroundColor_.g();
    backgroundColorObj["b"] = impl_->backgroundColor_.b();
    backgroundColorObj["a"] = impl_->backgroundColor_.a();
    obj["backgroundColor"] = backgroundColorObj;
    obj["responsiveLayout"] = impl_->responsiveLayout_.toJson();
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
    removeLayer(id);
}

ArtifactCompositionPtr ArtifactAbstractComposition::fromJson(const QJsonDocument& doc){
    if (!doc.isObject()) return nullptr;
    QJsonObject obj = doc.object();
    
    CompositionID compId;
    if (obj.contains("id")) {
        compId = CompositionID(obj["id"].toString());
    }
    
    ArtifactCompositionInitParams params;
    if (obj.contains("name")) {
        params.setCompositionName(obj["name"].toString());
    }
    if (obj.contains("width") && obj.contains("height")) {
        params.setResolution(obj["width"].toInt(), obj["height"].toInt());
    }
    if (obj.contains("backgroundColor") && obj["backgroundColor"].isObject()) {
        const QJsonObject backgroundColorObj = obj["backgroundColor"].toObject();
        params.setBackgroundColor(FloatColor{
            static_cast<float>(backgroundColorObj["r"].toDouble()),
            static_cast<float>(backgroundColorObj["g"].toDouble()),
            static_cast<float>(backgroundColorObj["b"].toDouble()),
            static_cast<float>(backgroundColorObj["a"].toDouble(1.0))
        });
    }
    auto comp = ArtifactCore::makeShared<ArtifactAbstractComposition>(compId, params);
    if (obj.contains("frameRange") && obj["frameRange"].isObject()) {
        comp->setFrameRange(FrameRange::fromJson(obj["frameRange"].toObject()));
    }
    if (obj.contains("workAreaRange") && obj["workAreaRange"].isObject()) {
        comp->setWorkAreaRange(FrameRange::fromJson(obj["workAreaRange"].toObject()));
    }
    if (obj.contains("compositionNote")) {
        comp->setCompositionNote(obj["compositionNote"].toString());
    }
    if (obj.contains("playbackSpeed")) {
        comp->setPlaybackSpeed(static_cast<float>(obj["playbackSpeed"].toDouble(1.0)));
    }
    if (obj.contains("looping")) {
        comp->setLooping(obj["looping"].toBool(false));
    }
    if (obj.contains("responsiveLayout") && obj["responsiveLayout"].isObject()) {
        comp->impl_->responsiveLayout_ = ResponsiveLayoutSet::fromJson(obj["responsiveLayout"].toObject());
        for (const auto& variant : comp->impl_->responsiveLayout_.variants) {
            if (variant.variantId == comp->impl_->responsiveLayout_.activeVariantId && variant.baseSize.isValid()) {
                comp->impl_->settings_.setCompositionSize(variant.baseSize);
                break;
            }
        }
    } else {
        comp->impl_->responsiveLayout_ = makeDefaultResponsiveLayoutSet(comp->impl_->settings_.compositionSize());
    }

    if (obj.contains("layers") && obj["layers"].isArray()) {
        QJsonArray arr = obj["layers"].toArray();
        QVector<ArtifactAbstractLayerPtr> loadedLayers;
        for (const auto& v : arr) {
            if (v.isObject()) {
                auto layer = createArtifactLayerFromJson(v.toObject());
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
    const int64_t restoredFrame = obj.contains("currentFrame")
        ? obj["currentFrame"].toVariant().toLongLong()
        : comp->frameRange().start();
    comp->goToFrame(comp->frameRange().clampFrame(restoredFrame));
    if (obj.contains("isPlaying")) {
        if (obj["isPlaying"].toBool(false)) {
            comp->play();
        } else {
            comp->pause();
        }
    }
    return comp;
}

QVector<ArtifactCore::AssetID> ArtifactAbstractComposition::getUsedAssets() const
{
  return impl_->getUsedAssets();
}

void ArtifactAbstractComposition::applyResolutionRemap(const QSize& newSize, RemapPolicy policy)
{
    const QSize oldSize = impl_->settings_.compositionSize();
    if (oldSize == newSize) return;

    setCompositionSize(newSize);

    const auto& layers = allLayerRef();
    for (const auto& layer : layers) {
        if (!layer) continue;

        // Remap mask vertex positions
        if (layer->hasMasks()) {
            for (int mi = 0; mi < layer->maskCount(); ++mi) {
                auto lm = layer->mask(mi);
                for (int pi = 0; pi < lm.maskPathCount(); ++pi) {
                    auto path = lm.maskPath(pi);
                    const int vc = path.vertexCount();
                    for (int vi = 0; vi < vc; ++vi) {
                        auto v = path.vertex(vi);
                        v.position = ResolutionRemap::remapPosition(
                            v.position, oldSize, newSize, policy);
                        v.inTangent = ResolutionRemap::remapPosition(
                            v.inTangent, oldSize, newSize, policy);
                        v.outTangent = ResolutionRemap::remapPosition(
                            v.outTangent, oldSize, newSize, policy);
                        path.setVertex(vi, v);
                    }
                    // Remap animation keyframe snapshots
                    if (path.hasAnimationKeyframes()) {
                        auto snaps = path.animationKeyframes();
                        for (auto& snap : snaps) {
                            for (auto& sv : snap.vertices) {
                                sv.position = ResolutionRemap::remapPosition(
                                    sv.position, oldSize, newSize, policy);
                                sv.inTangent = ResolutionRemap::remapPosition(
                                    sv.inTangent, oldSize, newSize, policy);
                                sv.outTangent = ResolutionRemap::remapPosition(
                                    sv.outTangent, oldSize, newSize, policy);
                            }
                            path.clearAnimationKeyframes();
                            path.setAnimationKeyframe(snap.frame, snap);
                        }
                    }
                    lm.setMaskPath(pi, path);
                }
                layer->setMask(mi, lm);
            }
        }
    }
}

QImage ArtifactAbstractComposition::getThumbnail(int width, int height) const
{
    const int safeWidth = std::max(1, width);
    const int safeHeight = std::max(1, height);
    const QSize targetSize(safeWidth, safeHeight);

    if (impl_->thumbnailCacheValid_ &&
        impl_->thumbnailCacheSize_ == targetSize &&
        !impl_->thumbnailCache_.isNull()) {
        return impl_->thumbnailCache_;
    }

    const auto layers = impl_->allLayerBackToFront();
    for (const auto& layer : layers) {
        if (!layer) {
            continue;
        }
        const QImage thumbnail = layer->getThumbnail(safeWidth, safeHeight);
        if (!thumbnail.isNull()) {
            impl_->thumbnailCache_ = thumbnail;
            impl_->thumbnailCacheSize_ = targetSize;
            impl_->thumbnailCacheValid_ = true;
            return thumbnail;
        }
    }

    impl_->thumbnailCache_ = QImage(safeWidth, safeHeight,
                                    QImage::Format_ARGB32_Premultiplied);
    impl_->thumbnailCache_.fill(QColor(24, 24, 24, 255));
    impl_->thumbnailCacheSize_ = targetSize;
    impl_->thumbnailCacheValid_ = true;
    return impl_->thumbnailCache_;
}

};
