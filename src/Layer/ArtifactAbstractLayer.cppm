module;
#include <utility>
#include <QDebug>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <wobjectcpp.h>
#include <wobjectimpl.h>

// JSON and QVariant used in serialization
#include <QColor>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>

module Artifact.Layer.Abstract;

import std;

import Utils;
import Layer.State;
import Animation.Transform2D;
import Frame.Position;
import Time.Rational;
import Frame.Rate;
import Animation.Value;
import Transform.Hlper;

import Time.TimeRemap;

import Artifact.Layer.Settings;
import Artifact.Layer.Physics;
import Artifact.Layer.Matte;
import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Artifact.Mask.LayerMask;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Property.Group;
import Artifact.Event.Types;
import Event.Bus;

namespace Artifact {

using namespace ArtifactCore;

W_OBJECT_IMPL(ArtifactAbstractLayer)

namespace {
template <typename T> bool assignIfChanged(T &current, const T &next) {
  if (current == next) {
    return false;
  }
  current = next;
  return true;
}

void notifyLayerMutation(ArtifactAbstractLayer *layer, LayerDirtyFlag flag,
                         LayerDirtyReason reason) {
  if (!layer) {
    return;
  }
  layer->setDirty(flag);
  layer->addDirtyReason(reason);
  const auto *comp =
      static_cast<ArtifactAbstractComposition *>(layer->composition());
  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      comp ? comp->id().toString() : QString{},
      layer->id().toString(),
      LayerChangedEvent::ChangeType::Modified});
  Q_EMIT layer->changed();
}

double effectiveLayerFrameRate(const ArtifactAbstractLayer *layer) {
  if (!layer) {
    return 30.0;
  }
  auto *composition =
      static_cast<ArtifactAbstractComposition *>(layer->composition());
  if (!composition) {
    return 30.0;
  }
  const double fps = composition->frameRate().framerate();
  return fps > 0.0 ? fps : 30.0;
}

int64_t currentTimelineFrame(const ArtifactAbstractLayer *layer) {
  if (!layer) {
    return 0;
  }
  auto *composition =
      static_cast<ArtifactAbstractComposition *>(layer->composition());
  if (!composition) {
    return layer->currentFrame();
  }
  return composition->framePosition().framePosition();
}

RationalTime currentTimelineTime(const ArtifactAbstractLayer *layer) {
  return RationalTime(currentTimelineFrame(layer), effectiveLayerFrameRate(layer));
}
} // namespace

class ArtifactAbstractLayer::Impl {
public:
   bool is3D_ = false;
  bool isVisible_ = true;
  Id id;
  QString name_;
  QString layerNote_;
  ArtifactAbstractComposition *composition_ = nullptr;
  LayerID parentLayerId_;
  LAYER_BLEND_TYPE blendMode_ = LAYER_BLEND_TYPE::BLEND_NORMAL;
  LayerState state_;
  FramePosition inPoint_ = FramePosition(0);
  FramePosition outPoint_ = FramePosition(300); // Default 10s at 30fps
  FramePosition startTime_ = FramePosition(0);
  int64_t currentFrame_ = 0; // 現在のフレーム位置
  mutable QImage thumbnailCache_;
  mutable QSize thumbnailCacheSize_;
  int64_t currentFrame() const { return currentFrame_; }

  bool isLocked_ = false;
  bool isGuide_ = false;
  bool isSolo_ = false;
  bool isShy_ = false;
  bool isAdjustmentLayer_ = false;
  int labelColorIndex_ = 0;
  float opacity_ = 1.0f; // Opacity (0.0 - 1.0)

  // Physics components
  LayerPhysicsSettings physics_;

  // Matte components (Asset-based track mattes)
  std::vector<LayerMatteReference> mattes_;

  // Runtime Physics State
  mutable ArtifactCore::SpringState springX_;
  mutable ArtifactCore::SpringState springY_;
  mutable ArtifactCore::SpringState springRot_;
  mutable ArtifactCore::SpringState springSX_;
  mutable ArtifactCore::SpringState springSY_;
  mutable int64_t lastPhysicsFrame_ = -1;

  uint32_t dirtyFlags_ = (uint32_t)LayerDirtyFlag::All;
  uint64_t dirtyReasonMask_ =
      static_cast<uint64_t>(LayerDirtyReason::PropertyChanged);

  // エフェクトコンテナ
  std::vector<std::shared_ptr<ArtifactAbstractEffect>> effects_;

  // マスクコンテナ
  std::vector<LayerMask> masks_;
  mutable QHash<QString, std::shared_ptr<AbstractProperty>> propertyCache_;

  // Time remap
  std::unique_ptr<ArtifactCore::TimeRemapEffect> timeRemapEffect_;

  // Variants
  std::vector<std::unique_ptr<LayerVariant>> variants_;
  size_t activeVariantIndex_ = 0;

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
  void removeEffect(const UniString &effectID);
  void clearEffects();
  std::vector<std::shared_ptr<ArtifactAbstractEffect>> getEffects() const;
  std::shared_ptr<ArtifactAbstractEffect>
  getEffect(const UniString &effectID) const;
  int effectCount() const;

  // マスク管理
  void addMask(const LayerMask &mask);
  void removeMask(int index);
  void setMask(int index, const LayerMask &mask);
  LayerMask getMask(int index) const;
  int maskCount() const;
  void clearMasks();
};

ArtifactAbstractLayer::Impl::Impl() {
  // Avoid undefined draw bounds when a layer is queried before explicit size
  // assignment.
  sourceSize_ = Size_2D(1920, 1080);
}

ArtifactAbstractLayer::Impl::~Impl() {}

void ArtifactAbstractLayer::Impl::goToStartFrame() {}

void ArtifactAbstractLayer::Impl::goToEndFrame() {}

void ArtifactAbstractLayer::Impl::goToNextFrame() {}

void ArtifactAbstractLayer::Impl::goToPrevFrame() {}

bool ArtifactAbstractLayer::Impl::is3D() const { return is3D_; }

ArtifactAbstractLayer::ArtifactAbstractLayer() : impl_(new Impl()) {
  impl_->id = Id(); // Generate new ID
  impl_->variants_.push_back(std::make_unique<LayerVariant>(this, "A"));
  impl_->activeVariantIndex_ = 0;
}

ArtifactAbstractLayer::~ArtifactAbstractLayer() { delete impl_; }

void ArtifactAbstractLayer::setVisible(bool visible /*=true*/) {
  if (!assignIfChanged(impl_->isVisible_, visible)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::VisibilityChanged);
}

void ArtifactAbstractLayer::Show() { setVisible(true); }

void ArtifactAbstractLayer::Hide() { setVisible(false); }

LAYER_BLEND_TYPE ArtifactAbstractLayer::layerBlendType() const {
  const auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::BlendMode) && var->blendModeOverride.has_value()) {
      return var->blendModeOverride.value();
  }
  return impl_->blendMode_;
}

void ArtifactAbstractLayer::setBlendMode(LAYER_BLEND_TYPE type) {
  if (impl_->activeVariantIndex_ != 0) {
      auto* var = getActiveVariant();
      if (var) {
          var->blendModeOverride = type;
          SetFlag(var->overrideFlags_, VariantOverrideFlags::BlendMode);
          setDirty(LayerDirtyFlag::Effect);
          addDirtyReason(LayerDirtyReason::PropertyChanged);
          Q_EMIT changed();
          return;
      }
  }

  if (impl_->blendMode_ == type) {
    return;
  }
  impl_->blendMode_ = type;
  setDirty(LayerDirtyFlag::Effect);
  addDirtyReason(LayerDirtyReason::PropertyChanged);
  Q_EMIT changed();
}

LayerID ArtifactAbstractLayer::id() const { return impl_->id; }

QString ArtifactAbstractLayer::layerName() const { return impl_->name_; }

UniString ArtifactAbstractLayer::className() const { return QString(""); }

void ArtifactAbstractLayer::setLayerName(const QString &name) {
  if (!assignIfChanged(impl_->name_, name)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::PropertyChanged);
}

QString ArtifactAbstractLayer::layerNote() const { return impl_->layerNote_; }

