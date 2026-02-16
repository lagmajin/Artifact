module;

#include <QPointF>
#include <QRectF>
#include <wobjectcpp.h>
#include <wobjectimpl.h>
#include <QDebug>

module Artifact.Layer.Abstract;

import std;

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
import Utils;
import Layer.State;
import Animation.Transform2D;
import Frame.Position;
import Artifact.Layer.Settings;
import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Artifact.Mask.LayerMask;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;


namespace Artifact {

 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactAbstractLayer)

  class ArtifactAbstractLayer::Impl {
  private:

    bool is3D_ = true;
    Id id;
    LayerState state_;
    //FramePosition framePosition_

    // エフェクトコンテナ
    std::vector<std::shared_ptr<ArtifactAbstractEffect>> effects_;

    // マスクコンテナ
    std::vector<LayerMask> masks_;

   public:
   Impl();
   ~Impl();
   std::type_index type_index_ = typeid(void);
   void goToStartFrame();
   void goToEndFrame();
   void goToNextFrame();
   void goToPrevFrame();

   bool is3D() const;
   AnimatableTransform3D transform_;
   AnimatableTransform2D transform2d_;
   Size_2D sourceSize_;

   // エフェクト管理メソッド
   void addEffect(std::shared_ptr<ArtifactAbstractEffect> effect);
   void removeEffect(const UniString& effectID);
   void clearEffects();
   std::vector<std::shared_ptr<ArtifactAbstractEffect>> getEffects() const;
   std::shared_ptr<ArtifactAbstractEffect> getEffect(const UniString& effectID) const;
    int effectCount() const;

    // マスク管理
    void addMask(const LayerMask& mask);
    void removeMask(int index);
    void setMask(int index, const LayerMask& mask);
    LayerMask getMask(int index) const;
    int maskCount() const;
    void clearMasks();
   };

  ArtifactAbstractLayer::Impl::Impl()
  {
  }

  ArtifactAbstractLayer::Impl::~Impl()
  {
  }

  void ArtifactAbstractLayer::Impl::goToStartFrame()
  {

  }

  void ArtifactAbstractLayer::Impl::goToEndFrame()
  {

  }

  void ArtifactAbstractLayer::Impl::goToNextFrame()
  {

  }

  void ArtifactAbstractLayer::Impl::goToPrevFrame()
  {

  }

  bool ArtifactAbstractLayer::Impl::is3D() const
  {
   return is3D_;
  }

  ArtifactAbstractLayer::ArtifactAbstractLayer():impl_(new Impl())
 {

 }

 ArtifactAbstractLayer::~ArtifactAbstractLayer()
 {
  delete impl_;
 }
 
 void ArtifactAbstractLayer::setVisible(bool visible/*=true*/)
 {
  //impl_->state_.setVisible(visible);
 }

 void ArtifactAbstractLayer::Show()
 {

 }

 void ArtifactAbstractLayer::Hide()
 {


 }

 LAYER_BLEND_TYPE ArtifactAbstractLayer::layerBlendType() const
 {

  return LAYER_BLEND_TYPE::BLEND_ADD;
 }

 void ArtifactAbstractLayer::setBlendMode(LAYER_BLEND_TYPE type)
 {

 }

 LayerID ArtifactAbstractLayer::id() const
 {

  return LayerID();
 }

 QString ArtifactAbstractLayer::layerName() const
 {

  return QString();
 }

 UniString ArtifactAbstractLayer::className() const
{
  return QString("");
 }

 void ArtifactAbstractLayer::setLayerName(const QString& name)
 {

 }

 std::type_index ArtifactAbstractLayer::type_index() const
 {
  return impl_->type_index_;
 }

 void ArtifactAbstractLayer::goToStartFrame()
 {

 }

 void ArtifactAbstractLayer::goToEndFrame()
 {

 }

 void ArtifactAbstractLayer::goToNextFrame()
 {

 }

 void ArtifactAbstractLayer::goToPrevFrame()
 {

 }

 void ArtifactAbstractLayer::goToFrame(int64_t frameNumber /*= 0*/)
 {
    // Minimal synchronous evaluation implementation.
    // 1) Update internal state (could store current frame if needed)
    // 2) Evaluate transforms/animation for this frame (AnimatableTransform2D/3D)
    // 3) Evaluate each effect synchronously using CPU backend and update a local cache image

    // For now we keep a very small local evaluation: create a temporary ImageF32x4RGBAWithCache
    // and pass it through effects sequentially. Concrete layers (image/video) should override
    // to supply actual source image for the frame. This generic path does nothing but serve as
    // a fallback.

    Q_UNUSED(frameNumber);

    // Basic evaluation: evaluate transforms and apply effects sequentially if any.
    // This is a conservative default; concrete layers should override to provide
    // actual source content for the frame.

    // Evaluate transforms (sync)
    (void)transform2D();
    (void)transform3D();

    // If there are effects, run them using CPU impl sequentially on a temporary image.
    auto effects = getEffects();
    if (effects.empty()) return;

    // Create a small dummy image matching sourceSize as placeholder
    Size_2D sz = sourceSize();
    if (sz.width <= 0 || sz.height <= 0) return;

    ArtifactCore::ImageF32x4_RGBA baseColor;
    baseColor.resize(sz.width, sz.height);
    ArtifactCore::ImageF32x4RGBAWithCache temp(baseColor);

    ArtifactCore::ImageF32x4RGBAWithCache cur = temp;
    ArtifactCore::ImageF32x4RGBAWithCache out;
    for (const auto& eff : effects) {
        if (!eff) continue;
        // prefer CPU path for deterministic synchronous evaluation
        eff->setComputeMode(Artifact::ComputeMode::CPU);
        auto cpuBackend = eff->cpuImpl();
        if (cpuBackend) {
            cpuBackend->applyCPU(cur, out);
        } else {
            out = cur.DeepCopy();
        }
        cur = out.DeepCopy();
    }

    // TODO: store cur into layer cache if present
    return;
 }
 bool ArtifactAbstractLayer::isAdjustmentLayer() const
 {
  return true;
 }
 void ArtifactAbstractLayer::setAdjustmentLayer(bool isAdjustment)
 {
  //adjustmentLayer = isAdjustment;
 }

 bool ArtifactAbstractLayer::isVisible() const
 {
  return false;
 }

 void ArtifactAbstractLayer::setParentById(LayerID& id)
 {

 }

 bool ArtifactAbstractLayer::is3D() const
 {
  return false;
 }

 void ArtifactAbstractLayer::setTimeRemapEnabled(bool)
 {

 }

 void ArtifactAbstractLayer::setTimeRemapKey(int64_t compFrame, double sourceFrame)
 {

 }

 bool ArtifactAbstractLayer::isTimeRemapEnabled() const
 {
  return false;
 }

 bool ArtifactAbstractLayer::isNullLayer() const
 {
  return false;
 }

 bool ArtifactAbstractLayer::hasAudio() const
 {
  return true;
 }

 bool ArtifactAbstractLayer::hasVideo() const
 {
  return true;
 }

