module;
#include <algorithm>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QDebug>
#include <memory>
#include <vector>
#include <wobjectimpl.h>

module Artifact.Service.Effect;

import Utils.String.UniString;
import Utils.Id;
import Artifact.Effect.Abstract;
import Artifact.Effects.Manager;
import Artifact.Effect.Ofx.Host;
import Artifact.Effect.Ofx.Impl;
import Artifact.Project.PresetManager;
import Artifact.Service.Project;
import Artifact.Event.Types;
import Event.Bus;
import BrightnessEffect;
import Artifact.Effect.Creative;
import Artifact.Effect.DirectionalGlow;
import Artifact.Effect.Rasterizer.Sharpen;
import Artifact.Effect.Rasterizer.FindEdges;
import Artifact.Effect.Rasterizer.RadialBlur;
import Artifact.Effect.Rasterizer.AddNoise;
import Artifact.Effect.Rasterizer.RadialShadow;
import Artifact.Effect.Rasterizer.OpticsCompensation;
import Artifact.Effect.Rasterizer.Mosaic;
import Artifact.Effect.Rasterizer.TurbulentDisplace;
import Artifact.Effect.Rasterizer.Bevel;
import Artifact.Effect.Rasterizer.LinearWipe;
import Artifact.Effect.Glow;
import Artifact.Effect.Glow.EdgeBloom;
import Artifact.Effect.Glow.ChromaticGlow;
import Artifact.Effect.Glow.ReactiveGlow;
import Artifact.Effect.GauusianBlur;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.LensDistortion;
import Artifact.Effect.LiftGammaGain;
import ExposureEffect;
import Artifact.Effect.Rasterizer.Blur;
import Artifact.Effect.Rasterizer.DropShadow;
import Artifact.Effect.Render.PBRMaterial;
import Artifact.Effect.Spherize;
import Artifact.Effect.Transform.Bend;
import Artifact.Effect.Transform.Twist;
import Artifact.Effect.Wave;
import HueAndSaturation;
import Artifact.Effect.WhiteBalance;
import TritoneEffect;
import ColoramaEffect;
import PhotoFilterEffect;
import GradientRampEffect;
import FillEffect;
import ColorBalanceEffect;
import LevelsEffect;
import ChannelMixerEffect;
import SelectiveColorEffect;
import ColorWheelsEffect;
import CurvesEffect;

namespace Artifact
{
 using namespace ArtifactCore;

namespace {
QString stripDuplicateSuffix(const QString &effectId) {
  QString base = effectId.trimmed();
  while (true) {
    if (base.endsWith(QStringLiteral("__copy"))) {
      base.chop(QStringLiteral("__copy").size());
      continue;
    }

    const int copyPos = base.lastIndexOf(QStringLiteral("__copy"));
    if (copyPos >= 0) {
      const QString copyTail = base.mid(copyPos + QStringLiteral("__copy").size());
      const bool hasNumericSuffix =
          !copyTail.isEmpty() &&
          std::all_of(copyTail.begin(), copyTail.end(), [](const QChar ch) {
            return ch.isDigit();
          });
      if (hasNumericSuffix) {
        base = base.left(copyPos);
        continue;
      }
    }

    const int dashPos = base.lastIndexOf(QLatin1Char('-'));
    if (dashPos >= 0) {
      const QString dashTail = base.mid(dashPos + 1);
      const bool hasNumericSuffix =
          !dashTail.isEmpty() &&
          std::all_of(dashTail.begin(), dashTail.end(), [](const QChar ch) {
            return ch.isDigit();
          });
      if (hasNumericSuffix) {
        base = base.left(dashPos);
        continue;
      }
    }

    break;
  }
  return base;
}
} // namespace

W_OBJECT_IMPL(ArtifactEffectService)

 class ArtifactEffectService::Impl
 {
 public:
  ArtifactGlobalEffectManager* effectManager_ = nullptr;

  Impl() {
   effectManager_ = ArtifactGlobalEffectManager::effectManager();
  }
  ~Impl() = default;
 };

