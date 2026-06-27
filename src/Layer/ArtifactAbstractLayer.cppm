module;
#include <compare>
#include <utility>
#include <QDebug>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QVector3D>
#include <QVector4D>
#include <QStringList>
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
import Artifact.Layer.Modifier;
import Artifact.Layer.Matte;
import Layer.Matte;
import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Artifact.Effect.Generator.Cloner;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
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
struct ClonerTransformOperation {
  QString name = QStringLiteral("Transform");
  bool enabled = true;
  QVector3D position{0.0f, 0.0f, 0.0f};
  QVector3D rotation{0.0f, 0.0f, 0.0f};
  QVector3D scale{1.0f, 1.0f, 1.0f};
};

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
      dynamic_cast<const ArtifactAbstractComposition *>(layer->compositionObject());
  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      comp ? comp->id().toString() : QString{},
      layer->id().toString(),
      LayerChangedEvent::ChangeType::Modified});
  Q_EMIT layer->changed();
}

QRectF mapRectWithMatrix(const QMatrix4x4 &matrix, const QRectF &rect) {
  if (!rect.isValid() || rect.width() <= 0.0 || rect.height() <= 0.0) {
    return QRectF();
  }

  const QVector4D corners[] = {
      QVector4D(static_cast<float>(rect.left()), static_cast<float>(rect.top()),
                0.0f, 1.0f),
      QVector4D(static_cast<float>(rect.right()), static_cast<float>(rect.top()),
                0.0f, 1.0f),
      QVector4D(static_cast<float>(rect.right()),
                static_cast<float>(rect.bottom()), 0.0f, 1.0f),
      QVector4D(static_cast<float>(rect.left()),
                static_cast<float>(rect.bottom()), 0.0f, 1.0f)};

  float minX = std::numeric_limits<float>::infinity();
  float minY = std::numeric_limits<float>::infinity();
  float maxX = -std::numeric_limits<float>::infinity();
  float maxY = -std::numeric_limits<float>::infinity();

  for (const auto &corner : corners) {
    const QVector4D mapped = matrix * corner;
    minX = std::min(minX, mapped.x());
    minY = std::min(minY, mapped.y());
    maxX = std::max(maxX, mapped.x());
    maxY = std::max(maxY, mapped.y());
  }

  if (!std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(maxX) ||
      !std::isfinite(maxY) || maxX <= minX || maxY <= minY) {
    return QRectF();
  }

  return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

QMatrix4x4 matrixFromTransform2D(const QTransform& transform) {
  return QMatrix4x4(
      static_cast<float>(transform.m11()), static_cast<float>(transform.m21()), 0.0f, static_cast<float>(transform.m31()),
      static_cast<float>(transform.m12()), static_cast<float>(transform.m22()), 0.0f, static_cast<float>(transform.m32()),
      0.0f,                               0.0f,                               1.0f, 0.0f,
      static_cast<float>(transform.m13()), static_cast<float>(transform.m23()), 0.0f, static_cast<float>(transform.m33()));
}

QString slugifyEffectId(const QString &text) {
  QString slug;
  slug.reserve(text.size());
  bool lastWasDash = false;
  for (const QChar ch : text.trimmed().toLower()) {
    if (ch.isLetterOrNumber()) {
      slug.append(ch);
      lastWasDash = false;
    } else if (!slug.isEmpty() && !lastWasDash) {
      slug.append(QChar('-'));
      lastWasDash = true;
    }
  }
  while (slug.endsWith(QChar('-'))) {
    slug.chop(1);
  }
  if (slug.isEmpty()) {
    slug = QStringLiteral("effect");
  }
  return slug;
}

QString uniqueEffectIdForLayer(
    const std::vector<std::shared_ptr<ArtifactAbstractEffect>> &effects,
    const QString &displayName, const QString &preferredId) {
  QString baseId = preferredId.trimmed();
  if (baseId.isEmpty()) {
    baseId = slugifyEffectId(displayName);
  }
  if (baseId.isEmpty()) {
    baseId = QStringLiteral("effect");
  }

  auto idExists = [&effects](const QString &candidate) {
    return std::any_of(
        effects.begin(), effects.end(),
        [&candidate](const std::shared_ptr<ArtifactAbstractEffect> &effect) {
          return effect && effect->effectID().toQString() == candidate;
        });
  };

  if (!idExists(baseId)) {
    return baseId;
  }

  QString uniqueId = baseId;
  int suffix = 2;
  while (idExists(uniqueId)) {
    uniqueId = QStringLiteral("%1-%2").arg(baseId).arg(suffix++);
  }
  return uniqueId;
}

QString uniqueModifierIdForLayer(
    const std::vector<std::shared_ptr<ArtifactLayerModifier>> &modifiers,
    const QString &displayName, const QString &preferredId) {
  QString baseId = preferredId.trimmed();
  if (baseId.isEmpty()) {
    baseId = slugifyEffectId(displayName);
  }
  if (baseId.isEmpty()) {
    baseId = QStringLiteral("modifier");
  }

  auto idExists = [&modifiers](const QString &candidate) {
    return std::any_of(
        modifiers.begin(), modifiers.end(),
        [&candidate](const std::shared_ptr<ArtifactLayerModifier> &modifier) {
          return modifier && modifier->modifierId() == candidate;
        });
  };

  if (!idExists(baseId)) {
    return baseId;
  }

  QString uniqueId = baseId;
  int suffix = 2;
  while (idExists(uniqueId)) {
    uniqueId = QStringLiteral("%1-%2").arg(baseId).arg(suffix++);
  }
  return uniqueId;
}

double effectiveLayerFrameRate(const ArtifactAbstractLayer *layer) {
  if (!layer) {
    return 30.0;
  }
  auto *composition =
      dynamic_cast<ArtifactAbstractComposition *>(layer->compositionObject());
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
      dynamic_cast<ArtifactAbstractComposition *>(layer->compositionObject());
  if (!composition) {
    return layer->currentFrame();
  }
  return composition->framePosition().framePosition();
}

RationalTime currentTimelineTime(const ArtifactAbstractLayer *layer) {
  return RationalTime(currentTimelineFrame(layer), effectiveLayerFrameRate(layer));
}

RationalTime timelineTimeForFramePosition(const ArtifactAbstractLayer *layer,
                                          const FramePosition &position) {
  return RationalTime(position.framePosition(), effectiveLayerFrameRate(layer));
}

struct MaskPropertyAddress {
  int maskIndex = -1;
  int pathIndex = -1;
  QString field;
};

std::optional<MaskPropertyAddress>
parseMaskPropertyPath(const QString &propertyPath) {
  const QStringList parts = propertyPath.split(QLatin1Char('.'), Qt::SkipEmptyParts);
  if (parts.size() < 3 || parts[0] != QStringLiteral("mask")) {
    return std::nullopt;
  }

  bool ok = false;
  const int maskIndex = parts[1].toInt(&ok);
  if (!ok || maskIndex < 0) {
    return std::nullopt;
  }

  if (parts.size() == 3 && parts[2] == QStringLiteral("enabled")) {
    return MaskPropertyAddress{maskIndex, -1, parts[2]};
  }

  if (parts.size() != 5 || parts[2] != QStringLiteral("path")) {
    return std::nullopt;
  }

  const int pathIndex = parts[3].toInt(&ok);
  if (!ok || pathIndex < 0) {
    return std::nullopt;
  }

  const QString field = parts[4];
  if (field == QStringLiteral("closed") ||
      field == QStringLiteral("opacity") ||
      field == QStringLiteral("feather") ||
      field == QStringLiteral("featherHorizontal") ||
      field == QStringLiteral("featherVertical") ||
      field == QStringLiteral("featherInner") ||
      field == QStringLiteral("featherOuter") ||
      field == QStringLiteral("expansion") ||
      field == QStringLiteral("inverted") ||
      field == QStringLiteral("mode") ||
      field == QStringLiteral("name")) {
    return MaskPropertyAddress{maskIndex, pathIndex, field};
  }

  return std::nullopt;
}

QString maskPropertyPrefix(const int maskIndex) {
  return QStringLiteral("mask.%1").arg(maskIndex);
}

QString maskPathPropertyPrefix(const int maskIndex, const int pathIndex) {
  return QStringLiteral("mask.%1.path.%2").arg(maskIndex).arg(pathIndex);
}

void applyMaskPropertyState(const ArtifactAbstractLayer *layer,
                            const int maskIndex, LayerMask &mask) {
  if (!layer) {
    return;
  }

  const RationalTime time = currentTimelineTime(layer);
  const auto resolveBool = [layer, time](const QString &propertyPath,
                                         const bool fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) {
      return fallback;
    }
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toBool() : fallback;
  };
  const auto resolveInt = [layer, time](const QString &propertyPath,
                                        const int fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) {
      return fallback;
    }
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toInt() : fallback;
  };
  const auto resolveDouble = [layer, time](const QString &propertyPath,
                                           const double fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) {
      return fallback;
    }
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toDouble() : fallback;
  };
  const auto resolveString = [layer, time](const QString &propertyPath,
                                           const QString &fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) {
      return fallback;
    }
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toString() : fallback;
  };

  const QString maskPrefix = maskPropertyPrefix(maskIndex);
  mask.setEnabled(resolveBool(maskPrefix + QStringLiteral(".enabled"),
                              mask.isEnabled()));

  for (int pathIndex = 0; pathIndex < mask.maskPathCount(); ++pathIndex) {
    MaskPath path = mask.maskPath(pathIndex);
    const QString pathPrefix = maskPathPropertyPrefix(maskIndex, pathIndex);
    path.setClosed(resolveBool(pathPrefix + QStringLiteral(".closed"),
                               path.isClosed()));
    path.setOpacity(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".opacity"),
                      static_cast<double>(path.opacity()))));
    path.setFeather(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".feather"),
                      static_cast<double>(path.feather()))));
    path.setFeatherHorizontal(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".featherHorizontal"),
                      static_cast<double>(path.featherHorizontal()))));
    path.setFeatherVertical(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".featherVertical"),
                      static_cast<double>(path.featherVertical()))));
    path.setFeatherInner(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".featherInner"),
                      static_cast<double>(path.featherInner()))));
    path.setFeatherOuter(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".featherOuter"),
                      static_cast<double>(path.featherOuter()))));
    path.setExpansion(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".expansion"),
                      static_cast<double>(path.expansion()))));
    path.setInverted(resolveBool(pathPrefix + QStringLiteral(".inverted"),
                                 path.isInverted()));
    path.setMode(static_cast<MaskMode>(
        resolveInt(pathPrefix + QStringLiteral(".mode"),
                   static_cast<int>(path.mode()))));
    path.setName(UniString(resolveString(pathPrefix + QStringLiteral(".name"),
                                         path.name().toQString())
                               .toStdString()));
    mask.setMaskPath(pathIndex, path);
  }
}

struct ClonerTransformPropertyAddress {
  int index = -1;
  QString field;
};

std::optional<ClonerTransformPropertyAddress>
parseClonerTransformPropertyPath(const QString &propertyPath) {
  const QString prefix = QStringLiteral("component.cloner.transforms.");
  if (!propertyPath.startsWith(prefix, Qt::CaseInsensitive)) {
    return std::nullopt;
  }
  const QString tail = propertyPath.mid(prefix.size());
  const QStringList parts = tail.split(QLatin1Char('.'), Qt::SkipEmptyParts);
  if (parts.size() != 2) {
    return std::nullopt;
  }
  bool ok = false;
  const int index = parts[0].toInt(&ok);
  if (!ok || index < 0) {
    return std::nullopt;
  }
  return ClonerTransformPropertyAddress{index, parts[1]};
}
} // namespace


class ArtifactAbstractLayer::Impl {
public:
   bool is3D_ = false;
  bool isVisible_ = true;
  Id id;
  QString name_;
  QString layerNote_;
  QPointer<QObject> composition_;
  mutable std::mutex compositionMutex_;
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
  bool isSelectionLocked_ = false;
  bool isTransformLocked_ = false;
  bool isTimingLocked_ = false;
  bool isGuide_ = false;
  bool isSolo_ = false;
  bool isShy_ = false;
  bool isAdjustmentLayer_ = false;
  LayerCachePolicy layerCachePolicy_ = LayerCachePolicy::Default;
  int labelColorIndex_ = 0;
  float opacity_ = 1.0f; // Opacity (0.0 - 1.0)

    // Physics component
    PhysicsLayerComponent physicsComponent_;
    bool scriptComponentEnabled_ = false;
    bool clonerComponentEnabled_ = false;
    bool layoutComponentEnabled_ = false;
    int layoutMode_ = 0;
    int layoutAnchorMode_ = 0;
    int layoutHorizontalPin_ = 0;
    int layoutVerticalPin_ = 0;
    int layoutScaleMode_ = 0;
    bool layoutSafeAreaEnabled_ = false;
    float layoutSafeAreaPaddingX_ = 0.0f;
    float layoutSafeAreaPaddingY_ = 0.0f;
    int layoutStackDirection_ = 0;
    float layoutGap_ = 24.0f;
    int layoutMaxPerRow_ = 0;
    int clonerMode_ = 0;
    int clonerCloneCount_ = 3;
    float clonerOffsetX_ = 160.0f;
    float clonerOffsetY_ = 48.0f;
    float clonerOffsetZ_ = 0.0f;
    float clonerJitterX_ = 0.0f;
    float clonerJitterY_ = 0.0f;
    float clonerJitterZ_ = 0.0f;
    int clonerSeed_ = 0;
    int clonerColumns_ = 3;
    int clonerRows_ = 3;
    int clonerDepth_ = 1;
    float clonerSpacingX_ = 160.0f;
    float clonerSpacingY_ = 48.0f;
    float clonerSpacingZ_ = 0.0f;
    int clonerRadialCount_ = 8;
    float clonerRadius_ = 160.0f;
    float clonerStartAngle_ = 0.0f;
    float clonerEndAngle_ = 360.0f;
    float clonerRotationStep_ = 0.0f;
    float clonerOpacityDecay_ = 0.0f;
    std::vector<ClonerTransformOperation> clonerTransforms_;
    QJsonObject scriptBinding_;