Size_2D ArtifactAbstractLayer::sourceSize() const
{
 return impl_->sourceSize_;
}

void ArtifactAbstractLayer::setSourceSize(const Size_2D& size)
{
 impl_->sourceSize_ = size;
}

Size_2D ArtifactAbstractLayer::aabb() const
{
 const auto bounds = transformedBoundingBox();
 if (bounds.width() <= 0 || bounds.height() <= 0) {
  return Size_2D();
 }
 Size_2D result;
 result.width = static_cast<int>(std::ceil(bounds.width()));
 result.height = static_cast<int>(std::ceil(bounds.height()));
 return result;
}

QRectF ArtifactAbstractLayer::transformedBoundingBox() const
{
 const auto size = sourceSize();
 if (size.isEmpty()) {
  return QRectF();
 }

 const float width = static_cast<float>(size.width);
 const float height = static_cast<float>(size.height);
 const float centerX = width * 0.5f;
 const float centerY = height * 0.5f;

 const float scaleX = transform3D().scaleX();
 const float scaleY = transform3D().scaleY();
 const float rotationDeg = transform3D().rotation();
 const float translateX = transform3D().positionX();
 const float translateY = transform3D().positionY();

 const float radians = rotationDeg * (3.14159265358979323846f / 180.0f);
 const float cosA = std::cos(radians);
 const float sinA = std::sin(radians);

 const std::array<QPointF, 4> corners = {
  QPointF(0.0f, 0.0f),
  QPointF(width, 0.0f),
  QPointF(width, height),
  QPointF(0.0f, height)
 };

 float minX = std::numeric_limits<float>::max();
 float minY = std::numeric_limits<float>::max();
 float maxX = std::numeric_limits<float>::lowest();
 float maxY = std::numeric_limits<float>::lowest();

 for (const auto& corner : corners) {
  QPointF pt = corner;
  pt -= QPointF(centerX, centerY);
  pt.setX(pt.x() * scaleX);
  pt.setY(pt.y() * scaleY);
  const float rotatedX = pt.x() * cosA - pt.y() * sinA;
  const float rotatedY = pt.x() * sinA + pt.y() * cosA;
  pt.setX(rotatedX + centerX + translateX);
  pt.setY(rotatedY + centerY + translateY);
  if (pt.x() < minX) {
   minX = pt.x();
  }
  if (pt.x() > maxX) {
   maxX = pt.x();
  }
  if (pt.y() < minY) {
   minY = pt.y();
  }
  if (pt.y() > maxY) {
   maxY = pt.y();
  }
 }

 return QRectF(minX, minY, maxX - minX, maxY - minY);
}