void ArtifactAbstractLayer::setLayerNote(const QString &note) {
  if (!assignIfChanged(impl_->layerNote_, note)) {
    return;
  }
  Q_EMIT layerNoteChanged(note);
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::PropertyChanged);
}

std::type_index ArtifactAbstractLayer::type_index() const {
  return impl_->type_index_;
}

void ArtifactAbstractLayer::goToStartFrame() {}

void ArtifactAbstractLayer::goToEndFrame() {}

void ArtifactAbstractLayer::goToNextFrame() {}

void ArtifactAbstractLayer::goToPrevFrame() {}

void ArtifactAbstractLayer::goToFrame(int64_t frameNumber /*= 0*/) {
  // グローバルフレーム → レイヤー相対フレーム:
  // relativeFrame = globalFrame - inPoint + startTime
  impl_->currentFrame_ = frameNumber - impl_->inPoint_.framePosition() +
                         impl_->startTime_.framePosition();
}

int64_t ArtifactAbstractLayer::currentFrame() const {
  return impl_->currentFrame_;
}

FramePosition ArtifactAbstractLayer::inPoint() const { return impl_->inPoint_; }
void ArtifactAbstractLayer::setInPoint(const FramePosition &pos) {
  if (!assignIfChanged(impl_->inPoint_, pos)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::TimelineChanged);
}
FramePosition ArtifactAbstractLayer::outPoint() const {
  return impl_->outPoint_;
}
void ArtifactAbstractLayer::setOutPoint(const FramePosition &pos) {
  if (!assignIfChanged(impl_->outPoint_, pos)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::TimelineChanged);
}
FramePosition ArtifactAbstractLayer::startTime() const {
  return impl_->startTime_;
}
void ArtifactAbstractLayer::setStartTime(const FramePosition &pos) {
  if (!assignIfChanged(impl_->startTime_, pos)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::TimelineChanged);
}

bool ArtifactAbstractLayer::isActiveAt(const FramePosition &pos) const {
  return pos.framePosition() >= impl_->inPoint_.framePosition() &&
         pos.framePosition() < impl_->outPoint_.framePosition();
}

bool ArtifactAbstractLayer::isGuide() const { return impl_->isGuide_; }
void ArtifactAbstractLayer::setGuide(bool guide) {
  if (!assignIfChanged(impl_->isGuide_, guide)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::VisibilityChanged);
}
bool ArtifactAbstractLayer::isSolo() const { return impl_->isSolo_; }
void ArtifactAbstractLayer::setSolo(bool solo) {
  if (!assignIfChanged(impl_->isSolo_, solo)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::PlaybackChanged);
}
bool ArtifactAbstractLayer::isLocked() const { return impl_->isLocked_; }
void ArtifactAbstractLayer::setLocked(bool locked) {
  if (!assignIfChanged(impl_->isLocked_, locked)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::PropertyChanged);
}
bool ArtifactAbstractLayer::isShy() const { return impl_->isShy_; }
void ArtifactAbstractLayer::setShy(bool shy) {
  if (!assignIfChanged(impl_->isShy_, shy)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::PropertyChanged);
}

int ArtifactAbstractLayer::labelColorIndex() const {
  return impl_->labelColorIndex_;
}
void ArtifactAbstractLayer::setLabelColorIndex(int index) {
  if (impl_->labelColorIndex_ != index) {
    impl_->labelColorIndex_ = index;
    Q_EMIT changed();
  }
}

void ArtifactAbstractLayer::setDirty(LayerDirtyFlag flag) {
  impl_->dirtyFlags_ |= (uint32_t)flag;
}
void ArtifactAbstractLayer::clearDirty(LayerDirtyFlag flag) {
  impl_->dirtyFlags_ &= ~(uint32_t)flag;
}
bool ArtifactAbstractLayer::isDirty(LayerDirtyFlag flag) const {
  return (impl_->dirtyFlags_ & (uint32_t)flag) != 0;
}
void ArtifactAbstractLayer::addDirtyReason(LayerDirtyReason reason) {
  impl_->dirtyReasonMask_ |= static_cast<uint64_t>(reason);
}
bool ArtifactAbstractLayer::hasDirtyReason(LayerDirtyReason reason) const {
  return (impl_->dirtyReasonMask_ & static_cast<uint64_t>(reason)) != 0;
}
uint64_t ArtifactAbstractLayer::dirtyReasonMask() const {
  return impl_->dirtyReasonMask_;
}
void ArtifactAbstractLayer::clearDirtyReasons() {
  impl_->dirtyReasonMask_ = static_cast<uint64_t>(LayerDirtyReason::None);
}

// ============================================================================
// Variants Management
// ============================================================================

size_t ArtifactAbstractLayer::getActiveVariantIndex() const {
    return impl_->activeVariantIndex_;
}

void ArtifactAbstractLayer::setActiveVariant(size_t index) {
    if (index < impl_->variants_.size() && impl_->activeVariantIndex_ != index) {
        impl_->activeVariantIndex_ = index;
        notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::PropertyChanged);
    }
}

LayerVariant* ArtifactAbstractLayer::getActiveVariant() const {
    if (impl_->activeVariantIndex_ < impl_->variants_.size()) {
        return impl_->variants_[impl_->activeVariantIndex_].get();
    }
    return nullptr;
}

LayerVariant* ArtifactAbstractLayer::createVariantFromCurrent(const std::string& newName) {
    auto newVariant = std::make_unique<LayerVariant>(this, newName);
    
    if (impl_->activeVariantIndex_ < impl_->variants_.size()) {
        auto* current = impl_->variants_[impl_->activeVariantIndex_].get();
        newVariant->overrideFlags_ = current->overrideFlags_;
        newVariant->transform2DOverride = current->transform2DOverride;
        newVariant->transform3DOverride = current->transform3DOverride;
        newVariant->opacityOverride = current->opacityOverride;
        newVariant->blendModeOverride = current->blendModeOverride;
    }
    
    impl_->variants_.push_back(std::move(newVariant));
    notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::PropertyChanged);
    return impl_->variants_.back().get();
}

void ArtifactAbstractLayer::resetVariantOverride(VariantOverrideFlags specificFlag) {
    if (impl_->activeVariantIndex_ == 0) return; // Base (A) はリセット不可

    auto* var = getActiveVariant();
    if (!var) return;

    if (specificFlag == VariantOverrideFlags::None) {
        var->overrideFlags_ = VariantOverrideFlags::None;
        var->transform2DOverride.reset();
        var->transform3DOverride.reset();
        var->opacityOverride.reset();
        var->blendModeOverride.reset();
    } else {
        ClearFlag(var->overrideFlags_, specificFlag);
        
        if (HasFlag(specificFlag, VariantOverrideFlags::Transform)) {
            var->transform2DOverride.reset();
            var->transform3DOverride.reset();
        }
        if (HasFlag(specificFlag, VariantOverrideFlags::Opacity)) {
            var->opacityOverride.reset();
        }
        if (HasFlag(specificFlag, VariantOverrideFlags::BlendMode)) {
            var->blendModeOverride.reset();
        }
    }
    
    notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::PropertyChanged);
}

std::vector<LayerVariant*> ArtifactAbstractLayer::getVariants() const {
    std::vector<LayerVariant*> result;
    result.reserve(impl_->variants_.size());
    for(auto& v : impl_->variants_) {
        result.push_back(v.get());
    }
    return result;
}

std::unique_ptr<LayerVariant> ArtifactAbstractLayer::extractVariant(size_t index) {
    if (index < impl_->variants_.size()) {
       auto var = std::move(impl_->variants_[index]);
       impl_->variants_.erase(impl_->variants_.begin() + index);
       if (impl_->activeVariantIndex_ >= impl_->variants_.size()) {
           impl_->activeVariantIndex_ = impl_->variants_.empty() ? 0 : impl_->variants_.size() - 1;
       } else if (impl_->activeVariantIndex_ >= index && impl_->activeVariantIndex_ > 0) {
           impl_->activeVariantIndex_--;
       }
       notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::PropertyChanged);
       return var;
    }
    return nullptr;
}