  // Matte components (Asset-based track mattes)
  std::vector<LayerMatteReference> mattes_;

  uint32_t dirtyFlags_ = (uint32_t)LayerDirtyFlag::All;
  uint64_t dirtyReasonMask_ =
      static_cast<uint64_t>(LayerDirtyReason::PropertyChanged);
  mutable quint64 geometryRevision_ = 1;
  mutable quint64 cachedGlobalTransformRevision_ = 0;
  mutable quint64 cachedGlobalTransformParentRevision_ = 0;
  mutable int64_t cachedGlobalTransformFrame_ =
      std::numeric_limits<int64_t>::min();
  mutable LayerID cachedGlobalTransformParentId_;
  mutable QTransform cachedGlobalTransform_;
  mutable quint64 cachedBoundingBoxRevision_ = 0;
  mutable quint64 cachedBoundingBoxParentRevision_ = 0;
  mutable int64_t cachedBoundingBoxFrame_ = std::numeric_limits<int64_t>::min();
  mutable LayerID cachedBoundingBoxParentId_;
  mutable QRectF cachedBoundingBox_;

  // エフェクトコンテナ
  std::vector<std::shared_ptr<ArtifactAbstractEffect>> effects_;

  // レイヤーモディファイアコンテナ
  LayerModifierStack modifiers_;

  // マスクコンテナ
  std::vector<LayerMask> masks_;
  mutable QHash<QString, std::shared_ptr<AbstractProperty>> propertyCache_;
  mutable std::mutex propertyCacheMutex_;

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

  // モディファイア管理メソッド
  void addModifier(std::shared_ptr<ArtifactLayerModifier> modifier);
  void removeModifier(const QString& modifierId);
  void clearModifiers();
  std::vector<std::shared_ptr<ArtifactLayerModifier>> getModifiers() const;
  std::shared_ptr<ArtifactLayerModifier> getModifier(const QString& modifierId) const;
  int modifierCount() const;
  bool hasModifiers() const;

  // マスク管理
  void addMask(const LayerMask &mask);
  void removeMask(int index);
  void setMask(int index, const LayerMask &mask);
  LayerMask getMask(int index) const;
  int maskCount() const;
  void clearMasks();
};

namespace {
bool g_globalLayerCacheEnabled = true;
}

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

void ArtifactAbstractLayer::drawLOD(ArtifactIRenderer *renderer, DetailLevel)
{
  draw(renderer);
}

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
  if (impl_->isTimingLocked_) {
    return;
  }
  const FramePosition oldIn = impl_->inPoint_;
  const FramePosition oldOut = impl_->outPoint_;
  if (!assignIfChanged(impl_->inPoint_, pos)) {
    return;
  }
  for (auto it = impl_->propertyCache_.begin(); it != impl_->propertyCache_.end(); ++it) {
    const auto& property = it.value();
    if (property && property->isAnimatable()) {
      property->retimeKeyFramesForLayerPointChange(
          timelineTimeForFramePosition(this, oldIn),
          timelineTimeForFramePosition(this, oldOut),
          timelineTimeForFramePosition(this, pos),
          timelineTimeForFramePosition(this, oldOut));
    }
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::TimelineChanged);
}
FramePosition ArtifactAbstractLayer::outPoint() const {
  return impl_->outPoint_;
}
void ArtifactAbstractLayer::setOutPoint(const FramePosition &pos) {
  if (impl_->isTimingLocked_) {
    return;
  }
  const FramePosition oldIn = impl_->inPoint_;
  const FramePosition oldOut = impl_->outPoint_;
  if (!assignIfChanged(impl_->outPoint_, pos)) {
    return;
  }
  for (auto it = impl_->propertyCache_.begin(); it != impl_->propertyCache_.end(); ++it) {
    const auto& property = it.value();
    if (property && property->isAnimatable()) {
      property->retimeKeyFramesForLayerPointChange(
          timelineTimeForFramePosition(this, oldIn),
          timelineTimeForFramePosition(this, oldOut),
          timelineTimeForFramePosition(this, oldIn),
          timelineTimeForFramePosition(this, pos));
    }
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::TimelineChanged);
}
FramePosition ArtifactAbstractLayer::startTime() const {
  return impl_->startTime_;
}
void ArtifactAbstractLayer::setStartTime(const FramePosition &pos) {
  if (impl_->isTimingLocked_) {
    return;
  }
  if (!assignIfChanged(impl_->startTime_, pos)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::TimelineChanged);
}

void ArtifactAbstractLayer::setTimelineWindow(FramePosition inPoint, FramePosition outPoint) {
  if (impl_->isTimingLocked_) {
    return;
  }
  if (outPoint.framePosition() <= inPoint.framePosition()) {
    outPoint = FramePosition(inPoint.framePosition() + 1);
  }
  setInPoint(inPoint);
  setOutPoint(outPoint);
}

void ArtifactAbstractLayer::slideTimingBy(const qint64 deltaFrames) {
  if (impl_->isTimingLocked_) {
    return;
  }
  if (deltaFrames == 0) {
    return;
  }
  const FramePosition nextIn(impl_->inPoint_.framePosition() + deltaFrames);
  const FramePosition nextOut(impl_->outPoint_.framePosition() + deltaFrames);
  setTimelineWindow(nextIn, nextOut);
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
LayerCachePolicy ArtifactAbstractLayer::layerCachePolicy() const {
  return impl_->layerCachePolicy_;
}

void ArtifactAbstractLayer::setLayerCachePolicy(LayerCachePolicy policy) {
  if (!assignIfChanged(impl_->layerCachePolicy_, policy)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Property,
                      LayerDirtyReason::PropertyChanged);
}

bool ArtifactAbstractLayer::usesLayerCache() const {
  return isGlobalLayerCacheEnabled() &&
         impl_->layerCachePolicy_ != LayerCachePolicy::Disabled;
}

bool ArtifactAbstractLayer::isGlobalLayerCacheEnabled() {
  return g_globalLayerCacheEnabled;
}

void ArtifactAbstractLayer::setGlobalLayerCacheEnabled(bool enabled) {
  g_globalLayerCacheEnabled = enabled;
}
bool ArtifactAbstractLayer::isSelectionLocked() const { return impl_->isSelectionLocked_; }
void ArtifactAbstractLayer::setSelectionLocked(bool locked) {
  if (!assignIfChanged(impl_->isSelectionLocked_, locked)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::PropertyChanged);
}
bool ArtifactAbstractLayer::isTransformLocked() const { return impl_->isTransformLocked_; }
void ArtifactAbstractLayer::setTransformLocked(bool locked) {
  if (!assignIfChanged(impl_->isTransformLocked_, locked)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::PropertyChanged);
}
bool ArtifactAbstractLayer::isTimingLocked() const { return impl_->isTimingLocked_; }
void ArtifactAbstractLayer::setTimingLocked(bool locked) {
  if (!assignIfChanged(impl_->isTimingLocked_, locked)) {
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
  ++impl_->geometryRevision_;
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

void ArtifactAbstractLayer::setComposition(QObject *comp) {
  std::lock_guard<std::mutex> lock(impl_->compositionMutex_);
  impl_->composition_ = comp;
}

void ArtifactAbstractLayer::setComposition(void *comp) {
  setComposition(static_cast<QObject *>(comp));
}

void *ArtifactAbstractLayer::composition() const {
  std::lock_guard<std::mutex> lock(impl_->compositionMutex_);
  return impl_->composition_.data();
}

QObject *ArtifactAbstractLayer::compositionObject() const {
  std::lock_guard<std::mutex> lock(impl_->compositionMutex_);
  return impl_->composition_.data();
}

ArtifactAbstractLayerPtr ArtifactAbstractLayer::parentLayer() const {
  auto *composition = dynamic_cast<ArtifactAbstractComposition *>(compositionObject());
  if (!composition || impl_->parentLayerId_.isNil())
  return nullptr;
  return composition->layerById(impl_->parentLayerId_);
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
  if (impl_->physicsComponent_.enabled()) {
    const double fps = effectiveLayerFrameRate(this);
    const int64_t curFrame = currentTimelineFrame(this);
    const RationalTime prevTime(curFrame - 1, fps);
    auto evalAt = [this, &prevTime, &t](const QString &path, double fallback) {
      const auto it = impl_->propertyCache_.constFind(path);
      if (it == impl_->propertyCache_.constEnd() || !it.value()) {
        return fallback;
      }
      const QVariant v = it.value()->interpolateValue(prevTime);
      return v.isValid() ? v.toDouble() : fallback;
    };

    const LayerPhysicsFrameOutput physicsOutput = impl_->physicsComponent_.apply(
        LayerPhysicsFrameInput{
            positionX,
            positionY,
            rotation,
            evalAt(QStringLiteral("transform.position.x"), t.positionX()),
            evalAt(QStringLiteral("transform.position.y"), t.positionY()),
            evalAt(QStringLiteral("transform.rotation"), t.rotation()),
            time.toDouble(),
            fps,
            curFrame});

    positionX = physicsOutput.positionX;
    positionY = physicsOutput.positionY;
    rotation = physicsOutput.rotation;
  }

  const double scaleX =
      evaluateDouble(QStringLiteral("transform.scale.x"), t.scaleX());
  const double scaleY =
      evaluateDouble(QStringLiteral("transform.scale.y"), t.scaleY());
  const double anchorX =
      evaluateDouble(QStringLiteral("transform.anchor.x"), t.anchorX());
  const double anchorY =
      evaluateDouble(QStringLiteral("transform.anchor.y"), t.anchorY());

  QTransform transform = makeLayerTransform2D(positionX, positionY, rotation, scaleX, scaleY,
                                              anchorX, anchorY);
  transform = impl_->modifiers_.apply(transform, localBounds(), time.toDouble());
  return transform;
}

QTransform ArtifactAbstractLayer::getGlobalTransform() const {
  auto parent = parentLayer();
  const LayerID parentId = impl_->parentLayerId_;
  const quint64 parentRevision = parent ? parent->impl_->geometryRevision_ : 0;
  const int64_t frame = impl_->currentFrame_;
  if (impl_->cachedGlobalTransformRevision_ == impl_->geometryRevision_ &&
      impl_->cachedGlobalTransformParentRevision_ == parentRevision &&
      impl_->cachedGlobalTransformFrame_ == frame &&
      impl_->cachedGlobalTransformParentId_ == parentId) {
    return impl_->cachedGlobalTransform_;
  }
  QTransform local = getLocalTransform();
  impl_->cachedGlobalTransform_ =
      parent ? combineLayerTransform2D(local, parent->getGlobalTransform())
             : local;
  impl_->cachedGlobalTransformRevision_ = impl_->geometryRevision_;
  impl_->cachedGlobalTransformParentRevision_ = parentRevision;
  impl_->cachedGlobalTransformFrame_ = frame;
  impl_->cachedGlobalTransformParentId_ = parentId;
  return impl_->cachedGlobalTransform_;
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

  QTransform transform = makeLayerTransform2D(positionX, positionY, rotation, scaleX, scaleY,
                                              anchorX, anchorY);
  transform = impl_->modifiers_.apply(transform, localBounds(), time.toDouble());
  return transform;
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
  double positionX =
      evaluateDouble(QStringLiteral("transform.position.x"), t.positionX());
  double positionY =
      evaluateDouble(QStringLiteral("transform.position.y"), t.positionY());
  const double positionZ = t.positionZAt(time);
  double rotation =
      evaluateDouble(QStringLiteral("transform.rotation"), t.rotation());
  const double scaleX =
      evaluateDouble(QStringLiteral("transform.scale.x"), t.scaleX());
  const double scaleY =
      evaluateDouble(QStringLiteral("transform.scale.y"), t.scaleY());
  const double anchorX =
      evaluateDouble(QStringLiteral("transform.anchor.x"), t.anchorXAt(time));
  const double anchorY =
      evaluateDouble(QStringLiteral("transform.anchor.y"), t.anchorYAt(time));
  const double anchorZ = t.anchorZAt(time);

  if (impl_->physicsComponent_.enabled()) {
    const double fps = effectiveLayerFrameRate(this);
    const int64_t curFrame = currentTimelineFrame(this);
    const RationalTime prevTime(curFrame - 1, fps);
    auto evalAt = [this, &prevTime, &t](const QString &path, double fallback) {
      const auto it = impl_->propertyCache_.constFind(path);
      if (it == impl_->propertyCache_.constEnd() || !it.value()) {
        return fallback;
      }
      const QVariant v = it.value()->interpolateValue(prevTime);
      return v.isValid() ? v.toDouble() : fallback;
    };

    const LayerPhysicsFrameOutput physicsOutput = impl_->physicsComponent_.apply(
        LayerPhysicsFrameInput{
            positionX,
            positionY,
            rotation,
            evalAt(QStringLiteral("transform.position.x"), t.positionXAt(prevTime)),
            evalAt(QStringLiteral("transform.position.y"), t.positionYAt(prevTime)),
            evalAt(QStringLiteral("transform.rotation"), t.rotationAt(prevTime)),
            time.toDouble(),
            fps,
            curFrame});

    positionX = physicsOutput.positionX;
    positionY = physicsOutput.positionY;
    rotation = physicsOutput.rotation;
  }

  const QTransform local2D = impl_->modifiers_.apply(
      makeLayerTransform2D(positionX, positionY, rotation, scaleX, scaleY,
                           anchorX, anchorY),
      localBounds(), time.toDouble());

  Q_UNUSED(anchorZ);
  QMatrix4x4 result = matrixFromTransform2D(local2D);
  if (positionZ != 0.0) {
    result.translate(0.0f, 0.0f, static_cast<float>(positionZ));
  }
  return result;
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
    auto *composition = dynamic_cast<ArtifactAbstractComposition *>(impl_->composition_.data());
    auto parent = composition->layerById(id);
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
      auto node = composition->layerById(cursor);
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
    if (!assignIfChanged(impl_->is3D_, value)) {
      return;
    }
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::setTimeRemapEnabled(bool enabled) {
    if (!impl_->timeRemapEffect_) {
        impl_->timeRemapEffect_ = std::make_unique<ArtifactCore::TimeRemapEffect>();
    }
    impl_->timeRemapEffect_->setEnabled(enabled);
    impl_->timeRemapEffect_->setHasAudio(hasAudio());
    notifyLayerMutation(this, LayerDirtyFlag::All,
                        LayerDirtyReason::TimelineChanged);
}

void ArtifactAbstractLayer::clearTimeRemap() {
    if (!impl_->timeRemapEffect_) {
        return;
    }
    impl_->timeRemapEffect_.reset();
    notifyLayerMutation(this, LayerDirtyFlag::All,
                        LayerDirtyReason::TimelineChanged);
}

void ArtifactAbstractLayer::setTimeRemapKey(int64_t compFrame,
                                            double sourceFrame) {
    setTimeRemapKey(compFrame, sourceFrame,
                    ArtifactCore::TimeRemapKeyframe::Interpolation::Linear);
}

void ArtifactAbstractLayer::setTimeRemapKey(
    int64_t compFrame,
    double sourceFrame,
    ArtifactCore::TimeRemapKeyframe::Interpolation interpolation) {
    if (!impl_->timeRemapEffect_) {
        impl_->timeRemapEffect_ = std::make_unique<ArtifactCore::TimeRemapEffect>();
    }

    double fps = 30.0;
    if (impl_->composition_) {
        auto *composition = dynamic_cast<ArtifactAbstractComposition *>(impl_->composition_.data());
        fps = composition->frameRate().framerate();
        if (fps <= 0.0) {
            fps = 30.0;
        }
    }

    const double outputTime = static_cast<double>(compFrame) / fps;
    const double sourceTime = sourceFrame / fps;

    ArtifactCore::TimeRemapKeyframe kf;
    kf.outputTime = outputTime;
    kf.sourceTime = sourceTime;
    kf.interpolation = interpolation;

    impl_->timeRemapEffect_->remap().addKeyframe(kf);
    impl_->timeRemapEffect_->remap().setFrameRate(ArtifactCore::FrameRate(fps));
    notifyLayerMutation(this, LayerDirtyFlag::All,
                        LayerDirtyReason::TimelineChanged);
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
        auto *composition = dynamic_cast<ArtifactAbstractComposition *>(impl_->composition_.data());
        fps = composition->frameRate().framerate();
        if (fps <= 0.0) {
            fps = 30.0;
        }
    }

    const double outputTime = static_cast<double>(compFrame) / fps;
    float blendFwd = 0.0f, blendBwd = 0.0f;
    return impl_->timeRemapEffect_->processFrame(outputTime, blendFwd, blendBwd);
}

bool ArtifactAbstractLayer::isNullLayer() const { return false; }

bool ArtifactAbstractLayer::isConstructionLayer() const { return false; }

bool ArtifactAbstractLayer::isCompositionBackgroundLayer() const { return false; }

bool ArtifactAbstractLayer::shouldIncludeInFinalRender() const { return true; }

bool ArtifactAbstractLayer::isCloneLayer() const { return false; }

bool ArtifactAbstractLayer::hasAudio() const { return false; }

bool ArtifactAbstractLayer::hasVideo() const { return false; }

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

QRectF LayerBounds::boundsFor(LayerBoundsKind kind) const {
  switch (kind) {
    case LayerBoundsKind::Source:
      return sourceBounds;
    case LayerBoundsKind::Visible:
      return visibleBounds;
    case LayerBoundsKind::Effect:
      return effectBounds;
    case LayerBoundsKind::Mask:
      return maskBounds;
    case LayerBoundsKind::Layout:
      return layoutBounds;
  }
  return layoutBounds;
}

LayerBounds ArtifactAbstractLayer::contentBounds() const {
  const QRectF source = localBounds();
  const QRectF visible = transformedBoundingBox();
  LayerBounds bounds;
  bounds.sourceBounds = source;
  bounds.visibleBounds = visible;
  bounds.effectBounds = visible;
  bounds.maskBounds = visible;
  bounds.layoutBounds = source.isValid() ? source : visible;
  return bounds;
}

QJsonObject GuideDefinition::toJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("guideId"), guideId);
  obj.insert(QStringLiteral("name"), name);
  obj.insert(QStringLiteral("purpose"), purpose);
  obj.insert(QStringLiteral("orientation"), static_cast<int>(orientation));
  obj.insert(QStringLiteral("position"), position);
  obj.insert(QStringLiteral("start"), start);
  obj.insert(QStringLiteral("end"), end);
  obj.insert(QStringLiteral("enabled"), enabled);
  obj.insert(QStringLiteral("priority"), static_cast<int>(priority));
  obj.insert(QStringLiteral("semanticTag"), static_cast<int>(semanticTag));
  return obj;
}

GuideDefinition GuideDefinition::fromJson(const QJsonObject &obj) {
  GuideDefinition guide;
  guide.guideId = obj.value(QStringLiteral("guideId")).toString();
  guide.name = obj.value(QStringLiteral("name")).toString();
  guide.purpose = obj.value(QStringLiteral("purpose")).toString();
  guide.orientation = static_cast<GuideOrientation>(
      obj.value(QStringLiteral("orientation")).toInt(static_cast<int>(GuideOrientation::Horizontal)));
  guide.position = obj.value(QStringLiteral("position")).toDouble(0.0);
  guide.start = obj.value(QStringLiteral("start")).toDouble(0.0);
  guide.end = obj.value(QStringLiteral("end")).toDouble(0.0);
  guide.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
  guide.priority = static_cast<GuidePriority>(
      obj.value(QStringLiteral("priority")).toInt(static_cast<int>(GuidePriority::Normal)));
  guide.semanticTag = static_cast<GuideSemanticTag>(
      obj.value(QStringLiteral("semanticTag")).toInt(static_cast<int>(GuideSemanticTag::Custom)));
  return guide;
}

QJsonObject GuideBinding::toJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("guideId"), guideId);
  obj.insert(QStringLiteral("role"), role);
  obj.insert(QStringLiteral("offset"), offset);
  obj.insert(QStringLiteral("follow"), follow);
  obj.insert(QStringLiteral("enabled"), enabled);
  obj.insert(QStringLiteral("priority"), static_cast<int>(priority));
  return obj;
}