 ArtifactEffectService::ArtifactEffectService(QObject* parent)
  : QObject(parent), impl_(new Impl())
 {
 }

 ArtifactEffectService::~ArtifactEffectService()
 {
  delete impl_;
 }

 ArtifactEffectService* ArtifactEffectService::instance()
 {
  static ArtifactEffectService svc;
  return &svc;
 }

 std::unique_ptr<ArtifactAbstractEffect> ArtifactEffectService::createEffect(const EffectID& id) const
 {
  const QString effectId = id.toString();
  if (effectId == QStringLiteral("effect.colorcorrection.colorwheels")) {
   return std::make_unique<ColorWheelsEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.curves")) {
   return std::make_unique<CurvesEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.brightness")) {
   auto effect = std::make_unique<BrightnessEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Brightness / Contrast"));
   return effect;
  }
  if (effectId == QStringLiteral("effect.colorcorrection.hsl")) {
   auto effect = std::make_unique<HueAndSaturation>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Hue / Saturation"));
   return effect;
  }
  if (effectId == QStringLiteral("effect.colorcorrection.exposure")) {
   auto effect = std::make_unique<ExposureEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Exposure"));
   return effect;
  }
  if (effectId == QStringLiteral("brightness")) {
   auto effect = std::make_unique<BrightnessEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Brightness / Contrast"));
   return effect;
  }
  if (effectId == QStringLiteral("hue_saturation")) {
   auto effect = std::make_unique<HueAndSaturation>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Hue / Saturation"));
   return effect;
  }
  if (effectId == QStringLiteral("exposure")) {
   auto effect = std::make_unique<ExposureEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Exposure"));
   return effect;
  }
  if (effectId == QStringLiteral("effect.colorcorrection.tint")) {
   auto effect = std::make_unique<WhiteBalanceEffect>();
   effect->setDisplayName(UniString("Tint"));
   effect->setEffectID(UniString::fromQString(effectId));
   return effect;
  }
  if (effectId == QStringLiteral("effect.colorcorrection.photofilter")) {
   return std::make_unique<PhotoFilterEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.gradientramp")) {
   return std::make_unique<GradientRampEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.fill")) {
   return std::make_unique<FillEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.tritone")) {
   return std::make_unique<TritoneEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.colorama")) {
   return std::make_unique<ColoramaEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.colorbalance")) {
   return std::make_unique<ColorBalanceEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.levels")) {
   return std::make_unique<LevelsEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.channelmixer")) {
   return std::make_unique<ChannelMixerEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.selectivecolor")) {
   return std::make_unique<SelectiveColorEffect>();
  }
  if (effectId == QStringLiteral("chroma_key")) {
   auto effect = std::make_unique<ChromaKeyEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Chroma Key"));
   return effect;
  }
  if (effectId == QStringLiteral("Effect.Keying.ChromaKey")) {
   auto effect = std::make_unique<ChromaKeyEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Chroma Key"));
   return effect;
  }
  if (effectId == QStringLiteral("effect.blur.gaussian")) {
   auto effect = std::make_unique<GaussianBlur>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Gaussian Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("drop_shadow")) {
   auto effect = std::make_unique<DropShadowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Drop Shadow"));
   return effect;
  }
  if (effectId == QStringLiteral("glow")) {
   auto effect = std::make_unique<GlowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Glow"));
   return effect;
  }
  if (effectId == QStringLiteral("edge_bloom") ||
      effectId == QStringLiteral("effect.glow.edgebloom")) {
   auto effect = std::make_unique<EdgeBloomEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Edge Bloom"));
   return effect;
  }
  if (effectId == QStringLiteral("chromatic_glow") ||
      effectId == QStringLiteral("effect.glow.chromatic")) {
   auto effect = std::make_unique<ChromaticGlowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Chromatic Glow"));
   return effect;
  }
  if (effectId == QStringLiteral("reactive_glow") ||
      effectId == QStringLiteral("effect.glow.reactive")) {
   auto effect = std::make_unique<ReactiveGlowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Reactive Glow"));
   return effect;
  }
  if (effectId == QStringLiteral("blur")) {
   auto effect = std::make_unique<BlurEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("wave")) {
   auto effect = std::make_unique<WaveEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Wave"));
   return effect;
  }
  if (effectId == QStringLiteral("spherize")) {
   auto effect = std::make_unique<SpherizeEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Spherize"));
   return effect;
  }
  if (effectId == QStringLiteral("directional_glow")) {
   auto effect = std::make_unique<DirectionalGlowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Directional Glow / Streaks"));
   return effect;
  }
  if (effectId == QStringLiteral("lift_gamma_gain")) {
   auto effect = std::make_unique<LiftGammaGainEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Lift / Gamma / Gain"));
   return effect;
  }
  if (effectId == QStringLiteral("lens_distortion")) {
   auto effect = std::make_unique<LensDistortionEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Lens Distortion"));
   return effect;
  }
  if (effectId == QStringLiteral("twist")) {
   auto effect = std::make_unique<TwistTransform>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Twist (Geo Transform)"));
   return effect;
  }
  if (effectId == QStringLiteral("bend")) {
   auto effect = std::make_unique<BendTransform>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Bend (Geo Transform)"));
   return effect;
  }
  if (effectId == QStringLiteral("pbr_material")) {
   auto effect = std::make_unique<PBRMaterialEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("PBR Material"));
   return effect;
  }
  if (effectId == QStringLiteral("builtin.halftone")) {
   auto effect = std::make_unique<ArtifactHalftoneEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Halftone"));
   return effect;
  }
  if (effectId == QStringLiteral("halftone")) {
   auto effect = std::make_unique<ArtifactHalftoneEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Halftone"));
   return effect;
  }
  if (effectId == QStringLiteral("sharpen")) {
   auto effect = std::make_unique<SharpenEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Sharpen"));
   return effect;
  }
  if (effectId == QStringLiteral("find_edges")) {
   auto effect = std::make_unique<FindEdgesEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Find Edges"));
   return effect;
  }
  if (effectId == QStringLiteral("radial_blur")) {
   auto effect = std::make_unique<RadialBlurEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Radial Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("add_noise")) {
   auto effect = std::make_unique<AddNoiseEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Add Noise"));
   return effect;
  }
  if (effectId == QStringLiteral("radial_shadow")) {
   auto effect = std::make_unique<RadialShadowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Radial Shadow"));
   return effect;
  }
  if (effectId == QStringLiteral("optics_compensation")) {
   auto effect = std::make_unique<OpticsCompensationEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Optics Compensation"));
   return effect;
  }
  if (effectId == QStringLiteral("mosaic")) {
   auto effect = std::make_unique<MosaicEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Mosaic"));
   return effect;
  }
  if (effectId == QStringLiteral("turbulent_displace")) {
   auto effect = std::make_unique<TurbulentDisplaceEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Turbulent Displace"));
   return effect;
  }
  if (effectId == QStringLiteral("bevel")) {
   auto effect = std::make_unique<BevelEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Bevel"));
   return effect;
  }
  if (effectId == QStringLiteral("linear_wipe")) {
   auto effect = std::make_unique<LinearWipeEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Linear Wipe"));
   return effect;
  }
  if (effectId.startsWith(QStringLiteral("ofx."))) {
   const QString pluginId = effectId.mid(QStringLiteral("ofx.").size());
   Artifact::Ofx::ArtifactOfxHost::instance().initialize();
   for (const auto& plugin : Artifact::Ofx::ArtifactOfxHost::instance().getLoadedPlugins()) {
    if (plugin.identifier.toQString().compare(pluginId, Qt::CaseInsensitive) != 0) {
     continue;
    }
    return Artifact::Ofx::makeOfxEffect(plugin);
   }

   auto effect = std::make_unique<ArtifactAbstractEffect>();
   effect->setEffectID(effectId);
   effect->setDisplayName(QStringLiteral("OFX: %1").arg(pluginId));
   return effect;
  }

