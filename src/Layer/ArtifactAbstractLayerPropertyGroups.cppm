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

bool isTimelineHiddenLayerPropertyGroup(const QString& groupName) {
  return LayerAbstractUtilities::computeTimelineHiddenLayerPropertyGroup(
      groupName);
}

bool isTimelineExpandedByDefaultLayerPropertyGroup(
    const QString& groupName) {
  return LayerAbstractUtilities::computeTimelineExpandedByDefaultLayerPropertyGroup(
      groupName);
}

bool isInspectorHiddenLayerPropertyGroup(const QString& groupName) {
  const QString normalized = groupName.trimmed();
  const bool isComponentSettingsGroup =
      normalized.compare(QStringLiteral("Components"), Qt::CaseInsensitive) == 0 ||
      normalized.compare(QStringLiteral("Collision"), Qt::CaseInsensitive) == 0 ||
      normalized.compare(QStringLiteral("Layout"), Qt::CaseInsensitive) == 0 ||
      normalized.compare(QStringLiteral("Cloner"), Qt::CaseInsensitive) == 0 ||
      normalized.compare(QStringLiteral("Crowd"), Qt::CaseInsensitive) == 0 ||
      normalized.compare(QStringLiteral("Particle Emitter"), Qt::CaseInsensitive) == 0 ||
      normalized.compare(QStringLiteral("Fluid"), Qt::CaseInsensitive) == 0;
  return isComponentSettingsGroup ||
         LayerAbstractUtilities::computeInspectorHiddenLayerPropertyGroup(
             normalized);
}

bool isInspectorExpandedByDefaultLayerPropertyGroup(
    const QString& groupName) {
  return LayerAbstractUtilities::computeInspectorExpandedByDefaultLayerPropertyGroup(
      groupName);
}

bool isClonerLayerPropertyGroup(const QString& groupName) {
  return LayerAbstractUtilities::computeClonerLayerPropertyGroup(groupName);
}

bool isSourceReframeLayerPropertyGroup(const QString& groupName) {
  return LayerAbstractUtilities::computeSourceReframeLayerPropertyGroup(
      groupName);
}

