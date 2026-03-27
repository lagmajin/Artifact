module;
#include <QObject>
#include <wobjectdefs.h>
#include <wobjectimpl.h>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <memory>
#include <vector>

export module Artifact.Service.Effect;

import Utils.String.UniString;
import Artifact.Effect.Abstract;
import Artifact.Effects.Manager;
import Utils.Id;

W_REGISTER_ARGTYPE(ArtifactCore::LayerID)

export namespace Artifact
{
 using namespace ArtifactCore;

 struct EffectServiceResult {
  bool success = false;
  QString message;
  QString effectId;

  static EffectServiceResult ok(const QString& id) { return {true, {}, id}; }
  static EffectServiceResult fail(const QString& msg) { return {false, msg, {}}; }
 };

 struct EffectInfo {
  EffectID id;
  QString displayName;
 };

 class ArtifactEffectService : public QObject
 {
  W_OBJECT(ArtifactEffectService)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactEffectService(QObject* parent = nullptr);
  ~ArtifactEffectService();
  static ArtifactEffectService* instance();

  ArtifactEffectService(const ArtifactEffectService&) = delete;
  ArtifactEffectService& operator=(const ArtifactEffectService&) = delete;

  // Effect factory
  std::unique_ptr<ArtifactAbstractEffect> createEffect(const EffectID& id) const;
  std::vector<EffectInfo> availableEffects() const;
  QStringList availableEffectNames() const;

  // Layer effect operations (delegates to ProjectService)
  EffectServiceResult addEffectToLayer(const LayerID& layerId, const EffectID& effectId);
  EffectServiceResult removeEffectFromLayer(const LayerID& layerId, const QString& effectId);
  EffectServiceResult setEffectEnabled(const LayerID& layerId, const QString& effectId, bool enabled);
  EffectServiceResult moveEffect(const LayerID& layerId, const QString& effectId, int direction);
  EffectServiceResult duplicateEffect(const LayerID& layerId, const QString& effectId);

  // Property operations
  EffectServiceResult setEffectProperty(const LayerID& layerId, const QString& effectId,
   const QString& propertyName, const QVariant& value);

  // Preset operations
  bool saveEffectPreset(const ArtifactAbstractEffectPtr& effect, const QString& filePath) const;
  bool loadEffectPreset(ArtifactAbstractEffectPtr& effect, const QString& filePath) const;

 public /*signals*/:
  void effectAdded(const LayerID& layerId, const QString& effectId)
   W_SIGNAL(effectAdded, layerId, effectId);
  void effectRemoved(const LayerID& layerId, const QString& effectId)
   W_SIGNAL(effectRemoved, layerId, effectId);
  void effectChanged(const LayerID& layerId, const QString& effectId)
   W_SIGNAL(effectChanged, layerId, effectId);
  void effectOrderChanged(const LayerID& layerId)
   W_SIGNAL(effectOrderChanged, layerId);
 };

};
