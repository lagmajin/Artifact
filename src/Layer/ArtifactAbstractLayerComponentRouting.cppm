module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>
#include <QDebug>
#include <QImage>
#include <QPointF>
#include <QSet>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QVector>
#include <QVector3D>
#include <QVector4D>
#include <QString>
#include <QStringList>
#include <wobjectcpp.h>
#include <wobjectimpl.h>

// JSON and QVariant used in serialization
#include <QColor>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <limits>

module Artifact.Layer.Abstract;

import :Impl;
import Memory.SharedPtr;
import Utils;
import Layer.State;
import Animation.Transform2D;
import Animation.Dynamics;
import Frame.Position;
import Time.Rational;
import Frame.Rate;
import Artifact.Render.IRenderer;
import Animation.Value;
import Transform.Hlper;

import Time.TimeRemap;

import Artifact.Layer.Settings;
import Artifact.Layer.Physics;
import Artifact.Layer.Component.System;
import Artifact.Layer.Modifier;
import Artifact.Layer.Matte;
import Artifact.Layer.MaskMatteState;
import Artifact.Layer.Serialization;
import Artifact.Layer.ThumbnailSupport;
import Color.Float;
import Geometry.Fracture;
import Physics.Fluid;
import Physics.SoftBody;
import Physics2D;
import Layer.Matte;
import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Render.ROI;
import Artifact.Effect.ImplBase;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.Keying.LumaKey;
import Artifact.Effect.Keying.DifferenceKey;
import Artifact.Effect.Rasterizer.DifferenceMatte;
import Artifact.Effect.Rasterizer.PosterizeTime;
import Artifact.Effect.Keying.IBKKeyer;
import Artifact.Effect.Generator.Cloner;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Container.NamedVector;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import Graphics.ParticleData;
import Property.Abstract;
import Property.Group;
import Property.SerializationBridge;
import Script.Expression.Evaluator;
import Audio.Modulation.Router;
import Artifact.Event.Types;
import Event.Bus;
import Artifact.Layer.RuntimeSupport;
import Artifact.Layer.RuntimeRenderSupport;
import Artifact.Layer.FluidRuntimeState;
import Artifact.Layer.Abstract.Utilities;
import Artifact.Layer.PhysicsBridge;