bool isTimelineTextAnimatorLayerPropertyGroup(
    const ArtifactCore::PropertyGroup& group) {
  const auto properties = group.allProperties();
  if (properties.empty()) return false;
  for (const auto& property : properties) {
    if (!property) return false;
    const QStringList pathParts = property->getName().split(QLatin1Char('.'));
    if (pathParts.size() < 4 || pathParts[0] != QStringLiteral("text") ||
        pathParts[1] != QStringLiteral("animators")) {
      return false;
    }
    bool indexIsNumeric = false;
    pathParts[2].toUInt(&indexIsNumeric);
    if (!indexIsNumeric) return false;
  }
  return true;
}

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
  auto twoPointFiveDEnabled = makeProp(QStringLiteral("layer.2_5d.enabled"),
      PropertyType::Boolean, impl_->twoPointFiveDEnabled_, -143);
  twoPointFiveDEnabled->setDisplayLabel(QStringLiteral("2.5D Lens"));
  twoPointFiveDEnabled->setTooltip(QStringLiteral("Uses a local 2D-stack lens; it does not use the composition 3D camera."));
  twoPointFiveDEnabled->setInlineHelp(QStringLiteral("Fake 3D; ignores the comp camera."));
  twoPointFiveDEnabled->setWhatsThis(QStringLiteral("Renders this layer with a local 2.5D lens for a pseudo-3D look.\nIt does NOT use the composition 3D camera or scene lights.\nFor real 3D, use a 3D layer type instead."));
  layerGroup.addProperty(twoPointFiveDEnabled);
  auto twoPointFiveDDepth = makeProp(QStringLiteral("layer.2_5d.depth"),
      PropertyType::Float, impl_->twoPointFiveDDepth_, -142);
  twoPointFiveDDepth->setDisplayLabel(QStringLiteral("2.5D Depth"));
  twoPointFiveDDepth->setSoftRange(-1000.0, 1000.0);
  layerGroup.addProperty(twoPointFiveDDepth);
  auto twoPointFiveDCameraDistance = makeProp(QStringLiteral("layer.2_5d.cameraDistance"),
      PropertyType::Float, impl_->twoPointFiveDCameraDistance_, -141);
  twoPointFiveDCameraDistance->setDisplayLabel(QStringLiteral("2.5D Camera Distance"));
  twoPointFiveDCameraDistance->setHardRange(1.0, 1000000.0);
  twoPointFiveDCameraDistance->setSoftRange(100.0, 5000.0);
  layerGroup.addProperty(twoPointFiveDCameraDistance);
  layerGroup.addProperty(makeProp(QStringLiteral("layer.2_5d.depthOfFieldEnabled"),
      PropertyType::Boolean, impl_->twoPointFiveDDepthOfFieldEnabled_, -140));
  auto twoPointFiveDFocusDepth = makeProp(QStringLiteral("layer.2_5d.focusDepth"),
      PropertyType::Float, impl_->twoPointFiveDFocusDepth_, -139);
  twoPointFiveDFocusDepth->setDisplayLabel(QStringLiteral("2.5D Focus Depth"));
  twoPointFiveDFocusDepth->setSoftRange(-1000.0, 1000.0);
  layerGroup.addProperty(twoPointFiveDFocusDepth);
  auto twoPointFiveDFocusRange = makeProp(QStringLiteral("layer.2_5d.focusRange"),
      PropertyType::Float, impl_->twoPointFiveDFocusRange_, -138);
  twoPointFiveDFocusRange->setDisplayLabel(QStringLiteral("2.5D Focus Range"));
  twoPointFiveDFocusRange->setHardRange(1.0, 1000000.0);
  layerGroup.addProperty(twoPointFiveDFocusRange);
  auto twoPointFiveDMaxBlur = makeProp(QStringLiteral("layer.2_5d.maxBlur"),
      PropertyType::Float, impl_->twoPointFiveDMaxBlur_, -137);
  twoPointFiveDMaxBlur->setDisplayLabel(QStringLiteral("2.5D Max Blur"));
  twoPointFiveDMaxBlur->setHardRange(0.0, 64.0);
  layerGroup.addProperty(twoPointFiveDMaxBlur);
  layerGroup.addProperty(makeProp(QStringLiteral("layer.2_5d.motionBlurEnabled"),
      PropertyType::Boolean, impl_->twoPointFiveDMotionBlurEnabled_, -136));
  auto twoPointFiveDShutter = makeProp(QStringLiteral("layer.2_5d.motionBlurShutterAngle"),
      PropertyType::Float, impl_->twoPointFiveDMotionBlurShutterAngle_, -135);
  twoPointFiveDShutter->setDisplayLabel(QStringLiteral("2.5D Motion Blur Shutter"));
  twoPointFiveDShutter->setHardRange(0.0, 720.0);
  layerGroup.addProperty(twoPointFiveDShutter);
   auto twoPointFiveDSamples = makeProp(QStringLiteral("layer.2_5d.motionBlurSamples"),
       PropertyType::Integer, impl_->twoPointFiveDMotionBlurSamples_, -134);
   twoPointFiveDSamples->setDisplayLabel(QStringLiteral("2.5D Motion Blur Samples"));
   twoPointFiveDSamples->setHardRange(2.0, 8.0);
   layerGroup.addProperty(twoPointFiveDSamples);

   auto projectionEnabled = makeProp(QStringLiteral("projection.enabled"),
       PropertyType::Boolean, impl_->projectionEnabled_, -146);
   projectionEnabled->setDisplayLabel(QStringLiteral("Project3D"));
   projectionEnabled->setTooltip(QStringLiteral("Projects a source layer's 3D camera view onto this 3D card."));
   projectionEnabled->setInlineHelp(QStringLiteral("Render a source layer through its own camera onto this card."));
   layerGroup.addProperty(projectionEnabled);
   auto projectionSource = makeProp(QStringLiteral("projection.sourceLayerId"),
       PropertyType::String, impl_->projectionSourceLayerId_, -145);
   projectionSource->setDisplayLabel(QStringLiteral("Projector Source"));
   projectionSource->setTooltip(QStringLiteral("Layer whose 3D camera output is projected onto this card."));
   layerGroup.addProperty(projectionSource);

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
  const auto sz = sourceSize();
  QSize compositionSize(1920, 1080);
  if (const auto *composition =
          dynamic_cast<const ArtifactAbstractComposition *>(
              compositionObject())) {
    const QSize candidate = composition->settings().compositionSize();
    if (candidate.width() > 0 && candidate.height() > 0) {
      compositionSize = candidate;
    }
  }
  const int sourceWidth = std::max(1, sz.width);
  const int sourceHeight = std::max(1, sz.height);
  const double positionRangeX =
      static_cast<double>(std::max(compositionSize.width() * 2, 4096));
  const double positionRangeY =
      static_cast<double>(std::max(compositionSize.height() * 2, 4096));
  const double anchorRangeX =
      static_cast<double>(std::max(sourceWidth * 2, 2048));
  const double anchorRangeY =
      static_cast<double>(std::max(sourceHeight * 2, 2048));

  PropertyGroup initialGroup(QStringLiteral("Initial"));
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
  posXProp->setStep(1.0);
  posXProp->setSoftRange(-positionRangeX, positionRangeX);
  posXProp->setAnimatable(true);
  transformGroup.addProperty(posXProp);

  auto posYProp = makeProp(QStringLiteral("transform.position.y"),
                           PropertyType::Float, t3.positionY(), -299);
  posYProp->setDisplayLabel(QStringLiteral("Position Y"));
  posYProp->setUnit(QStringLiteral("px"));
  posYProp->setStep(1.0);
  posYProp->setSoftRange(-positionRangeY, positionRangeY);
  posYProp->setAnimatable(true);
  transformGroup.addProperty(posYProp);

  auto scaleXProp = makeProp(QStringLiteral("transform.scale.x"),
                             PropertyType::Float, t3.scaleX(), -298);
  scaleXProp->setDisplayLabel(QStringLiteral("Scale X"));
  scaleXProp->setAnimatable(true);
  scaleXProp->setStep(0.01);
  scaleXProp->setSoftRange(0.0, 2.0);  // Soft range for typical use (0-200%)
  transformGroup.addProperty(scaleXProp);

  auto scaleYProp = makeProp(QStringLiteral("transform.scale.y"),
                             PropertyType::Float, t3.scaleY(), -297);
  scaleYProp->setDisplayLabel(QStringLiteral("Scale Y"));
  scaleYProp->setAnimatable(true);
  scaleYProp->setStep(0.01);
  scaleYProp->setSoftRange(0.0, 2.0);  // Soft range for typical use (0-200%)
  transformGroup.addProperty(scaleYProp);

  auto rotationProp = makeProp(QStringLiteral("transform.rotation"),
                               PropertyType::Float, t3.rotation(), -296);
  rotationProp->setDisplayLabel(is3D() ? QStringLiteral("Rotation Z")
                                       : QStringLiteral("Rotation"));
  rotationProp->setUnit(QStringLiteral("deg"));
  rotationProp->setStep(1.0);
  rotationProp->setSoftRange(-180.0, 180.0);
  rotationProp->setAnimatable(true);
  transformGroup.addProperty(rotationProp);

  if (is3D()) {
    append3DTransformProperties(transformGroup, t3, positionRangeX,
                                anchorRangeX);
  }

  auto autoOrientProp =
      makeProp(QStringLiteral("transform.autoOrient"), PropertyType::Integer,
               static_cast<int>(t3.autoOrientMode()), -295);
  autoOrientProp->setDisplayLabel(QStringLiteral("Auto-Orient"));
  autoOrientProp->setTooltip(QStringLiteral(
      "Off: keep the current rotation.\n"
      "Along Path: rotate the layer to follow the motion path tangent.\n"
      "Along Path at Frame Start: use the tangent at the start of the current segment."));
  autoOrientProp->setInlineHelp(QStringLiteral("Auto-rotate along the motion path."));
  autoOrientProp->setWhatsThis(QStringLiteral("Rotates the layer automatically from its motion path.\nOff keeps keyframed rotation; Along Path follows the tangent.\nUseful for vehicles, arrows and characters walking a path."));
  transformGroup.addProperty(autoOrientProp);

  auto anchorXProp = makeProp(QStringLiteral("transform.anchor.x"),
                              PropertyType::Float, t3.anchorX(), -295);
  anchorXProp->setDisplayLabel(QStringLiteral("Anchor X"));
  anchorXProp->setUnit(QStringLiteral("px"));
  anchorXProp->setStep(1.0);
  anchorXProp->setSoftRange(-anchorRangeX, anchorRangeX);
  anchorXProp->setAnimatable(true);
  transformGroup.addProperty(anchorXProp);

  auto anchorYProp = makeProp(QStringLiteral("transform.anchor.y"),
                              PropertyType::Float, t3.anchorY(), -294);
  anchorYProp->setDisplayLabel(QStringLiteral("Anchor Y"));
  anchorYProp->setUnit(QStringLiteral("px"));
  anchorYProp->setStep(1.0);
  anchorYProp->setSoftRange(-anchorRangeY, anchorRangeY);
  anchorYProp->setAnimatable(true);
  transformGroup.addProperty(anchorYProp);

  auto inPointProp =
      makeProp(QStringLiteral("time.inPoint"), PropertyType::Integer,
               static_cast<qint64>(inPoint().framePosition()), -90);
  inPointProp->setUnit(QStringLiteral("frames"));
  inPointProp->setTooltip(QStringLiteral("Layer in-point on timeline"));
  inPointProp->setWhatsThis(QStringLiteral("First timeline frame where the layer is visible.\nDrag the clip head in the Timeline right pane to change it.\nCompare Start Time, which shifts the source content instead."));
  layerGroup.addProperty(inPointProp);

  auto outPointProp =
      makeProp(QStringLiteral("time.outPoint"), PropertyType::Integer,
               static_cast<qint64>(outPoint().framePosition()), -80);
  outPointProp->setUnit(QStringLiteral("frames"));
  outPointProp->setTooltip(QStringLiteral("Layer out-point on timeline"));
  outPointProp->setWhatsThis(QStringLiteral("First timeline frame where the layer is already gone (exclusive end).\nDrag the clip tail in the Timeline right pane to change it."));
  layerGroup.addProperty(outPointProp);

  auto startTimeProp =
      makeProp(QStringLiteral("time.startTime"), PropertyType::Integer,
               static_cast<qint64>(startTime().framePosition()), -70);
  startTimeProp->setUnit(QStringLiteral("frames"));
  startTimeProp->setTooltip(
      QStringLiteral("Layer start offset in source time"));
  startTimeProp->setInlineHelp(QStringLiteral("Shifts source content, not visibility."));
  startTimeProp->setWhatsThis(QStringLiteral("Offsets the source content against the timeline.\nIn/Out points decide WHEN the layer shows; Start Time decides WHAT part of the source shows."));
  layerGroup.addProperty(startTimeProp);

  // 物理演算プロパティグループ
  PropertyGroup physicsGroup(QStringLiteral("Physics"));

  auto physicsEnabledProp =
      makeProp(QStringLiteral("physics.enabled"), PropertyType::Boolean,
               impl_->physicsComponent_.settings().enabled, -100);
  physicsGroup.addProperty(physicsEnabledProp);

  auto softBodyEnabledProp =
      makeProp(QStringLiteral("physics.softBody.enabled"), PropertyType::Boolean,
               impl_->softBodyPhysicsEnabled_, -99);
  softBodyEnabledProp->setDisplayLabel(QStringLiteral("Soft Body Grid"));
  softBodyEnabledProp->setTooltip(
      QStringLiteral("Simulate rectangular Shape layers as a deformable grid."));
  physicsGroup.addProperty(softBodyEnabledProp);

  auto cloth3DEnabledProp =
      makeProp(QStringLiteral("physics.cloth3D.enabled"), PropertyType::Boolean,
               impl_->cloth3DPhysicsEnabled_, -99);
  cloth3DEnabledProp->setDisplayLabel(QStringLiteral("Cloth 3D Grid"));
  cloth3DEnabledProp->setTooltip(
      QStringLiteral("Simulate this layer as a 3D cloth grid (base slice)."));
  physicsGroup.addProperty(cloth3DEnabledProp);

  auto materialEnabledProp =
      makeProp(QStringLiteral("physics.material.enabled"), PropertyType::Boolean,
               impl_->materialPhysicsEnabled_, -98);
  materialEnabledProp->setDisplayLabel(QStringLiteral("Material Simulation"));
  materialEnabledProp->setTooltip(
      QStringLiteral("Use the continuum material solver for flesh, foam, rubber, or wood."));
  physicsGroup.addProperty(materialEnabledProp);

  auto materialPresetProp =
      makeProp(QStringLiteral("physics.material.preset"), PropertyType::Integer,
               impl_->materialPhysicsPreset_, -97);
  materialPresetProp->setDisplayLabel(QStringLiteral("Material Preset"));
  materialPresetProp->setTooltip(
      QStringLiteral("0=Flesh, 1=Foam, 2=Hard Rubber, 3=Wood."));
  materialPresetProp->setHardRange(0.0, 3.0);
  materialPresetProp->setSoftRange(0.0, 3.0);
  physicsGroup.addProperty(materialPresetProp);

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

  auto fallProfileProp =
      makeProp(QStringLiteral("physics.fallProfile"), PropertyType::Integer,
               impl_->physicsComponent_.settings().fallProfile, -96);
  fallProfileProp->setDisplayLabel(QStringLiteral("Fall Profile"));
  fallProfileProp->setTooltip(
      QStringLiteral("0=Custom, 1=Light, 2=Normal, 3=Heavy, 4=Floaty."));
  fallProfileProp->setHardRange(0.0, 4.0);
  fallProfileProp->setSoftRange(0.0, 4.0);
  fallProfileProp->setStep(1.0);
  physicsGroup.addProperty(fallProfileProp);

  auto gravityYProp =
      makeProp(QStringLiteral("physics.gravityY"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().gravityY), -96);
  gravityYProp->setDisplayLabel(QStringLiteral("World Gravity Y (Advanced)"));
  gravityYProp->setTooltip(
      QStringLiteral("Gravity used by falling rigid and soft-body layers."));
  gravityYProp->setUnit(QStringLiteral("px/s^2"));
  gravityYProp->setHardRange(-5000.0, 5000.0);
  gravityYProp->setSoftRange(-2000.0, 2000.0);
  gravityYProp->setStep(10.0);
  physicsGroup.addProperty(gravityYProp);

  auto linearDampingProp =
      makeProp(QStringLiteral("physics.linearDamping"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().linearDamping), -95);
  linearDampingProp->setDisplayLabel(QStringLiteral("Air Drag"));
  linearDampingProp->setTooltip(
      QStringLiteral("Linear air resistance applied to rigid and soft-body layers."));
  linearDampingProp->setHardRange(0.0, 50.0);
  linearDampingProp->setSoftRange(0.0, 10.0);
  linearDampingProp->setStep(0.1);
  physicsGroup.addProperty(linearDampingProp);

  auto angularDampingProp =
      makeProp(QStringLiteral("physics.angularDamping"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().angularDamping), -94);
  angularDampingProp->setDisplayLabel(QStringLiteral("Angular Drag"));
  angularDampingProp->setTooltip(
      QStringLiteral("Rotational air resistance applied to the Box2D body."));
  angularDampingProp->setHardRange(0.0, 50.0);
  angularDampingProp->setSoftRange(0.0, 10.0);
  angularDampingProp->setStep(0.1);
  physicsGroup.addProperty(angularDampingProp);

  auto gravityScaleProp =
      makeProp(QStringLiteral("physics.gravityScale"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().gravityScale), -93);
  gravityScaleProp->setDisplayLabel(QStringLiteral("Gravity Scale"));
  gravityScaleProp->setTooltip(
      QStringLiteral("Per-layer multiplier for the composition gravity."));
  gravityScaleProp->setHardRange(-10.0, 10.0);
  gravityScaleProp->setSoftRange(-2.0, 2.0);
  gravityScaleProp->setStep(0.01);
  physicsGroup.addProperty(gravityScaleProp);

  auto windEnabledProp =
      makeProp(QStringLiteral("physics.wind.enabled"), PropertyType::Boolean,
               impl_->physicsComponent_.settings().windEnabled, -92);
  windEnabledProp->setDisplayLabel(QStringLiteral("Wind Enabled"));
  windEnabledProp->setTooltip(
      QStringLiteral("Apply directional wind to this rigid body. Targeted Live Fields mask its strength."));
  physicsGroup.addProperty(windEnabledProp);

  auto windXProp =
      makeProp(QStringLiteral("physics.wind.x"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().windX), -92);
  windXProp->setDisplayLabel(QStringLiteral("Wind Direction X"));
  windXProp->setHardRange(-1.0, 1.0);
  windXProp->setSoftRange(-1.0, 1.0);
  windXProp->setStep(0.01);
  physicsGroup.addProperty(windXProp);

  auto windYProp =
      makeProp(QStringLiteral("physics.wind.y"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().windY), -92);
  windYProp->setDisplayLabel(QStringLiteral("Wind Direction Y"));
  windYProp->setHardRange(-1.0, 1.0);
  windYProp->setSoftRange(-1.0, 1.0);
  windYProp->setStep(0.01);
  physicsGroup.addProperty(windYProp);

  auto windStrengthProp =
      makeProp(QStringLiteral("physics.wind.strength"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().windStrength), -92);
  windStrengthProp->setDisplayLabel(QStringLiteral("Wind Strength"));
  windStrengthProp->setHardRange(0.0, 100000.0);
  windStrengthProp->setSoftRange(0.0, 5000.0);
  windStrengthProp->setStep(10.0);
  physicsGroup.addProperty(windStrengthProp);

  auto windTorqueProp =
      makeProp(QStringLiteral("physics.wind.torque"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().windTorque), -92);
  windTorqueProp->setDisplayLabel(QStringLiteral("Wind Spin"));
  windTorqueProp->setTooltip(
      QStringLiteral("Additional rotational force from wind; 0 leaves rotation to collisions."));
  windTorqueProp->setHardRange(-100.0, 100.0);
  windTorqueProp->setSoftRange(-10.0, 10.0);
  windTorqueProp->setStep(0.01);
  physicsGroup.addProperty(windTorqueProp);

  auto restitutionProp =
      makeProp(QStringLiteral("physics.restitution"), PropertyType::Float,
               static_cast<double>(
                   impl_->physicsComponent_.settings().restitution),
               -94);
  restitutionProp->setDisplayLabel(QStringLiteral("Collision Bounce"));
  restitutionProp->setHardRange(0.0, 1.0);
  restitutionProp->setSoftRange(0.0, 1.0);
  restitutionProp->setStep(0.01);
  physicsGroup.addProperty(restitutionProp);

  auto initialVelocityYProp =
      makeProp(QStringLiteral("physics.initialVelocityY"), PropertyType::Float,
               static_cast<double>(impl_->clonePhysicsInitialVelocityY_), -92);
  initialVelocityYProp->setDisplayLabel(QStringLiteral("Initial Velocity Y"));
  initialVelocityYProp->setUnit(QStringLiteral("px/s"));
  initialVelocityYProp->setHardRange(-5000.0, 5000.0);
  initialVelocityYProp->setSoftRange(-2000.0, 2000.0);
  initialVelocityYProp->setStep(10.0);
  physicsGroup.addProperty(initialVelocityYProp);

  auto maxBouncesProp =
      makeProp(QStringLiteral("physics.maxBounces"), PropertyType::Integer,
               impl_->clonePhysicsMaxBounces_, -91);
  maxBouncesProp->setDisplayLabel(QStringLiteral("Max Bounces"));
  maxBouncesProp->setHardRange(0.0, 32.0);
  maxBouncesProp->setSoftRange(0.0, 16.0);
  maxBouncesProp->setStep(1.0);
  physicsGroup.addProperty(maxBouncesProp);

  auto wiggleFreqProp =
      makeProp(QStringLiteral("physics.wiggleFreq"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().wiggleFreq), -93);
  wiggleFreqProp->setUnit(QStringLiteral("Hz"));
  wiggleFreqProp->setSoftRange(0.0, 10.0);
  physicsGroup.addProperty(wiggleFreqProp);

  auto wiggleAmpProp =
      makeProp(QStringLiteral("physics.wiggleAmp"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().wiggleAmp), -93);
  wiggleAmpProp->setSoftRange(0.0, 100.0);
  physicsGroup.addProperty(wiggleAmpProp);

  PropertyGroup motionGroup(QStringLiteral("Motion"));
  auto motionEnabledProp =
      makeProp(QStringLiteral("motion.enabled"), PropertyType::Boolean,
               impl_->motionDynamicsEnabled_, -92);
  motionEnabledProp->setDisplayLabel(QStringLiteral("Enable"));
  motionEnabledProp->setTooltip(
      QStringLiteral("Use animation dynamics for transform follow-through."));
  motionGroup.addProperty(motionEnabledProp);

  auto motionModeProp =
      makeProp(QStringLiteral("motion.mode"), PropertyType::Integer,
               impl_->motionDynamicsMode_, -91);
  motionModeProp->setDisplayLabel(QStringLiteral("Mode"));
  motionModeProp->setTooltip(
      QStringLiteral("0=Off, 1=Spring, 2=LagFollow. Choose the follow-through model."));
  motionGroup.addProperty(motionModeProp);

  auto motionStiffnessProp =
      makeProp(QStringLiteral("motion.stiffness"), PropertyType::Float,
               static_cast<double>(impl_->motionDynamicsStiffness_), -90);
  motionStiffnessProp->setHardRange(0.0, 1000.0);
  motionStiffnessProp->setSoftRange(0.0, 500.0);
  motionStiffnessProp->setDisplayLabel(QStringLiteral("Stiffness"));
  motionStiffnessProp->setTooltip(
      QStringLiteral("Spring strength used by the follow-through solver."));
  motionGroup.addProperty(motionStiffnessProp);

  auto motionDampingProp =
      makeProp(QStringLiteral("motion.damping"), PropertyType::Float,
               static_cast<double>(impl_->motionDynamicsDamping_), -89);
  motionDampingProp->setHardRange(0.0, 100.0);
  motionDampingProp->setSoftRange(0.0, 50.0);
  motionDampingProp->setDisplayLabel(QStringLiteral("Damping"));
  motionDampingProp->setTooltip(
      QStringLiteral("Energy loss per step. Higher values settle faster."));
  motionGroup.addProperty(motionDampingProp);

  auto motionMassProp =
      makeProp(QStringLiteral("motion.mass"), PropertyType::Float,
               static_cast<double>(impl_->motionDynamicsMass_), -88);
  motionMassProp->setHardRange(0.1, 100.0);
  motionMassProp->setSoftRange(0.1, 10.0);
  motionMassProp->setDisplayLabel(QStringLiteral("Mass"));
  motionMassProp->setTooltip(
      QStringLiteral("Mass used by the follow-through response."));
  motionGroup.addProperty(motionMassProp);

  auto motionLagTauProp =
      makeProp(QStringLiteral("motion.lagTau"), PropertyType::Float,
               static_cast<double>(impl_->motionDynamicsLagTau_), -87);
  motionLagTauProp->setHardRange(0.001, 10.0);
  motionLagTauProp->setSoftRange(0.01, 1.0);
  motionLagTauProp->setDisplayLabel(QStringLiteral("Lag Tau"));
  motionLagTauProp->setTooltip(
      QStringLiteral("Time constant used by the lag-follow mode."));
  motionLagTauProp->setInlineHelp(QStringLiteral("Bigger = slower, floatier follow."));
  motionLagTauProp->setWhatsThis(QStringLiteral("How sluggishly the layer follows in lag-follow mode.\nSmall values stick tightly; large values trail behind softly.\nPair with Clamp Overshoot to keep the trailing from swinging too far."));
  motionGroup.addProperty(motionLagTauProp);

  auto motionClampOvershootProp =
      makeProp(QStringLiteral("motion.clampOvershoot"), PropertyType::Boolean,
               impl_->motionDynamicsClampOvershoot_, -86);
  motionClampOvershootProp->setDisplayLabel(QStringLiteral("Clamp Overshoot"));
  motionClampOvershootProp->setTooltip(
      QStringLiteral("Keep the solver from overshooting too far."));
  motionClampOvershootProp->setInlineHelp(QStringLiteral("Caps the swing past the target."));
  motionClampOvershootProp->setWhatsThis(QStringLiteral("Limits how far the follow-through may swing past its target.\nOff allows free overshoot (springier, wilder); on caps it at Overshoot Limit."));
  motionGroup.addProperty(motionClampOvershootProp);

  auto motionOvershootLimitProp =
      makeProp(QStringLiteral("motion.overshootLimit"), PropertyType::Float,
               static_cast<double>(impl_->motionDynamicsOvershootLimit_), -85);
  motionOvershootLimitProp->setHardRange(0.0, 2.0);
  motionOvershootLimitProp->setSoftRange(0.0, 1.0);
  motionOvershootLimitProp->setDisplayLabel(QStringLiteral("Overshoot Limit"));
  motionOvershootLimitProp->setTooltip(
      QStringLiteral("Maximum overshoot ratio when clamping is enabled."));
  motionGroup.addProperty(motionOvershootLimitProp);

  PropertyGroup fractureGroup(QStringLiteral("Fracture"));
  auto fractureEnabledProp =
      makeProp(QStringLiteral("fracture.enabled"), PropertyType::Boolean,
               impl_->fractureEnabled_, -84);
  fractureEnabledProp->setDisplayLabel(QStringLiteral("Enable"));
  fractureEnabledProp->setTooltip(
      QStringLiteral("Enable fracture overlay, shard motion, and crack state."));
  fractureGroup.addProperty(fractureEnabledProp);

  auto fracturePreGenerateProp =
      makeProp(QStringLiteral("fracture.preGenerate"), PropertyType::Boolean,
               impl_->fracturePreGenerate_, -835);
  fracturePreGenerateProp->setDisplayLabel(QStringLiteral("Pre-generate Shards"));
  fracturePreGenerateProp->setTooltip(
      QStringLiteral("Prepare deterministic shard geometry before an impact so downstream components can use it."));
  fractureGroup.addProperty(fracturePreGenerateProp);

  auto fractureTriggerFrameProp = makeProp(
      QStringLiteral("fracture.triggerFrame"), PropertyType::Integer,
      static_cast<qint64>(impl_->fractureTriggerFrame_), -834);
  fractureTriggerFrameProp->setDisplayLabel(QStringLiteral("Trigger Frame"));
  fractureTriggerFrameProp->setTooltip(
      QStringLiteral("Start fracture at this composition frame. -1 disables the automatic trigger."));
  fractureTriggerFrameProp->setInlineHelp(QStringLiteral("-1 = break manually only."));
  fractureTriggerFrameProp->setWhatsThis(QStringLiteral("Composition frame where the layer breaks on its own.\n-1 disables the automatic trigger.\nPre-generate Shards first if downstream components need the pieces early."));
  fractureTriggerFrameProp->setHardRange(-1.0, 1000000.0);
  fractureGroup.addProperty(fractureTriggerFrameProp);

  auto fracturePresetProp =
      makeProp(QStringLiteral("fracture.preset"), PropertyType::Integer,
               impl_->fracturePreset_, -83);
  fracturePresetProp->setDisplayLabel(QStringLiteral("Profile"));
  fracturePresetProp->setTooltip(
      QStringLiteral("Base fracture profile. 0=Glass, 1=Concrete, 2=Stone, 3=Metal, 4=Wood, 5=Dust. Fine-tune the threshold and shard controls below."));
  fracturePresetProp->setHardRange(0.0, 5.0);
  fracturePresetProp->setSoftRange(0.0, 5.0);
  fractureGroup.addProperty(fracturePresetProp);

  auto fractureCrackThresholdProp =
      makeProp(QStringLiteral("fracture.crackThreshold"), PropertyType::Float,
               static_cast<double>(impl_->fractureCrackThreshold_), -82);
  fractureCrackThresholdProp->setHardRange(0.0, 1000.0);
  fractureCrackThresholdProp->setSoftRange(0.0, 100.0);
  fractureCrackThresholdProp->setDisplayLabel(QStringLiteral("Crack Threshold"));
  fractureCrackThresholdProp->setTooltip(
      QStringLiteral("Damage required before the layer enters a cracked state."));
  fractureGroup.addProperty(fractureCrackThresholdProp);

  auto fractureShatterThresholdProp =
      makeProp(QStringLiteral("fracture.shatterThreshold"), PropertyType::Float,
               static_cast<double>(impl_->fractureShatterThreshold_), -81);
  fractureShatterThresholdProp->setHardRange(0.0, 1000.0);
  fractureShatterThresholdProp->setSoftRange(0.0, 200.0);
  fractureShatterThresholdProp->setDisplayLabel(QStringLiteral("Shatter Threshold"));
  fractureShatterThresholdProp->setTooltip(
      QStringLiteral("Damage required before the layer spawns fracture shards."));
  fractureGroup.addProperty(fractureShatterThresholdProp);

  auto fractureShardCountProp =
      makeProp(QStringLiteral("fracture.shardCount"), PropertyType::Integer,
               impl_->fractureShardCount_, -80);
  fractureShardCountProp->setHardRange(1.0, 256.0);
  fractureShardCountProp->setSoftRange(4.0, 64.0);
  fractureShardCountProp->setDisplayLabel(QStringLiteral("Shard Count"));
  fractureShardCountProp->setTooltip(
      QStringLiteral("Number of shards spawned when the layer fractures."));
  fractureGroup.addProperty(fractureShardCountProp);

  auto fractureShardDampingProp =
      makeProp(QStringLiteral("fracture.shardDamping"), PropertyType::Float,
               static_cast<double>(impl_->fractureShardDamping_), -79);
  fractureShardDampingProp->setHardRange(0.0, 1.0);
  fractureShardDampingProp->setSoftRange(0.0, 1.0);
  fractureShardDampingProp->setDisplayLabel(QStringLiteral("Shard Damping"));
  fractureShardDampingProp->setTooltip(
      QStringLiteral("How quickly the spawned shards lose momentum."));
  fractureGroup.addProperty(fractureShardDampingProp);

  auto fractureShardGravityProp =
      makeProp(QStringLiteral("fracture.shardGravity"), PropertyType::Float,
               static_cast<double>(impl_->fractureShardGravity_), -78);
  fractureShardGravityProp->setUnit(QStringLiteral("px/s^2"));
  fractureShardGravityProp->setHardRange(-5000.0, 5000.0);
  fractureShardGravityProp->setSoftRange(-2000.0, 2000.0);
  fractureShardGravityProp->setDisplayLabel(QStringLiteral("Shard Gravity"));
  fractureShardGravityProp->setTooltip(
      QStringLiteral("Vertical gravity applied to fracture shards."));
  fractureGroup.addProperty(fractureShardGravityProp);

  auto fractureImpactSensitivityProp =
      makeProp(QStringLiteral("fracture.impactSensitivity"), PropertyType::Float,
               static_cast<double>(impl_->fractureImpactSensitivity_), -77);
  fractureImpactSensitivityProp->setHardRange(0.0, 10.0);
  fractureImpactSensitivityProp->setSoftRange(0.0, 2.0);
  fractureImpactSensitivityProp->setDisplayLabel(QStringLiteral("Impact Sensitivity"));
  fractureImpactSensitivityProp->setTooltip(
      QStringLiteral("How strongly incoming impacts contribute to fracture damage."));
  fractureGroup.addProperty(fractureImpactSensitivityProp);

  PropertyGroup trailGroup(QStringLiteral("Trail"));
  auto trailEnabledProp = makeProp(QStringLiteral("trail.enabled"),
      PropertyType::Boolean, impl_->motionTrailEnabled_, -76);
  trailEnabledProp->setDisplayLabel(QStringLiteral("Enable"));
  trailGroup.addProperty(trailEnabledProp);
  auto trailLengthProp = makeProp(QStringLiteral("trail.length"),
      PropertyType::Integer, impl_->motionTrailLength_, -75);
  trailLengthProp->setDisplayLabel(QStringLiteral("Length"));
  trailLengthProp->setHardRange(2.0, 256.0);
  trailLengthProp->setSoftRange(4.0, 64.0);
  trailGroup.addProperty(trailLengthProp);
  auto trailFadeProp = makeProp(QStringLiteral("trail.fade"),
      PropertyType::Float, static_cast<double>(impl_->motionTrailFade_), -74);
  trailFadeProp->setDisplayLabel(QStringLiteral("Fade"));
  trailFadeProp->setHardRange(0.0, 1.0);
  trailFadeProp->setSoftRange(0.0, 1.0);
  trailGroup.addProperty(trailFadeProp);
  auto trailWidthProp = makeProp(QStringLiteral("trail.width"),
      PropertyType::Float, static_cast<double>(impl_->motionTrailWidth_), -73);
  trailWidthProp->setDisplayLabel(QStringLiteral("Width"));
  trailWidthProp->setUnit(QStringLiteral("px"));
  trailWidthProp->setHardRange(0.1, 128.0);
  trailWidthProp->setSoftRange(0.5, 16.0);
  trailGroup.addProperty(trailWidthProp);

  PropertyGroup fragmentAppearanceGroup(QStringLiteral("Fragment Appearance"));
  auto velocityStretchEnabledProp = makeProp(
      QStringLiteral("fragment.velocityStretch.enabled"), PropertyType::Boolean,
      impl_->fragmentVelocityStretchEnabled_, -72);
  velocityStretchEnabledProp->setDisplayLabel(QStringLiteral("Velocity Stretch"));
  fragmentAppearanceGroup.addProperty(velocityStretchEnabledProp);
  auto velocityStretchStrengthProp = makeProp(
      QStringLiteral("fragment.velocityStretch.strength"), PropertyType::Float,
      static_cast<double>(impl_->fragmentVelocityStretchStrength_), -71);
  velocityStretchStrengthProp->setDisplayLabel(QStringLiteral("Stretch Strength"));
  velocityStretchStrengthProp->setHardRange(0.0, 1.0);
  velocityStretchStrengthProp->setSoftRange(0.0, 0.1);
  fragmentAppearanceGroup.addProperty(velocityStretchStrengthProp);
  auto velocityStretchMaxProp = makeProp(
      QStringLiteral("fragment.velocityStretch.max"), PropertyType::Float,
      static_cast<double>(impl_->fragmentVelocityStretchMax_), -70);
  velocityStretchMaxProp->setDisplayLabel(QStringLiteral("Max Stretch"));
  velocityStretchMaxProp->setHardRange(1.0, 32.0);
  velocityStretchMaxProp->setSoftRange(1.0, 8.0);
  fragmentAppearanceGroup.addProperty(velocityStretchMaxProp);
  auto colorVariationEnabledProp = makeProp(
      QStringLiteral("fragment.colorVariation.enabled"), PropertyType::Boolean,
      impl_->fragmentColorVariationEnabled_, -69);
  colorVariationEnabledProp->setDisplayLabel(QStringLiteral("Color Variation"));
  fragmentAppearanceGroup.addProperty(colorVariationEnabledProp);
  auto colorVariationProp = makeProp(
      QStringLiteral("fragment.colorVariation.amount"), PropertyType::Float,
      static_cast<double>(impl_->fragmentColorVariation_), -68);
  colorVariationProp->setDisplayLabel(QStringLiteral("Variation Amount"));
  colorVariationProp->setHardRange(0.0, 1.0);
  colorVariationProp->setSoftRange(0.0, 1.0);
  fragmentAppearanceGroup.addProperty(colorVariationProp);
  auto fragmentClonerOutputEnabledProp = makeProp(
      QStringLiteral("fragment.clonerOutput.enabled"), PropertyType::Boolean,
      impl_->fragmentClonerOutputEnabled_, -67);
  fragmentClonerOutputEnabledProp->setDisplayLabel(
      QStringLiteral("Fragment Cloner Output"));
  fragmentAppearanceGroup.addProperty(fragmentClonerOutputEnabledProp);
  auto fragmentClonerOutputCountProp = makeProp(
      QStringLiteral("fragment.clonerOutput.count"), PropertyType::Integer,
      impl_->fragmentClonerOutputCount_, -66);
  fragmentClonerOutputCountProp->setDisplayLabel(QStringLiteral("Clone Count"));
  fragmentClonerOutputCountProp->setHardRange(1.0, 256.0);
  fragmentClonerOutputCountProp->setSoftRange(1.0, 32.0);
  fragmentAppearanceGroup.addProperty(fragmentClonerOutputCountProp);
  auto fragmentClonerOutputSpacingXProp = makeProp(
      QStringLiteral("fragment.clonerOutput.spacingX"), PropertyType::Float,
      static_cast<double>(impl_->fragmentClonerOutputSpacingX_), -65);
  fragmentClonerOutputSpacingXProp->setDisplayLabel(QStringLiteral("Clone Spacing X"));
  fragmentClonerOutputSpacingXProp->setUnit(QStringLiteral("px"));
  fragmentAppearanceGroup.addProperty(fragmentClonerOutputSpacingXProp);
  auto fragmentClonerOutputSpacingYProp = makeProp(
      QStringLiteral("fragment.clonerOutput.spacingY"), PropertyType::Float,
      static_cast<double>(impl_->fragmentClonerOutputSpacingY_), -64);
  fragmentClonerOutputSpacingYProp->setDisplayLabel(QStringLiteral("Clone Spacing Y"));
  fragmentClonerOutputSpacingYProp->setUnit(QStringLiteral("px"));
  fragmentAppearanceGroup.addProperty(fragmentClonerOutputSpacingYProp);
  auto fragmentClonerOutputTimeOffsetProp = makeProp(
      QStringLiteral("fragment.clonerOutput.timeOffsetFrames"), PropertyType::Float,
      static_cast<double>(impl_->fragmentClonerOutputTimeOffsetFrames_), -63);
  fragmentClonerOutputTimeOffsetProp->setDisplayLabel(
      QStringLiteral("Time Offset"));
  fragmentClonerOutputTimeOffsetProp->setUnit(QStringLiteral("frames"));
  fragmentClonerOutputTimeOffsetProp->setHardRange(-10000.0, 10000.0);
  fragmentClonerOutputTimeOffsetProp->setSoftRange(-60.0, 60.0);
  fragmentAppearanceGroup.addProperty(fragmentClonerOutputTimeOffsetProp);

  // Component-specific groups are built by getComponentPropertyGroups().
  // Keeping them out of this path avoids constructing and then discarding a
  // large property graph for the normal layer inspector.
  auto isAdjustmentProp =
      makeProp(QStringLiteral("layer.isAdjustment"), PropertyType::Boolean,
               isAdjustmentLayer(), -50);
  isAdjustmentProp->setTooltip(
      QStringLiteral("Apply effects to all layers below"));
  isAdjustmentProp->setInlineHelp(QStringLiteral("Effects here apply to layers below."));
  isAdjustmentProp->setWhatsThis(QStringLiteral("Turns this layer into an adjustment layer.\nIts effects apply to every visible layer underneath it."));
  layerGroup.addProperty(isAdjustmentProp);

  std::vector<PropertyGroup> groups;
  groups.reserve(9 + static_cast<std::size_t>(maskCount()));
  groups.push_back(std::move(initialGroup));
  groups.push_back(std::move(transformGroup));
  groups.push_back(std::move(physicsGroup));
  groups.push_back(std::move(motionGroup));
  groups.push_back(std::move(fractureGroup));
  groups.push_back(std::move(trailGroup));
  groups.push_back(std::move(fragmentAppearanceGroup));
  for (std::size_t layerIndex = 0;
       layerIndex < impl_->animationLayers_.layerCount(); ++layerIndex) {
    const auto &animationLayer = impl_->animationLayers_.layer(layerIndex);
    PropertyGroup animationGroup(
        QStringLiteral("Animation Layer %1").arg(static_cast<int>(layerIndex + 1)));
    const QString prefix = QStringLiteral("animationLayers.%1").arg(
        static_cast<int>(layerIndex));
    auto weight = makeProp(prefix + QStringLiteral(".weight"), PropertyType::Float,
                            static_cast<double>(animationLayer.state.weight), -130);
    weight->setDisplayLabel(QStringLiteral("Weight"));
    weight->setHardRange(0.0, 1.0);
    weight->setSoftRange(0.0, 1.0);
    weight->setStep(0.01);
    animationGroup.addProperty(weight);
    // Do not instantiate generic interpolation merely to label an inspector
    // field. Frame evaluation stays in the renderer and animation editor.
    auto value = makeProp(prefix + QStringLiteral(".value"), PropertyType::Float,
                          static_cast<double>(animationLayer.values.current()), -131);
    value->setDisplayLabel(QStringLiteral("Value"));
    value->setSoftRange(0.0, 1.0);
    value->setStep(0.01);
    animationGroup.addProperty(value);
    auto interpolation = makeProp(
        prefix + QStringLiteral(".interpolation"), PropertyType::Integer,
        static_cast<int>(animationLayer.values.getKeyFrameInterpolationAt(
            FramePosition(currentFrame()))), -132);
    interpolation->setDisplayLabel(QStringLiteral("Interpolation"));
    interpolation->setHardRange(0.0, 32.0);
    interpolation->setStep(1.0);
    animationGroup.addProperty(interpolation);
    auto muted = makeProp(prefix + QStringLiteral(".muted"), PropertyType::Boolean,
                          animationLayer.state.muted, -129);
    muted->setDisplayLabel(QStringLiteral("Mute"));
    animationGroup.addProperty(muted);
    auto solo = makeProp(prefix + QStringLiteral(".solo"), PropertyType::Boolean,
                         animationLayer.state.solo, -128);
    solo->setDisplayLabel(QStringLiteral("Solo"));
    animationGroup.addProperty(solo);
    auto mode = makeProp(prefix + QStringLiteral(".blendMode"), PropertyType::Integer,
                         animationLayer.state.blendMode ==
                                 ArtifactCore::AnimationLayerBlendMode::Override
                             ? 1
                             : 0,
                         -127);
    mode->setDisplayLabel(QStringLiteral("Blend Mode (0 Additive / 1 Override)"));
    mode->setHardRange(0.0, 1.0);
    mode->setStep(1.0);
    animationGroup.addProperty(mode);
    groups.push_back(std::move(animationGroup));
  }
  for (auto propertyIt = impl_->animationPropertyLayers_.constBegin();
       propertyIt != impl_->animationPropertyLayers_.constEnd(); ++propertyIt) {
    const QString propertyPath = propertyIt.key();
    const auto &propertyStack = propertyIt.value();
    for (std::size_t layerIndex = 0; layerIndex < propertyStack.layerCount(); ++layerIndex) {
      const auto &animationLayer = propertyStack.layer(layerIndex);
      PropertyGroup animationGroup(
          QStringLiteral("Anim: %1 / Layer %2")
              .arg(propertyPath)
              .arg(static_cast<int>(layerIndex + 1)));
      const QString prefix = QStringLiteral("animationLayers.%1.%2").arg(
          propertyPath, QString::number(static_cast<int>(layerIndex)));
      auto weight = makeProp(prefix + QStringLiteral(".weight"), PropertyType::Float,
                              static_cast<double>(animationLayer.state.weight), -120);
      weight->setDisplayLabel(QStringLiteral("Weight"));
      weight->setHardRange(0.0, 1.0);
      weight->setSoftRange(0.0, 1.0);
      weight->setStep(0.01);
      animationGroup.addProperty(weight);
      auto value = makeProp(prefix + QStringLiteral(".value"), PropertyType::Float,
                            static_cast<double>(animationLayer.values.current()), -121);
      value->setDisplayLabel(QStringLiteral("Value"));
      value->setStep(0.01);
      animationGroup.addProperty(value);
      auto interpolation = makeProp(
          prefix + QStringLiteral(".interpolation"), PropertyType::Integer,
          static_cast<int>(animationLayer.values.getKeyFrameInterpolationAt(
              FramePosition(currentFrame()))), -122);
      interpolation->setDisplayLabel(QStringLiteral("Interpolation"));
      interpolation->setHardRange(0.0, 32.0);
      interpolation->setStep(1.0);
      animationGroup.addProperty(interpolation);
      auto muted = makeProp(prefix + QStringLiteral(".muted"), PropertyType::Boolean,
                            animationLayer.state.muted, -119);
      muted->setDisplayLabel(QStringLiteral("Mute"));
      animationGroup.addProperty(muted);
      auto solo = makeProp(prefix + QStringLiteral(".solo"), PropertyType::Boolean,
                           animationLayer.state.solo, -118);
      solo->setDisplayLabel(QStringLiteral("Solo"));
      animationGroup.addProperty(solo);
      auto mode = makeProp(prefix + QStringLiteral(".blendMode"), PropertyType::Integer,
                           animationLayer.state.blendMode ==
                                   ArtifactCore::AnimationLayerBlendMode::Override
                               ? 1
                               : 0,
                           -117);
      mode->setDisplayLabel(QStringLiteral("Blend Mode (0 Additive / 1 Override)"));
      mode->setHardRange(0.0, 1.0);
      mode->setStep(1.0);
      animationGroup.addProperty(mode);
      groups.push_back(std::move(animationGroup));
    }
  }
  groups.push_back(std::move(layerGroup));
  appendMaskPropertyGroups(groups);
  return groups;
}