  if (!impl_->effectManager_) return nullptr;
  // Delegate to the global effect manager's factory
  impl_->effectManager_->factoryByID(id);
  // The manager creates and registers internally; retrieve via ID
  // For now return nullptr as factoryByID doesn't return the effect directly
  // The actual creation is done through project service
  return nullptr;
 }

 std::vector<EffectInfo> ArtifactEffectService::availableEffects() const
 {
  // Return known effect types
 std::vector<EffectInfo> effects;
  effects.push_back({EffectID("effect.colorcorrection.brightness"), "Brightness / Contrast"});
  effects.push_back({EffectID("effect.colorcorrection.hsl"), "Hue / Saturation"});
  effects.push_back({EffectID("effect.colorcorrection.exposure"), "Exposure"});
  effects.push_back({EffectID("effect.colorcorrection.colorwheels"), "Color Wheels"});
  effects.push_back({EffectID("effect.colorcorrection.curves"), "Curves"});
  effects.push_back({EffectID("effect.colorcorrection.tint"), "Tint"});
  effects.push_back({EffectID("effect.colorcorrection.photofilter"), "Photo Filter"});
  effects.push_back({EffectID("effect.colorcorrection.gradientramp"), "Gradient Ramp"});
  effects.push_back({EffectID("effect.colorcorrection.fill"), "Fill"});
  effects.push_back({EffectID("effect.colorcorrection.tritone"), "Tritone"});
  effects.push_back({EffectID("effect.colorcorrection.colorama"), "Colorama"});
  effects.push_back({EffectID("effect.colorcorrection.colorbalance"), "Color Balance"});
  effects.push_back({EffectID("effect.colorcorrection.levels"), "Levels"});
  effects.push_back({EffectID("effect.colorcorrection.channelmixer"), "Channel Mixer"});
  effects.push_back({EffectID("effect.colorcorrection.selectivecolor"), "Selective Color"});
  effects.push_back({EffectID("chroma_key"), "Chroma Key"});
  effects.push_back({EffectID("drop_shadow"), "Drop Shadow"});
  effects.push_back({EffectID("directional_glow"), "Directional Glow / Streaks"});
  effects.push_back({EffectID("glow"), "Glow"});
  effects.push_back({EffectID("edge_bloom"), "Edge Bloom"});
  effects.push_back({EffectID("chromatic_glow"), "Chromatic Glow"});
  effects.push_back({EffectID("reactive_glow"), "Reactive Glow"});
  effects.push_back({EffectID("effect.blur.gaussian"), "Gaussian Blur"});
  effects.push_back({EffectID("blur"), "Blur"});
  effects.push_back({EffectID("lift_gamma_gain"), "Lift / Gamma / Gain"});
  effects.push_back({EffectID("lens_distortion"), "Lens Distortion"});
  effects.push_back({EffectID("wave"), "Wave"});
  effects.push_back({EffectID("spherize"), "Spherize"});
  effects.push_back({EffectID("twist"), "Twist"});
  effects.push_back({EffectID("bend"), "Bend"});
  effects.push_back({EffectID("pbr_material"), "PBR Material"});
  effects.push_back({EffectID("halftone"), "Halftone"});
  effects.push_back({EffectID("sharpen"), "Sharpen"});
  effects.push_back({EffectID("find_edges"), "Find Edges"});
  effects.push_back({EffectID("radial_blur"), "Radial Blur"});
  effects.push_back({EffectID("add_noise"), "Add Noise"});
  effects.push_back({EffectID("radial_shadow"), "Radial Shadow"});
  effects.push_back({EffectID("optics_compensation"), "Optics Compensation"});
  effects.push_back({EffectID("mosaic"), "Mosaic"});
  effects.push_back({EffectID("turbulent_displace"), "Turbulent Displace"});
  effects.push_back({EffectID("bevel"), "Bevel"});
  effects.push_back({EffectID("linear_wipe"), "Linear Wipe"});

  Artifact::Ofx::ArtifactOfxHost::instance().initialize();
  for (const auto& plugin : Artifact::Ofx::ArtifactOfxHost::instance().getLoadedPlugins()) {
   const QString pluginId = plugin.identifier.toQString().trimmed();
   if (pluginId.isEmpty()) {
    continue;
   }

   effects.push_back({
       EffectID(QStringLiteral("ofx.%1").arg(pluginId)),
       QStringLiteral("OFX: %1").arg(pluginId),
   });
  }
  return effects;
 }

 QStringList ArtifactEffectService::availableEffectNames() const
 {
  QStringList names;
  for (const auto& info : availableEffects()) {
   names.append(info.displayName);
  }
  return names;
 }

 EffectServiceResult ArtifactEffectService::addEffectToLayer(const LayerID& layerId, const EffectID& effectId)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  // Create the effect through the global manager
  auto effect = createEffect(effectId);
  if (!effect) {
   effect = std::make_unique<ArtifactAbstractEffect>();
   effect->setEffectID(effectId.toString());
   effect->setDisplayName(effectId.toString());
  }

  auto effectPtr = std::shared_ptr<ArtifactAbstractEffect>(effect.release());
  if (ps->addEffectToLayerInCurrentComposition(layerId, effectPtr)) {
   const QString actualEffectId = effectPtr ? effectPtr->effectID().toQString() : effectId.toString();
   Q_EMIT effectAdded(layerId, actualEffectId);
   return EffectServiceResult::ok(actualEffectId);
  }
  return EffectServiceResult::fail("Failed to add effect");
 }

 EffectServiceResult ArtifactEffectService::removeEffectFromLayer(const LayerID& layerId, const QString& effectId)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  if (ps->removeEffectFromLayerInCurrentComposition(layerId, effectId)) {
   Q_EMIT effectRemoved(layerId, effectId);
   return EffectServiceResult::ok(effectId);
  }
  return EffectServiceResult::fail("Failed to remove effect");
 }

 EffectServiceResult ArtifactEffectService::setEffectEnabled(const LayerID& layerId, const QString& effectId, bool enabled)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  if (ps->setEffectEnabledInLayerInCurrentComposition(layerId, effectId, enabled)) {
   Q_EMIT effectChanged(layerId, effectId);
   return EffectServiceResult::ok(effectId);
  }
  return EffectServiceResult::fail("Failed to set effect enabled state");
 }

 EffectServiceResult ArtifactEffectService::moveEffect(const LayerID& layerId, const QString& effectId, int direction)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  if (ps->moveEffectInLayerInCurrentComposition(layerId, effectId, direction)) {
   Q_EMIT effectOrderChanged(layerId);
   return EffectServiceResult::ok(effectId);
  }
  return EffectServiceResult::fail("Failed to move effect");
 }

 EffectServiceResult ArtifactEffectService::duplicateEffect(const LayerID& layerId, const QString& effectId)
 {
  // Find the existing effect and clone its properties
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  auto comp = ps->currentComposition().lock();
  if (!comp || layerId.isNil()) {
   return EffectServiceResult::fail("Composition not available");
  }
  auto layer = comp->layerById(layerId);
  if (!layer) {
   return EffectServiceResult::fail("Layer not available");
  }

  QString baseId = effectId.trimmed();
  if (baseId.isEmpty()) {
   baseId = QStringLiteral("effect");
  }
  QString sourceDisplayName = effectId;
  bool sourceEnabled = true;
  std::shared_ptr<ArtifactAbstractEffect> sourceEffect;
  for (const auto &effect : layer->getEffects()) {
   if (!effect || effect->effectID().toQString() != effectId) {
    continue;
   }
   sourceEffect = effect;
   const QString displayName = effect->displayName().toQString().trimmed();
   if (!displayName.isEmpty()) {
    sourceDisplayName = displayName;
   }
   sourceEnabled = effect->isEnabled();
   break;
  }
  QString sourceTypeId =
      sourceEffect ? stripDuplicateSuffix(sourceEffect->effectID().toQString())
                   : baseId;
  if (sourceTypeId.isEmpty()) {
    sourceTypeId = baseId;
  }
  std::unique_ptr<ArtifactAbstractEffect> effectCopy =
      createEffect(EffectID(sourceTypeId));
  if (!effectCopy) {
    effectCopy = std::make_unique<ArtifactAbstractEffect>();
  }
  const QString copyBaseId = sourceTypeId.isEmpty() ? baseId : sourceTypeId;
  QString copyId = copyBaseId + QStringLiteral("__copy");
  int copyIndex = 2;
  const auto existingEffects = layer->getEffects();
  const auto idExists = [&existingEffects](const QString& candidate) {
   return std::any_of(existingEffects.begin(), existingEffects.end(),
    [&candidate](const std::shared_ptr<ArtifactAbstractEffect>& effect) {
     return effect && effect->effectID().toQString() == candidate;
    });
  };
  while (idExists(copyId)) {
   copyId = QStringLiteral("%1__copy%2").arg(copyBaseId).arg(copyIndex++);
  }
  effectCopy->setEffectID(copyId);
  effectCopy->setDisplayName(QStringLiteral("%1 (copy)").arg(sourceDisplayName));
  effectCopy->setEnabled(sourceEnabled);
  if (sourceEffect) {
   effectCopy->setPipelineStage(sourceEffect->pipelineStage());
   effectCopy->setComputeMode(sourceEffect->computeMode());
  }
  if (sourceEffect) {
   for (const auto &property : sourceEffect->getProperties()) {
    effectCopy->setPropertyValue(
        UniString(property.getName().toStdString()), property.getValue());
   }
  }

  auto copyPtr = std::shared_ptr<ArtifactAbstractEffect>(effectCopy.release());
  if (ps->addEffectToLayerInCurrentComposition(layerId, copyPtr)) {
   const QString newEffectId = copyPtr ? copyPtr->effectID().toQString() : copyId;
   Q_EMIT effectAdded(layerId, newEffectId);
   return EffectServiceResult::ok(newEffectId);
  }
  return EffectServiceResult::fail("Failed to duplicate effect");
 }

 EffectServiceResult ArtifactEffectService::setEffectProperty(const LayerID& layerId,
  const QString& effectId, const QString& propertyName, const QVariant& value)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  auto comp = ps->currentComposition().lock();
  if (!comp || layerId.isNil()) {
   return EffectServiceResult::fail("Composition not available");
  }
  auto layer = comp->layerById(layerId);
  if (!layer) {
   return EffectServiceResult::fail("Layer not available");
  }
  const QString normalizedPropertyName = propertyName.trimmed();
  if (normalizedPropertyName.isEmpty()) {
   return EffectServiceResult::fail("Property not found");
  }

  for (const auto &effect : layer->getEffects()) {
   if (!effect || effect->effectID().toQString() != effectId) {
    continue;
   }
   const auto properties = effect->getProperties();
   const auto propertyExists = std::any_of(
       properties.begin(), properties.end(),
       [&normalizedPropertyName](const ArtifactCore::AbstractProperty &property) {
         return property.getName().compare(normalizedPropertyName, Qt::CaseInsensitive) == 0;
       });
   if (!propertyExists) {
    return EffectServiceResult::fail("Property not found");
   }
   effect->setPropertyValue(UniString::fromQString(normalizedPropertyName), value);
   ArtifactCore::globalEventBus().publish(LayerChangedEvent{
       comp->id().toString(), layerId.toString(),
       LayerChangedEvent::ChangeType::Modified});
   notifyLayerMutation(comp->id().toString(), layerId);
   notifyProjectMutation(impl_->projectManager());
   Q_EMIT effectChanged(layerId, effectId);
   return EffectServiceResult::ok(effectId);
  }

  return EffectServiceResult::fail("Effect not found");
 }

 bool ArtifactEffectService::saveEffectPreset(const ArtifactAbstractEffectPtr& effect, const QString& filePath) const
 {
  return ArtifactPresetManager::saveEffectPreset(effect, filePath);
 }

 bool ArtifactEffectService::loadEffectPreset(ArtifactAbstractEffectPtr& effect, const QString& filePath) const
 {
  return ArtifactPresetManager::loadEffectPreset(effect, filePath);
 }

}; // namespace Artifact
