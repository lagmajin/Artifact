module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>

export module Artifact.Effect.Ofx.Impl;

import Image.ImageF32x4RGBAWithCache;
import Artifact.Effect.ImplBase;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;
import Artifact.Effect.Ofx.Host;

namespace Artifact {
namespace Ofx {

using namespace ArtifactCore;

class ArtifactOfxEffect final : public ArtifactAbstractEffect {
public:
  explicit ArtifactOfxEffect(const OfxPluginDescriptor &descriptor) {
    setEffectID(UniString::fromQString(QStringLiteral("ofx.%1")
                                           .arg(descriptor.identifier.toQString())));
    setDisplayName(QStringLiteral("OFX: %1").arg(descriptor.identifier.toQString()));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    addMetadataProperties(descriptor);
    addGenericBridgeParameters();
    addPreviewProperties(descriptor.previewProperties);
  }

  void apply(const ImageF32x4RGBAWithCache &src,
             ImageF32x4RGBAWithCache &dst) override {
    // OFX processing is not wired yet, so keep the effect as a stable passthrough.
    // The bridge parameters are already stored here and will be used once the
    // real OFX render path lands.
    if (bypass_ || mix_ <= 0.0) {
      dst = src.DeepCopy();
      return;
    }
    dst = src.DeepCopy();
  }

  std::vector<AbstractProperty> getProperties() const override {
    return properties_;
  }

  void setPropertyValue(const UniString &name, const QVariant &value) override {
    const QString key = name.toQString();
    for (auto &property : properties_) {
      if (property.getName().compare(key, Qt::CaseInsensitive) != 0) {
        continue;
      }
      property.setValue(value);
      syncBridgeState(property);
      break;
    }
  }

  void registerParameter(const AbstractProperty &property) {
    properties_.push_back(property);
  }

private:
  void addMetadataProperties(const OfxPluginDescriptor &descriptor) {
    AbstractProperty pathProp;
    pathProp.setName(QStringLiteral("ofx.plugin.path"));
    pathProp.setDisplayLabel(QStringLiteral("Plugin Path"));
    pathProp.setType(PropertyType::String);
    pathProp.setValue(descriptor.pluginPath.toQString());
    pathProp.setAnimatable(false);
    properties_.push_back(pathProp);

    AbstractProperty idProp;
    idProp.setName(QStringLiteral("ofx.plugin.identifier"));
    idProp.setDisplayLabel(QStringLiteral("Identifier"));
    idProp.setType(PropertyType::String);
    idProp.setValue(descriptor.identifier.toQString());
    idProp.setAnimatable(false);
    properties_.push_back(idProp);

    AbstractProperty versionProp;
    versionProp.setName(QStringLiteral("ofx.plugin.version"));
    versionProp.setDisplayLabel(QStringLiteral("Version"));
    versionProp.setType(PropertyType::String);
    versionProp.setValue(descriptor.version.toQString());
    versionProp.setAnimatable(false);
    properties_.push_back(versionProp);
  }

  void addGenericBridgeParameters() {
    AbstractProperty mixProp;
    mixProp.setName(QStringLiteral("ofx.mix"));
    mixProp.setDisplayLabel(QStringLiteral("Mix"));
    mixProp.setType(PropertyType::Float);
    mixProp.setDefaultValue(1.0);
    mixProp.setValue(1.0);
    mixProp.setMinValue(0.0);
    mixProp.setMaxValue(1.0);
    mixProp.setAnimatable(true);
    properties_.push_back(mixProp);

    AbstractProperty bypassProp;
    bypassProp.setName(QStringLiteral("ofx.bypass"));
    bypassProp.setDisplayLabel(QStringLiteral("Bypass"));
    bypassProp.setType(PropertyType::Boolean);
    bypassProp.setDefaultValue(false);
    bypassProp.setValue(false);
    bypassProp.setAnimatable(true);
    properties_.push_back(bypassProp);
  }

  void addPreviewProperties(const std::vector<AbstractProperty> &previewProps) {
    for (const auto &property : previewProps) {
      if (property.getName().isEmpty()) {
        continue;
      }
      properties_.push_back(property);
    }
  }

  void syncBridgeState(const AbstractProperty &property) {
    const QString key = property.getName();
    if (key.compare(QStringLiteral("ofx.mix"), Qt::CaseInsensitive) == 0) {
      mix_ = std::clamp(property.getValue().toDouble(), 0.0, 1.0);
    } else if (key.compare(QStringLiteral("ofx.bypass"), Qt::CaseInsensitive) == 0) {
      bypass_ = property.getValue().toBool();
    }
  }

  std::vector<AbstractProperty> properties_;
  double mix_ = 1.0;
  bool bypass_ = false;
};

export std::unique_ptr<ArtifactAbstractEffect>
makeOfxEffect(const OfxPluginDescriptor &descriptor) {
  return std::make_unique<ArtifactOfxEffect>(descriptor);
}

export std::vector<AbstractProperty>
makeOfxBridgePreviewProperties(const OfxPluginDescriptor &descriptor) {
  std::vector<AbstractProperty> props;

  AbstractProperty mixProp;
  mixProp.setName(QStringLiteral("ofx.mix"));
  mixProp.setDisplayLabel(QStringLiteral("Mix"));
  mixProp.setType(PropertyType::Float);
  mixProp.setDefaultValue(1.0);
  mixProp.setValue(1.0);
  mixProp.setMinValue(0.0);
  mixProp.setMaxValue(1.0);
  mixProp.setAnimatable(true);
  props.push_back(mixProp);

  AbstractProperty bypassProp;
  bypassProp.setName(QStringLiteral("ofx.bypass"));
  bypassProp.setDisplayLabel(QStringLiteral("Bypass"));
  bypassProp.setType(PropertyType::Boolean);
  bypassProp.setDefaultValue(false);
  bypassProp.setValue(false);
  bypassProp.setAnimatable(true);
  props.push_back(bypassProp);

  AbstractProperty idProp;
  idProp.setName(QStringLiteral("ofx.plugin.identifier"));
  idProp.setDisplayLabel(QStringLiteral("Identifier"));
  idProp.setType(PropertyType::String);
  idProp.setValue(descriptor.identifier.toQString());
  idProp.setAnimatable(false);
  props.push_back(idProp);

  return props;
}

} // namespace Ofx
} // namespace Artifact
