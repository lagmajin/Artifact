module;

#include <QPointF>
#include <QRectF>
#include <wobjectcpp.h>
#include <wobjectimpl.h>
#include <QDebug>
// JSON and QVariant used in serialization
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QColor>

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
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
module Artifact.Layer.Abstract;





import Utils;
import Layer.State;
import Animation.Transform2D;
import Frame.Position;
import Time.Rational;
import Artifact.Layer.Settings;
import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Artifact.Mask.LayerMask;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Property.Group;


namespace Artifact {

 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactAbstractLayer)

  class ArtifactAbstractLayer::Impl {
  public:

    bool is3D_ = true;
    bool isVisible_ = true;
    Id id;
    QString name_;
    ArtifactAbstractComposition* composition_ = nullptr;
    LayerID parentLayerId_;
    LAYER_BLEND_TYPE blendMode_ = LAYER_BLEND_TYPE::BLEND_NORMAL;
    LayerState state_;
    FramePosition inPoint_ = FramePosition(0);
    FramePosition outPoint_ = FramePosition(300); // Default 10s at 30fps
    FramePosition startTime_ = FramePosition(0);

    bool isLocked_ = false;
    bool isGuide_ = false;
    bool isSolo_ = false;
    bool isShy_ = false;

    uint32_t dirtyFlags_ = (uint32_t)LayerDirtyFlag::All;
    uint64_t dirtyReasonMask_ = static_cast<uint64_t>(LayerDirtyReason::PropertyChanged);

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
     impl_->id = Id(); // Generate new ID
 }

 ArtifactAbstractLayer::~ArtifactAbstractLayer()
 {
  delete impl_;
 }
 
 void ArtifactAbstractLayer::setVisible(bool visible/*=true*/)
 {
  impl_->isVisible_ = visible;
 }

 void ArtifactAbstractLayer::Show()
 {
  setVisible(true);
 }

 void ArtifactAbstractLayer::Hide()
 {
  setVisible(false);

 }

LAYER_BLEND_TYPE ArtifactAbstractLayer::layerBlendType() const
{
  return impl_->blendMode_;
}