GuideBinding GuideBinding::fromJson(const QJsonObject &obj) {
  GuideBinding binding;
  binding.guideId = obj.value(QStringLiteral("guideId")).toString();
  binding.role = obj.value(QStringLiteral("role")).toString();
  binding.offset = obj.value(QStringLiteral("offset")).toDouble(0.0);
  binding.follow = obj.value(QStringLiteral("follow")).toBool(false);
  binding.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
  binding.priority = static_cast<GuidePriority>(
      obj.value(QStringLiteral("priority")).toInt(static_cast<int>(GuidePriority::Normal)));
  return binding;
}

QJsonObject GuideSet::toJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("ownerId"), ownerId);
  QJsonArray guideArray;
  for (const auto &guide : guides) {
    guideArray.append(guide.toJson());
  }
  obj.insert(QStringLiteral("guides"), guideArray);
  QJsonArray bindingArray;
  for (const auto &binding : bindings) {
    bindingArray.append(binding.toJson());
  }
  obj.insert(QStringLiteral("bindings"), bindingArray);
  return obj;
}

GuideSet GuideSet::fromJson(const QJsonObject &obj) {
  GuideSet set;
  set.ownerId = obj.value(QStringLiteral("ownerId")).toString();
  for (const auto &value : obj.value(QStringLiteral("guides")).toArray()) {
    set.guides.append(GuideDefinition::fromJson(value.toObject()));
  }
  for (const auto &value : obj.value(QStringLiteral("bindings")).toArray()) {
    set.bindings.append(GuideBinding::fromJson(value.toObject()));
  }
  return set;
}

QVector<GuideDefinition> GuideSet::guidesForSemanticTag(GuideSemanticTag tag) const {
  QVector<GuideDefinition> result;
  for (const auto& g : guides) {
    if (g.semanticTag == tag) {
      result.append(g);
    }
  }
  return result;
}

QVector<GuideDefinition> GuideSet::enabledGuides() const {
  QVector<GuideDefinition> result;
  for (const auto& g : guides) {
    if (g.enabled) {
      result.append(g);
    }
  }
  return result;
}

QVector<GuideBinding> GuideSet::enabledBindings() const {
  QVector<GuideBinding> result;
  for (const auto& b : bindings) {
    if (b.enabled) {
      result.append(b);
    }
  }
  return result;
}

GuideDefinition* GuideSet::guideById(const QString& guideId) {
  for (auto& g : guides) {
    if (g.guideId == guideId) {
      return &g;
    }
  }
  return nullptr;
}

void GuideSet::sortByPriority() {
  std::sort(guides.begin(), guides.end(), [](const GuideDefinition& a, const GuideDefinition& b) {
    return static_cast<int>(a.priority) > static_cast<int>(b.priority);
  });
  std::sort(bindings.begin(), bindings.end(), [](const GuideBinding& a, const GuideBinding& b) {
    return static_cast<int>(a.priority) > static_cast<int>(b.priority);
  });
}

QRectF ArtifactAbstractLayer::contentBounds(LayerBoundsKind kind) const {
  return contentBounds().boundsFor(kind);
}

QRectF ArtifactAbstractLayer::sourceBounds() const {
  return contentBounds(LayerBoundsKind::Source);
}

QRectF ArtifactAbstractLayer::visibleBounds() const {
  return contentBounds(LayerBoundsKind::Visible);
}

QString ArtifactAbstractLayer::contentBoundsSummary() const {
  const LayerBounds bounds = contentBounds();
  const auto rectString = [](const QRectF &rect) {
    return rect.isValid()
               ? QStringLiteral("%1,%2 %3x%4")
                     .arg(rect.x(), 0, 'f', 1)
                     .arg(rect.y(), 0, 'f', 1)
                     .arg(rect.width(), 0, 'f', 1)
                     .arg(rect.height(), 0, 'f', 1)
               : QStringLiteral("invalid");
  };

  return QStringLiteral("source=%1 visible=%2 effect=%3 mask=%4 layout=%5")
      .arg(rectString(bounds.sourceBounds),
           rectString(bounds.visibleBounds),
           rectString(bounds.effectBounds),
           rectString(bounds.maskBounds),
           rectString(bounds.layoutBounds));
}

QRectF ArtifactAbstractLayer::effectBounds() const {
  return contentBounds(LayerBoundsKind::Effect);
}

QRectF ArtifactAbstractLayer::maskBounds() const {
  return contentBounds(LayerBoundsKind::Mask);
}

QRectF ArtifactAbstractLayer::layoutBounds() const {
  return contentBounds(LayerBoundsKind::Layout);
}

QRectF ArtifactAbstractLayer::localBounds() const {
  const auto size = sourceSize();
  if (size.width <= 0 || size.height <= 0) {
    return QRectF();
  }
  return QRectF(0.0, 0.0, static_cast<qreal>(size.width),
                static_cast<qreal>(size.height));
}