namespace Artifact {
using namespace ArtifactCore;
using LayerAbstractUtilities::finiteClampedValue;
using namespace LayerAbstractUtilities;
namespace LayerPhysics = LayerPhysicsBridge;

void notifyLayerMutation(ArtifactAbstractLayer* layer, LayerDirtyFlag flag,
                         LayerDirtyReason reason);
QVariant evaluateAnimatedPropertyValue(
    const ArtifactCore::AbstractProperty& property,
    const ArtifactCore::RationalTime& time);
float applyAutomationClipOverlay(const ArtifactAbstractLayer* layer,
                                 const QString& targetPath, float baseValue,
                                 double parentSeconds, double freeSeconds);
void applyCompositionTransformFields(const ArtifactAbstractLayer* layer,
                                     double& positionX, double& positionY,
                                     double& scaleX, double& scaleY);
double effectiveLayerFrameRate(const ArtifactAbstractLayer* layer);
int64_t currentTimelineFrame(const ArtifactAbstractLayer* layer);
RationalTime currentTimelineTime(const ArtifactAbstractLayer* layer);
RationalTime timelineTimeForFramePosition(const ArtifactAbstractLayer* layer,
                                          const FramePosition& position);
bool configureLiquidContainerPolygon(
    const ArtifactAbstractLayer* layer, ArtifactCore::LiquidSolver2D& liquid,
    int requestedOpeningEdge = -1,
    NamedVector<QPointF>* configuredPoints = nullptr,
    std::size_t* configuredOpeningEdge = nullptr);
QRectF layerCollisionLocalBounds(const ArtifactAbstractLayer* layer);
bool resolveLiquidPointAgainstCollisionLayer(
    const ArtifactAbstractLayer* layer, int64_t frameNumber,
    float particleRadius, float previousWorldX, float previousWorldY,
    float& worldX, float& worldY, float& worldVx, float& worldVy,
    float& collisionImpact);
QJsonObject serializeLayerModulationRouter(
    const Audio::Modulation::ModulationRouter& router);
void restoreLayerModulationRouter(
    const QJsonObject& object, Audio::Modulation::ModulationRouter& router);
QJsonObject serializeLayerTransform(
    const ArtifactCore::AnimatableTransform3D& transform);
void restoreLayerTransform(const QJsonObject& object,
                           ArtifactCore::AnimatableTransform3D& transform,
                           double frameRate);

bool ArtifactAbstractLayer::setComponentDescriptorPropertyValue(
    const QString &propertyPath, const QVariant &value) {
  if (propertyPath == QStringLiteral("component.script.enabled")) {
    impl_->scriptComponentEnabled_ = value.toBool();
    Q_EMIT changed();
    return true;
  }
    if (propertyPath == QStringLiteral("component.generators.add")) {
      const QString requestedType = value.toString().trimmed().toLower();
      LayerGeneratorDescriptor descriptor;
      const int nextIndex =
          static_cast<int>(impl_->extraGeneratorDescriptors_.count()) + 1;
      descriptor.generatorId =
          QStringLiteral("generator.extra.%1").arg(nextIndex);
      descriptor.enabled = true;
      descriptor.order = 1000 + nextIndex * 10;
      if (requestedType == QStringLiteral("radial")) {
        descriptor.typeId = QStringLiteral("artifact.generator.cloner.radial");
        descriptor.settings[QStringLiteral("radialCount")] = 8;
        descriptor.settings[QStringLiteral("radius")] = 160.0;
        descriptor.settings[QStringLiteral("startAngle")] = 0.0;
        descriptor.settings[QStringLiteral("endAngle")] = 360.0;
      } else {
        descriptor.typeId = requestedType == QStringLiteral("matrix")
            ? QStringLiteral("artifact.generator.cloner.matrix")
            : QStringLiteral("artifact.generator.cloner.grid");
        descriptor.settings[QStringLiteral("columns")] = 3;
        descriptor.settings[QStringLiteral("rows")] = 3;
        descriptor.settings[QStringLiteral("depth")] = 1;
        descriptor.settings[QStringLiteral("spacingX")] = 160.0;
        descriptor.settings[QStringLiteral("spacingY")] = 48.0;
        descriptor.settings[QStringLiteral("spacingZ")] = 0.0;
      }
      descriptor.settings[QStringLiteral("timeOffsetStep")] = 0.0;
      descriptor.settings[QStringLiteral("sequenceEnabled")] = false;
      descriptor.settings[QStringLiteral("sequenceRate")] = 8.0;
      descriptor.settings[QStringLiteral("sequenceSoftness")] = 1.0;
      impl_->extraGeneratorDescriptors_.add(std::move(descriptor));
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.generators.removeLast")) {
      if (impl_->extraGeneratorDescriptors_.isEmpty()) {
        return false;
      }
      impl_->extraGeneratorDescriptors_.takeLast();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.generators.remove")) {
      const QString generatorId = value.toString().trimmed();
      if (generatorId.isEmpty() ||
          generatorId == QStringLiteral("generator.compat.cloner.0")) {
        return false;
      }
      for (std::size_t i = 0; i < impl_->extraGeneratorDescriptors_.count(); ++i) {
        const auto* generator = impl_->extraGeneratorDescriptors_.at(i);
        if (!generator || generator->generatorId != generatorId) {
          continue;
        }
        impl_->extraGeneratorDescriptors_.removeAt(i);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.generators.moveUp")) {
      const QString generatorId = value.toString().trimmed();
      for (std::size_t i = 1; i < impl_->extraGeneratorDescriptors_.count(); ++i) {
        auto* current = impl_->extraGeneratorDescriptors_.at(i);
        auto* previous = impl_->extraGeneratorDescriptors_.at(i - 1);
        if (!current || !previous || current->generatorId != generatorId) {
          continue;
        }
        std::swap(*current, *previous);
        previous->order = 1000 + static_cast<int>((i - 1) + 1) * 10;
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.generators.moveDown")) {
      if (impl_->extraGeneratorDescriptors_.count() < 2) {
        return false;
      }
      const QString generatorId = value.toString().trimmed();
      for (std::size_t i = 0; i + 1 < impl_->extraGeneratorDescriptors_.count(); ++i) {
        auto* current = impl_->extraGeneratorDescriptors_.at(i);
        auto* next = impl_->extraGeneratorDescriptors_.at(i + 1);
        if (!current || !next || current->generatorId != generatorId) {
          continue;
        }
        std::swap(*current, *next);
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        next->order = 1000 + static_cast<int>((i + 1) + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath.startsWith(QStringLiteral("component.generators."))) {
      const QStringList parts = propertyPath.split(QLatin1Char('.'));
      if (parts.size() == 4) {
        bool ok = false;
        const int generatorIndex = parts[2].toInt(&ok);
        if (!ok || generatorIndex < 0 ||
            generatorIndex >=
                static_cast<int>(impl_->extraGeneratorDescriptors_.count())) {
          return false;
        }
        auto* descriptor = impl_->extraGeneratorDescriptors_.at(
            static_cast<std::size_t>(generatorIndex));
        if (!descriptor) {
          return false;
        }
        const QString field = parts[3];
        auto setSetting = [&](const QString& key, const QJsonValue& settingValue) {
          descriptor->settings.insert(key, settingValue);
          notifyLayerMutation(this, LayerDirtyFlag::Effect,
                              LayerDirtyReason::PropertyChanged);
          return true;
        };
        auto setFiniteOnlySetting = [&](const QString &key, double raw,
                                       double fallback) {
          const double existing =
              descriptor->settings.value(key).toDouble(fallback);
          const double safeExisting =
              std::isfinite(existing) ? existing : fallback;
          return setSetting(key, std::isfinite(raw) ? raw : safeExisting);
        };
        auto setFiniteSetting = [&](const QString &key, double raw,
                                    double fallback, double minimum,
                                    double maximum) {
          return setSetting(
              key, finiteClampedValue(
                      raw, descriptor->settings.value(key).toDouble(fallback),
                      minimum, maximum));
        };
        if (field == QStringLiteral("enabled")) {
          descriptor->enabled = value.toBool();
          notifyLayerMutation(this, LayerDirtyFlag::Effect,
                              LayerDirtyReason::PropertyChanged);
          return true;
        }
        if (field == QStringLiteral("type")) {
          return false;
        }
        if (field == QStringLiteral("timeOffsetStep")) {
          return setSetting(
              field, finiteClampedValue(
                         value.toDouble(),
                         descriptor->settings.value(field).toDouble(0.0),
                         -10000.0, 10000.0));
        }
        if (field == QStringLiteral("sequenceEnabled")) {
          return setSetting(field, value.toBool());
        }
        if (field == QStringLiteral("sequenceRate")) {
          return setSetting(
              field, finiteClampedValue(
                         value.toDouble(),
                         descriptor->settings.value(field).toDouble(8.0),
                         0.1, 240.0));
        }
        if (field == QStringLiteral("sequenceSoftness")) {
          return setSetting(
              field, finiteClampedValue(
                         value.toDouble(),
                         descriptor->settings.value(field).toDouble(1.0),
                         0.1, 32.0));
        }
        if (field == QStringLiteral("columns") ||
            field == QStringLiteral("rows") ||
            field == QStringLiteral("depth") ||
            field == QStringLiteral("radialCount")) {
          return setSetting(field, std::max(1, value.toInt()));
        }
        if (field == QStringLiteral("spacingX") ||
            field == QStringLiteral("spacingY") ||
            field == QStringLiteral("spacingZ") ||
            field == QStringLiteral("radius") ||
            field == QStringLiteral("startAngle") ||
            field == QStringLiteral("endAngle")) {
          const double fallback =
              descriptor->settings.value(field).toDouble(0.0);
          const bool angle = field == QStringLiteral("startAngle") ||
                             field == QStringLiteral("endAngle");
          return setSetting(
              field, finiteClampedValue(value.toDouble(), fallback,
                                        angle ? -360000.0 : -100000.0,
                                        angle ? 360000.0 : 100000.0));
        }
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.fields.add")) {
      const QString requestedType = value.toString().trimmed().toLower();
      LayerFieldDescriptor descriptor;
      const int nextIndex =
          static_cast<int>(impl_->extraFieldDescriptors_.count()) + 1;
      descriptor.fieldId = QStringLiteral("field.extra.%1").arg(nextIndex);
      descriptor.enabled = true;
      descriptor.order = 1000 + nextIndex * 10;
      descriptor.blendMode = QStringLiteral("normal");
      descriptor.strength = 1.0f;
      descriptor.invert = false;
      descriptor.settings[QStringLiteral("centerX")] = 0.0;
      descriptor.settings[QStringLiteral("centerY")] = 0.0;
      descriptor.settings[QStringLiteral("angle")] = 0.0;
      if (requestedType == QStringLiteral("sphere")) {
        descriptor.typeId = QStringLiteral("artifact.field.sphere");
        descriptor.settings[QStringLiteral("radius")] = 160.0;
        descriptor.settings[QStringLiteral("falloffWidth")] = 40.0;
      } else if (requestedType == QStringLiteral("box")) {
        descriptor.typeId = QStringLiteral("artifact.field.box");
        descriptor.settings[QStringLiteral("halfX")] = 120.0;
        descriptor.settings[QStringLiteral("halfY")] = 120.0;
        descriptor.settings[QStringLiteral("halfZ")] = 120.0;
        descriptor.settings[QStringLiteral("falloffWidth")] = 40.0;
      } else if (requestedType == QStringLiteral("linear")) {
        descriptor.typeId = QStringLiteral("artifact.field.linear");
        descriptor.settings[QStringLiteral("length")] = 320.0;
        descriptor.settings[QStringLiteral("useSmoothstep")] = true;
      } else if (requestedType == QStringLiteral("radial")) {
        descriptor.typeId = QStringLiteral("artifact.field.radial");
        descriptor.settings[QStringLiteral("innerRadius")] = 0.0;
        descriptor.settings[QStringLiteral("outerRadius")] = 160.0;
      } else if (requestedType == QStringLiteral("noise")) {
        descriptor.typeId = QStringLiteral("artifact.field.noise");
        descriptor.settings[QStringLiteral("scale")] = 120.0;
        descriptor.settings[QStringLiteral("amplitude")] = 1.0;
        descriptor.settings[QStringLiteral("octaves")] = 3;
      } else {
        descriptor.typeId = QStringLiteral("artifact.field.solid");
        descriptor.settings[QStringLiteral("value")] = 1.0;
      }
      impl_->extraFieldDescriptors_.add(std::move(descriptor));
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fields.remove")) {
      const QString fieldId = value.toString().trimmed();
      for (std::size_t i = 0; i < impl_->extraFieldDescriptors_.count(); ++i) {
        const auto* field = impl_->extraFieldDescriptors_.at(i);
        if (!field || field->fieldId != fieldId) {
          continue;
        }
        impl_->extraFieldDescriptors_.removeAt(i);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.fields.moveUp")) {
      const QString fieldId = value.toString().trimmed();
      for (std::size_t i = 1; i < impl_->extraFieldDescriptors_.count(); ++i) {
        auto* current = impl_->extraFieldDescriptors_.at(i);
        auto* previous = impl_->extraFieldDescriptors_.at(i - 1);
        if (!current || !previous || current->fieldId != fieldId) {
          continue;
        }
        std::swap(*current, *previous);
        previous->order = 1000 + static_cast<int>((i - 1) + 1) * 10;
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.fields.moveDown")) {
      if (impl_->extraFieldDescriptors_.count() < 2) {
        return false;
      }
      const QString fieldId = value.toString().trimmed();
      for (std::size_t i = 0; i + 1 < impl_->extraFieldDescriptors_.count(); ++i) {
        auto* current = impl_->extraFieldDescriptors_.at(i);
        auto* next = impl_->extraFieldDescriptors_.at(i + 1);
        if (!current || !next || current->fieldId != fieldId) {
          continue;
        }
        std::swap(*current, *next);
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        next->order = 1000 + static_cast<int>((i + 1) + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath.startsWith(QStringLiteral("component.fields."))) {
      const QStringList parts = propertyPath.split(QLatin1Char('.'));
      if (parts.size() == 4) {
        bool ok = false;
        const int fieldIndex = parts[2].toInt(&ok);
        if (!ok || fieldIndex < 0 ||
            fieldIndex >= static_cast<int>(impl_->extraFieldDescriptors_.count())) {
          return false;
        }
        auto* descriptor =
            impl_->extraFieldDescriptors_.at(static_cast<std::size_t>(fieldIndex));
        if (!descriptor) {
          return false;
        }
        const QString fieldName = parts[3];
        auto setSetting = [&](const QString& key, const QJsonValue& settingValue) {
          descriptor->settings.insert(key, settingValue);
          notifyLayerMutation(this, LayerDirtyFlag::Effect,
                              LayerDirtyReason::PropertyChanged);
          return true;
        };
        auto setFiniteOnlySetting = [&](const QString &key, double raw,
                                       double fallback) {
          const double existing =
              descriptor->settings.value(key).toDouble(fallback);
          const double safeExisting =
              std::isfinite(existing) ? existing : fallback;
          return setSetting(key, std::isfinite(raw) ? raw : safeExisting);
        };
        if (fieldName == QStringLiteral("enabled")) {
          descriptor->enabled = value.toBool();
        } else if (fieldName == QStringLiteral("strength")) {
          const double strength = value.toDouble();
          descriptor->strength = std::isfinite(strength)
                                     ? static_cast<float>(
                                           std::clamp(strength, 0.0, 1.0))
                                     : 1.0f;
        } else if (fieldName == QStringLiteral("invert")) {
          descriptor->invert = value.toBool();
        } else if (fieldName == QStringLiteral("value") ||
                   fieldName == QStringLiteral("centerX") ||
                   fieldName == QStringLiteral("centerY") ||
                   fieldName == QStringLiteral("angle") ||
                   fieldName == QStringLiteral("radius") ||
                   fieldName == QStringLiteral("falloffWidth") ||
                   fieldName == QStringLiteral("halfX") ||
                   fieldName == QStringLiteral("halfY") ||
                   fieldName == QStringLiteral("halfZ") ||
                   fieldName == QStringLiteral("length") ||
                   fieldName == QStringLiteral("innerRadius") ||
                   fieldName == QStringLiteral("outerRadius") ||
                   fieldName == QStringLiteral("scale") ||
                   fieldName == QStringLiteral("amplitude")) {
          return setFiniteOnlySetting(fieldName, value.toDouble(), 0.0);
        } else if (fieldName == QStringLiteral("octaves")) {
          return setSetting(fieldName, std::max(1, value.toInt()));
        } else if (fieldName == QStringLiteral("useSmoothstep")) {
          return setSetting(fieldName, value.toBool());
        } else if (fieldName == QStringLiteral("summary")) {
          return true;
        } else {
          return false;
        }
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.cloneModifiers.add")) {
      const QString requestedType = value.toString().trimmed().toLower();
      LayerModifierDescriptor descriptor;
      const int nextIndex =
          static_cast<int>(impl_->extraCloneModifierDescriptors_.count()) + 1;
      descriptor.modifierId =
          QStringLiteral("modifier.extra.%1").arg(nextIndex);
      descriptor.enabled = true;
      descriptor.order = 1000 + nextIndex * 10;
      if (requestedType == QStringLiteral("sequence")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.sequence");
        descriptor.settings[QStringLiteral("rate")] = 8.0;
        descriptor.settings[QStringLiteral("softness")] = 1.0;
      } else if (requestedType == QStringLiteral("plain")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.plain");
        descriptor.settings[QStringLiteral("positionX")] = 0.0;
        descriptor.settings[QStringLiteral("positionY")] = 0.0;
        descriptor.settings[QStringLiteral("positionZ")] = 0.0;
        descriptor.settings[QStringLiteral("rotationX")] = 0.0;
        descriptor.settings[QStringLiteral("rotationY")] = 0.0;
        descriptor.settings[QStringLiteral("rotationZ")] = 0.0;
        descriptor.settings[QStringLiteral("scaleX")] = 1.0;
        descriptor.settings[QStringLiteral("scaleY")] = 1.0;
        descriptor.settings[QStringLiteral("scaleZ")] = 1.0;
        descriptor.settings[QStringLiteral("strength")] = 1.0;
      } else if (requestedType == QStringLiteral("random")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.random");
        descriptor.settings[QStringLiteral("seed")] = 1;
        descriptor.settings[QStringLiteral("positionX")] = 0.0;
        descriptor.settings[QStringLiteral("positionY")] = 0.0;
        descriptor.settings[QStringLiteral("positionZ")] = 0.0;
        descriptor.settings[QStringLiteral("rotationX")] = 0.0;
        descriptor.settings[QStringLiteral("rotationY")] = 0.0;
        descriptor.settings[QStringLiteral("rotationZ")] = 0.0;
        descriptor.settings[QStringLiteral("scaleVariance")] = 0.0;
        descriptor.settings[QStringLiteral("strength")] = 1.0;
      } else if (requestedType == QStringLiteral("step")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.step");
        descriptor.settings[QStringLiteral("positionX")] = 0.0;
        descriptor.settings[QStringLiteral("positionY")] = 0.0;
        descriptor.settings[QStringLiteral("positionZ")] = 0.0;
        descriptor.settings[QStringLiteral("rotationX")] = 0.0;
        descriptor.settings[QStringLiteral("rotationY")] = 0.0;
        descriptor.settings[QStringLiteral("rotationZ")] = 0.0;
        descriptor.settings[QStringLiteral("scaleX")] = 1.0;
        descriptor.settings[QStringLiteral("scaleY")] = 1.0;
        descriptor.settings[QStringLiteral("scaleZ")] = 1.0;
        descriptor.settings[QStringLiteral("strength")] = 1.0;
      } else if (requestedType == QStringLiteral("formula")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.formula");
        descriptor.settings[QStringLiteral("amplitudeX")] = 0.0;
        descriptor.settings[QStringLiteral("amplitudeY")] = 0.0;
        descriptor.settings[QStringLiteral("amplitudeZ")] = 0.0;
        descriptor.settings[QStringLiteral("frequency")] = 1.0;
        descriptor.settings[QStringLiteral("phase")] = 0.0;
        descriptor.settings[QStringLiteral("indexPhase")] = 0.0;
        descriptor.settings[QStringLiteral("strength")] = 1.0;
      } else if (requestedType == QStringLiteral("spline")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.spline");
        descriptor.settings[QStringLiteral("startX")] = 0.0;
        descriptor.settings[QStringLiteral("startY")] = 0.0;
        descriptor.settings[QStringLiteral("startZ")] = 0.0;
        descriptor.settings[QStringLiteral("controlX")] = 0.0;
        descriptor.settings[QStringLiteral("controlY")] = 0.0;
        descriptor.settings[QStringLiteral("controlZ")] = 0.0;
        descriptor.settings[QStringLiteral("endX")] = 0.0;
        descriptor.settings[QStringLiteral("endY")] = 0.0;
        descriptor.settings[QStringLiteral("endZ")] = 0.0;
        descriptor.settings[QStringLiteral("indexScale")] = 0.1;
        descriptor.settings[QStringLiteral("strength")] = 1.0;
      } else {
        descriptor.typeId = QStringLiteral("artifact.modifier.time-offset");
        descriptor.settings[QStringLiteral("step")] = 0.0;
      }
      impl_->extraCloneModifierDescriptors_.add(std::move(descriptor));
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloneModifiers.remove")) {
      const QString modifierId = value.toString().trimmed();
      for (std::size_t i = 0; i < impl_->extraCloneModifierDescriptors_.count(); ++i) {
        const auto* modifier = impl_->extraCloneModifierDescriptors_.at(i);
        if (!modifier || modifier->modifierId != modifierId) {
          continue;
        }
        impl_->extraCloneModifierDescriptors_.removeAt(i);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.cloneModifiers.moveUp")) {
      const QString modifierId = value.toString().trimmed();
      for (std::size_t i = 1; i < impl_->extraCloneModifierDescriptors_.count(); ++i) {
        auto* current = impl_->extraCloneModifierDescriptors_.at(i);
        auto* previous = impl_->extraCloneModifierDescriptors_.at(i - 1);
        if (!current || !previous || current->modifierId != modifierId) {
          continue;
        }
        std::swap(*current, *previous);
        previous->order = 1000 + static_cast<int>((i - 1) + 1) * 10;
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.cloneModifiers.moveDown")) {
      if (impl_->extraCloneModifierDescriptors_.count() < 2) {
        return false;
      }
      const QString modifierId = value.toString().trimmed();
      for (std::size_t i = 0; i + 1 < impl_->extraCloneModifierDescriptors_.count(); ++i) {
        auto* current = impl_->extraCloneModifierDescriptors_.at(i);
        auto* next = impl_->extraCloneModifierDescriptors_.at(i + 1);
        if (!current || !next || current->modifierId != modifierId) {
          continue;
        }
        std::swap(*current, *next);
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        next->order = 1000 + static_cast<int>((i + 1) + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath.startsWith(
            QStringLiteral("component.cloner.modifiers.compat.timeOffset."))) {
      const QString field =
          propertyPath.mid(QStringLiteral(
                               "component.cloner.modifiers.compat.timeOffset.")
                               .size());
      if (field == QStringLiteral("enabled")) {
        return true;
      }
      if (field == QStringLiteral("step")) {
        impl_->clonerTimeOffsetStep_ = finiteClampedValue(
            value.toDouble(), impl_->clonerTimeOffsetStep_, -10000.0, 10000.0);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath.startsWith(
            QStringLiteral("component.cloner.modifiers.compat.sequence."))) {
      const QString field =
          propertyPath.mid(QStringLiteral(
                               "component.cloner.modifiers.compat.sequence.")
                               .size());
      if (field == QStringLiteral("enabled")) {
        impl_->clonerSequenceEnabled_ = value.toBool();
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      if (field == QStringLiteral("rate")) {
        impl_->clonerSequenceRate_ = finiteClampedValue(
            value.toDouble(), impl_->clonerSequenceRate_, 0.1, 240.0);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      if (field == QStringLiteral("softness")) {
        impl_->clonerSequenceSoftness_ = finiteClampedValue(
            value.toDouble(), impl_->clonerSequenceSoftness_, 0.1, 32.0);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath.startsWith(QStringLiteral("component.cloneModifiers."))) {
      const QStringList parts = propertyPath.split(QLatin1Char('.'));
      if (parts.size() == 4) {
        bool ok = false;
        const int modifierIndex = parts[2].toInt(&ok);
        if (!ok || modifierIndex < 0 ||
            modifierIndex >=
                static_cast<int>(impl_->extraCloneModifierDescriptors_.count())) {
          return false;
        }
        auto* descriptor = impl_->extraCloneModifierDescriptors_.at(
            static_cast<std::size_t>(modifierIndex));
        if (!descriptor) {
          return false;
        }
        const QString field = parts[3];
        auto setSetting = [&](const QString& key, const QJsonValue& settingValue) {
          descriptor->settings.insert(key, settingValue);
          notifyLayerMutation(this, LayerDirtyFlag::Effect,
                              LayerDirtyReason::PropertyChanged);
          return true;
        };
        auto setFiniteOnlySetting = [&](const QString& key, double raw,
                                       double fallback) {
          const double existing =
              descriptor->settings.value(key).toDouble(fallback);
          const double safeExisting =
              std::isfinite(existing) ? existing : fallback;
          return setSetting(key, std::isfinite(raw) ? raw : safeExisting);
        };
        auto setFiniteSetting = [&](const QString& key, double raw,
                                   double fallback, double minimum,
                                   double maximum) {
          return setSetting(
              key, finiteClampedValue(
                      raw, descriptor->settings.value(key).toDouble(fallback),
                      minimum, maximum));
        };
        if (field == QStringLiteral("enabled")) {
          descriptor->enabled = value.toBool();
          notifyLayerMutation(this, LayerDirtyFlag::Effect,
                              LayerDirtyReason::PropertyChanged);
          return true;
        }
        if (field == QStringLiteral("step")) {
          return setFiniteSetting(field, value.toDouble(), 0.0, -10000.0,
                                  10000.0);
        }
        if (field == QStringLiteral("rate")) {
          return setFiniteSetting(field, value.toDouble(), 8.0, 0.1, 240.0);
        }
        if (field == QStringLiteral("softness")) {
          return setFiniteSetting(field, value.toDouble(), 1.0, 0.1, 32.0);
        }
        if (field == QStringLiteral("seed")) {
          return setSetting(field, value.toInt());
        }
        if (field == QStringLiteral("strength")) {
          return setFiniteSetting(field, value.toDouble(), 1.0, 0.0, 1.0);
        }
        if (field == QStringLiteral("frequency")) {
          return setFiniteSetting(field, value.toDouble(), 1.0, 0.0, 60.0);
        }
        if (field == QStringLiteral("phase") ||
            field == QStringLiteral("indexPhase")) {
          return setFiniteSetting(field, value.toDouble(), 0.0,
                                  -6.28318530718, 6.28318530718);
        }
        if (field == QStringLiteral("indexScale")) {
          return setFiniteSetting(field, value.toDouble(), 0.1, 0.0001, 1.0);
        }
        if (field == QStringLiteral("amplitudeX") ||
            field == QStringLiteral("amplitudeY") ||
            field == QStringLiteral("amplitudeZ") ||
            field == QStringLiteral("startX") ||
            field == QStringLiteral("startY") ||
            field == QStringLiteral("startZ") ||
            field == QStringLiteral("controlX") ||
            field == QStringLiteral("controlY") ||
            field == QStringLiteral("controlZ") ||
            field == QStringLiteral("endX") ||
            field == QStringLiteral("endY") ||
            field == QStringLiteral("endZ")) {
          return setFiniteSetting(field, value.toDouble(), 0.0, -10000.0,
                                  10000.0);
        }
        if (field == QStringLiteral("positionX") ||
            field == QStringLiteral("positionY") ||
            field == QStringLiteral("positionZ") ||
            field == QStringLiteral("rotationX") ||
            field == QStringLiteral("rotationY") ||
            field == QStringLiteral("rotationZ") ||
            field == QStringLiteral("scaleX") ||
            field == QStringLiteral("scaleY") ||
            field == QStringLiteral("scaleZ") ||
            field == QStringLiteral("scaleVariance") ||
            field == QStringLiteral("amplitudeX") ||
            field == QStringLiteral("amplitudeY") ||
            field == QStringLiteral("amplitudeZ") ||
            field == QStringLiteral("frequency") ||
            field == QStringLiteral("phase") ||
            field == QStringLiteral("indexPhase") ||
            field == QStringLiteral("startX") ||
            field == QStringLiteral("startY") ||
            field == QStringLiteral("startZ") ||
            field == QStringLiteral("controlX") ||
            field == QStringLiteral("controlY") ||
            field == QStringLiteral("controlZ") ||
            field == QStringLiteral("endX") ||
            field == QStringLiteral("endY") ||
            field == QStringLiteral("endZ") ||
            field == QStringLiteral("indexScale")) {
          return setFiniteOnlySetting(field, value.toDouble(), 0.0);
        }
      }
      return false;
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
        impl_->clonerTransforms_.removeAt(static_cast<size_t>(index));
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
            static_cast<size_t>(index + 1), copy);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.moveUp")) {
      const int index = value.toInt();
      if (index > 0 &&
          index < static_cast<int>(impl_->clonerTransforms_.size())) {
        impl_->clonerTransforms_.swapItemsAt(
            static_cast<size_t>(index), static_cast<size_t>(index - 1));
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.moveDown")) {
      const int index = value.toInt();
      if (index >= 0 &&
          index + 1 < static_cast<int>(impl_->clonerTransforms_.size())) {
        impl_->clonerTransforms_.swapItemsAt(
            static_cast<size_t>(index), static_cast<size_t>(index + 1));
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
        op.position.setX(finiteClampedValue(
            value.toDouble(), op.position.x(), -100000.0, 100000.0));
      } else if (field == QStringLiteral("positionY")) {
        op.position.setY(finiteClampedValue(
            value.toDouble(), op.position.y(), -100000.0, 100000.0));
      } else if (field == QStringLiteral("positionZ")) {
        op.position.setZ(finiteClampedValue(
            value.toDouble(), op.position.z(), -100000.0, 100000.0));
      } else if (field == QStringLiteral("rotationX")) {
        op.rotation.setX(finiteClampedValue(
            value.toDouble(), op.rotation.x(), -360000.0, 360000.0));
      } else if (field == QStringLiteral("rotationY")) {
        op.rotation.setY(finiteClampedValue(
            value.toDouble(), op.rotation.y(), -360000.0, 360000.0));
      } else if (field == QStringLiteral("rotationZ")) {
        op.rotation.setZ(finiteClampedValue(
            value.toDouble(), op.rotation.z(), -360000.0, 360000.0));
      } else if (field == QStringLiteral("scaleX")) {
        op.scale.setX(finiteClampedValue(
            value.toDouble(), op.scale.x(), -100000.0, 100000.0));
      } else if (field == QStringLiteral("scaleY")) {
        op.scale.setY(finiteClampedValue(
            value.toDouble(), op.scale.y(), -100000.0, 100000.0));
      } else if (field == QStringLiteral("scaleZ")) {
        op.scale.setZ(finiteClampedValue(
            value.toDouble(), op.scale.z(), -100000.0, 100000.0));
      } else {
        return false;
      }
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    return false;
}

} // namespace Artifact