void ArtifactAbstractLayer::insertVariant(size_t index, std::unique_ptr<LayerVariant> variant) {
    if (!variant) return;
    if (index > impl_->variants_.size()) index = impl_->variants_.size();
    impl_->variants_.insert(impl_->variants_.begin() + index, std::move(variant));
    if (impl_->activeVariantIndex_ >= index) {
        impl_->activeVariantIndex_++;
    }
    notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::setComposition(void *comp) {
  impl_->composition_ = static_cast<ArtifactAbstractComposition *>(comp);
}

void *ArtifactAbstractLayer::composition() const { return impl_->composition_; }

ArtifactAbstractLayerPtr ArtifactAbstractLayer::parentLayer() const {
  if (!impl_->composition_ || impl_->parentLayerId_.isNil())
    return nullptr;
  return impl_->composition_->layerById(impl_->parentLayerId_);
}

QTransform ArtifactAbstractLayer::getLocalTransform() const {
  const auto &t = transform3D();
  const RationalTime time = currentTimelineTime(this);
  const auto* var = getActiveVariant();
  bool hasTransVar = var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform3DOverride.has_value();

  auto evaluateDouble = [this, &time, hasTransVar](const QString &propertyPath,
                                      double fallback) {
    if (hasTransVar) return fallback;
    const auto it = impl_->propertyCache_.constFind(propertyPath);
    if (it == impl_->propertyCache_.constEnd() || !it.value()) {
      return fallback;
    }
    const auto &property = *it.value();
    if (!property.isAnimatable() || property.getKeyFrames().empty()) {
      return fallback;
    }
    const QVariant animatedValue = property.interpolateValue(time);
    return animatedValue.isValid() ? animatedValue.toDouble() : fallback;
  };

  double positionX =
      evaluateDouble(QStringLiteral("transform.position.x"), t.positionX());
  double positionY =
      evaluateDouble(QStringLiteral("transform.position.y"), t.positionY());
  double rotation =
      evaluateDouble(QStringLiteral("transform.rotation"), t.rotation());

  // 物理演算オフセットの適用
  if (impl_->physics_.enabled) {
    const double fps = effectiveLayerFrameRate(this);
    const float dt = 1.0f / static_cast<float>(fps);
    const int64_t curFrame = currentTimelineFrame(this);

    // スクラブや逆再生、初回起動時のリセット
    if (impl_->lastPhysicsFrame_ == -1 ||
        curFrame <= impl_->lastPhysicsFrame_ ||
        (curFrame - impl_->lastPhysicsFrame_) > 10) {
      impl_->springX_.currentValue = 0.0f;
      impl_->springX_.velocity = 0.0f;
      impl_->springY_.currentValue = 0.0f;
      impl_->springY_.velocity = 0.0f;
      impl_->springRot_.currentValue = 0.0f;
      impl_->springRot_.velocity = 0.0f;
      impl_->springX_.initialized = true;
      impl_->springY_.initialized = true;
      impl_->springRot_.initialized = true;
    } else {
      // 前のフレームのベース位置を評価
      const RationalTime prevTime(impl_->lastPhysicsFrame_, fps);
      auto evalAt = [this, &prevTime, &t](const QString &path,
                                          double fallback) {
        const auto it = impl_->propertyCache_.constFind(path);
        if (it == impl_->propertyCache_.constEnd() || !it.value())
          return fallback;
        const QVariant v = it.value()->interpolateValue(prevTime);
        return v.isValid() ? v.toDouble() : fallback;
      };

      const double prevBaseX =
          evalAt(QStringLiteral("transform.position.x"), t.positionX());
      const double prevBaseY =
          evalAt(QStringLiteral("transform.position.y"), t.positionY());
      const double prevBaseRot =
          evalAt(QStringLiteral("transform.rotation"), t.rotation());

      // 1フレーム分の移動量（速度）を外力としてバネに注入する
      const float velocityX = static_cast<float>(positionX - prevBaseX) / dt;
      const float velocityY = static_cast<float>(positionY - prevBaseY) / dt;
      const float velocityRot = static_cast<float>(rotation - prevBaseRot) / dt;

      const int64_t steps = curFrame - impl_->lastPhysicsFrame_;
      const float currentSec = static_cast<float>(time.toDouble());

      for (int64_t i = 0; i < steps; ++i) {
        const float stepTime = currentSec + (i * dt);

        auto updateChannel = [&](ArtifactCore::SpringState &state,
                                 float baseVelocity, float channelIndex) {
          state.stiffness = impl_->physics_.stiffness;
          state.damping = impl_->physics_.damping;

          // 1. Follow Through: キーフレームの速度による反動
          state.velocity -= baseVelocity * impl_->physics_.followThroughGain;

          // 2. Wiggle: 自律的な揺れ（複数の正弦波で不規則さをシミュレート）
          float wiggleForce = 0.0f;
          if (impl_->physics_.wiggleFreq > 0.01f &&
              impl_->physics_.wiggleAmp > 0.01f) {
            // 決定論的なノイズ（位相をチャンネルごとにずらす）
            float phase = channelIndex * 1.57f;
            float noise =
                std::sin(stepTime * impl_->physics_.wiggleFreq * 6.28f +
                         phase) +
                std::sin(stepTime * impl_->physics_.wiggleFreq * 1.33f * 6.28f +
                         phase * 0.5f) *
                    0.5f;
            wiggleForce =
                noise * impl_->physics_.wiggleAmp * state.stiffness * 0.1f;
          }

          // ターゲット(0)への復元力 + Wiggleの外力
          float force = -state.stiffness * state.currentValue -
                        state.damping * state.velocity + wiggleForce;
          state.velocity += force * dt;
          state.currentValue += state.velocity * dt;
        };

        updateChannel(impl_->springX_, velocityX, 0.0f);
        updateChannel(impl_->springY_, velocityY, 1.0f);
        updateChannel(impl_->springRot_, velocityRot, 2.0f);
      }
    }
    impl_->lastPhysicsFrame_ = curFrame;

    positionX += impl_->springX_.currentValue;
    positionY += impl_->springY_.currentValue;
    rotation += impl_->springRot_.currentValue;
  }

  const double scaleX =
      evaluateDouble(QStringLiteral("transform.scale.x"), t.scaleX());
  const double scaleY =
      evaluateDouble(QStringLiteral("transform.scale.y"), t.scaleY());
  const double anchorX =
      evaluateDouble(QStringLiteral("transform.anchor.x"), t.anchorX());
  const double anchorY =
      evaluateDouble(QStringLiteral("transform.anchor.y"), t.anchorY());

  return makeLayerTransform2D(positionX, positionY, rotation, scaleX, scaleY,
                              anchorX, anchorY);
}

QTransform ArtifactAbstractLayer::getGlobalTransform() const {
  QTransform local = getLocalTransform();
  auto parent = parentLayer();
  if (parent) {
    return combineLayerTransform2D(local, parent->getGlobalTransform());
  }
  return local;
}

QTransform ArtifactAbstractLayer::getLocalTransformAt(int64_t frameNumber) const {
  const auto &t = transform3D();
  const RationalTime time(frameNumber, effectiveLayerFrameRate(this));
  const auto* var = getActiveVariant();
  bool hasTransVar = var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform3DOverride.has_value();

  auto evaluateDouble = [this, &time, hasTransVar](const QString &propertyPath, double fallback) {
    if (hasTransVar) return fallback;
    const auto it = impl_->propertyCache_.constFind(propertyPath);
    if (it == impl_->propertyCache_.constEnd() || !it.value()) {
      return fallback;
    }
    const auto &property = *it.value();
    if (!property.isAnimatable() || property.getKeyFrames().empty()) {
      return fallback;
    }
    const QVariant animatedValue = property.interpolateValue(time);
    return animatedValue.isValid() ? animatedValue.toDouble() : fallback;
  };

  const double positionX = evaluateDouble(QStringLiteral("transform.position.x"), t.positionXAt(time));
  const double positionY = evaluateDouble(QStringLiteral("transform.position.y"), t.positionYAt(time));
  const double rotation = evaluateDouble(QStringLiteral("transform.rotation"), t.rotationAt(time));
  const double scaleX = evaluateDouble(QStringLiteral("transform.scale.x"), t.scaleXAt(time));
  const double scaleY = evaluateDouble(QStringLiteral("transform.scale.y"), t.scaleYAt(time));
  const double anchorX = evaluateDouble(QStringLiteral("transform.anchor.x"), t.anchorXAt(time));
  const double anchorY = evaluateDouble(QStringLiteral("transform.anchor.y"), t.anchorYAt(time));

  // Skip physics for random access evaluating (e.g. motion path rendering) to maintain determinism.

  return makeLayerTransform2D(positionX, positionY, rotation, scaleX, scaleY,
                              anchorX, anchorY);
}

QTransform ArtifactAbstractLayer::getGlobalTransformAt(int64_t frameNumber) const {
  QTransform local = getLocalTransformAt(frameNumber);
  auto parent = parentLayer();
  if (parent) {
    return combineLayerTransform2D(local, parent->getGlobalTransformAt(frameNumber)); // Time remapping on parent not considered here yet
  }
  return local;
}

QMatrix4x4 ArtifactAbstractLayer::getLocalTransform4x4() const {
  const auto &t = transform3D();
  const RationalTime time = currentTimelineTime(this);
  auto evaluateDouble = [this, &time](const QString &propertyPath,
                                      double fallback) {
    const auto it = impl_->propertyCache_.constFind(propertyPath);
    if (it == impl_->propertyCache_.constEnd() || !it.value()) {
      return fallback;
    }
    const auto &property = *it.value();
    if (!property.isAnimatable() || property.getKeyFrames().empty()) {
      return fallback;
    }
    const QVariant animatedValue = property.interpolateValue(time);
    return animatedValue.isValid() ? animatedValue.toDouble() : fallback;
  };
  const double positionX =
      evaluateDouble(QStringLiteral("transform.position.x"), t.positionX());
  const double positionY =
      evaluateDouble(QStringLiteral("transform.position.y"), t.positionY());
  const double positionZ = t.positionZ();
  const double rotation =
      evaluateDouble(QStringLiteral("transform.rotation"), t.rotation());
  const double scaleX =
      evaluateDouble(QStringLiteral("transform.scale.x"), t.scaleX());
  const double scaleY =
      evaluateDouble(QStringLiteral("transform.scale.y"), t.scaleY());
  const double anchorX =
      evaluateDouble(QStringLiteral("transform.anchor.x"), t.anchorX());
  const double anchorY =
      evaluateDouble(QStringLiteral("transform.anchor.y"), t.anchorY());
  const double anchorZ = t.anchorZ();
  return makeLayerTransform3D(positionX, positionY, positionZ, rotation,
                              scaleX, scaleY, 1.0, anchorX, anchorY, anchorZ);
}

QMatrix4x4 ArtifactAbstractLayer::getGlobalTransform4x4() const {
  QMatrix4x4 local = getLocalTransform4x4();
  auto parent = parentLayer();
  if (parent) {
    return combineLayerTransform3D(local, parent->getGlobalTransform4x4());
  }
  return local;
}

bool ArtifactAbstractLayer::isAdjustmentLayer() const {
  return impl_->isAdjustmentLayer_;
}
void ArtifactAbstractLayer::setAdjustmentLayer(bool isAdjustment) {
  if (impl_->isAdjustmentLayer_ != isAdjustment) {
    impl_->isAdjustmentLayer_ = isAdjustment;
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
  }
}

bool ArtifactAbstractLayer::isVisible() const { return impl_->isVisible_; }

void ArtifactAbstractLayer::setParentById(const LayerID &id) {
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

LayerID ArtifactAbstractLayer::parentLayerId() const {
  return impl_->parentLayerId_;
}

void ArtifactAbstractLayer::clearParent() {
  if (impl_->parentLayerId_.isNil()) {
    return;
  }
  impl_->parentLayerId_ = LayerID();
  setDirty(LayerDirtyFlag::Transform);
  addDirtyReason(LayerDirtyReason::TransformChanged);
  Q_EMIT changed();
}

bool ArtifactAbstractLayer::hasParent() const {
  return !impl_->parentLayerId_.isNil();
}

bool ArtifactAbstractLayer::isGroupLayer() const {
  return false;
}

bool ArtifactAbstractLayer::is3D() const { return impl_->is3D_; }

void ArtifactAbstractLayer::setIs3D(bool value) {
    impl_->is3D_ = value;
}

void ArtifactAbstractLayer::setTimeRemapEnabled(bool enabled) {
    if (!impl_->timeRemapEffect_) {
        impl_->timeRemapEffect_ = std::make_unique<ArtifactCore::TimeRemapEffect>();
    }
    impl_->timeRemapEffect_->setEnabled(enabled);
    impl_->timeRemapEffect_->setHasAudio(hasAudio());
    setDirty(LayerDirtyFlag::All);
}

void ArtifactAbstractLayer::setTimeRemapKey(int64_t compFrame,
                                            double sourceFrame) {
    if (!impl_->timeRemapEffect_) {
        impl_->timeRemapEffect_ = std::make_unique<ArtifactCore::TimeRemapEffect>();
    }

    // FrameRateを取得
    double fps = 30.0;
    if (impl_->composition_) {
        // TODO: compositionからFrameRateを取得
        fps = 30.0;
    }

    const double outputTime = static_cast<double>(compFrame) / fps;
    const double sourceTime = sourceFrame / fps;

    ArtifactCore::TimeRemapKeyframe kf;
    kf.outputTime = outputTime;
    kf.sourceTime = sourceTime;
    kf.interpolation = ArtifactCore::TimeRemapKeyframe::Interpolation::Linear;

    impl_->timeRemapEffect_->remap().addKeyframe(kf);
    impl_->timeRemapEffect_->remap().setFrameRate(ArtifactCore::FrameRate(fps));
    setDirty(LayerDirtyFlag::All);
}

bool ArtifactAbstractLayer::isTimeRemapEnabled() const {
    return impl_->timeRemapEffect_ && impl_->timeRemapEffect_->isEnabled();
}

double ArtifactAbstractLayer::getSourceFrameAtCompFrame(int64_t compFrame) const {
    if (!isTimeRemapEnabled()) {
        return static_cast<double>(compFrame);
    }

    double fps = 30.0;
    if (impl_->composition_) {
        fps = 30.0; // TODO: compositionから取得
    }

    const double outputTime = static_cast<double>(compFrame) / fps;
    float blendFwd = 0.0f, blendBwd = 0.0f;
    return impl_->timeRemapEffect_->processFrame(outputTime, blendFwd, blendBwd);
}

bool ArtifactAbstractLayer::isNullLayer() const { return false; }

bool ArtifactAbstractLayer::isCloneLayer() const { return false; }

bool ArtifactAbstractLayer::hasAudio() const { return false; }

bool ArtifactAbstractLayer::hasVideo() const { return true; }

Size_2D ArtifactAbstractLayer::sourceSize() const { return impl_->sourceSize_; }

void ArtifactAbstractLayer::setSourceSize(const Size_2D &size) {
  impl_->sourceSize_ = size;
}

Size_2D ArtifactAbstractLayer::aabb() const {
  const auto bounds = transformedBoundingBox();
  if (bounds.width() <= 0 || bounds.height() <= 0) {
    return Size_2D();
  }
  Size_2D result;
  result.width = static_cast<int>(std::ceil(bounds.width()));
  result.height = static_cast<int>(std::ceil(bounds.height()));
  return result;
}

QRectF ArtifactAbstractLayer::localBounds() const {
  const auto size = sourceSize();
  if (size.width <= 0 || size.height <= 0) {
    return QRectF();
  }
  return QRectF(0.0, 0.0, static_cast<qreal>(size.width),
                static_cast<qreal>(size.height));
}

bool ArtifactAbstractLayer::getAudio(AudioSegment &outSegment,
                                     const FramePosition &start, int frameCount,
                                     int sampleRate) {
  // Default implementation: no audio
  Q_UNUSED(outSegment);
  Q_UNUSED(start);
  Q_UNUSED(frameCount);
  Q_UNUSED(sampleRate);
  return false;
}

QRectF ArtifactAbstractLayer::transformedBoundingBox() const {
  const QRectF localRect = localBounds();
  if (!localRect.isValid() || localRect.width() <= 0.0 ||
      localRect.height() <= 0.0) {
    return QRectF();
  }

  QTransform global = getGlobalTransform();
  return global.mapRect(localRect);
}

AnimatableTransform2D &ArtifactAbstractLayer::transform2D() {
  auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform2DOverride.has_value()) {
    return var->transform2DOverride.value();
  }
  return impl_->transform2d_;
}

const AnimatableTransform2D &ArtifactAbstractLayer::transform2D() const {
  const auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform2DOverride.has_value()) {
    return var->transform2DOverride.value();
  }
  return impl_->transform2d_;
}

