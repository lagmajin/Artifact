module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>
#include <QDebug>
#ifdef _WIN32
#include <windows.h>
#endif
#include <ofx/ofxCore.h>
#include <ofx/ofxImageEffect.h>

export module Artifact.Effect.Ofx.Impl;

import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Artifact.Effect.ImplBase;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;
import Time.Rational;
import Artifact.Effect.Ofx.Host;
import Memory.SharedPtr;

namespace Artifact {
namespace Ofx {

using namespace ArtifactCore;
using OfxGetPluginFn = OfxPlugin *(*)(int);

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

  ~ArtifactOfxEffect() override {
    // Tear the plugin instance down while its library is still mapped. A
    // rescan unloads the DLL without notifying live effects, so skipping this
    // leaves the plugin holding resources it never releases.
    if (!renderState_) {
      return;
    }
    // If a rescan already unloaded the library, the entry points we would call
    // are gone; touching renderState_ is still correct because it is host-owned
    // memory, but no plugin code may run.
    if (!ArtifactOfxHost::instance().isGenerationCurrent(descriptor_)) {
      renderState_.reset();
      return;
    }
    auto *plugin = findPlugin();
    if (!plugin) {
      renderState_.reset();
      return;
    }
    OfxPointD scale{1.0, 1.0};
    pluginActionEndSequenceRender(plugin, *renderState_, scale);
    pluginActionDestroyInstance(plugin, *renderState_);
    renderState_.reset();
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
    // A rescan unloads the library this descriptor captured. Calling into the
    // stale handle would be a use-after-free, so pass the frame through until
    // the effect is rebuilt against the new scan.
    if (!host.isGenerationCurrent(descriptor_)) {
      dst = src.DeepCopy();
      return;
    }

    if (!renderState_) {
      renderState_ = host.createRenderInstance(descriptor_.identifier);
      if (!renderState_) {
        dst = src.DeepCopy();
        return;
      }
      auto *plugin = findPlugin();
      if (plugin) {
        pluginActionCreateInstance(plugin, *renderState_);
        // Negotiate clip depth/components before the first render; plugins that
        // expect this action to be trapped will otherwise assume 8-bit RGBA and
        // misread the host's float buffers.
        pluginActionGetClipPreferences(plugin, *renderState_);
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
    if (w <= 0 || h <= 0) {
      dst = src.DeepCopy();
      return;
    }
    const int rowBytes = static_cast<int>(w * 4 * sizeof(float));
    const auto *srcData = reinterpret_cast<const unsigned char *>(srcImage.rgba32fData());

    // Blend into a scratch copy of the source so the mix control has both the
    // original and the plugin result available; the plugin must not write into
    // the host input buffer.
    ImageF32x4RGBAWithCache mixed = src.DeepCopy();
    auto &mixedImage = mixed.image();
    auto *dstData = reinterpret_cast<unsigned char *>(mixedImage.rgba32fData());

    renderState_->renderFrame.srcPixelData = srcData;
    renderState_->renderFrame.srcWidth = w;
    renderState_->renderFrame.srcHeight = h;
    renderState_->renderFrame.srcRowBytes = rowBytes;
    renderState_->renderFrame.dstPixelData = dstData;
    renderState_->renderFrame.dstRowBytes = rowBytes;
    renderState_->renderFrame.currentTime = static_cast<double>(renderTimeSeconds());

    syncParametersToPlugin();

    const OfxTime time = renderTimeSeconds();
    OfxPointD scale{1.0, 1.0};

    // A plugin that reports an identity transform for this frame needs no
    // render call at all; the scratch buffer already holds the source copy.
    QString failureMessage;
    if (pluginActionIsIdentity(plugin, *renderState_, time, scale)) {
      dst = mix_ >= 1.0 ? std::move(mixed) : blendWithSource(src, mixed, mix_);
      return;
    }

    const OfxStatus renderStatus =
        pluginActionRender(plugin, *renderState_, time, scale, &failureMessage);
    if (renderStatus != kOfxStatOK && !failureMessage.isEmpty()) {
      qWarning("[OFX] %s: %s",
               descriptor_.identifier.toQString().toUtf8().constData(),
               failureMessage.toUtf8().constData());
    }

    // The mix control is a real blend, not just a bypass threshold.
    dst = mix_ >= 1.0 ? std::move(mixed) : blendWithSource(src, mixed, mix_);
  }

  std::vector<AbstractProperty> getProperties() const override {
    return properties_;
  }

  // The OFX render action is time-parameterised; the bridge used to hardcode
  // t=0 so every frame evaluated the effect as if it were the first one. The
  // composition frame arrives through the shared effect context.
  void onContextUpdated(const EffectContext &context) override {
    renderTime_ = context.compositionFrame / (context.frameRate > 0.0 ? context.frameRate : 1.0);
  }

  void setPropertyValue(const UniString &name, const QVariant &value) override {
    const QString key = name.toQString();
    for (auto &property : properties_) {
      if (property.getName().compare(key, Qt::CaseInsensitive) != 0) {
        continue;
      }
      property.setValue(value);
      syncBridgeState(property);
      // Mirror the edit into the bridged parameter so the plugin reads it and
      // so the keyframe track (if any) evaluates from a consistent base.
      if (ParamState *param = findPluginParam(key)) {
        param->property.setValue(value);
      }
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

  OfxTime renderTimeSeconds() const {
    return static_cast<OfxTime>(renderTime_);
  }

  // Resolves an editor property back to the OFX parameter it came from.
  // toAbstractProperty names grouped/page parameters "parent/name", while the
  // param set is keyed by the bare name, so a direct lookup never matched and
  // every parameter inside a group or page was ignored at render time.
  ParamState *findPluginParam(const QString &propertyName) {
    if (!renderState_) {
      return nullptr;
    }
    auto &params = renderState_->paramSet.params;
    auto direct = params.find(propertyName.toStdString());
    if (direct != params.end()) {
      return direct->second.get();
    }
    const int slash = propertyName.lastIndexOf(QLatin1Char('/'));
    if (slash > 0) {
      auto leaf = params.find(propertyName.mid(slash + 1).toStdString());
      if (leaf != params.end()) {
        return leaf->second.get();
      }
    }
    return nullptr;
  }

  void syncParametersToPlugin() {
    for (const auto &prop : properties_) {
      ParamState *param = findPluginParam(prop.getName());
      if (!param) {
        continue;
      }
      const QString ptype = param->paramType;

      // A keyframed parameter animates: evaluate its own track at the frame
      // being rendered instead of reading the static preview value. Without
      // this the editor's keyframes never reach the plugin.
      const bool hasKeys = param->property.hasKeyFrames();
      QVariant val = prop.getValue();
      if (hasKeys) {
        const double rate = param->frameRate > 0.0 ? param->frameRate : 30.0;
        const auto frames = static_cast<int64_t>(std::llround(renderTime_ * rate));
        const QVariant evaluated = param->property.interpolateValue(
            RationalTime::fromFrameCount(frames, static_cast<int64_t>(rate)));
        if (evaluated.isValid()) {
          val = evaluated;
        }
      }

      // Order matters: "RGBA" contains "RGB", so an RGB-first test swallowed
      // every RGBA parameter and dropped the alpha channel.
      if (paramTypeIs(ptype, QStringLiteral("String")) ||
          paramTypeIs(ptype, QStringLiteral("StrChoice")) ||
          paramTypeIs(ptype, QStringLiteral("Custom"))) {
        param->currentStringValue = val.toString();
        param->currentUtf8Value = param->currentStringValue.toUtf8().toStdString();
        param->currentValues = {param->currentStringValue};
        continue;
      }
      if (paramTypeIs(ptype, QStringLiteral("RGBA"))) {
        const QColor c = val.value<QColor>();
        param->currentValues = {c.redF(), c.greenF(), c.blueF(), c.alphaF()};
        continue;
      }
      if (paramTypeIs(ptype, QStringLiteral("RGB"))) {
        const QColor c = val.value<QColor>();
        param->currentValues = {c.redF(), c.greenF(), c.blueF()};
        continue;
      }
      if (paramTypeIs(ptype, QStringLiteral("Boolean")) ||
          paramTypeIs(ptype, QStringLiteral("Choice")) ||
          paramTypeIs(ptype, QStringLiteral("Integer"))) {
        param->currentValues = {val.toInt()};
        continue;
      }
      if (paramTypeIs(ptype, QStringLiteral("Integer2D")) ||
          paramTypeIs(ptype, QStringLiteral("Integer3D"))) {
        // A 2D/3D integer parameter is stored as a single QVariant holding the
        // whole vector; hand the plugin each component instead of only the
        // first one plus implicit zeros.
        const QVariantList list = val.toList();
        param->currentValues.clear();
        for (const QVariant &component : list) {
          param->currentValues.push_back(component.toInt());
        }
        if (param->currentValues.empty()) {
          param->currentValues = {val.toInt()};
        }
        continue;
      }
      if (paramTypeIs(ptype, QStringLiteral("Double2D")) ||
          paramTypeIs(ptype, QStringLiteral("Double3D"))) {
        const QVariantList list = val.toList();
        param->currentValues.clear();
        for (const QVariant &component : list) {
          param->currentValues.push_back(component.toDouble());
        }
        if (param->currentValues.empty()) {
          param->currentValues = {val.toDouble()};
        }
        continue;
      }
      param->currentValues = {val.toDouble()};
    }
  }

  static bool paramTypeIs(const QString &paramType, const QString &token) {
    // Exact token match, not substring: "Integer2D" must not be read as an
    // "Integer" and "RGBA" not as an "RGB".
    const QString needle = QStringLiteral("OfxParamType") + token;
    return paramType.compare(needle, Qt::CaseInsensitive) == 0;
  }

  static ImageF32x4RGBAWithCache
blendWithSource(const ImageF32x4RGBAWithCache &src,
                const ImageF32x4RGBAWithCache &pluginResult, double mix) {
    ImageF32x4RGBAWithCache blended = src.DeepCopy();
    auto &out = blended.image();
    const auto *a = src.image().rgba32fData();
    const auto *b = pluginResult.image().rgba32fData();
    float *o = out.rgba32fData();
    if (!a || !b || !o) {
      return blended;
    }
    // The plugin result may differ in size if it ignored the negotiated clip
    // format; blending past the smaller buffer would read out of bounds.
    if (src.width() != pluginResult.width() ||
        src.height() != pluginResult.height() ||
        out.width() != src.width() || out.height() != src.height()) {
      return blended;
    }
    const std::size_t count =
        static_cast<std::size_t>(src.width()) * static_cast<std::size_t>(src.height());
    const float weight = static_cast<float>(std::clamp(mix, 0.0, 1.0));
    for (std::size_t i = 0; i < count * 4; ++i) {
      o[i] = a[i] * (1.0f - weight) + b[i] * weight;
    }
    return blended;
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
    // Resolving the entry points goes through the host's portable loader so
    // this works on every platform the host was built for, not just Win32.
    if (!descriptor_.libraryHandle) return nullptr;
    auto fn = reinterpret_cast<OfxGetPluginFn>(
        resolvePluginSymbol(descriptor_.libraryHandle, "OfxGetPlugin"));
    auto countFn = reinterpret_cast<int(*)()>(
        resolvePluginSymbol(descriptor_.libraryHandle, "OfxGetNumberOfPlugins"));
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
  double renderTime_ = 0.0;
  OfxPluginDescriptor descriptor_;
  SharedPtr<ImageEffectState> renderState_;
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