void ArtifactAbstractLayer::setBlendMode(LAYER_BLEND_TYPE type)
{
  if (impl_->blendMode_ == type) {
   return;
  }
  impl_->blendMode_ = type;
  setDirty(LayerDirtyFlag::Effect);
  addDirtyReason(LayerDirtyReason::PropertyChanged);
  Q_EMIT changed();
}

 LayerID ArtifactAbstractLayer::id() const
 {
  return impl_->id;
 }

 QString ArtifactAbstractLayer::layerName() const
 {
  return impl_->name_;
 }

 UniString ArtifactAbstractLayer::className() const
{
  return QString("");
 }

 void ArtifactAbstractLayer::setLayerName(const QString& name)
 {
     impl_->name_ = name;
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

 FramePosition ArtifactAbstractLayer::inPoint() const { return impl_->inPoint_; }
 void ArtifactAbstractLayer::setInPoint(const FramePosition& pos) { impl_->inPoint_ = pos; }
 FramePosition ArtifactAbstractLayer::outPoint() const { return impl_->outPoint_; }
 void ArtifactAbstractLayer::setOutPoint(const FramePosition& pos) { impl_->outPoint_ = pos; }
 FramePosition ArtifactAbstractLayer::startTime() const { return impl_->startTime_; }
 void ArtifactAbstractLayer::setStartTime(const FramePosition& pos) { impl_->startTime_ = pos; }

  bool ArtifactAbstractLayer::isActiveAt(const FramePosition& pos) const
  {
      return pos.framePosition() >= impl_->inPoint_.framePosition() && 
             pos.framePosition() < impl_->outPoint_.framePosition();
  }

  bool ArtifactAbstractLayer::isGuide() const { return impl_->isGuide_; }
  void ArtifactAbstractLayer::setGuide(bool guide) { impl_->isGuide_ = guide; }
  bool ArtifactAbstractLayer::isSolo() const { return impl_->isSolo_; }
  void ArtifactAbstractLayer::setSolo(bool solo) { impl_->isSolo_ = solo; }
  bool ArtifactAbstractLayer::isLocked() const { return impl_->isLocked_; }
  void ArtifactAbstractLayer::setLocked(bool locked) { impl_->isLocked_ = locked; }
  bool ArtifactAbstractLayer::isShy() const { return impl_->isShy_; }
  void ArtifactAbstractLayer::setShy(bool shy) { impl_->isShy_ = shy; }

  void ArtifactAbstractLayer::setDirty(LayerDirtyFlag flag) { impl_->dirtyFlags_ |= (uint32_t)flag; }
  void ArtifactAbstractLayer::clearDirty(LayerDirtyFlag flag) { impl_->dirtyFlags_ &= ~(uint32_t)flag; }
  bool ArtifactAbstractLayer::isDirty(LayerDirtyFlag flag) const { return (impl_->dirtyFlags_ & (uint32_t)flag) != 0; }
  void ArtifactAbstractLayer::addDirtyReason(LayerDirtyReason reason) { impl_->dirtyReasonMask_ |= static_cast<uint64_t>(reason); }
  bool ArtifactAbstractLayer::hasDirtyReason(LayerDirtyReason reason) const { return (impl_->dirtyReasonMask_ & static_cast<uint64_t>(reason)) != 0; }
  uint64_t ArtifactAbstractLayer::dirtyReasonMask() const { return impl_->dirtyReasonMask_; }
  void ArtifactAbstractLayer::clearDirtyReasons() { impl_->dirtyReasonMask_ = static_cast<uint64_t>(LayerDirtyReason::None); }

 void ArtifactAbstractLayer::setComposition(ArtifactAbstractComposition* comp)
 {
  impl_->composition_ = comp;
 }

 ArtifactAbstractComposition* ArtifactAbstractLayer::composition() const
 {
  return impl_->composition_;
 }

 ArtifactAbstractLayerPtr ArtifactAbstractLayer::parentLayer() const
 {
  if (!impl_->composition_ || impl_->parentLayerId_.isNil()) return nullptr;
  return impl_->composition_->layerById(impl_->parentLayerId_);
 }

 QTransform ArtifactAbstractLayer::getLocalTransform() const
 {
  const auto& t = transform3D();
  QTransform result;
  
  // AE-like transform: Translate(Pos) * Rotate(Rot) * Scale(Scale) * Translate(-Anchor)
  result.translate(t.positionX(), t.positionY());
  result.rotate(t.rotation());
  result.scale(t.scaleX(), t.scaleY());
  result.translate(-t.anchorX(), -t.anchorY());
  
  return result;
 }

 QTransform ArtifactAbstractLayer::getGlobalTransform() const
 {
  QTransform local = getLocalTransform();
  auto parent = parentLayer();
  if (parent) {
   // In After Effects, parent transform is applied to the child's space
   return local * parent->getGlobalTransform();
  }
  return local;
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
  return impl_->isVisible_;
 }

void ArtifactAbstractLayer::setParentById(const LayerID& id)
{
  if (id.isNil()) {
   clearParent();
   return;
  }

  if (id == this->id()) {
   qWarning() << "[Layer] Reject self-parent:" << id.toString();
   return;
  }

  if (impl_->composition_) {
   auto parent = impl_->composition_->layerById(id);
   if (!parent) {
    qWarning() << "[Layer] Reject invalid parent id:" << id.toString();
    return;
   }

   LayerID cursor = id;
   int guard = 0;
   while (!cursor.isNil() && guard++ < 1024) {
    if (cursor == this->id()) {
     qWarning() << "[Layer] Reject cyclic parent:" << id.toString();
     return;
    }
    auto node = impl_->composition_->layerById(cursor);
    if (!node) {
     break;
    }
    cursor = node->parentLayerId();
   }
  }

  if (impl_->parentLayerId_ == id) {
   return;
  }

  impl_->parentLayerId_ = id;
  setDirty(LayerDirtyFlag::Transform);
  addDirtyReason(LayerDirtyReason::TransformChanged);
  qDebug() << "[Layer] Parent set to:" << id.toString();
  Q_EMIT changed();
}

 LayerID ArtifactAbstractLayer::parentLayerId() const
 {
  return impl_->parentLayerId_;
 }

void ArtifactAbstractLayer::clearParent()
{
  if (impl_->parentLayerId_.isNil()) {
   return;
  }
  impl_->parentLayerId_ = LayerID();
  setDirty(LayerDirtyFlag::Transform);
  addDirtyReason(LayerDirtyReason::TransformChanged);
  Q_EMIT changed();
}

 bool ArtifactAbstractLayer::hasParent() const
 {
  return !impl_->parentLayerId_.isNil();
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
 if (size.width <= 0 || size.height <= 0) {
  return QRectF();
 }

 QTransform global = getGlobalTransform();
 QRectF localRect(0, 0, static_cast<float>(size.width), static_cast<float>(size.height));
 return global.mapRect(localRect);
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

    QJsonObject obj;
    // Basic metadata
    obj["id"] = id().toString();
    obj["name"] = layerName();
    obj["type"] = static_cast<int>(LayerType::Unknown);
    obj["parentId"] = parentLayerId().toString();
    obj["inPoint"] = (qint64)impl_->inPoint_.framePosition();
    obj["outPoint"] = (qint64)impl_->outPoint_.framePosition();
    obj["startTime"] = (qint64)impl_->startTime_.framePosition();
    obj["isVisible"] = isVisible();
    obj["is3D"] = is3D();
    obj["blendMode"] = static_cast<int>(layerBlendType());
    obj["isLocked"] = impl_->isLocked_;
    obj["isGuide"] = impl_->isGuide_;
    obj["isSolo"] = impl_->isSolo_;
    obj["isShy"] = impl_->isShy_;

    // Transform
    QJsonObject trans;
    const auto& t3 = transform3D();
    trans["px"] = t3.positionX();
    trans["py"] = t3.positionY();
    trans["pz"] = t3.positionZ();
    trans["rx"] = t3.rotation(); // Currently only 1 rotation in ixx outline
    trans["sx"] = t3.scaleX();
    trans["sy"] = t3.scaleY();
    trans["ax"] = t3.anchorX();
    trans["ay"] = t3.anchorY();
    trans["az"] = t3.anchorZ();
    obj["transform"] = trans;

    // Effects and their properties
    QJsonArray effectsArr;
    for (const auto& eff : getEffects()) {
        if (!eff) continue;
        QJsonObject eobj;
        eobj["id"] = eff->effectID().toQString();
        eobj["displayName"] = eff->displayName().toQString();

        QJsonArray propsArr;
        auto props = eff->getProperties();
        for (const auto& p : props) {
            QJsonObject pobj;
            pobj["name"] = p.getName();
            pobj["type"] = static_cast<int>(p.getType());
            // Serialize value depending on type
            switch (p.getType()) {
                case ArtifactCore::PropertyType::Float:
                case ArtifactCore::PropertyType::Integer:
                case ArtifactCore::PropertyType::Boolean:
                case ArtifactCore::PropertyType::String:
                    pobj["value"] = QJsonValue::fromVariant(p.getValue());
                    break;
                case ArtifactCore::PropertyType::Color: {
                    QColor c = p.getColorValue();
                    QJsonObject col;
                    col["r"] = c.redF();
                    col["g"] = c.greenF();
                    col["b"] = c.blueF();
                    col["a"] = c.alphaF();
                    pobj["value"] = col;
                    break;
                }
                default:
                    pobj["value"] = QJsonValue();
                    break;
            }
            propsArr.append(pobj);
        }
        eobj["properties"] = propsArr;
        effectsArr.append(eobj);
    }
    obj["effects"] = effectsArr;

    return obj;
}

 ArtifactAbstractLayerPtr ArtifactAbstractLayer::fromJson(const QJsonObject& obj)
 {
  // Default: base class is abstract and cannot be instantiated here.
  // Subclasses should implement their own fromJson factory. Return nullptr
  // to indicate this layer cannot be constructed generically.
  Q_UNUSED(obj);
  return ArtifactAbstractLayerPtr();
 }

void ArtifactAbstractLayer::applyPropertiesFromJson(const QJsonObject& obj)
{
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
            int t = pobj.value("type").toInt(static_cast<int>(ArtifactCore::PropertyType::String));
            ArtifactCore::PropertyType ptype = static_cast<ArtifactCore::PropertyType>(t);
            QVariant val;
            if (pobj.contains("value")) {
                if (ptype == ArtifactCore::PropertyType::Color && pobj.value("value").isObject()) {
                    auto col = pobj.value("value").toObject();
                    double r = col.value("r").toDouble(0.0);
                    double g = col.value("g").toDouble(0.0);
                    double b = col.value("b").toDouble(0.0);
                    double a = col.value("a").toDouble(1.0);
                    QColor qc;
                    qc.setRedF(static_cast<float>(r));
                    qc.setGreenF(static_cast<float>(g));
                    qc.setBlueF(static_cast<float>(b));
                    qc.setAlphaF(static_cast<float>(a));
                    val = QVariant(qc);
                } else {
                    val = pobj.value("value").toVariant();
                }
            }
            eff->setPropertyValue(UniString(name.toStdString()), val);
        }
    }
}