AnimatableTransform3D &ArtifactAbstractLayer::transform3D() {
  auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform3DOverride.has_value()) {
    return var->transform3DOverride.value();
  }
  return impl_->transform_;
}

const AnimatableTransform3D &ArtifactAbstractLayer::transform3D() const {
  const auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform3DOverride.has_value()) {
    return var->transform3DOverride.value();
  }
  return impl_->transform_;
}

QVector3D ArtifactAbstractLayer::position3D() const {
  const auto time = currentTimelineTime(this);
  return QVector3D(impl_->transform_.positionXAt(time),
                   impl_->transform_.positionYAt(time),
                   impl_->transform_.positionZAt(time));
}

void ArtifactAbstractLayer::setPosition3D(const QVector3D &pos) {
  const auto time = currentTimelineTime(this);
  impl_->transform_.setPosition(time, pos.x(), pos.y());
  impl_->transform_.setPositionZ(time, pos.z());
  changed();
}

QVector3D ArtifactAbstractLayer::rotation3D() const {
  const auto time = currentTimelineTime(this);
  return QVector3D(impl_->transform_.rotationAt(time), 0,
                   0); // Only X rotation for now
}

void ArtifactAbstractLayer::setRotation3D(const QVector3D &rot) {
  const auto time = currentTimelineTime(this);
  impl_->transform_.setRotation(time, rot.x());
  changed();
}