QRectF ArtifactAbstractLayer::visualLocalBounds() const {
  const QRectF baseBounds = localBounds();
  if (!baseBounds.isValid() || baseBounds.width() <= 0.0 ||
      baseBounds.height() <= 0.0) {
    return QRectF();
  }
  if (!impl_->clonerComponentEnabled_) {
    return baseBounds;
  }

  QRectF visualBounds = baseBounds;
  const auto uniteCloneBounds = [&](const QTransform &cloneTransform) {
    const QRectF cloneBounds = cloneTransform.mapRect(baseBounds);
    if (cloneBounds.isValid() && cloneBounds.width() > 0.0 &&
        cloneBounds.height() > 0.0) {
      visualBounds = visualBounds.united(cloneBounds);
    }
  };
  const auto applyClonerComponentTransform2D = [this](QTransform &cloneTransform) {
    for (const auto &op : impl_->clonerTransforms_) {
      if (!op.enabled) {
        continue;
      }
      if (op.position.x() != 0.0f || op.position.y() != 0.0f) {
        cloneTransform.translate(op.position.x(), op.position.y());
      }
      if (op.rotation.z() != 0.0f) {
        cloneTransform.rotate(op.rotation.z());
      }
      if (op.scale.x() != 1.0f || op.scale.y() != 1.0f) {
        cloneTransform.scale(op.scale.x(), op.scale.y());
      }
    }
  };

  const int mode = impl_->clonerMode_;
  if (mode == 1) {
    const int cols = std::max(1, impl_->clonerColumns_);
    const int rows = std::max(1, impl_->clonerRows_);
    const int depth = std::max(1, impl_->clonerDepth_);
    const QVector3D startPos(
        -((cols - 1) * impl_->clonerSpacingX_) * 0.5f,
        -((rows - 1) * impl_->clonerSpacingY_) * 0.5f,
        -((depth - 1) * impl_->clonerSpacingZ_) * 0.5f);
    for (int z = 0; z < depth; ++z) {
      for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
          QTransform cloneTransform;
          cloneTransform.translate(startPos.x() + impl_->clonerSpacingX_ * x,
                                   startPos.y() + impl_->clonerSpacingY_ * y);
          applyClonerComponentTransform2D(cloneTransform);
          uniteCloneBounds(cloneTransform);
        }
      }
    }
  } else if (mode == 2) {
    const int count = std::max(1, impl_->clonerRadialCount_);
    const float angleStep =
        count > 1 ? (impl_->clonerEndAngle_ - impl_->clonerStartAngle_) /
                        static_cast<float>(count - 1)
                  : 0.0f;
    constexpr float kPi = 3.14159265358979323846f;
    for (int i = 0; i < count; ++i) {
      const float angle =
          impl_->clonerStartAngle_ + angleStep * static_cast<float>(i);
      const float rad = angle * kPi / 180.0f;
      QTransform cloneTransform;
      cloneTransform.translate(std::cos(rad) * impl_->clonerRadius_,
                               std::sin(rad) * impl_->clonerRadius_);
      if (impl_->clonerRotationStep_ != 0.0f) {
        cloneTransform.rotate(angle +
                              impl_->clonerRotationStep_ *
                                  static_cast<float>(i));
      }
      applyClonerComponentTransform2D(cloneTransform);
      uniteCloneBounds(cloneTransform);
    }
  } else {
    const int count = std::max(1, impl_->clonerCloneCount_);
    for (int i = 0; i < count; ++i) {
      QTransform cloneTransform;
      cloneTransform.translate(impl_->clonerOffsetX_ * static_cast<float>(i),
                               impl_->clonerOffsetY_ * static_cast<float>(i));
      if (impl_->clonerRotationStep_ != 0.0f) {
        cloneTransform.rotate(impl_->clonerRotationStep_ *
                              static_cast<float>(i));
      }
      applyClonerComponentTransform2D(cloneTransform);
      uniteCloneBounds(cloneTransform);
    }
  }

  for (const auto &effect : getEffects()) {
    const auto cloner = std::dynamic_pointer_cast<ClonerGenerator>(effect);
    if (!cloner || !cloner->isEnabled()) {
      continue;
    }
    for (const auto &clone : cloner->generateCloneData()) {
      if (!clone.visible) {
        continue;
      }
      const QRectF cloneBounds = mapRectWithMatrix(clone.transform, baseBounds);
      if (cloneBounds.isValid() && cloneBounds.width() > 0.0 &&
          cloneBounds.height() > 0.0) {
        visualBounds = visualBounds.united(cloneBounds);
      }
    }
  }

  return visualBounds;
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
  auto parent = parentLayer();
  const LayerID parentId = impl_->parentLayerId_;
  const quint64 parentRevision = parent ? parent->impl_->geometryRevision_ : 0;
  const int64_t frame = impl_->currentFrame_;
  if (impl_->cachedBoundingBoxRevision_ == impl_->geometryRevision_ &&
      impl_->cachedBoundingBoxParentRevision_ == parentRevision &&
      impl_->cachedBoundingBoxFrame_ == frame &&
      impl_->cachedBoundingBoxParentId_ == parentId) {
    return impl_->cachedBoundingBox_;
  }
  const QRectF localRect = localBounds();
  if (!localRect.isValid() || localRect.width() <= 0.0 ||
      localRect.height() <= 0.0) {
    impl_->cachedBoundingBox_ = QRectF();
  } else {
    impl_->cachedBoundingBox_ = getGlobalTransform().mapRect(visualLocalBounds());
  }
  impl_->cachedBoundingBoxRevision_ = impl_->geometryRevision_;
  impl_->cachedBoundingBoxParentRevision_ = parentRevision;
  impl_->cachedBoundingBoxFrame_ = frame;
  impl_->cachedBoundingBoxParentId_ = parentId;
  return impl_->cachedBoundingBox_;
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
  obj["isSelectionLocked"] = impl_->isSelectionLocked_;
  obj["isTransformLocked"] = impl_->isTransformLocked_;
  obj["isTimingLocked"] = impl_->isTimingLocked_;
  obj["isGuide"] = impl_->isGuide_;
  obj["isSolo"] = impl_->isSolo_;
  obj["layerCachePolicy"] = static_cast<int>(impl_->layerCachePolicy_);
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

  // Modifiers and effects
  QJsonArray modifiersArr;
  for (const auto &modifier : getModifiers()) {
    if (!modifier) {
      continue;
    }
    modifiersArr.append(serializeLayerModifier(*modifier));
  }
  obj["modifiers"] = modifiersArr;

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
  obj["physics"] = impl_->physicsComponent_.settings().toJson();
  QJsonObject componentsObj;
  componentsObj["scriptEnabled"] = impl_->scriptComponentEnabled_;
  componentsObj["clonerEnabled"] = impl_->clonerComponentEnabled_;
  componentsObj["layoutEnabled"] = impl_->layoutComponentEnabled_;
  componentsObj["layoutMode"] = impl_->layoutMode_;
  componentsObj["layoutAnchorMode"] = impl_->layoutAnchorMode_;
  componentsObj["layoutHorizontalPin"] = impl_->layoutHorizontalPin_;
  componentsObj["layoutVerticalPin"] = impl_->layoutVerticalPin_;
  componentsObj["layoutScaleMode"] = impl_->layoutScaleMode_;
  componentsObj["layoutSafeAreaEnabled"] = impl_->layoutSafeAreaEnabled_;
  componentsObj["layoutSafeAreaPaddingX"] = static_cast<double>(impl_->layoutSafeAreaPaddingX_);
  componentsObj["layoutSafeAreaPaddingY"] = static_cast<double>(impl_->layoutSafeAreaPaddingY_);
  componentsObj["layoutStackDirection"] = impl_->layoutStackDirection_;
  componentsObj["layoutGap"] = static_cast<double>(impl_->layoutGap_);
  componentsObj["layoutMaxPerRow"] = impl_->layoutMaxPerRow_;
  componentsObj["clonerMode"] = impl_->clonerMode_;
  componentsObj["clonerCloneCount"] = impl_->clonerCloneCount_;
  componentsObj["clonerOffsetX"] = static_cast<double>(impl_->clonerOffsetX_);
  componentsObj["clonerOffsetY"] = static_cast<double>(impl_->clonerOffsetY_);
  componentsObj["clonerOffsetZ"] = static_cast<double>(impl_->clonerOffsetZ_);
  componentsObj["clonerJitterX"] = static_cast<double>(impl_->clonerJitterX_);
  componentsObj["clonerJitterY"] = static_cast<double>(impl_->clonerJitterY_);
  componentsObj["clonerJitterZ"] = static_cast<double>(impl_->clonerJitterZ_);
  componentsObj["clonerSeed"] = impl_->clonerSeed_;
  componentsObj["clonerColumns"] = impl_->clonerColumns_;
  componentsObj["clonerRows"] = impl_->clonerRows_;
  componentsObj["clonerDepth"] = impl_->clonerDepth_;
  componentsObj["clonerSpacingX"] = static_cast<double>(impl_->clonerSpacingX_);
  componentsObj["clonerSpacingY"] = static_cast<double>(impl_->clonerSpacingY_);
  componentsObj["clonerSpacingZ"] = static_cast<double>(impl_->clonerSpacingZ_);
  componentsObj["clonerRadialCount"] = impl_->clonerRadialCount_;
  componentsObj["clonerRadius"] = static_cast<double>(impl_->clonerRadius_);
  componentsObj["clonerStartAngle"] = static_cast<double>(impl_->clonerStartAngle_);
  componentsObj["clonerEndAngle"] = static_cast<double>(impl_->clonerEndAngle_);
  componentsObj["clonerRotationStep"] = static_cast<double>(impl_->clonerRotationStep_);
  componentsObj["clonerOpacityDecay"] = static_cast<double>(impl_->clonerOpacityDecay_);
  QJsonArray clonerTransformsArr;
  for (const auto &op : impl_->clonerTransforms_) {
    QJsonObject transformObj;
    transformObj["name"] = op.name;
    transformObj["enabled"] = op.enabled;
    transformObj["positionX"] = static_cast<double>(op.position.x());
    transformObj["positionY"] = static_cast<double>(op.position.y());
    transformObj["positionZ"] = static_cast<double>(op.position.z());
    transformObj["rotationX"] = static_cast<double>(op.rotation.x());
    transformObj["rotationY"] = static_cast<double>(op.rotation.y());
    transformObj["rotationZ"] = static_cast<double>(op.rotation.z());
    transformObj["scaleX"] = static_cast<double>(op.scale.x());
    transformObj["scaleY"] = static_cast<double>(op.scale.y());
    transformObj["scaleZ"] = static_cast<double>(op.scale.z());
    clonerTransformsArr.append(transformObj);
  }
  componentsObj["clonerTransforms"] = clonerTransformsArr;
  if (!impl_->scriptBinding_.isEmpty()) {
    componentsObj["scriptBinding"] = impl_->scriptBinding_;
  }
  obj["components"] = componentsObj;

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

  // Masks
  if (hasMasks()) {
    QJsonArray masksArr;
    for (int maskIndex = 0; maskIndex < maskCount(); ++maskIndex) {
      const auto layerMask = impl_->getMask(maskIndex);
      QJsonObject mobj;
      mobj["enabled"] = layerMask.isEnabled();

      QJsonArray pathsArr;
      for (int pathIndex = 0; pathIndex < layerMask.maskPathCount();
           ++pathIndex) {
        const auto path = layerMask.maskPath(pathIndex);
        QJsonObject pobj;

        // vertices: 各頂点は position / inTangent / outTangent（QPointF = x,y）
        QJsonArray vertsArr;
        for (int vi = 0; vi < path.vertexCount(); ++vi) {
          const auto v = path.vertex(vi);
          QJsonObject vobj;
          vobj["px"] = v.position.x();
          vobj["py"] = v.position.y();
          vobj["ix"] = v.inTangent.x();
          vobj["iy"] = v.inTangent.y();
          vobj["ox"] = v.outTangent.x();
          vobj["oy"] = v.outTangent.y();
          vertsArr.append(vobj);
        }
        pobj["vertices"] = vertsArr;
        pobj["closed"] = path.isClosed();
        pobj["opacity"] = static_cast<double>(path.opacity());
        pobj["feather"] = static_cast<double>(path.feather());
        pobj["featherHorizontal"] = static_cast<double>(path.featherHorizontal());
        pobj["featherVertical"] = static_cast<double>(path.featherVertical());
        pobj["featherInner"] = static_cast<double>(path.featherInner());
        pobj["featherOuter"] = static_cast<double>(path.featherOuter());
        pobj["expansion"] = static_cast<double>(path.expansion());
        pobj["inverted"] = path.isInverted();
        pobj["mode"] = static_cast<int>(path.mode());
        pobj["name"] = path.name().toQString();

        // animation keyframes
        if (path.hasAnimationKeyframes()) {
          QJsonArray kfArr;
          for (const auto &kf : path.animationKeyframes()) {
            QJsonObject kfobj;
            kfobj["frame"] = static_cast<qint64>(kf.frame);
            kfobj["closed"] = kf.closed;
            kfobj["opacity"] = static_cast<double>(kf.opacity);
            kfobj["feather"] = static_cast<double>(kf.feather);
            kfobj["featherHorizontal"] = static_cast<double>(kf.featherHorizontal);
            kfobj["featherVertical"] = static_cast<double>(kf.featherVertical);
            kfobj["featherInner"] = static_cast<double>(kf.featherInner);
            kfobj["featherOuter"] = static_cast<double>(kf.featherOuter);
            kfobj["expansion"] = static_cast<double>(kf.expansion);
            kfobj["inverted"] = kf.inverted;
            kfobj["mode"] = static_cast<int>(kf.mode);
            kfobj["name"] = kf.name.toQString();

            QJsonArray kfVertsArr;
            for (const auto &v : kf.vertices) {
              QJsonObject vobj;
              vobj["px"] = v.position.x();
              vobj["py"] = v.position.y();
              vobj["ix"] = v.inTangent.x();
              vobj["iy"] = v.inTangent.y();
              vobj["ox"] = v.outTangent.x();
              vobj["oy"] = v.outTangent.y();
              kfVertsArr.append(vobj);
            }
            kfobj["vertices"] = kfVertsArr;
            kfArr.append(kfobj);
          }
          pobj["animationKeyframes"] = kfArr;
        }
        pathsArr.append(pobj);
      }
      mobj["paths"] = pathsArr;
      masksArr.append(mobj);
    }
    obj["masks"] = masksArr;
  }

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
  if (obj.contains("is3D"))
    setIs3D(obj["is3D"].toBool());
  if (obj.contains("isLocked"))
    setLocked(obj["isLocked"].toBool());
  if (obj.contains("isSelectionLocked"))
    setSelectionLocked(obj["isSelectionLocked"].toBool());
  if (obj.contains("isTransformLocked"))
    setTransformLocked(obj["isTransformLocked"].toBool());
  if (obj.contains("isTimingLocked"))
    setTimingLocked(obj["isTimingLocked"].toBool());
  if (obj.contains("isGuide"))
    setGuide(obj["isGuide"].toBool());
  if (obj.contains("isSolo"))
    setSolo(obj["isSolo"].toBool());
  if (obj.contains("layerCachePolicy"))
    setLayerCachePolicy(static_cast<LayerCachePolicy>(
        obj["layerCachePolicy"].toInt(static_cast<int>(LayerCachePolicy::Default))));
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
    // Time zero only needs a stable scale; avoid implying a fake fps.
    RationalTime t0(0, 1);
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

  if (obj.contains("modifiers") && obj["modifiers"].isArray()) {
      impl_->modifiers_.clear();
      QJsonArray arr = obj["modifiers"].toArray();
      for (const auto& mv : arr) {
          if (!mv.isObject()) {
              continue;
          }
          auto modifier = deserializeLayerModifier(mv.toObject());
          if (modifier) {
              impl_->modifiers_.add(std::move(modifier));
          }
      }
  }

  if (obj.contains("physics") && obj["physics"].isObject()) {
      impl_->physicsComponent_.settings().fromJson(obj["physics"].toObject());
      impl_->physicsComponent_.reset();
  }
  if (obj.contains("components") && obj["components"].isObject()) {
      const QJsonObject componentsObj = obj["components"].toObject();
        impl_->scriptComponentEnabled_ =
            componentsObj.value(QStringLiteral("scriptEnabled")).toBool(false);
        impl_->clonerComponentEnabled_ =
            componentsObj.value(QStringLiteral("clonerEnabled")).toBool(false);
        impl_->layoutComponentEnabled_ =
            componentsObj.value(QStringLiteral("layoutEnabled")).toBool(false);
        impl_->layoutMode_ =
            componentsObj.value(QStringLiteral("layoutMode")).toInt(0);
        impl_->layoutAnchorMode_ =
            componentsObj.value(QStringLiteral("layoutAnchorMode")).toInt(0);
        impl_->layoutHorizontalPin_ =
            componentsObj.value(QStringLiteral("layoutHorizontalPin")).toInt(0);
        impl_->layoutVerticalPin_ =
            componentsObj.value(QStringLiteral("layoutVerticalPin")).toInt(0);
        impl_->layoutScaleMode_ =
            componentsObj.value(QStringLiteral("layoutScaleMode")).toInt(0);
        impl_->layoutSafeAreaEnabled_ =
            componentsObj.value(QStringLiteral("layoutSafeAreaEnabled")).toBool(false);
        impl_->layoutSafeAreaPaddingX_ = static_cast<float>(
            componentsObj.value(QStringLiteral("layoutSafeAreaPaddingX")).toDouble(0.0));
        impl_->layoutSafeAreaPaddingY_ = static_cast<float>(
            componentsObj.value(QStringLiteral("layoutSafeAreaPaddingY")).toDouble(0.0));
        impl_->layoutStackDirection_ =
            componentsObj.value(QStringLiteral("layoutStackDirection")).toInt(0);
        impl_->layoutGap_ = static_cast<float>(
            componentsObj.value(QStringLiteral("layoutGap")).toDouble(24.0));
        impl_->layoutMaxPerRow_ =
            std::max(0, componentsObj.value(QStringLiteral("layoutMaxPerRow")).toInt(0));
        impl_->clonerMode_ =
            componentsObj.value(QStringLiteral("clonerMode")).toInt(0);
        impl_->clonerCloneCount_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerCloneCount")).toInt(3));
        impl_->clonerOffsetX_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerOffsetX")).toDouble(160.0));
        impl_->clonerOffsetY_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerOffsetY")).toDouble(48.0));
        impl_->clonerOffsetZ_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerOffsetZ")).toDouble(0.0));
        impl_->clonerJitterX_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerJitterX")).toDouble(0.0));
        impl_->clonerJitterY_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerJitterY")).toDouble(0.0));
        impl_->clonerJitterZ_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerJitterZ")).toDouble(0.0));
        impl_->clonerSeed_ =
            componentsObj.value(QStringLiteral("clonerSeed")).toInt(0);
        impl_->clonerColumns_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerColumns")).toInt(3));
        impl_->clonerRows_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerRows")).toInt(3));
        impl_->clonerDepth_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerDepth")).toInt(1));
        impl_->clonerSpacingX_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerSpacingX")).toDouble(160.0));
        impl_->clonerSpacingY_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerSpacingY")).toDouble(48.0));
        impl_->clonerSpacingZ_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerSpacingZ")).toDouble(0.0));
        impl_->clonerRadialCount_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerRadialCount")).toInt(8));
        impl_->clonerRadius_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerRadius")).toDouble(160.0));
        impl_->clonerStartAngle_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerStartAngle")).toDouble(0.0));
        impl_->clonerEndAngle_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerEndAngle")).toDouble(360.0));
        impl_->clonerRotationStep_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerRotationStep")).toDouble(0.0));
        impl_->clonerOpacityDecay_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerOpacityDecay")).toDouble(0.0));
        impl_->clonerTransforms_.clear();
        if (componentsObj.contains(QStringLiteral("clonerTransforms")) &&
            componentsObj.value(QStringLiteral("clonerTransforms")).isArray()) {
          const QJsonArray transformArr =
              componentsObj.value(QStringLiteral("clonerTransforms")).toArray();
          impl_->clonerTransforms_.reserve(static_cast<size_t>(transformArr.size()));
          for (const auto &entry : transformArr) {
            if (!entry.isObject()) {
              continue;
            }
            const QJsonObject transformObj = entry.toObject();
            ClonerTransformOperation op;
            op.name = transformObj.value(QStringLiteral("name"))
                          .toString(QStringLiteral("Transform"));
            op.enabled =
                transformObj.value(QStringLiteral("enabled")).toBool(true);
            op.position.setX(static_cast<float>(
                transformObj.value(QStringLiteral("positionX")).toDouble(0.0)));
            op.position.setY(static_cast<float>(
                transformObj.value(QStringLiteral("positionY")).toDouble(0.0)));
            op.position.setZ(static_cast<float>(
                transformObj.value(QStringLiteral("positionZ")).toDouble(0.0)));
            op.rotation.setX(static_cast<float>(
                transformObj.value(QStringLiteral("rotationX")).toDouble(0.0)));
            op.rotation.setY(static_cast<float>(
                transformObj.value(QStringLiteral("rotationY")).toDouble(0.0)));
            op.rotation.setZ(static_cast<float>(
                transformObj.value(QStringLiteral("rotationZ")).toDouble(0.0)));
            op.scale.setX(static_cast<float>(
                transformObj.value(QStringLiteral("scaleX")).toDouble(1.0)));
            op.scale.setY(static_cast<float>(
                transformObj.value(QStringLiteral("scaleY")).toDouble(1.0)));
            op.scale.setZ(static_cast<float>(
                transformObj.value(QStringLiteral("scaleZ")).toDouble(1.0)));
            impl_->clonerTransforms_.push_back(op);
          }
        } else {
          const bool hasLegacyTransform =
              componentsObj.contains(QStringLiteral("clonerTransformPositionX")) ||
              componentsObj.contains(QStringLiteral("clonerTransformPositionY")) ||
              componentsObj.contains(QStringLiteral("clonerTransformPositionZ")) ||
              componentsObj.contains(QStringLiteral("clonerTransformRotationX")) ||
              componentsObj.contains(QStringLiteral("clonerTransformRotationY")) ||
              componentsObj.contains(QStringLiteral("clonerTransformRotationZ")) ||
              componentsObj.contains(QStringLiteral("clonerTransformScaleX")) ||
              componentsObj.contains(QStringLiteral("clonerTransformScaleY")) ||
              componentsObj.contains(QStringLiteral("clonerTransformScaleZ"));
          if (hasLegacyTransform) {
            ClonerTransformOperation op;
            op.name = QStringLiteral("Transform 1");
            op.position.setX(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformPositionX")).toDouble(0.0)));
            op.position.setY(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformPositionY")).toDouble(0.0)));
            op.position.setZ(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformPositionZ")).toDouble(0.0)));
            op.rotation.setX(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformRotationX")).toDouble(0.0)));
            op.rotation.setY(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformRotationY")).toDouble(0.0)));
            op.rotation.setZ(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformRotationZ")).toDouble(0.0)));
            op.scale.setX(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformScaleX")).toDouble(1.0)));
            op.scale.setY(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformScaleY")).toDouble(1.0)));
            op.scale.setZ(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformScaleZ")).toDouble(1.0)));
            impl_->clonerTransforms_.push_back(op);
          }
        }
        impl_->scriptBinding_ = componentsObj.value(QStringLiteral("scriptBinding")).toObject();
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
              RationalTime t0(0, 1);
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

  // Masks
  if (obj.contains("masks") && obj["masks"].isArray()) {
    impl_->clearMasks();
    const auto masksArr = obj["masks"].toArray();
    for (const auto &maskVal : masksArr) {
      if (!maskVal.isObject()) continue;
      const auto mobj = maskVal.toObject();

      LayerMask layerMask;
      if (mobj.contains("enabled")) {
        layerMask.setEnabled(mobj["enabled"].toBool(true));
      }

      if (mobj.contains("paths") && mobj["paths"].isArray()) {
        const auto pathsArr = mobj["paths"].toArray();
        for (const auto &pathVal : pathsArr) {
          if (!pathVal.isObject()) continue;
          const auto pobj = pathVal.toObject();

          MaskPath path;
          path.clearVertices();
          if (pobj.contains("vertices") && pobj["vertices"].isArray()) {
            const auto vertsArr = pobj["vertices"].toArray();
            for (const auto &vVal : vertsArr) {
              if (!vVal.isObject()) continue;
              const auto vobj = vVal.toObject();
              MaskVertex v;
              v.position = QPointF(vobj["px"].toDouble(), vobj["py"].toDouble());
              v.inTangent = QPointF(vobj["ix"].toDouble(), vobj["iy"].toDouble());
              v.outTangent = QPointF(vobj["ox"].toDouble(), vobj["oy"].toDouble());
              path.addVertex(v);
            }
          }
          if (pobj.contains("closed"))
            path.setClosed(pobj["closed"].toBool(true));
          if (pobj.contains("opacity"))
            path.setOpacity(static_cast<float>(pobj["opacity"].toDouble(1.0)));
          if (pobj.contains("feather"))
            path.setFeather(static_cast<float>(pobj["feather"].toDouble(0.0)));
          if (pobj.contains("featherHorizontal"))
            path.setFeatherHorizontal(static_cast<float>(pobj["featherHorizontal"].toDouble(0.0)));
          if (pobj.contains("featherVertical"))
            path.setFeatherVertical(static_cast<float>(pobj["featherVertical"].toDouble(0.0)));
          if (pobj.contains("featherInner"))
            path.setFeatherInner(static_cast<float>(pobj["featherInner"].toDouble(0.0)));
          if (pobj.contains("featherOuter"))
            path.setFeatherOuter(static_cast<float>(pobj["featherOuter"].toDouble(0.0)));
          if (pobj.contains("expansion"))
            path.setExpansion(static_cast<float>(pobj["expansion"].toDouble(0.0)));
          if (pobj.contains("inverted"))
            path.setInverted(pobj["inverted"].toBool(false));
          if (pobj.contains("mode"))
            path.setMode(static_cast<MaskMode>(pobj["mode"].toInt(static_cast<int>(MaskMode::Add))));
          if (pobj.contains("name"))
            path.setName(UniString::fromQString(pobj["name"].toString()));

          // animation keyframes
          if (pobj.contains("animationKeyframes") && pobj["animationKeyframes"].isArray()) {
            const auto kfArr = pobj["animationKeyframes"].toArray();
            for (const auto &kfVal : kfArr) {
              if (!kfVal.isObject()) continue;
              const auto kfobj = kfVal.toObject();

              MaskPathKeyframeSnapshot snap;
              snap.frame = static_cast<int64_t>(kfobj["frame"].toVariant().toLongLong());
              snap.closed = kfobj["closed"].toBool(true);
              snap.opacity = static_cast<float>(kfobj["opacity"].toDouble(1.0));
              snap.feather = static_cast<float>(kfobj["feather"].toDouble(0.0));
              snap.featherHorizontal = static_cast<float>(kfobj["featherHorizontal"].toDouble(0.0));
              snap.featherVertical = static_cast<float>(kfobj["featherVertical"].toDouble(0.0));
              snap.featherInner = static_cast<float>(kfobj["featherInner"].toDouble(0.0));
              snap.featherOuter = static_cast<float>(kfobj["featherOuter"].toDouble(0.0));
              snap.expansion = static_cast<float>(kfobj["expansion"].toDouble(0.0));
              snap.inverted = kfobj["inverted"].toBool(false);
              snap.mode = static_cast<MaskMode>(kfobj["mode"].toInt(static_cast<int>(MaskMode::Add)));
              snap.name = UniString::fromQString(kfobj["name"].toString());

              if (kfobj.contains("vertices") && kfobj["vertices"].isArray()) {
                const auto kfVertsArr = kfobj["vertices"].toArray();
                for (const auto &vVal : kfVertsArr) {
                  if (!vVal.isObject()) continue;
                  const auto vobj = vVal.toObject();
                  MaskVertex v;
                  v.position = QPointF(vobj["px"].toDouble(), vobj["py"].toDouble());
                  v.inTangent = QPointF(vobj["ix"].toDouble(), vobj["iy"].toDouble());
                  v.outTangent = QPointF(vobj["ox"].toDouble(), vobj["oy"].toDouble());
                  snap.vertices.push_back(v);
                }
              }
              path.setAnimationKeyframe(snap.frame, snap);
            }
          }

          layerMask.addMaskPath(path);
        }
      }

      impl_->addMask(layerMask);
    }
    changed();
  }

  applyPropertiesFromJson(obj);
}

void ArtifactAbstractLayer::Impl::addEffect(
    std::shared_ptr<ArtifactAbstractEffect> effect) {
  if (!effect)
    return;
  const QString currentId = effect->effectID().toQString().trimmed();
  const QString uniqueId = uniqueEffectIdForLayer(
      effects_, effect->displayName().toQString(), currentId);
  if (currentId.isEmpty() || currentId != uniqueId) {
    effect->setEffectID(UniString::fromQString(uniqueId));
  }
  effects_.push_back(effect);
  qDebug() << "[ArtifactAbstractLayer] Effect added:"
           << effect->displayName().toQString() << "id="
           << effect->effectID().toQString();
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

void ArtifactAbstractLayer::Impl::addModifier(
    std::shared_ptr<ArtifactLayerModifier> modifier) {
  if (!modifier) {
    return;
  }

  const QString currentId = modifier->modifierId().trimmed();
  const QString uniqueId = uniqueModifierIdForLayer(
      modifiers_.modifiers(), modifier->displayName(), currentId);
  if (currentId.isEmpty() || currentId != uniqueId) {
    modifier->setModifierId(uniqueId);
  }
  modifiers_.add(std::move(modifier));
}

void ArtifactAbstractLayer::Impl::removeModifier(const QString& modifierId) {
  modifiers_.remove(modifierId);
}

void ArtifactAbstractLayer::Impl::clearModifiers() {
  modifiers_.clear();
}

std::vector<std::shared_ptr<ArtifactLayerModifier>>
ArtifactAbstractLayer::Impl::getModifiers() const {
  return modifiers_.modifiers();
}

std::shared_ptr<ArtifactLayerModifier>
ArtifactAbstractLayer::Impl::getModifier(const QString& modifierId) const {
  return modifiers_.modifier(modifierId);
}

int ArtifactAbstractLayer::Impl::modifierCount() const {
  return modifiers_.count();
}

bool ArtifactAbstractLayer::Impl::hasModifiers() const {
  return !modifiers_.isEmpty();
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

void ArtifactAbstractLayer::addModifier(
    std::shared_ptr<ArtifactLayerModifier> modifier) {
  impl_->addModifier(std::move(modifier));
  notifyLayerMutation(this, LayerDirtyFlag::Transform,
                      LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::removeModifier(const QString& modifierId) {
  impl_->removeModifier(modifierId);
  notifyLayerMutation(this, LayerDirtyFlag::Transform,
                      LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::clearModifiers() {
  impl_->clearModifiers();
  notifyLayerMutation(this, LayerDirtyFlag::Transform,
                      LayerDirtyReason::PropertyChanged);
}

std::vector<std::shared_ptr<ArtifactLayerModifier>>
ArtifactAbstractLayer::getModifiers() const {
  return impl_->getModifiers();
}

std::shared_ptr<ArtifactLayerModifier>
ArtifactAbstractLayer::getModifier(const QString& modifierId) const {
  return impl_->getModifier(modifierId);
}

int ArtifactAbstractLayer::modifierCount() const { return impl_->modifierCount(); }

bool ArtifactAbstractLayer::hasModifiers() const { return impl_->hasModifiers(); }

QJsonObject ArtifactAbstractLayer::scriptBinding() const {
  return impl_->scriptBinding_;
}

void ArtifactAbstractLayer::setScriptBinding(const QJsonObject& binding) {
  impl_->scriptBinding_ = binding;
  notifyLayerMutation(this, LayerDirtyFlag::Property,
                      LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::clearScriptBinding() {
  if (impl_->scriptBinding_.isEmpty()) {
    return;
  }
  impl_->scriptBinding_ = QJsonObject{};
  notifyLayerMutation(this, LayerDirtyFlag::Property,
                      LayerDirtyReason::PropertyChanged);
}

bool ArtifactAbstractLayer::hasScriptBinding() const {
  return !impl_->scriptBinding_.isEmpty();
}

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
  layerGroup.addProperty(makeProp(QStringLiteral("layer.selectionLocked"),
                                  PropertyType::Boolean, isSelectionLocked(), -179));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.transformLocked"),
                                  PropertyType::Boolean, isTransformLocked(), -178));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.timingLocked"),
                                  PropertyType::Boolean, isTimingLocked(), -177));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.guide"),
                                  PropertyType::Boolean, isGuide(), -170));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.solo"),
                                  PropertyType::Boolean, isSolo(), -160));
  auto cachePolicyProp = makeProp(QStringLiteral("layer.cachePolicy"),
                                  PropertyType::Integer,
                                  static_cast<int>(layerCachePolicy()),
                                  -159);
  cachePolicyProp->setDisplayLabel(QStringLiteral("Cache Policy"));
  layerGroup.addProperty(cachePolicyProp);
  layerGroup.addProperty(makeProp(QStringLiteral("layer.shy"),
                                  PropertyType::Boolean, isShy(), -150));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.labelColorIndex"),
                                  PropertyType::Integer, labelColorIndex(),
                                  -145));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.is3D"),
                                  PropertyType::Boolean, is3D(), -144));

  auto opacityProp =
      makeProp(QStringLiteral("layer.opacity"), PropertyType::Float,
               static_cast<double>(opacity()), -140);
  opacityProp->setDisplayLabel(QStringLiteral("Opacity"));
  opacityProp->setHardRange(0.0, 1.0);
  opacityProp->setSoftRange(0.0, 1.0);
  opacityProp->setStep(0.01);
  opacityProp->setAnimatable(true);
  layerGroup.addProperty(opacityProp);

  // トランスフォームのプロパティグループ（優先度を高く設定）
  PropertyGroup transformGroup(QStringLiteral("Transform"));
  const auto &t3 = transform3D();

  PropertyGroup initialGroup(QStringLiteral("Initial"));
  const auto sz = sourceSize();
  auto sourceWidthProp =
      makeProp(QStringLiteral("source.width"), PropertyType::Integer,
               sz.width, -500);
  sourceWidthProp->setDisplayLabel(QStringLiteral("Initial Width"));
  sourceWidthProp->setUnit(QStringLiteral("px"));
  sourceWidthProp->setTooltip(
      QStringLiteral("Base layer width. This value is not keyframeable."));
  sourceWidthProp->setHardRange(1.0, 16384.0);
  sourceWidthProp->setSoftRange(1.0, 4096.0);
  initialGroup.addProperty(sourceWidthProp);

  auto sourceHeightProp =
      makeProp(QStringLiteral("source.height"), PropertyType::Integer,
               sz.height, -499);
  sourceHeightProp->setDisplayLabel(QStringLiteral("Initial Height"));
  sourceHeightProp->setUnit(QStringLiteral("px"));
  sourceHeightProp->setTooltip(
      QStringLiteral("Base layer height. This value is not keyframeable."));
  sourceHeightProp->setHardRange(1.0, 16384.0);
  sourceHeightProp->setSoftRange(1.0, 4096.0);
  initialGroup.addProperty(sourceHeightProp);

  auto initialRotationProp =
      makeProp(QStringLiteral("transform.initialRotation"),
               PropertyType::Float, t3.initialRotation(), -498);
  initialRotationProp->setDisplayLabel(QStringLiteral("Initial Angle"));
  initialRotationProp->setUnit(QStringLiteral("deg"));
  initialRotationProp->setTooltip(
      QStringLiteral("Base layer angle. This value is not keyframeable."));
  initialRotationProp->setSoftRange(-180.0, 180.0);
  initialGroup.addProperty(initialRotationProp);

  auto posXProp = makeProp(QStringLiteral("transform.position.x"),
                           PropertyType::Float, t3.positionX(), -300);
  posXProp->setDisplayLabel(QStringLiteral("Position X"));
  posXProp->setUnit(QStringLiteral("px"));
  posXProp->setAnimatable(true);
  transformGroup.addProperty(posXProp);

  auto posYProp = makeProp(QStringLiteral("transform.position.y"),
                           PropertyType::Float, t3.positionY(), -299);
  posYProp->setDisplayLabel(QStringLiteral("Position Y"));
  posYProp->setUnit(QStringLiteral("px"));
  posYProp->setAnimatable(true);
  transformGroup.addProperty(posYProp);

  auto scaleXProp = makeProp(QStringLiteral("transform.scale.x"),
                             PropertyType::Float, t3.scaleX(), -298);
  scaleXProp->setDisplayLabel(QStringLiteral("Scale X"));
  scaleXProp->setAnimatable(true);
   scaleXProp->setSoftRange(0.0, 2.0);  // Soft range for typical use (0-200%)
  transformGroup.addProperty(scaleXProp);

  auto scaleYProp = makeProp(QStringLiteral("transform.scale.y"),
                             PropertyType::Float, t3.scaleY(), -297);
  scaleYProp->setDisplayLabel(QStringLiteral("Scale Y"));
  scaleYProp->setAnimatable(true);
   scaleYProp->setSoftRange(0.0, 2.0);  // Soft range for typical use (0-200%)
  transformGroup.addProperty(scaleYProp);

  auto rotationProp = makeProp(QStringLiteral("transform.rotation"),
                               PropertyType::Float, t3.rotation(), -296);
  rotationProp->setDisplayLabel(QStringLiteral("Rotation"));
  rotationProp->setUnit(QStringLiteral("deg"));
  rotationProp->setAnimatable(true);
  transformGroup.addProperty(rotationProp);

  auto autoOrientProp =
      makeProp(QStringLiteral("transform.autoOrient"), PropertyType::Boolean,
               t3.isAutoOrient(), -295);
  autoOrientProp->setDisplayLabel(QStringLiteral("Auto-Orient"));
  autoOrientProp->setTooltip(QStringLiteral(
      "Automatically orient the layer along its motion path."));
  transformGroup.addProperty(autoOrientProp);

  auto anchorXProp = makeProp(QStringLiteral("transform.anchor.x"),
                              PropertyType::Float, t3.anchorX(), -295);
  anchorXProp->setDisplayLabel(QStringLiteral("Anchor X"));
  anchorXProp->setUnit(QStringLiteral("px"));
  anchorXProp->setAnimatable(true);
  transformGroup.addProperty(anchorXProp);

  auto anchorYProp = makeProp(QStringLiteral("transform.anchor.y"),
                              PropertyType::Float, t3.anchorY(), -294);
  anchorYProp->setDisplayLabel(QStringLiteral("Anchor Y"));
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

  // 物理演算プロパティグループ
  PropertyGroup physicsGroup(QStringLiteral("Physics"));

  auto physicsEnabledProp =
      makeProp(QStringLiteral("physics.enabled"), PropertyType::Boolean,
               impl_->physicsComponent_.settings().enabled, -100);
  physicsGroup.addProperty(physicsEnabledProp);

  auto stiffnessProp =
      makeProp(QStringLiteral("physics.stiffness"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().stiffness), -99);
  stiffnessProp->setHardRange(0.0, 1000.0);
  stiffnessProp->setSoftRange(0.0, 500.0);
  stiffnessProp->setStep(1.0);
  physicsGroup.addProperty(stiffnessProp);

  auto dampingProp =
      makeProp(QStringLiteral("physics.damping"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().damping), -98);
  dampingProp->setHardRange(0.0, 100.0);
  dampingProp->setSoftRange(0.0, 50.0);
  dampingProp->setStep(0.1);
  physicsGroup.addProperty(dampingProp);

  auto followThroughProp =
      makeProp(QStringLiteral("physics.followThroughGain"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().followThroughGain), -97);
  followThroughProp->setHardRange(0.0, 2.0);
  followThroughProp->setSoftRange(0.0, 1.0);
  followThroughProp->setStep(0.01);
  physicsGroup.addProperty(followThroughProp);

  auto gravityYProp =
      makeProp(QStringLiteral("physics.gravityY"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().gravityY), -96);
  gravityYProp->setUnit(QStringLiteral("px/s^2"));
  gravityYProp->setHardRange(-5000.0, 5000.0);
  gravityYProp->setSoftRange(-2000.0, 2000.0);
  gravityYProp->setStep(10.0);
  physicsGroup.addProperty(gravityYProp);

  auto linearDampingProp =
      makeProp(QStringLiteral("physics.linearDamping"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().linearDamping), -95);
  linearDampingProp->setHardRange(0.0, 50.0);
  linearDampingProp->setSoftRange(0.0, 10.0);
  linearDampingProp->setStep(0.1);
  physicsGroup.addProperty(linearDampingProp);

  auto wiggleFreqProp =
      makeProp(QStringLiteral("physics.wiggleFreq"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().wiggleFreq), -94);
  wiggleFreqProp->setUnit(QStringLiteral("Hz"));
  wiggleFreqProp->setSoftRange(0.0, 10.0);
  physicsGroup.addProperty(wiggleFreqProp);

  auto wiggleAmpProp =
      makeProp(QStringLiteral("physics.wiggleAmp"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().wiggleAmp), -93);
  wiggleAmpProp->setSoftRange(0.0, 100.0);
  physicsGroup.addProperty(wiggleAmpProp);

  PropertyGroup componentGroup(QStringLiteral("Components"));
  auto scriptComponentEnabledProp =
      makeProp(QStringLiteral("component.script.enabled"),
               PropertyType::Boolean, impl_->scriptComponentEnabled_, -100);
  scriptComponentEnabledProp->setDisplayLabel(QStringLiteral("Script Component Enabled"));
  componentGroup.addProperty(scriptComponentEnabledProp);

  auto clonerComponentEnabledProp =
      makeProp(QStringLiteral("component.cloner.enabled"),
               PropertyType::Boolean, impl_->clonerComponentEnabled_, -90);
  clonerComponentEnabledProp->setDisplayLabel(QStringLiteral("Cloner Enabled"));
  componentGroup.addProperty(clonerComponentEnabledProp);
  PropertyGroup layoutGroup(QStringLiteral("Layout"));
  PropertyGroup clonerGroup(QStringLiteral("Cloner"));
  auto layoutComponentEnabledProp =
      makeProp(QStringLiteral("component.layout.enabled"),
               PropertyType::Boolean, impl_->layoutComponentEnabled_, -89);
  layoutComponentEnabledProp->setDisplayLabel(QStringLiteral("Layout Enabled"));
  layoutGroup.addProperty(layoutComponentEnabledProp);
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.mode"),
                PropertyType::Integer, impl_->layoutMode_, -88));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.anchorMode"),
                PropertyType::Integer, impl_->layoutAnchorMode_, -87));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.horizontalPin"),
                PropertyType::Integer, impl_->layoutHorizontalPin_, -86));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.verticalPin"),
                PropertyType::Integer, impl_->layoutVerticalPin_, -85));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.scaleMode"),
                PropertyType::Integer, impl_->layoutScaleMode_, -84));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.safeAreaEnabled"),
                PropertyType::Boolean, impl_->layoutSafeAreaEnabled_, -83));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.safeAreaPaddingX"),
                PropertyType::Float,
                static_cast<double>(impl_->layoutSafeAreaPaddingX_), -82));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.safeAreaPaddingY"),
                PropertyType::Float,
                static_cast<double>(impl_->layoutSafeAreaPaddingY_), -81));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.stackDirection"),
                PropertyType::Integer, impl_->layoutStackDirection_, -80));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.gap"),
                PropertyType::Float,
                static_cast<double>(impl_->layoutGap_), -79));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.maxPerRow"),
                PropertyType::Integer, impl_->layoutMaxPerRow_, -78));
  auto clonerModeProp =
      makeProp(QStringLiteral("component.cloner.mode"),
               PropertyType::Integer, impl_->clonerMode_, -85);
  clonerModeProp->setDisplayLabel(QStringLiteral("Mode"));
  clonerGroup.addProperty(clonerModeProp);
  auto clonerCloneCountProp =
      makeProp(QStringLiteral("component.cloner.cloneCount"),
                PropertyType::Integer, impl_->clonerCloneCount_, -80);
  clonerCloneCountProp->setDisplayLabel(QStringLiteral("Count"));
  clonerCloneCountProp->setHardRange(1.0, 256.0);
  clonerCloneCountProp->setSoftRange(1.0, 32.0);
  clonerGroup.addProperty(clonerCloneCountProp);
  auto clonerOffsetXProp =
      makeProp(QStringLiteral("component.cloner.offsetX"),
               PropertyType::Float,
               static_cast<double>(impl_->clonerOffsetX_), -70);
  clonerOffsetXProp->setDisplayLabel(QStringLiteral("Offset X"));
  clonerOffsetXProp->setSoftRange(-1000.0, 1000.0);
  clonerGroup.addProperty(clonerOffsetXProp);
  auto clonerOffsetYProp =
      makeProp(QStringLiteral("component.cloner.offsetY"),
               PropertyType::Float,
               static_cast<double>(impl_->clonerOffsetY_), -60);
  clonerOffsetYProp->setDisplayLabel(QStringLiteral("Offset Y"));
  clonerOffsetYProp->setSoftRange(-1000.0, 1000.0);
  clonerGroup.addProperty(clonerOffsetYProp);
  auto clonerOffsetZProp =
      makeProp(QStringLiteral("component.cloner.offsetZ"), PropertyType::Float,
               static_cast<double>(impl_->clonerOffsetZ_), -59);
  clonerOffsetZProp->setDisplayLabel(QStringLiteral("Offset Z"));
  clonerGroup.addProperty(clonerOffsetZProp);
  auto clonerJitterXProp =
      makeProp(QStringLiteral("component.cloner.jitterX"), PropertyType::Float,
               static_cast<double>(impl_->clonerJitterX_), -58);
  clonerJitterXProp->setDisplayLabel(QStringLiteral("Jitter X"));
  clonerGroup.addProperty(clonerJitterXProp);
  auto clonerJitterYProp =
      makeProp(QStringLiteral("component.cloner.jitterY"), PropertyType::Float,
               static_cast<double>(impl_->clonerJitterY_), -57);
  clonerJitterYProp->setDisplayLabel(QStringLiteral("Jitter Y"));
  clonerGroup.addProperty(clonerJitterYProp);
  auto clonerJitterZProp =
      makeProp(QStringLiteral("component.cloner.jitterZ"), PropertyType::Float,
               static_cast<double>(impl_->clonerJitterZ_), -56);
  clonerJitterZProp->setDisplayLabel(QStringLiteral("Jitter Z"));
  clonerGroup.addProperty(clonerJitterZProp);
  auto clonerSeedProp =
      makeProp(QStringLiteral("component.cloner.seed"), PropertyType::Integer,
               impl_->clonerSeed_, -55);
  clonerSeedProp->setDisplayLabel(QStringLiteral("Seed"));
  clonerGroup.addProperty(clonerSeedProp);
  auto clonerColumnsProp =
      makeProp(QStringLiteral("component.cloner.columns"), PropertyType::Integer,
               impl_->clonerColumns_, -54);
  clonerColumnsProp->setDisplayLabel(QStringLiteral("Columns"));
  clonerGroup.addProperty(clonerColumnsProp);
  auto clonerRowsProp =
      makeProp(QStringLiteral("component.cloner.rows"), PropertyType::Integer,
               impl_->clonerRows_, -53);
  clonerRowsProp->setDisplayLabel(QStringLiteral("Rows"));
  clonerGroup.addProperty(clonerRowsProp);
  auto clonerDepthProp =
      makeProp(QStringLiteral("component.cloner.depth"), PropertyType::Integer,
               impl_->clonerDepth_, -52);
  clonerDepthProp->setDisplayLabel(QStringLiteral("Depth"));
  clonerGroup.addProperty(clonerDepthProp);
  auto clonerSpacingXProp =
      makeProp(QStringLiteral("component.cloner.spacingX"), PropertyType::Float,
               static_cast<double>(impl_->clonerSpacingX_), -51);
  clonerSpacingXProp->setDisplayLabel(QStringLiteral("Spacing X"));
  clonerGroup.addProperty(clonerSpacingXProp);
  auto clonerSpacingYProp =
      makeProp(QStringLiteral("component.cloner.spacingY"), PropertyType::Float,
               static_cast<double>(impl_->clonerSpacingY_), -50);
  clonerSpacingYProp->setDisplayLabel(QStringLiteral("Spacing Y"));
  clonerGroup.addProperty(clonerSpacingYProp);
  auto clonerSpacingZProp =
      makeProp(QStringLiteral("component.cloner.spacingZ"), PropertyType::Float,
               static_cast<double>(impl_->clonerSpacingZ_), -49);
  clonerSpacingZProp->setDisplayLabel(QStringLiteral("Spacing Z"));
  clonerGroup.addProperty(clonerSpacingZProp);
  auto clonerRadialCountProp =
      makeProp(QStringLiteral("component.cloner.radialCount"), PropertyType::Integer,
               impl_->clonerRadialCount_, -48);
  clonerRadialCountProp->setDisplayLabel(QStringLiteral("Count"));
  clonerGroup.addProperty(clonerRadialCountProp);
  auto clonerRadiusProp =
      makeProp(QStringLiteral("component.cloner.radius"), PropertyType::Float,
               static_cast<double>(impl_->clonerRadius_), -47);
  clonerRadiusProp->setDisplayLabel(QStringLiteral("Radius"));
  clonerGroup.addProperty(clonerRadiusProp);
  auto clonerStartAngleProp =
      makeProp(QStringLiteral("component.cloner.startAngle"), PropertyType::Float,
               static_cast<double>(impl_->clonerStartAngle_), -46);
  clonerStartAngleProp->setDisplayLabel(QStringLiteral("Start Angle"));
  clonerGroup.addProperty(clonerStartAngleProp);
  auto clonerEndAngleProp =
      makeProp(QStringLiteral("component.cloner.endAngle"), PropertyType::Float,
               static_cast<double>(impl_->clonerEndAngle_), -45);
  clonerEndAngleProp->setDisplayLabel(QStringLiteral("End Angle"));
  clonerGroup.addProperty(clonerEndAngleProp);
  auto clonerRotationStepProp =
      makeProp(QStringLiteral("component.cloner.rotationStep"), PropertyType::Float,
               static_cast<double>(impl_->clonerRotationStep_), -44);
  clonerRotationStepProp->setDisplayLabel(QStringLiteral("Rotation Step"));
  clonerGroup.addProperty(clonerRotationStepProp);
  auto clonerOpacityDecayProp =
      makeProp(QStringLiteral("component.cloner.opacityDecay"), PropertyType::Float,
               static_cast<double>(impl_->clonerOpacityDecay_), -43);
  clonerOpacityDecayProp->setDisplayLabel(QStringLiteral("Opacity Decay"));
  clonerGroup.addProperty(clonerOpacityDecayProp);
  for (int transformIndex = 0;
       transformIndex < static_cast<int>(impl_->clonerTransforms_.size());
       ++transformIndex) {
    const auto &op = impl_->clonerTransforms_[static_cast<size_t>(transformIndex)];
    const QString prefix =
        QStringLiteral("component.cloner.transforms.%1.").arg(transformIndex);
    const QString title = op.name.trimmed().isEmpty()
                              ? QStringLiteral("Transform %1").arg(transformIndex + 1)
                              : op.name.trimmed();
    auto nameProp = makeProp(prefix + QStringLiteral("name"),
                             PropertyType::String, title, -42 - transformIndex * 10);
    nameProp->setDisplayLabel(QStringLiteral("Name"));
    clonerGroup.addProperty(nameProp);
    auto enabledProp = makeProp(prefix + QStringLiteral("enabled"),
                                PropertyType::Boolean, op.enabled,
                                -41 - transformIndex * 10);
    enabledProp->setDisplayLabel(QStringLiteral("Enabled"));
    clonerGroup.addProperty(enabledProp);
    auto posXProp = makeProp(prefix + QStringLiteral("positionX"),
                             PropertyType::Float,
                             static_cast<double>(op.position.x()),
                             -40 - transformIndex * 10);
    posXProp->setDisplayLabel(QStringLiteral("Position X"));
    clonerGroup.addProperty(posXProp);
    auto posYProp = makeProp(prefix + QStringLiteral("positionY"),
                             PropertyType::Float,
                             static_cast<double>(op.position.y()),
                             -39 - transformIndex * 10);
    posYProp->setDisplayLabel(QStringLiteral("Position Y"));
    clonerGroup.addProperty(posYProp);
    auto posZProp = makeProp(prefix + QStringLiteral("positionZ"),
                             PropertyType::Float,
                             static_cast<double>(op.position.z()),
                             -38 - transformIndex * 10);
    posZProp->setDisplayLabel(QStringLiteral("Position Z"));
    clonerGroup.addProperty(posZProp);
    auto rotXProp = makeProp(prefix + QStringLiteral("rotationX"),
                             PropertyType::Float,
                             static_cast<double>(op.rotation.x()),
                             -37 - transformIndex * 10);
    rotXProp->setDisplayLabel(QStringLiteral("Rotation X"));
    clonerGroup.addProperty(rotXProp);
    auto rotYProp = makeProp(prefix + QStringLiteral("rotationY"),
                             PropertyType::Float,
                             static_cast<double>(op.rotation.y()),
                             -36 - transformIndex * 10);
    rotYProp->setDisplayLabel(QStringLiteral("Rotation Y"));
    clonerGroup.addProperty(rotYProp);
    auto rotZProp = makeProp(prefix + QStringLiteral("rotationZ"),
                             PropertyType::Float,
                             static_cast<double>(op.rotation.z()),
                             -35 - transformIndex * 10);
    rotZProp->setDisplayLabel(QStringLiteral("Rotation Z"));
    clonerGroup.addProperty(rotZProp);
    auto scaleXProp = makeProp(prefix + QStringLiteral("scaleX"),
                               PropertyType::Float,
                               static_cast<double>(op.scale.x()),
                               -34 - transformIndex * 10);
    scaleXProp->setDisplayLabel(QStringLiteral("Scale X"));
    clonerGroup.addProperty(scaleXProp);
    auto scaleYProp = makeProp(prefix + QStringLiteral("scaleY"),
                               PropertyType::Float,
                               static_cast<double>(op.scale.y()),
                               -33 - transformIndex * 10);
    scaleYProp->setDisplayLabel(QStringLiteral("Scale Y"));
    clonerGroup.addProperty(scaleYProp);
    auto scaleZProp = makeProp(prefix + QStringLiteral("scaleZ"),
                               PropertyType::Float,
                               static_cast<double>(op.scale.z()),
                               -32 - transformIndex * 10);
    scaleZProp->setDisplayLabel(QStringLiteral("Scale Z"));
    clonerGroup.addProperty(scaleZProp);
  }

  auto isAdjustmentProp =
      makeProp(QStringLiteral("layer.isAdjustment"), PropertyType::Boolean,
               isAdjustmentLayer(), -50);
  isAdjustmentProp->setTooltip(
      QStringLiteral("Apply effects to all layers below"));
  layerGroup.addProperty(isAdjustmentProp);

  std::vector<PropertyGroup> maskGroups;
  maskGroups.reserve(static_cast<size_t>(maskCount()));
  for (int maskIndex = 0; maskIndex < maskCount(); ++maskIndex) {
    const LayerMask resolvedMask = mask(maskIndex);
    PropertyGroup maskGroup(QStringLiteral("Mask %1").arg(maskIndex + 1));

    auto maskEnabledProp =
        makeProp(maskPropertyPrefix(maskIndex) + QStringLiteral(".enabled"),
                 PropertyType::Boolean, resolvedMask.isEnabled(),
                 -240 - maskIndex);
    maskEnabledProp->setAnimatable(true);
    maskEnabledProp->setDisplayLabel(QStringLiteral("Enabled"));
    maskGroup.addProperty(maskEnabledProp);

    for (int pathIndex = 0; pathIndex < resolvedMask.maskPathCount();
         ++pathIndex) {
      const MaskPath path = resolvedMask.maskPath(pathIndex);
      const QString pathPrefix = maskPathPropertyPrefix(maskIndex, pathIndex);
      const QString pathLabel =
          QStringLiteral("Path %1").arg(pathIndex + 1);

      auto closedProp = makeProp(pathPrefix + QStringLiteral(".closed"),
                                 PropertyType::Boolean, path.isClosed(),
                                 -230 - pathIndex);
      closedProp->setAnimatable(true);
      closedProp->setDisplayLabel(pathLabel + QStringLiteral(" Closed"));
      maskGroup.addProperty(closedProp);

      auto opacityProp = makeProp(pathPrefix + QStringLiteral(".opacity"),
                                  PropertyType::Float,
                                  static_cast<double>(path.opacity()),
                                  -229 - pathIndex);
      opacityProp->setAnimatable(true);
      opacityProp->setHardRange(0.0, 1.0);
      opacityProp->setSoftRange(0.0, 1.0);
      opacityProp->setStep(0.01);
      opacityProp->setDisplayLabel(pathLabel + QStringLiteral(" Opacity"));
      maskGroup.addProperty(opacityProp);

      auto featherProp = makeProp(pathPrefix + QStringLiteral(".feather"),
                                  PropertyType::Float,
                                  static_cast<double>(path.feather()),
                                  -228 - pathIndex);
      featherProp->setAnimatable(true);
      featherProp->setSoftRange(0.0, 128.0);
      featherProp->setStep(0.5);
      featherProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather"));
      maskGroup.addProperty(featherProp);

      auto fhProp = makeProp(pathPrefix + QStringLiteral(".featherHorizontal"),
                             PropertyType::Float,
                             static_cast<double>(path.featherHorizontal()),
                             -232 - pathIndex);
      fhProp->setAnimatable(true);
      fhProp->setSoftRange(0.0, 128.0);
      fhProp->setStep(0.5);
      fhProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather H"));
      maskGroup.addProperty(fhProp);

      auto fvProp = makeProp(pathPrefix + QStringLiteral(".featherVertical"),
                             PropertyType::Float,
                             static_cast<double>(path.featherVertical()),
                             -233 - pathIndex);
      fvProp->setAnimatable(true);
      fvProp->setSoftRange(0.0, 128.0);
      fvProp->setStep(0.5);
      fvProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather V"));
      maskGroup.addProperty(fvProp);

      auto fiProp = makeProp(pathPrefix + QStringLiteral(".featherInner"),
                             PropertyType::Float,
                             static_cast<double>(path.featherInner()),
                             -234 - pathIndex);
      fiProp->setAnimatable(true);
      fiProp->setSoftRange(0.0, 128.0);
      fiProp->setStep(0.5);
      fiProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather Inner"));
      maskGroup.addProperty(fiProp);

      auto foProp = makeProp(pathPrefix + QStringLiteral(".featherOuter"),
                             PropertyType::Float,
                             static_cast<double>(path.featherOuter()),
                             -235 - pathIndex);
      foProp->setAnimatable(true);
      foProp->setSoftRange(0.0, 128.0);
      foProp->setStep(0.5);
      foProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather Outer"));
      maskGroup.addProperty(foProp);

      auto expansionProp = makeProp(pathPrefix + QStringLiteral(".expansion"),
                                    PropertyType::Float,
                                    static_cast<double>(path.expansion()),
                                    -227 - pathIndex);
      expansionProp->setAnimatable(true);
      expansionProp->setSoftRange(-256.0, 256.0);
      expansionProp->setStep(0.5);
      expansionProp->setDisplayLabel(pathLabel + QStringLiteral(" Expansion"));
      maskGroup.addProperty(expansionProp);

      auto invertedProp = makeProp(pathPrefix + QStringLiteral(".inverted"),
                                   PropertyType::Boolean, path.isInverted(),
                                   -226 - pathIndex);
      invertedProp->setAnimatable(true);
      invertedProp->setDisplayLabel(pathLabel + QStringLiteral(" Inverted"));
      maskGroup.addProperty(invertedProp);

      auto modeProp = makeProp(pathPrefix + QStringLiteral(".mode"),
                               PropertyType::Integer,
                               static_cast<int>(path.mode()),
                               -225 - pathIndex);
      modeProp->setAnimatable(true);
      modeProp->setTooltip(
          QStringLiteral("0=Add,1=Subtract,2=Intersect,3=Difference"));
      modeProp->setDisplayLabel(pathLabel + QStringLiteral(" Mode"));
      maskGroup.addProperty(modeProp);
    }

    maskGroups.push_back(std::move(maskGroup));
  }

  std::vector<PropertyGroup> groups;
  groups.reserve(7 + maskGroups.size());
  groups.push_back(std::move(initialGroup));
  groups.push_back(std::move(transformGroup));
  groups.push_back(std::move(physicsGroup));
  groups.push_back(std::move(componentGroup));
  groups.push_back(std::move(layoutGroup));
  groups.push_back(std::move(clonerGroup));
  groups.push_back(std::move(layerGroup));
  for (auto &group : maskGroups) {
    groups.push_back(std::move(group));
  }
  return groups;
}

