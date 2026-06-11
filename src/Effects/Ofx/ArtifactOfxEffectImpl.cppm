module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>
#include <windows.h>
#include <ofx/ofxCore.h>
#include <ofx/ofxImageEffect.h>

export module Artifact.Effect.Ofx.Impl;

import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
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
  explicit ArtifactOfxEffect(const OfxPluginDescriptor &descriptor)
      : descriptor_(descriptor) {
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
    if (bypass_ || mix_ <= 0.0) {
      dst = src.DeepCopy();
      return;
    }

    if (!descriptor_.libraryHandle || !descriptor_.descriptorState) {
      dst = src.DeepCopy();
      return;
    }

    auto &host = ArtifactOfxHost::instance();
    if (!renderState_) {
      renderState_ = host.createRenderInstance(descriptor_.identifier);
      if (!renderState_) {
        dst = src.DeepCopy();
        return;
      }
      auto *plugin = findPlugin();
      if (plugin) {
        pluginActionCreateInstance(plugin, *renderState_);
        OfxPointD scale{1.0, 1.0};
        pluginActionBeginSequenceRender(plugin, *renderState_, scale);
      }
    }

    auto *plugin = findPlugin();
    if (!plugin) {
      dst = src.DeepCopy();
      return;
    }

    auto &srcImage = src.image();
    const int w = src.width();
    const int h = src.height();
    const int rowBytes = static_cast<int>(w * 4 * sizeof(float));
    const auto *srcData = reinterpret_cast<const unsigned char *>(srcImage.rgba32fData());

    dst = src.DeepCopy();
    auto &dstImage = dst.image();
    auto *dstData = reinterpret_cast<unsigned char *>(dstImage.rgba32fData());

    renderState_->renderFrame.srcPixelData = srcData;
    renderState_->renderFrame.srcWidth = w;
    renderState_->renderFrame.srcHeight = h;
    renderState_->renderFrame.srcRowBytes = rowBytes;
    renderState_->renderFrame.dstPixelData = dstData;
    renderState_->renderFrame.dstRowBytes = rowBytes;

    for (const auto &prop : properties_) {
      auto it = renderState_->paramSet.params.find(prop.getName().toStdString());
      if (it != renderState_->paramSet.params.end() && it->second) {
        const QVariant val = prop.getValue();
        const QString ptype = it->second->paramType;
        if (ptype.contains(QStringLiteral("String"), Qt::CaseInsensitive)) {
          it->second->currentStringValue = val.toString();
          it->second->currentUtf8Value = val.toString().toUtf8().toStdString();
          it->second->currentValues = {it->second->currentStringValue};
        } else if (ptype.contains(QStringLiteral("Boolean"), Qt::CaseInsensitive)) {
          it->second->currentValues = {val.toBool() ? 1 : 0};
        } else if (ptype.contains(QStringLiteral("Integer"), Qt::CaseInsensitive)) {
          it->second->currentValues = {val.toInt()};
        } else if (ptype.contains(QStringLiteral("RGB"), Qt::CaseInsensitive)) {
          QColor c = val.value<QColor>();
          it->second->currentValues = {c.redF(), c.greenF(), c.blueF()};
        } else if (ptype.contains(QStringLiteral("RGBA"), Qt::CaseInsensitive)) {
          QColor c = val.value<QColor>();
          it->second->currentValues = {c.redF(), c.greenF(), c.blueF(), c.alphaF()};
        } else {
          it->second->currentValues = {val.toDouble()};
        }
      }
    }

    OfxPointD scale{1.0, 1.0};
    pluginActionRender(plugin, *renderState_, 0.0, scale);
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

  OfxPlugin *findPlugin() {
    if (!descriptor_.libraryHandle) return nullptr;
    auto fn = reinterpret_cast<OfxGetPluginFn>(
        GetProcAddress(descriptor_.libraryHandle, "OfxGetPlugin"));
    auto countFn = reinterpret_cast<int(*)()>(
        GetProcAddress(descriptor_.libraryHandle, "OfxGetNumberOfPlugins"));
    if (!fn || !countFn) return nullptr;
    int count = countFn();
    for (int i = 0; i < count; ++i) {
      OfxPlugin *p = fn(i);
      if (p && p->pluginIdentifier &&
          strlen(p->pluginIdentifier) > 0 &&
          descriptor_.identifier.toQString() == QString::fromLatin1(p->pluginIdentifier)) {
        return p;
      }
    }
    return nullptr;
  }

  std::vector<AbstractProperty> properties_;
  double mix_ = 1.0;
  bool bypass_ = false;
  OfxPluginDescriptor descriptor_;
  std::shared_ptr<ImageEffectState> renderState_;
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
