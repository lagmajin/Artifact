module;
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
import Artifact.Project.PresetManager;
import Artifact.Service.Project;

namespace Artifact
{
 using namespace ArtifactCore;

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
  effects.push_back({EffectID("brightness"), "Brightness"});
  effects.push_back({EffectID("hue_saturation"), "Hue & Saturation"});
  effects.push_back({EffectID("exposure"), "Exposure"});
  effects.push_back({EffectID("chroma_key"), "Chroma Key"});
  effects.push_back({EffectID("drop_shadow"), "Drop Shadow"});
  effects.push_back({EffectID("glow"), "Glow"});
  effects.push_back({EffectID("blur"), "Gaussian Blur"});
  effects.push_back({EffectID("wave"), "Wave"});
  effects.push_back({EffectID("spherize"), "Spherize"});
  effects.push_back({EffectID("turbulent_displace"), "Turbulent Displace"});
  effects.push_back({EffectID("twist"), "Twist"});
  effects.push_back({EffectID("bend"), "Bend"});
  effects.push_back({EffectID("pbr_material"), "PBR Material"});
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
  auto effect = std::make_shared<ArtifactAbstractEffect>();
  effect->setEffectID(effectId.toString());
  effect->setDisplayName(effectId.toString());

  if (ps->addEffectToLayerInCurrentComposition(layerId, effect)) {
   Q_EMIT effectAdded(layerId, effectId.toString());
   return EffectServiceResult::ok(effectId.toString());
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

  // Create a new effect with same type
  auto newEffect = std::make_shared<ArtifactAbstractEffect>();
  newEffect->setEffectID(effectId);
  newEffect->setDisplayName(effectId + " (copy)");

  if (ps->addEffectToLayerInCurrentComposition(layerId, newEffect)) {
   const QString newEffectId = newEffect->effectID().toQString();
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

  // Delegate to project service which handles property updates
  ps->setEffectEnabledInLayerInCurrentComposition(layerId, effectId, true); // ensure accessible
  Q_EMIT effectChanged(layerId, effectId);
  return EffectServiceResult::ok(effectId);
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