QJsonObject ArtifactAbstractLayer::toJson() const {

  QJsonObject obj;
  // Basic metadata
  obj["id"] = id().toString();
  obj["name"] = layerName();
  obj["layerNote"] = impl_->layerNote_;
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
  obj["labelColorIndex"] = impl_->labelColorIndex_;
  obj["opacity"] = static_cast<double>(impl_->opacity_);

  // Mattes
  QJsonArray mattesArr;
  for (const auto &matte : impl_->mattes_) {
    mattesArr.append(QJsonValue(matte.toJson()));
  }
  obj["mattes"] = mattesArr;

  // Transform
  QJsonObject trans;
  const auto &t3 = transform3D();
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
  for (const auto &eff : getEffects()) {
    if (!eff)
      continue;
    QJsonObject eobj;
    eobj["id"] = eff->effectID().toQString();
    eobj["displayName"] = eff->displayName().toQString();

    QJsonArray propsArr;
    auto props = eff->getProperties();
    for (const auto &p : props) {
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
  obj["isAdjustment"] = impl_->isAdjustmentLayer_;

  QJsonArray variantsArr;
  for (const auto& varPtr : impl_->variants_) {
      if (!varPtr) continue;
      QJsonObject varObj;
      varObj["name"] = QString::fromStdString(varPtr->name_);
      varObj["flags"] = static_cast<int>(varPtr->overrideFlags_);
      
      if (HasFlag(varPtr->overrideFlags_, VariantOverrideFlags::Opacity) && varPtr->opacityOverride.has_value()) {
          varObj["opacity"] = static_cast<double>(varPtr->opacityOverride.value());
      }
      if (HasFlag(varPtr->overrideFlags_, VariantOverrideFlags::BlendMode) && varPtr->blendModeOverride.has_value()) {
          varObj["blendMode"] = static_cast<int>(varPtr->blendModeOverride.value());
      }
      if (HasFlag(varPtr->overrideFlags_, VariantOverrideFlags::Transform) && varPtr->transform3DOverride.has_value()) {
          QJsonObject vtrans;
          const auto& vt3 = varPtr->transform3DOverride.value();
          vtrans["px"] = vt3.positionX();
          vtrans["py"] = vt3.positionY();
          vtrans["pz"] = vt3.positionZ();
          vtrans["rx"] = vt3.rotation();
          vtrans["sx"] = vt3.scaleX();
          vtrans["sy"] = vt3.scaleY();
          vtrans["ax"] = vt3.anchorX();
          vtrans["ay"] = vt3.anchorY();
          vtrans["az"] = vt3.anchorZ();
          varObj["transform"] = vtrans;
      }
      variantsArr.append(varObj);
  }
  obj["variants"] = variantsArr;
  obj["activeVariantIndex"] = static_cast<int>(impl_->activeVariantIndex_);

  return obj;
}

ArtifactAbstractLayerPtr
ArtifactAbstractLayer::fromJson(const QJsonObject &obj) {
  // Default: base class is abstract and cannot be instantiated here.
  // Subclasses should implement their own fromJson factory. Return nullptr
  // to indicate this layer cannot be constructed generically.
  Q_UNUSED(obj);
  return ArtifactAbstractLayerPtr();
}

void ArtifactAbstractLayer::applyPropertiesFromJson(const QJsonObject &obj) {
  // Default implementation: apply effect properties if matching effects exist
  // Subclasses should override to handle layer-specific fields
  if (!obj.contains("effects") || !obj["effects"].isArray())
    return;
  if (obj.contains("isAdjustment")) {
    setAdjustmentLayer(obj["isAdjustment"].toBool());
  }

  const auto arr = obj.value("effects").toArray();
  for (const auto &ev : arr) {
    if (!ev.isObject())
      continue;
    auto eobj = ev.toObject();
    if (!eobj.contains("id"))
      continue;
    UniString eid(eobj["id"].toString().toStdString());
    auto eff = getEffect(eid);
    if (!eff)
      continue;
    if (!eobj.contains("properties") || !eobj["properties"].isArray())
      continue;
    auto props = eobj["properties"].toArray();
    for (const auto &pv : props) {
      if (!pv.isObject())
        continue;
      auto pobj = pv.toObject();
      QString name = pobj.value("name").toString();
      int t = pobj.value("type").toInt(
          static_cast<int>(ArtifactCore::PropertyType::String));
      ArtifactCore::PropertyType ptype =
          static_cast<ArtifactCore::PropertyType>(t);
      QVariant val;
      if (pobj.contains("value")) {
        if (ptype == ArtifactCore::PropertyType::Color &&
            pobj.value("value").isObject()) {
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

void ArtifactAbstractLayer::fromJsonProperties(const QJsonObject &obj) {
  if (obj.contains("name"))
    setLayerName(obj["name"].toString());
  if (obj.contains("layerNote"))
    setLayerNote(obj["layerNote"].toString());
  if (obj.contains("inPoint"))
    setInPoint(FramePosition(obj["inPoint"].toVariant().toLongLong()));
  if (obj.contains("outPoint"))
    setOutPoint(FramePosition(obj["outPoint"].toVariant().toLongLong()));
  if (obj.contains("startTime"))
    setStartTime(FramePosition(obj["startTime"].toVariant().toLongLong()));
  if (obj.contains("isVisible"))
    setVisible(obj["isVisible"].toBool());
  if (obj.contains("isLocked"))
    setLocked(obj["isLocked"].toBool());
  if (obj.contains("isGuide"))
    setGuide(obj["isGuide"].toBool());
  if (obj.contains("isSolo"))
    setSolo(obj["isSolo"].toBool());
  if (obj.contains("isShy"))
    setShy(obj["isShy"].toBool());
  if (obj.contains("labelColorIndex"))
    setLabelColorIndex(obj["labelColorIndex"].toInt(0));
  if (obj.contains("opacity"))
    setOpacity(static_cast<float>(obj["opacity"].toDouble(1.0)));
  if (obj.contains("blendMode")) {
    const int mode = obj["blendMode"].toInt(
        static_cast<int>(LAYER_BLEND_TYPE::BLEND_NORMAL));
    setBlendMode(static_cast<LAYER_BLEND_TYPE>(mode));
  }

  // Mattes
  if (obj.contains("mattes") && obj["mattes"].isArray()) {
    auto mattesArr = obj["mattes"].toArray();
    impl_->mattes_.clear();
    for (const auto &matteVal : mattesArr) {
      if (matteVal.isObject()) {
        LayerMatteReference matte;
        matte.fromJson(matteVal.toObject());
        impl_->mattes_.push_back(matte);
      }
    }
  }

  if (obj.contains("parentId")) {
    const QString parentId = obj["parentId"].toString();
    if (parentId.isEmpty())
      clearParent();
    else
      setParentById(LayerID(parentId));
  }

  if (obj.contains("transform") && obj["transform"].isObject()) {
    QJsonObject trans = obj["transform"].toObject();
    auto &t3 = transform3D();
    // Since we are loading, we might want to set these as initial values or at
    // time 0
    RationalTime t0(0, 30000); // 0s
    if (trans.contains("px"))
      t3.setPosition(t0, trans["px"].toDouble(), trans["py"].toDouble());
    if (trans.contains("pz"))
      t3.setPositionZ(t0, trans["pz"].toDouble());
    if (trans.contains("rx"))
      t3.setRotation(t0, trans["rx"].toDouble());
    if (trans.contains("sx"))
      t3.setScale(t0, trans["sx"].toDouble(), trans["sy"].toDouble());
    if (trans.contains("ax"))
      t3.setAnchor(t0, trans["ax"].toDouble(), trans["ay"].toDouble(),
                   trans["az"].toDouble());
  }

  if (obj.contains("mattes") && obj["mattes"].isArray()) {
    auto mattesArr = obj["mattes"].toArray();
    impl_->mattes_.clear();
    for (const auto &matteVal : mattesArr) {
      if (matteVal.isObject()) {
        LayerMatteReference matte;
        matte.fromJson(matteVal.toObject());
        impl_->mattes_.push_back(matte);
      }
    }
  }

  if (obj.contains("variants") && obj["variants"].isArray()) {
      impl_->variants_.clear();
      QJsonArray arr = obj["variants"].toArray();
      for (int i = 0; i < arr.size(); ++i) {
          QJsonObject varObj = arr[i].toObject();
          auto newVariant = std::make_unique<LayerVariant>(this, varObj["name"].toString("A").toStdString());
          newVariant->overrideFlags_ = static_cast<VariantOverrideFlags>(varObj["flags"].toInt(0));
          
          if (varObj.contains("opacity")) {
              newVariant->opacityOverride = static_cast<float>(varObj["opacity"].toDouble(1.0));
          }
          if (varObj.contains("blendMode")) {
              newVariant->blendModeOverride = static_cast<LAYER_BLEND_TYPE>(varObj["blendMode"].toInt());
          }
          if (varObj.contains("transform") && varObj["transform"].isObject()) {
              QJsonObject vtrans = varObj["transform"].toObject();
              AnimatableTransform3D vt3;
              RationalTime t0(0, 30000);
              if (vtrans.contains("px")) vt3.setPosition(t0, vtrans["px"].toDouble(), vtrans["py"].toDouble(0));
              if (vtrans.contains("pz")) vt3.setPositionZ(t0, vtrans["pz"].toDouble());
              if (vtrans.contains("rx")) vt3.setRotation(t0, vtrans["rx"].toDouble());
              if (vtrans.contains("sx")) vt3.setScale(t0, vtrans["sx"].toDouble(), vtrans["sy"].toDouble(1.0));
              if (vtrans.contains("ax")) vt3.setAnchor(t0, vtrans["ax"].toDouble(), vtrans["ay"].toDouble(), vtrans["az"].toDouble());
              newVariant->transform3DOverride = vt3;
          }
          
          impl_->variants_.push_back(std::move(newVariant));
      }
  } else if (impl_->variants_.empty()) {
      impl_->variants_.push_back(std::make_unique<LayerVariant>(this, "A"));
  }
  
  if (obj.contains("activeVariantIndex")) {
      impl_->activeVariantIndex_ = obj["activeVariantIndex"].toInt(0);
      if (impl_->activeVariantIndex_ >= impl_->variants_.size()) {
          impl_->activeVariantIndex_ = 0;
      }
  }

  applyPropertiesFromJson(obj);
}

void ArtifactAbstractLayer::Impl::addEffect(
    std::shared_ptr<ArtifactAbstractEffect> effect) {
  if (!effect)
    return;
  effects_.push_back(effect);
  qDebug() << "[ArtifactAbstractLayer] Effect added:"
           << effect->displayName().toQString();
}

void ArtifactAbstractLayer::Impl::removeEffect(const UniString &effectID) {
  auto it = std::remove_if(
      effects_.begin(), effects_.end(),
      [&effectID](const std::shared_ptr<ArtifactAbstractEffect> &e) {
        return e && e->effectID() == effectID;
      });
  if (it != effects_.end()) {
    effects_.erase(it, effects_.end());
    qDebug() << "[ArtifactAbstractLayer] Effect removed:"
             << effectID.toQString();
  }
}

void ArtifactAbstractLayer::Impl::clearEffects() {
  effects_.clear();
  qDebug() << "[ArtifactAbstractLayer] All effects cleared";
}

std::vector<std::shared_ptr<ArtifactAbstractEffect>>
ArtifactAbstractLayer::Impl::getEffects() const {
  return effects_;
}

std::shared_ptr<ArtifactAbstractEffect>
ArtifactAbstractLayer::Impl::getEffect(const UniString &effectID) const {
  for (const auto &effect : effects_) {
    if (effect && effect->effectID() == effectID) {
      return effect;
    }
  }
  return nullptr;
}

int ArtifactAbstractLayer::Impl::effectCount() const {
  return static_cast<int>(effects_.size());
}

void ArtifactAbstractLayer::addEffect(
    std::shared_ptr<ArtifactAbstractEffect> effect) {
  impl_->addEffect(effect);
}

void ArtifactAbstractLayer::removeEffect(const UniString &effectID) {
  impl_->removeEffect(effectID);
}

void ArtifactAbstractLayer::clearEffects() { impl_->clearEffects(); }

std::vector<std::shared_ptr<ArtifactAbstractEffect>>
ArtifactAbstractLayer::getEffects() const {
  return impl_->getEffects();
}

std::shared_ptr<ArtifactAbstractEffect>
ArtifactAbstractLayer::getEffect(const UniString &effectID) const {
  return impl_->getEffect(effectID);
}

int ArtifactAbstractLayer::effectCount() const { return impl_->effectCount(); }

std::vector<ArtifactCore::PropertyGroup>
ArtifactAbstractLayer::getLayerPropertyGroups() const {
  using namespace ArtifactCore;
  PropertyGroup layerGroup(QStringLiteral("Layer"));

  auto makeProp = [this](const QString &name, PropertyType type,
                         const QVariant &value, int priority = 0) {
    return persistentLayerProperty(name, type, value, priority);
  };

  layerGroup.addProperty(makeProp(QStringLiteral("layer.name"),
                                  PropertyType::String, layerName(), -200));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.visible"),
                                  PropertyType::Boolean, isVisible(), -190));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.locked"),
                                  PropertyType::Boolean, isLocked(), -180));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.guide"),
                                  PropertyType::Boolean, isGuide(), -170));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.solo"),
                                  PropertyType::Boolean, isSolo(), -160));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.shy"),
                                  PropertyType::Boolean, isShy(), -150));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.labelColorIndex"),
                                  PropertyType::Integer, labelColorIndex(),
                                  -145));

  auto opacityProp =
      makeProp(QStringLiteral("layer.opacity"), PropertyType::Float,
               static_cast<double>(opacity()), -140);
  opacityProp->setHardRange(0.0, 1.0);
  opacityProp->setSoftRange(0.0, 1.0);
  opacityProp->setStep(0.01);
  opacityProp->setAnimatable(true);
  layerGroup.addProperty(opacityProp);

  // トランスフォームのプロパティグループ（優先度を高く設定）
  PropertyGroup transformGroup(QStringLiteral("Transform"));
  const auto &t3 = transform3D();

  auto posXProp = makeProp(QStringLiteral("transform.position.x"),
                           PropertyType::Float, t3.positionX(), -300);
  posXProp->setUnit(QStringLiteral("px"));
  posXProp->setAnimatable(true);
  transformGroup.addProperty(posXProp);

  auto posYProp = makeProp(QStringLiteral("transform.position.y"),
                           PropertyType::Float, t3.positionY(), -299);
  posYProp->setUnit(QStringLiteral("px"));
  posYProp->setAnimatable(true);
  transformGroup.addProperty(posYProp);

  auto scaleXProp = makeProp(QStringLiteral("transform.scale.x"),
                             PropertyType::Float, t3.scaleX(), -298);
  scaleXProp->setAnimatable(true);
   scaleXProp->setSoftRange(0.0, 2.0);  // Soft range for typical use (0-200%)
  transformGroup.addProperty(scaleXProp);

  auto scaleYProp = makeProp(QStringLiteral("transform.scale.y"),
                             PropertyType::Float, t3.scaleY(), -297);
  scaleYProp->setAnimatable(true);
   scaleYProp->setSoftRange(0.0, 2.0);  // Soft range for typical use (0-200%)
  transformGroup.addProperty(scaleYProp);

  auto rotationProp = makeProp(QStringLiteral("transform.rotation"),
                               PropertyType::Float, t3.rotation(), -296);
  rotationProp->setUnit(QStringLiteral("deg"));
  rotationProp->setAnimatable(true);
  transformGroup.addProperty(rotationProp);

  auto anchorXProp = makeProp(QStringLiteral("transform.anchor.x"),
                              PropertyType::Float, t3.anchorX(), -295);
  anchorXProp->setUnit(QStringLiteral("px"));
  anchorXProp->setAnimatable(true);
  transformGroup.addProperty(anchorXProp);

  auto anchorYProp = makeProp(QStringLiteral("transform.anchor.y"),
                              PropertyType::Float, t3.anchorY(), -294);
  anchorYProp->setUnit(QStringLiteral("px"));
  anchorYProp->setAnimatable(true);
  transformGroup.addProperty(anchorYProp);

  auto inPointProp =
      makeProp(QStringLiteral("time.inPoint"), PropertyType::Integer,
               static_cast<qint64>(inPoint().framePosition()), -90);
  inPointProp->setUnit(QStringLiteral("frames"));
  inPointProp->setTooltip(QStringLiteral("Layer in-point on timeline"));
  layerGroup.addProperty(inPointProp);

  auto outPointProp =
      makeProp(QStringLiteral("time.outPoint"), PropertyType::Integer,
               static_cast<qint64>(outPoint().framePosition()), -80);
  outPointProp->setUnit(QStringLiteral("frames"));
  outPointProp->setTooltip(QStringLiteral("Layer out-point on timeline"));
  layerGroup.addProperty(outPointProp);

  auto startTimeProp =
      makeProp(QStringLiteral("time.startTime"), PropertyType::Integer,
               static_cast<qint64>(startTime().framePosition()), -70);
  startTimeProp->setUnit(QStringLiteral("frames"));
  startTimeProp->setTooltip(
      QStringLiteral("Layer start offset in source time"));
  layerGroup.addProperty(startTimeProp);

  const auto sz = sourceSize();
  layerGroup.addProperty(makeProp(QStringLiteral("source.width"),
                                  PropertyType::Integer, sz.width, -40));
  layerGroup.addProperty(makeProp(QStringLiteral("source.height"),
                                  PropertyType::Integer, sz.height, -30));

  // 物理演算プロパティグループ
  PropertyGroup physicsGroup(QStringLiteral("Physics"));

  auto physicsEnabledProp =
      makeProp(QStringLiteral("physics.enabled"), PropertyType::Boolean,
               impl_->physics_.enabled, -100);
  physicsGroup.addProperty(physicsEnabledProp);

  auto stiffnessProp =
      makeProp(QStringLiteral("physics.stiffness"), PropertyType::Float,
               static_cast<double>(impl_->physics_.stiffness), -99);
  stiffnessProp->setHardRange(0.0, 1000.0);
  stiffnessProp->setSoftRange(0.0, 500.0);
  stiffnessProp->setStep(1.0);
  physicsGroup.addProperty(stiffnessProp);

  auto dampingProp =
      makeProp(QStringLiteral("physics.damping"), PropertyType::Float,
               static_cast<double>(impl_->physics_.damping), -98);
  dampingProp->setHardRange(0.0, 100.0);
  dampingProp->setSoftRange(0.0, 50.0);
  dampingProp->setStep(0.1);
  physicsGroup.addProperty(dampingProp);

  auto followThroughProp =
      makeProp(QStringLiteral("physics.followThroughGain"), PropertyType::Float,
               static_cast<double>(impl_->physics_.followThroughGain), -97);
  followThroughProp->setHardRange(0.0, 2.0);
  followThroughProp->setSoftRange(0.0, 1.0);
  followThroughProp->setStep(0.01);
  physicsGroup.addProperty(followThroughProp);

  auto wiggleFreqProp =
      makeProp(QStringLiteral("physics.wiggleFreq"), PropertyType::Float,
               static_cast<double>(impl_->physics_.wiggleFreq), -96);
  wiggleFreqProp->setUnit(QStringLiteral("Hz"));
  wiggleFreqProp->setSoftRange(0.0, 10.0);
  physicsGroup.addProperty(wiggleFreqProp);

  auto wiggleAmpProp =
      makeProp(QStringLiteral("physics.wiggleAmp"), PropertyType::Float,
               static_cast<double>(impl_->physics_.wiggleAmp), -95);
  wiggleAmpProp->setSoftRange(0.0, 100.0);
  physicsGroup.addProperty(wiggleAmpProp);

  auto isAdjustmentProp =
      makeProp(QStringLiteral("layer.isAdjustment"), PropertyType::Boolean,
               isAdjustmentLayer(), -50);
  isAdjustmentProp->setTooltip(
      QStringLiteral("Apply effects to all layers below"));
  layerGroup.addProperty(isAdjustmentProp);

  return {transformGroup, physicsGroup, layerGroup};
}