std::vector<ArtifactCore::PropertyGroup> ArtifactAbstractLayer::getComponentPropertyGroups() const {
  std::vector<ArtifactCore::PropertyGroup> groups;
  groups.reserve(16);
  auto makeProp = [this](const QString& name, PropertyType type, const QVariant& value, int prio) {
    return persistentLayerProperty(name, type, value, prio);
  };
  PropertyGroup componentGroup(QStringLiteral("Components"));
  componentGroup.addProperty(makeProp(QStringLiteral("component.script.enabled"), PropertyType::Boolean, impl_->scriptComponentEnabled_, -100));
  componentGroup.addProperty(makeProp(QStringLiteral("component.cloner.enabled"), PropertyType::Boolean, impl_->clonerComponentEnabled_, -90));
  componentGroup.addProperty(makeProp(QStringLiteral("component.collision.enabled"), PropertyType::Boolean, impl_->collisionComponentEnabled_, -89));
  componentGroup.addProperty(makeProp(QStringLiteral("component.collision.displayColor"), PropertyType::Color, impl_->collisionDisplayColor_, -88));
  componentGroup.addProperty(makeProp(QStringLiteral("component.crowd.enabled"), PropertyType::Boolean, impl_->crowdComponentEnabled_, -88));
  componentGroup.addProperty(makeProp(QStringLiteral("component.particleEmitter.enabled"), PropertyType::Boolean, impl_->particleEmitterComponentEnabled_, -87));
  componentGroup.addProperty(makeProp(QStringLiteral("component.fluid.enabled"), PropertyType::Boolean, impl_->fluidComponentEnabled_, -86));
  groups.push_back(std::move(componentGroup));
  return groups;
}

} // namespace Artifact