AnimatableTransform2D& ArtifactAbstractLayer::transform2D()
{
 return impl_->transform2d_;
}

const AnimatableTransform2D& ArtifactAbstractLayer::transform2D() const
{
 return impl_->transform2d_;
}

AnimatableTransform3D& ArtifactAbstractLayer::transform3D()
{
 return impl_->transform_;
}

const AnimatableTransform3D& ArtifactAbstractLayer::transform3D() const
{
 return impl_->transform_;
}

QJsonObject ArtifactAbstractLayer::toJson() const
{



 return QJsonObject();
}

 ArtifactAbstractLayerPtr ArtifactAbstractLayer::fromJson(const QJsonObject& obj)
 {
  // Default: create base layer and apply properties where possible
  auto layer = std::make_shared<ArtifactAbstractLayer>();
  layer->applyPropertiesFromJson(obj);
  return layer;
 }

void ArtifactAbstractLayer::applyPropertiesFromJson(const QJsonObject& obj)
{
    Q_UNUSED(obj);
    // Default implementation: apply effect properties if matching effects exist
    // Subclasses should override to handle layer-specific fields
    if (!obj.contains("effects") || !obj["effects"].isArray()) return;
    auto arr = obj["effects"].toArray();
    for (const auto& ev : arr) {
        if (!ev.isObject()) continue;
        auto eobj = ev.toObject();
        if (!eobj.contains("id")) continue;
        UniString eid(eobj["id"].toString().toStdString());
        auto eff = getEffect(eid);
        if (!eff) continue;
        if (!eobj.contains("properties") || !eobj["properties"].isArray()) continue;
        auto props = eobj["properties"].toArray();
        for (const auto& pv : props) {
            if (!pv.isObject()) continue;
            auto pobj = pv.toObject();
            QString name = pobj.value("name").toString();
            QVariant val = pobj.value("value").toVariant();
            eff->setPropertyValue(UniString(name.toStdString()), val);
        }
    }
}

 void ArtifactAbstractLayer::Impl::addEffect(std::shared_ptr<ArtifactAbstractEffect> effect)
 {
  if (!effect) return;
  effects_.push_back(effect);
  qDebug() << "[ArtifactAbstractLayer] Effect added:" << effect->displayName().toQString();
 }

 void ArtifactAbstractLayer::Impl::removeEffect(const UniString& effectID)
 {
  auto it = std::remove_if(effects_.begin(), effects_.end(),
   [&effectID](const std::shared_ptr<ArtifactAbstractEffect>& e) {
    return e && e->effectID() == effectID;
   });
  if (it != effects_.end()) {
   effects_.erase(it, effects_.end());
   qDebug() << "[ArtifactAbstractLayer] Effect removed:" << effectID.toQString();
  }
 }

 void ArtifactAbstractLayer::Impl::clearEffects()
 {
  effects_.clear();
  qDebug() << "[ArtifactAbstractLayer] All effects cleared";
 }

 std::vector<std::shared_ptr<ArtifactAbstractEffect>> ArtifactAbstractLayer::Impl::getEffects() const
 {
  return effects_;
 }

 std::shared_ptr<ArtifactAbstractEffect> ArtifactAbstractLayer::Impl::getEffect(const UniString& effectID) const
 {
  for (const auto& effect : effects_) {
   if (effect && effect->effectID() == effectID) {
    return effect;
   }
  }
  return nullptr;
 }

 int ArtifactAbstractLayer::Impl::effectCount() const
 {
  return static_cast<int>(effects_.size());
 }

 void ArtifactAbstractLayer::addEffect(std::shared_ptr<ArtifactAbstractEffect> effect)
 {
  impl_->addEffect(effect);
 }

 void ArtifactAbstractLayer::removeEffect(const UniString& effectID)
 {
  impl_->removeEffect(effectID);
 }

 void ArtifactAbstractLayer::clearEffects()
 {
  impl_->clearEffects();
 }

 std::vector<std::shared_ptr<ArtifactAbstractEffect>> ArtifactAbstractLayer::getEffects() const
 {
  return impl_->getEffects();
 }

 std::shared_ptr<ArtifactAbstractEffect> ArtifactAbstractLayer::getEffect(const UniString& effectID) const
 {
  return impl_->getEffect(effectID);
 }

 int ArtifactAbstractLayer::effectCount() const
 {
  return impl_->effectCount();
 }

 QImage ArtifactAbstractLayer::getThumbnail(int width, int height) const
 {
  // サムネイル用に黒いイメージを作成（プレースホルダー実装）
  QImage thumbnail(width, height, QImage::Format_ARGB32);
  thumbnail.fill(QColor(0, 0, 0, 255));  // 黒で塗りつぶし

  // TODO: 実際のレイヤーコンテンツをサムネイルにレンダリング
  qDebug() << "[Thumbnail] Generated placeholder thumbnail:" << width << "x" << height;

     return thumbnail;
    }

   // -- Mask Impl methods --

   void ArtifactAbstractLayer::Impl::addMask(const LayerMask& mask)
   {
    masks_.push_back(mask);
    qDebug() << "[ArtifactAbstractLayer] Mask added, count:" << masks_.size();
   }

   void ArtifactAbstractLayer::Impl::removeMask(int index)
   {
    if (index >= 0 && index < static_cast<int>(masks_.size())) {
     masks_.erase(masks_.begin() + index);
     qDebug() << "[ArtifactAbstractLayer] Mask removed at index:" << index;
    }
   }

   void ArtifactAbstractLayer::Impl::setMask(int index, const LayerMask& mask)
   {
    if (index >= 0 && index < static_cast<int>(masks_.size()))
     masks_[index] = mask;
   }

   LayerMask ArtifactAbstractLayer::Impl::getMask(int index) const
   {
    if (index >= 0 && index < static_cast<int>(masks_.size()))
     return masks_[index];
    return {};
   }

   int ArtifactAbstractLayer::Impl::maskCount() const
   {
    return static_cast<int>(masks_.size());
   }

   void ArtifactAbstractLayer::Impl::clearMasks()
   {
    masks_.clear();
   }

   // -- Mask public methods --

   void ArtifactAbstractLayer::addMask(const LayerMask& mask)
   {
    impl_->addMask(mask);
   }

   void ArtifactAbstractLayer::removeMask(int index)
   {
    impl_->removeMask(index);
   }

   void ArtifactAbstractLayer::setMask(int index, const LayerMask& mask)
   {
    impl_->setMask(index, mask);
   }

   LayerMask ArtifactAbstractLayer::mask(int index) const
   {
    return impl_->getMask(index);
   }

   int ArtifactAbstractLayer::maskCount() const
   {
    return impl_->maskCount();
   }

   void ArtifactAbstractLayer::clearMasks()
   {
    impl_->clearMasks();
   }

   bool ArtifactAbstractLayer::hasMasks() const
   {
    return impl_->maskCount() > 0;
   }

  };