std::shared_ptr<ArtifactCore::AbstractProperty>
ArtifactAbstractLayer::getProperty(const QString &name) const {
  std::lock_guard<std::mutex> lock(impl_->propertyCacheMutex_);
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
  std::lock_guard<std::mutex> lock(impl_->propertyCacheMutex_);
  auto &cache = impl_->propertyCache_;
  auto it = cache.find(propertyPath);
  if (it == cache.end() || !it.value()) {
    it = cache.insert(propertyPath, std::make_shared<AbstractProperty>());
  }
  auto property = it.value();
  const bool hasAnimatedValue =
      property->isAnimatable() && !property->getKeyFrames().empty();
  property->setName(propertyPath);
  property->setType(type);
  if (!hasAnimatedValue && !property->hasExpression()) {
    property->setValue(value);
  }
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
  if (propertyPath == QStringLiteral("layer.selectionLocked")) {
    setSelectionLocked(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.transformLocked")) {
    setTransformLocked(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.timingLocked")) {
    setTimingLocked(value.toBool());
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
  if (propertyPath == QStringLiteral("layer.cachePolicy")) {
    setLayerCachePolicy(static_cast<LayerCachePolicy>(value.toInt()));
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
  if (propertyPath == QStringLiteral("layer.is3D")) {
    setIs3D(value.toBool());
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

  if (const auto maskAddress = parseMaskPropertyPath(propertyPath)) {
    if (maskAddress->maskIndex < 0 ||
        maskAddress->maskIndex >= impl_->maskCount()) {
      return false;
    }

    if (maskAddress->pathIndex < 0) {
      LayerMask mask = impl_->getMask(maskAddress->maskIndex);
      if (maskAddress->field == QStringLiteral("enabled")) {
        mask.setEnabled(value.toBool());
        impl_->setMask(maskAddress->maskIndex, mask);
        notifyLayerMutation(this, LayerDirtyFlag::Mask,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }

    LayerMask mask = impl_->getMask(maskAddress->maskIndex);
    if (maskAddress->pathIndex >= mask.maskPathCount()) {
      return false;
    }

    MaskPath path = mask.maskPath(maskAddress->pathIndex);
    if (maskAddress->field == QStringLiteral("closed")) {
      path.setClosed(value.toBool());
    } else if (maskAddress->field == QStringLiteral("opacity")) {
      path.setOpacity(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("feather")) {
      path.setFeather(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("featherHorizontal")) {
      path.setFeatherHorizontal(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("featherVertical")) {
      path.setFeatherVertical(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("featherInner")) {
      path.setFeatherInner(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("featherOuter")) {
      path.setFeatherOuter(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("expansion")) {
      path.setExpansion(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("inverted")) {
      path.setInverted(value.toBool());
    } else if (maskAddress->field == QStringLiteral("mode")) {
      path.setMode(static_cast<MaskMode>(value.toInt()));
    } else if (maskAddress->field == QStringLiteral("name")) {
      path.setName(UniString(value.toString().toStdString()));
    } else {
      return false;
    }
    mask.setMaskPath(maskAddress->pathIndex, path);
    impl_->setMask(maskAddress->maskIndex, mask);
    notifyLayerMutation(this, LayerDirtyFlag::Mask,
                        LayerDirtyReason::PropertyChanged);
    return true;
  }

  // Physics properties
  if (propertyPath == QStringLiteral("physics.enabled")) {
    impl_->physicsComponent_.setEnabled(value.toBool());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("physics.stiffness")) {
    impl_->physicsComponent_.settings().stiffness = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.damping")) {
    impl_->physicsComponent_.settings().damping = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.followThroughGain")) {
    impl_->physicsComponent_.settings().followThroughGain = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.gravityY")) {
    impl_->physicsComponent_.settings().gravityY = static_cast<float>(value.toDouble());
    impl_->physicsComponent_.reset();
    return true;
  }
  if (propertyPath == QStringLiteral("physics.linearDamping")) {
    impl_->physicsComponent_.settings().linearDamping = static_cast<float>(value.toDouble());
    impl_->physicsComponent_.reset();
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wiggleFreq")) {
    impl_->physicsComponent_.settings().wiggleFreq = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wiggleAmp")) {
    impl_->physicsComponent_.settings().wiggleAmp = static_cast<float>(value.toDouble());
    return true;
  }

  if (propertyPath == QStringLiteral("component.script.enabled")) {
    impl_->scriptComponentEnabled_ = value.toBool();
    Q_EMIT changed();
    return true;
  }
    if (propertyPath == QStringLiteral("component.cloner.transforms.add")) {
      ClonerTransformOperation op;
      op.name = QStringLiteral("Transform %1")
                    .arg(static_cast<int>(impl_->clonerTransforms_.size()) + 1);
      impl_->clonerTransforms_.push_back(op);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.remove")) {
      const int index = value.toInt();
      if (index >= 0 &&
          index < static_cast<int>(impl_->clonerTransforms_.size())) {
        impl_->clonerTransforms_.erase(
            impl_->clonerTransforms_.begin() + index);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.duplicate")) {
      const int index = value.toInt();
      if (index >= 0 &&
          index < static_cast<int>(impl_->clonerTransforms_.size())) {
        auto copy = impl_->clonerTransforms_[static_cast<size_t>(index)];
        copy.name = copy.name.trimmed().isEmpty()
                        ? QStringLiteral("Transform %1")
                              .arg(static_cast<int>(impl_->clonerTransforms_.size()) + 1)
                        : copy.name + QStringLiteral(" Copy");
        impl_->clonerTransforms_.insert(
            impl_->clonerTransforms_.begin() + index + 1, copy);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.moveUp")) {
      const int index = value.toInt();
      if (index > 0 &&
          index < static_cast<int>(impl_->clonerTransforms_.size())) {
        std::swap(impl_->clonerTransforms_[static_cast<size_t>(index)],
                  impl_->clonerTransforms_[static_cast<size_t>(index - 1)]);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.moveDown")) {
      const int index = value.toInt();
      if (index >= 0 &&
          index + 1 < static_cast<int>(impl_->clonerTransforms_.size())) {
        std::swap(impl_->clonerTransforms_[static_cast<size_t>(index)],
                  impl_->clonerTransforms_[static_cast<size_t>(index + 1)]);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (const auto clonerTransformAddress =
            parseClonerTransformPropertyPath(propertyPath)) {
      if (clonerTransformAddress->index < 0 ||
          clonerTransformAddress->index >=
              static_cast<int>(impl_->clonerTransforms_.size())) {
        return false;
      }
      auto &op = impl_->clonerTransforms_[static_cast<size_t>(
          clonerTransformAddress->index)];
      const QString field = clonerTransformAddress->field;
      if (field == QStringLiteral("name")) {
        op.name = value.toString();
      } else if (field == QStringLiteral("enabled")) {
        op.enabled = value.toBool();
      } else if (field == QStringLiteral("positionX")) {
        op.position.setX(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("positionY")) {
        op.position.setY(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("positionZ")) {
        op.position.setZ(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("rotationX")) {
        op.rotation.setX(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("rotationY")) {
        op.rotation.setY(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("rotationZ")) {
        op.rotation.setZ(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("scaleX")) {
        op.scale.setX(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("scaleY")) {
        op.scale.setY(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("scaleZ")) {
        op.scale.setZ(static_cast<float>(value.toDouble()));
      } else {
        return false;
      }
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.enabled")) {
      impl_->layoutComponentEnabled_ = value.toBool();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.mode")) {
      impl_->layoutMode_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.anchorMode")) {
      impl_->layoutAnchorMode_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.horizontalPin")) {
      impl_->layoutHorizontalPin_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.verticalPin")) {
      impl_->layoutVerticalPin_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.scaleMode")) {
      impl_->layoutScaleMode_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaEnabled")) {
      impl_->layoutSafeAreaEnabled_ = value.toBool();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaPaddingX")) {
      impl_->layoutSafeAreaPaddingX_ = static_cast<float>(value.toDouble());
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaPaddingY")) {
      impl_->layoutSafeAreaPaddingY_ = static_cast<float>(value.toDouble());
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.stackDirection")) {
      impl_->layoutStackDirection_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.gap")) {
      impl_->layoutGap_ = static_cast<float>(value.toDouble());
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.maxPerRow")) {
      impl_->layoutMaxPerRow_ = std::max(0, value.toInt());
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.enabled")) {
      impl_->clonerComponentEnabled_ = value.toBool();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.mode")) {
      impl_->clonerMode_ = value.toInt();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.cloneCount")) {
      impl_->clonerCloneCount_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
  if (propertyPath == QStringLiteral("component.cloner.offsetX")) {
    impl_->clonerOffsetX_ = static_cast<float>(value.toDouble());
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    return true;
  }
    if (propertyPath == QStringLiteral("component.cloner.offsetY")) {
      impl_->clonerOffsetY_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.offsetZ")) {
      impl_->clonerOffsetZ_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterX")) {
      impl_->clonerJitterX_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterY")) {
      impl_->clonerJitterY_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterZ")) {
      impl_->clonerJitterZ_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.seed")) {
      impl_->clonerSeed_ = value.toInt();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.columns")) {
      impl_->clonerColumns_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.rows")) {
      impl_->clonerRows_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.depth")) {
      impl_->clonerDepth_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.spacingX")) {
      impl_->clonerSpacingX_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.spacingY")) {
      impl_->clonerSpacingY_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.spacingZ")) {
      impl_->clonerSpacingZ_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.radialCount")) {
      impl_->clonerRadialCount_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.radius")) {
      impl_->clonerRadius_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.startAngle")) {
      impl_->clonerStartAngle_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.endAngle")) {
      impl_->clonerEndAngle_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.rotationStep")) {
      impl_->clonerRotationStep_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.opacityDecay")) {
      impl_->clonerOpacityDecay_ = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
  auto &t3 = transform3D();
  const RationalTime currentTime = currentTimelineTime(this);

  if (propertyPath == QStringLiteral("transform.initialRotation")) {
    t3.setInitialRotation(currentTime, static_cast<float>(value.toDouble()));
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
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
    t3.setScale(currentTime, value.toDouble(), t3.scaleY());
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.scale.y")) {
    t3.setScale(currentTime, t3.scaleX(), value.toDouble());
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.rotation")) {
    if (t3.isAutoOrient()) {
      t3.setAutoOrient(false);
    }
    t3.setRotation(currentTime, value.toDouble());
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.autoOrient")) {
    t3.setAutoOrient(value.toBool());
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
    const int width = std::max(1, value.toInt());
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
    const int height = std::max(1, value.toInt());
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
  LayerMask resolved = impl_->getMask(index);
  applyMaskPropertyState(this, index, resolved);
  return resolved;
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

MatteStack ArtifactAbstractLayer::buildMatteStack() const {
    MatteStack stack;
    for (const auto& ref : impl_->mattes_) {
        if (ref.enabled && !ref.sourceLayerId.isNil()) {
            stack.addNode(ref.toCoreMatteNode());
        }
    }
    return stack;
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

} // namespace Artifact