std::shared_ptr<ArtifactCore::AbstractProperty>
ArtifactAbstractLayer::getProperty(const QString &name) const {
  auto &cache = impl_->propertyCache_;
  auto it = cache.find(name);
  if (it != cache.end()) {
    return it.value();
  }
  return nullptr;
}

std::shared_ptr<ArtifactCore::AbstractProperty>
ArtifactAbstractLayer::persistentLayerProperty(const QString &propertyPath,
                                               PropertyType type,
                                               const QVariant &value,
                                               int priority) const {
  auto &cache = impl_->propertyCache_;
  auto it = cache.find(propertyPath);
  if (it == cache.end() || !it.value()) {
    it = cache.insert(propertyPath, std::make_shared<AbstractProperty>());
  }
  auto property = it.value();
  property->setName(propertyPath);
  property->setType(type);
  property->setValue(value);
  property->setDisplayPriority(priority);
  if (type == PropertyType::Integer) {
    property->setStep(1);
  }
  return property;
}

bool ArtifactAbstractLayer::setLayerPropertyValue(const QString &propertyPath,
                                                  const QVariant &value) {
  if (propertyPath == QStringLiteral("layer.name")) {
    setLayerName(value.toString());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.visible")) {
    setVisible(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.locked")) {
    setLocked(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.guide")) {
    setGuide(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.solo")) {
    setSolo(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.shy")) {
    setShy(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.labelColorIndex")) {
    setLabelColorIndex(value.toInt());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.isAdjustment")) {
    setAdjustmentLayer(value.toBool());
    return true;
  }

  if (propertyPath == QStringLiteral("layer.opacity")) {
    setOpacity(static_cast<float>(value.toDouble()));
    return true;
  }

  // Physics properties
  if (propertyPath == QStringLiteral("physics.enabled")) {
    impl_->physics_.enabled = value.toBool();
    if (!impl_->physics_.enabled) {
      impl_->lastPhysicsFrame_ = -1; // Reset state when disabled
    }
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("physics.stiffness")) {
    impl_->physics_.stiffness = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.damping")) {
    impl_->physics_.damping = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.followThroughGain")) {
    impl_->physics_.followThroughGain = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wiggleFreq")) {
    impl_->physics_.wiggleFreq = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wiggleAmp")) {
    impl_->physics_.wiggleAmp = static_cast<float>(value.toDouble());
    return true;
  }

  // トランスフォームのプロパティ
  if (propertyPath.startsWith(QStringLiteral("transform."))) {
      if (impl_->activeVariantIndex_ != 0) {
          auto* var = getActiveVariant();
          if (var && !var->transform3DOverride.has_value()) {
              var->transform3DOverride = impl_->transform_;
              SetFlag(var->overrideFlags_, VariantOverrideFlags::Transform);
          }
      }
  }

  auto &t3 = transform3D();
  const RationalTime currentTime = currentTimelineTime(this);

  if (propertyPath == QStringLiteral("transform.position.x")) {
    t3.setPosition(currentTime, value.toDouble(), t3.positionYAt(currentTime));
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.position.y")) {
    t3.setPosition(currentTime, t3.positionXAt(currentTime), value.toDouble());
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.scale.x")) {
    t3.setScale(currentTime, value.toDouble(), t3.scaleYAt(currentTime));
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.scale.y")) {
    t3.setScale(currentTime, t3.scaleXAt(currentTime), value.toDouble());
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.rotation")) {
    t3.setRotation(currentTime, value.toDouble());
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.anchor.x")) {
    t3.setAnchor(currentTime, value.toDouble(), t3.anchorYAt(currentTime),
                 t3.anchorZAt(currentTime));
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.anchor.y")) {
    t3.setAnchor(currentTime, t3.anchorXAt(currentTime), value.toDouble(),
                 t3.anchorZAt(currentTime));
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }

  if (propertyPath == QStringLiteral("time.inPoint")) {
    setInPoint(FramePosition(value.toLongLong()));
    return true;
  }
  if (propertyPath == QStringLiteral("time.outPoint")) {
    setOutPoint(FramePosition(value.toLongLong()));
    return true;
  }
  if (propertyPath == QStringLiteral("time.startTime")) {
    setStartTime(FramePosition(value.toLongLong()));
    return true;
  }
  if (propertyPath == QStringLiteral("source.width")) {
    const auto cur = sourceSize();
    const int width = value.toInt();
    if (cur.width == width) {
      return true;
    }
    setSourceSize(Size_2D(width, cur.height));
    notifyLayerMutation(this, LayerDirtyFlag::Source,
                        LayerDirtyReason::SourceChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("source.height")) {
    const auto cur = sourceSize();
    const int height = value.toInt();
    if (cur.height == height) {
      return true;
    }
    setSourceSize(Size_2D(cur.width, height));
    notifyLayerMutation(this, LayerDirtyFlag::Source,
                        LayerDirtyReason::SourceChanged);
    return true;
  }
  return false;
}

QImage ArtifactAbstractLayer::getThumbnail(int width, int height) const {
  // サムネイル用に黒いイメージを作成（プレースホルダー実装）
  const QSize targetSize(std::max(1, width), std::max(1, height));
  if (!impl_->thumbnailCache_.isNull() &&
      impl_->thumbnailCacheSize_ == targetSize) {
    return impl_->thumbnailCache_;
  }

  QImage thumbnail(targetSize.width(), targetSize.height(),
                   QImage::Format_ARGB32);
  thumbnail.fill(QColor(0, 0, 0, 255)); // 黒で塗りつぶし
  impl_->thumbnailCache_ = thumbnail;
  impl_->thumbnailCacheSize_ = targetSize;

  // TODO: 実際のレイヤーコンテンツをサムネイルにレンダリング
  qDebug() << "[Thumbnail] Generated placeholder thumbnail:" << width << "x"
           << height;

  return impl_->thumbnailCache_;
}

// -- Mask Impl methods --

void ArtifactAbstractLayer::Impl::addMask(const LayerMask &mask) {
  masks_.push_back(mask);
  qDebug() << "[ArtifactAbstractLayer] Mask added, count:" << masks_.size();
}

void ArtifactAbstractLayer::Impl::removeMask(int index) {
  if (index >= 0 && index < static_cast<int>(masks_.size())) {
    masks_.erase(masks_.begin() + index);
    qDebug() << "[ArtifactAbstractLayer] Mask removed at index:" << index;
  }
}

void ArtifactAbstractLayer::Impl::setMask(int index, const LayerMask &mask) {
  if (index >= 0 && index < static_cast<int>(masks_.size()))
    masks_[index] = mask;
}

LayerMask ArtifactAbstractLayer::Impl::getMask(int index) const {
  if (index >= 0 && index < static_cast<int>(masks_.size()))
    return masks_[index];
  return {};
}

int ArtifactAbstractLayer::Impl::maskCount() const {
  return static_cast<int>(masks_.size());
}

void ArtifactAbstractLayer::Impl::clearMasks() { masks_.clear(); }

// -- Mask public methods --

void ArtifactAbstractLayer::addMask(const LayerMask &mask) {
  impl_->addMask(mask);
}

void ArtifactAbstractLayer::removeMask(int index) { impl_->removeMask(index); }

void ArtifactAbstractLayer::setMask(int index, const LayerMask &mask) {
  impl_->setMask(index, mask);
}

LayerMask ArtifactAbstractLayer::mask(int index) const {
  return impl_->getMask(index);
}

int ArtifactAbstractLayer::maskCount() const { return impl_->maskCount(); }

void ArtifactAbstractLayer::clearMasks() { impl_->clearMasks(); }

bool ArtifactAbstractLayer::hasMasks() const { return impl_->maskCount() > 0; }

std::vector<LayerMatteReference> ArtifactAbstractLayer::matteReferences() const {
  return impl_->mattes_;
}

void ArtifactAbstractLayer::setMatteReferences(const std::vector<LayerMatteReference>& refs) {
  impl_->mattes_ = refs;
}

void ArtifactAbstractLayer::addMatteReference(const LayerMatteReference& ref) {
  impl_->mattes_.push_back(ref);
}

void ArtifactAbstractLayer::clearMatteReferences() {
  impl_->mattes_.clear();
}

// Opacity
float ArtifactAbstractLayer::opacity() const {
  const auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Opacity) && var->opacityOverride.has_value()) {
      return var->opacityOverride.value();
  }

  const auto it =
      impl_->propertyCache_.constFind(QStringLiteral("layer.opacity"));
  if (it == impl_->propertyCache_.constEnd() || !it.value()) {
    return impl_->opacity_;
  }
  const auto &property = *it.value();
  if (!property.isAnimatable() || property.getKeyFrames().empty()) {
    return impl_->opacity_;
  }
  const RationalTime time = currentTimelineTime(this);
  const QVariant animatedValue = property.interpolateValue(time);
  return animatedValue.isValid() ? static_cast<float>(animatedValue.toDouble())
                                 : impl_->opacity_;
}

void ArtifactAbstractLayer::setOpacity(float value) {
  const float clamped = std::clamp(value, 0.0f, 1.0f);
  
  if (impl_->activeVariantIndex_ != 0) {
      auto* var = getActiveVariant();
      if (var) {
          var->opacityOverride = clamped;
          SetFlag(var->overrideFlags_, VariantOverrideFlags::Opacity);
          notifyLayerMutation(this, LayerDirtyFlag::Property,
                              LayerDirtyReason::PropertyChanged);
          return;
      }
  }

  bool changed = false;
  if (auto it = impl_->propertyCache_.find(QStringLiteral("layer.opacity"));
      it != impl_->propertyCache_.end() && it.value()) {
    auto& prop = *it.value();
    if (prop.isAnimatable() && !prop.getKeyFrames().empty()) {
        const RationalTime time = currentTimelineTime(this);
        prop.addKeyFrame(time, clamped);
        changed = true;
    } else {
        if (impl_->opacity_ != clamped) {
            impl_->opacity_ = clamped;
            prop.setValue(clamped);
            changed = true;
        }
    }
  } else {
    if (impl_->opacity_ != clamped) {
      impl_->opacity_ = clamped;
      changed = true;
    }
  }

  if (changed) {
    notifyLayerMutation(this, LayerDirtyFlag::Property,
                        LayerDirtyReason::PropertyChanged);
  }
}

void ArtifactAbstractLayer::drawLOD(ArtifactIRenderer *renderer,
                                    DetailLevel lod) {
  Q_UNUSED(lod);
  draw(renderer);
}

} // namespace Artifact