void ArtifactAbstractLayer::fromJsonProperties(const QJsonObject& obj)
{
    if (obj.contains("name")) setLayerName(obj["name"].toString());
    if (obj.contains("inPoint")) setInPoint(FramePosition(obj["inPoint"].toVariant().toLongLong()));
    if (obj.contains("outPoint")) setOutPoint(FramePosition(obj["outPoint"].toVariant().toLongLong()));
    if (obj.contains("startTime")) setStartTime(FramePosition(obj["startTime"].toVariant().toLongLong()));
    if (obj.contains("isVisible")) setVisible(obj["isVisible"].toBool());
    if (obj.contains("isLocked")) setLocked(obj["isLocked"].toBool());
    if (obj.contains("isGuide")) setGuide(obj["isGuide"].toBool());
    if (obj.contains("isSolo")) setSolo(obj["isSolo"].toBool());
    if (obj.contains("isShy")) setShy(obj["isShy"].toBool());
    if (obj.contains("blendMode")) {
        const int mode = obj["blendMode"].toInt(static_cast<int>(LAYER_BLEND_TYPE::BLEND_NORMAL));
        setBlendMode(static_cast<LAYER_BLEND_TYPE>(mode));
    }
    if (obj.contains("parentId")) {
        const QString parentId = obj["parentId"].toString();
        if (parentId.isEmpty()) clearParent();
        else setParentById(LayerID(parentId));
    }
    
    if (obj.contains("transform") && obj["transform"].isObject()) {
        QJsonObject trans = obj["transform"].toObject();
        auto& t3 = transform3D();
        // Since we are loading, we might want to set these as initial values or at time 0
        RationalTime t0(0, 30000); // 0s
        if (trans.contains("px")) t3.setPosition(t0, trans["px"].toDouble(), trans["py"].toDouble());
        if (trans.contains("pz")) t3.setPositionZ(t0, trans["pz"].toDouble());
        if (trans.contains("rx")) t3.setRotation(t0, trans["rx"].toDouble());
        if (trans.contains("sx")) t3.setScale(t0, trans["sx"].toDouble(), trans["sy"].toDouble());
        if (trans.contains("ax")) t3.setAnchor(t0, trans["ax"].toDouble(), trans["ay"].toDouble(), trans["az"].toDouble());
    }

    applyPropertiesFromJson(obj);
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

 std::vector<ArtifactCore::PropertyGroup> ArtifactAbstractLayer::getLayerPropertyGroups() const
 {
  using namespace ArtifactCore;
  PropertyGroup layerGroup(QStringLiteral("Layer"));

  auto makeProp = [](const QString& name, PropertyType type, const QVariant& value, int priority = 0) {
   auto p = std::make_shared<AbstractProperty>();
   p->setName(name);
   p->setType(type);
   p->setValue(value);
   p->setDisplayPriority(priority);
   if (type == PropertyType::Integer) {
    p->setStep(1);
   }
   return p;
  };

  layerGroup.addProperty(makeProp(QStringLiteral("layer.name"), PropertyType::String, layerName(), -200));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.visible"), PropertyType::Boolean, isVisible(), -190));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.locked"), PropertyType::Boolean, isLocked(), -180));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.guide"), PropertyType::Boolean, isGuide(), -170));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.solo"), PropertyType::Boolean, isSolo(), -160));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.shy"), PropertyType::Boolean, isShy(), -150));

  auto inPointProp = makeProp(QStringLiteral("time.inPoint"), PropertyType::Integer, static_cast<qint64>(inPoint().framePosition()), -90);
  inPointProp->setUnit(QStringLiteral("frames"));
  inPointProp->setTooltip(QStringLiteral("Layer in-point on timeline"));
  layerGroup.addProperty(inPointProp);

  auto outPointProp = makeProp(QStringLiteral("time.outPoint"), PropertyType::Integer, static_cast<qint64>(outPoint().framePosition()), -80);
  outPointProp->setUnit(QStringLiteral("frames"));
  outPointProp->setTooltip(QStringLiteral("Layer out-point on timeline"));
  layerGroup.addProperty(outPointProp);

  auto startTimeProp = makeProp(QStringLiteral("time.startTime"), PropertyType::Integer, static_cast<qint64>(startTime().framePosition()), -70);
  startTimeProp->setUnit(QStringLiteral("frames"));
  startTimeProp->setTooltip(QStringLiteral("Layer start offset in source time"));
  layerGroup.addProperty(startTimeProp);

  const auto sz = sourceSize();
  layerGroup.addProperty(makeProp(QStringLiteral("source.width"), PropertyType::Integer, sz.width, -40));
  layerGroup.addProperty(makeProp(QStringLiteral("source.height"), PropertyType::Integer, sz.height, -30));

  return {layerGroup};
 }

 bool ArtifactAbstractLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
 {
  if (propertyPath == QStringLiteral("layer.name")) {
   setLayerName(value.toString());
   addDirtyReason(LayerDirtyReason::PropertyChanged);
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("layer.visible")) {
   setVisible(value.toBool());
   addDirtyReason(LayerDirtyReason::VisibilityChanged);
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("layer.locked")) {
   setLocked(value.toBool());
   addDirtyReason(LayerDirtyReason::PropertyChanged);
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("layer.guide")) {
   setGuide(value.toBool());
   addDirtyReason(LayerDirtyReason::PropertyChanged);
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("layer.solo")) {
   setSolo(value.toBool());
   addDirtyReason(LayerDirtyReason::PlaybackChanged);
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("layer.shy")) {
   setShy(value.toBool());
   addDirtyReason(LayerDirtyReason::PropertyChanged);
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("time.inPoint")) {
   setInPoint(FramePosition(value.toLongLong()));
   addDirtyReason(LayerDirtyReason::TimelineChanged);
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("time.outPoint")) {
   setOutPoint(FramePosition(value.toLongLong()));
   addDirtyReason(LayerDirtyReason::TimelineChanged);
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("time.startTime")) {
   setStartTime(FramePosition(value.toLongLong()));
   addDirtyReason(LayerDirtyReason::TimelineChanged);
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("source.width")) {
   const auto cur = sourceSize();
   setSourceSize(Size_2D(value.toInt(), cur.height));
   addDirtyReason(LayerDirtyReason::SourceChanged);
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("source.height")) {
   const auto cur = sourceSize();
   setSourceSize(Size_2D(cur.width, value.toInt()));
   addDirtyReason(LayerDirtyReason::SourceChanged);
   Q_EMIT changed();
   return true;
  }
  return false;
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
