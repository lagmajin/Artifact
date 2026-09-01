module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QSize>
#include <QVariant>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/TextureView.h>

module Artifact.Layers.Noise;

import Artifact.Layer.CloneEffectSupport;
import Artifact.Composition.Access;
import Time.Rational;

import std;
import Artifact.Layers.Abstract._2D;
import Artifact.Render.IRenderer;
import Image.ImageF32x4_RGBA;
import ImageProcessing.ProceduralTexture;
import Graphics.GPUcomputeContext;
import Property.Abstract;
import Property.Group;
import Property.SerializationBridge;

namespace Artifact {

namespace {

const auto clampUnit = [](float value, float fallback) {
  return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
};

QString noiseKindToString(
    ArtifactCore::ProceduralTextureGeneratorKind kind,
    ArtifactCore::ProceduralTextureVoronoiMode voronoiMode,
    ArtifactCore::ProceduralTextureGradientMode gradientMode) {
  using K = ArtifactCore::ProceduralTextureGeneratorKind;
  switch (kind) {
  case K::Simplex:
    return QStringLiteral("simplex");
  case K::FBM:
    return QStringLiteral("fbm");
  case K::Voronoi:
    switch (voronoiMode) {
    case ArtifactCore::ProceduralTextureVoronoiMode::Cell:
      return QStringLiteral("voronoiCell");
    case ArtifactCore::ProceduralTextureVoronoiMode::Edge:
      return QStringLiteral("voronoiEdge");
    default:
      return QStringLiteral("voronoiDistance");
    }
  case K::White:
    return QStringLiteral("white");
  case K::Value:
    return QStringLiteral("value");
  case K::Gradient:
    return gradientMode == ArtifactCore::ProceduralTextureGradientMode::Radial
               ? QStringLiteral("gradientRadial")
               : QStringLiteral("gradientLinear");
  default:
    return QStringLiteral("perlin");
  }
}

void noiseKindFromString(
    const QString& text, ArtifactCore::ProceduralTextureGeneratorKind& kind,
    ArtifactCore::ProceduralTextureVoronoiMode& voronoiMode,
    ArtifactCore::ProceduralTextureGradientMode& gradientMode) {
  kind = ArtifactCore::ProceduralTextureGeneratorKind::Perlin;
  voronoiMode = ArtifactCore::ProceduralTextureVoronoiMode::Distance;
  gradientMode = ArtifactCore::ProceduralTextureGradientMode::Linear;
  if (text == QLatin1String("simplex")) {
    kind = ArtifactCore::ProceduralTextureGeneratorKind::Simplex;
  } else if (text == QLatin1String("fbm")) {
    kind = ArtifactCore::ProceduralTextureGeneratorKind::FBM;
  } else if (text == QLatin1String("voronoiDistance") ||
             text == QLatin1String("voronoiCell") ||
             text == QLatin1String("voronoiEdge")) {
    kind = ArtifactCore::ProceduralTextureGeneratorKind::Voronoi;
    if (text == QLatin1String("voronoiCell")) {
      voronoiMode = ArtifactCore::ProceduralTextureVoronoiMode::Cell;
    } else if (text == QLatin1String("voronoiEdge")) {
      voronoiMode = ArtifactCore::ProceduralTextureVoronoiMode::Edge;
    }
  } else if (text == QLatin1String("white")) {
    kind = ArtifactCore::ProceduralTextureGeneratorKind::White;
  } else if (text == QLatin1String("value")) {
    kind = ArtifactCore::ProceduralTextureGeneratorKind::Value;
  } else if (text == QLatin1String("gradientLinear") ||
             text == QLatin1String("gradientRadial")) {
    kind = ArtifactCore::ProceduralTextureGeneratorKind::Gradient;
    if (text == QLatin1String("gradientRadial")) {
      gradientMode = ArtifactCore::ProceduralTextureGradientMode::Radial;
    }
  }
}

QString noiseSignatureKey(
    const ArtifactCore::ProceduralTextureSettings& settings,
    bool colorMappingEnabled, const FloatColor& colorA,
    const FloatColor& colorB) {
  const auto& p = settings.primary;
  const auto& post = settings.post;
  QString key;
  key += QString::number(static_cast<int>(p.kind));
  key += QLatin1Char('|') + QString::number(static_cast<int>(p.voronoiMode));
  key += QLatin1Char('|') + QString::number(static_cast<int>(p.gradientMode));
  key += QLatin1Char('|') + QString::number(p.seed);
  key += QLatin1Char('|') + QString::number(p.scale[0]);
  key += QLatin1Char('|') + QString::number(p.scale[1]);
  key += QLatin1Char('|') + QString::number(p.offset[0]);
  key += QLatin1Char('|') + QString::number(p.offset[1]);
  key += QLatin1Char('|') + QString::number(p.rotation);
  key += QLatin1Char('|') + QString::number(p.amplitude);
  key += QLatin1Char('|') + QString::number(p.octaves);
  key += QLatin1Char('|') + QString::number(p.lacunarity);
  key += QLatin1Char('|') + QString::number(p.gain);
  key += QLatin1Char('|') + QString::number(p.cellJitter);
  key += QLatin1Char('|') + QString::number(post.invert);
  key += QLatin1Char('|') + QString::number(post.gamma);
  key += QLatin1Char('|') + QString::number(post.domainWarpEnabled);
  key += QLatin1Char('|') + QString::number(post.warpAmplitude);
  key += QLatin1Char('|') + QString::number(post.useSecondary);
  key += QLatin1Char('|') + QString::number(settings.seamless);
  key += QLatin1Char('|') + QString::number(colorMappingEnabled);
  for (const FloatColor* c : {&colorA, &colorB}) {
    key += QLatin1Char('|') + QString::number(c->r()) + QLatin1Char(',') +
           QString::number(c->g()) + QLatin1Char(',') +
           QString::number(c->b()) + QLatin1Char(',') +
           QString::number(c->a());
  }
  return key;
}

void sanitizeNoiseSettings(ArtifactCore::ProceduralTextureSettings& settings) {
  settings.width = std::clamp(settings.width, 1, 16384);
  settings.height = std::clamp(settings.height, 1, 16384);
  settings.primary.octaves = std::clamp(settings.primary.octaves, 1u, 12u);
  settings.primary.lacunarity =
      std::isfinite(settings.primary.lacunarity)
          ? std::clamp(settings.primary.lacunarity, 1.0f, 8.0f)
          : 2.0f;
  settings.primary.gain =
      std::isfinite(settings.primary.gain)
          ? std::clamp(settings.primary.gain, 0.0f, 1.0f)
          : 0.5f;
  settings.primary.cellJitter =
      std::isfinite(settings.primary.cellJitter)
          ? std::clamp(settings.primary.cellJitter, 0.0f, 1.0f)
          : 0.75f;
}

float evaluatedNoiseProperty(const ArtifactNoiseLayer* layer,
                             const QString& path, float fallback,
                             const ArtifactCore::RationalTime& time,
                             int64_t frame) {
  if (!layer) return fallback;
  float value = fallback;
  if (const auto property = layer->getProperty(path);
      property && property->isAnimatable() &&
          !property->getKeyFrames().empty()) {
    const QVariant animated = property->interpolateValue(time);
    if (animated.isValid() && std::isfinite(animated.toDouble())) {
      value = static_cast<float>(animated.toDouble());
    }
  }
  if (const auto* stack = layer->animationLayerStack(path);
      stack && stack->layerCount() > 0) {
    value = stack->evaluateWithBase(ArtifactCore::FramePosition(frame), value);
  }
  return value;
}

ArtifactCore::RationalTime noiseEvaluationTime(const ArtifactNoiseLayer* layer) {
  if (!layer) return ArtifactCore::RationalTime(0, 30);
  auto* composition = dynamic_cast<ArtifactAbstractCompositionAccess*>(
      layer->compositionObject());
  if (!composition) {
    return ArtifactCore::RationalTime(layer->currentFrame(), 30);
  }
  const double fps = composition->frameRate().framerate();
  return ArtifactCore::RationalTime(
      composition->framePosition().framePosition(), fps > 0.0 ? fps : 30.0);
}

ArtifactCore::ProceduralTextureSettings evaluatedNoiseSettings(
    const ArtifactNoiseLayer* layer,
    const ArtifactCore::ProceduralTextureSettings& base) {
  auto settings = base;
  if (!layer) return settings;
  const auto time = noiseEvaluationTime(layer);
  const int64_t frame = time.value();
  auto& p = settings.primary;
  p.scale[0] = evaluatedNoiseProperty(layer, QStringLiteral("noise.scaleX"),
                                      p.scale[0], time, frame);
  p.scale[1] = evaluatedNoiseProperty(layer, QStringLiteral("noise.scaleY"),
                                      p.scale[1], time, frame);
  p.offset[0] = evaluatedNoiseProperty(layer, QStringLiteral("noise.offsetX"),
                                       p.offset[0], time, frame);
  p.offset[1] = evaluatedNoiseProperty(layer, QStringLiteral("noise.offsetY"),
                                       p.offset[1], time, frame);
  p.rotation = evaluatedNoiseProperty(layer, QStringLiteral("noise.rotation"),
                                      p.rotation, time, frame);
  p.amplitude = evaluatedNoiseProperty(layer, QStringLiteral("noise.amplitude"),
                                       p.amplitude, time, frame);
  p.octaves = static_cast<std::uint32_t>(std::clamp(
      std::lround(evaluatedNoiseProperty(layer, QStringLiteral("noise.octaves"),
                                          static_cast<float>(p.octaves), time,
                                          frame)),
      1l, 12l));
  p.lacunarity = evaluatedNoiseProperty(layer, QStringLiteral("noise.lacunarity"),
                                        p.lacunarity, time, frame);
  p.gain = evaluatedNoiseProperty(layer, QStringLiteral("noise.gain"),
                                  p.gain, time, frame);
  sanitizeNoiseSettings(settings);
  return settings;
}

const QStringList& animatedNoisePropertySuffixes() {
  static const QStringList suffixes = {
      QStringLiteral("seed"), QStringLiteral("scaleX"),
      QStringLiteral("scaleY"), QStringLiteral("offsetX"),
      QStringLiteral("offsetY"), QStringLiteral("rotation"),
      QStringLiteral("amplitude"), QStringLiteral("octaves"),
      QStringLiteral("lacunarity"), QStringLiteral("gain")};
  return suffixes;
}

QJsonObject serializeNoiseAnimatedProperties(const ArtifactNoiseLayer* layer) {
  QJsonObject result;
  if (!layer) return result;
  for (const auto& suffix : animatedNoisePropertySuffixes()) {
    const auto property = layer->getProperty(QStringLiteral("noise.") + suffix);
    if (!property) continue;
    const auto serialized =
        ArtifactCore::PropertySerializationBridge::serializeProperty(property);
    if (serialized.expression.isEmpty() && serialized.keyframes.isEmpty() &&
        serialized.envelopes.isEmpty()) {
      continue;
    }
    QJsonObject propertyObject;
    propertyObject[QStringLiteral("type")] = serialized.type;
    propertyObject[QStringLiteral("value")] = serialized.value;
    if (!serialized.expression.isEmpty()) {
      propertyObject[QStringLiteral("expression")] = serialized.expression;
    }
    if (!serialized.keyframes.isEmpty()) {
      propertyObject[QStringLiteral("keyframes")] = serialized.keyframes;
    }
    if (!serialized.envelopes.isEmpty()) {
      propertyObject[QStringLiteral("envelopes")] = serialized.envelopes;
    }
    result[suffix] = propertyObject;
  }
  return result;
}

void restoreNoiseAnimatedProperties(ArtifactNoiseLayer* layer,
                                    const QJsonObject& properties) {
  if (!layer || properties.isEmpty()) return;
  // Construct the persistent properties before applying serialized animation.
  (void)layer->getLayerPropertyGroups();
  for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
    const QString suffix = it.key();
    if (!animatedNoisePropertySuffixes().contains(suffix) ||
        !it.value().isObject()) {
      continue;
    }
    const auto property =
        layer->getProperty(QStringLiteral("noise.") + suffix);
    if (!property) continue;
    const auto propertyObject = it.value().toObject();
    ArtifactCore::SerializedProperty serialized;
    serialized.name = property->getName();
    serialized.type = propertyObject.value(QStringLiteral("type")).toInt(
        static_cast<int>(property->getType()));
    serialized.value = propertyObject.value(QStringLiteral("value"));
    serialized.expression = propertyObject.value(QStringLiteral("expression"))
                                .toString().trimmed().left(16384);
    serialized.keyframes =
        propertyObject.value(QStringLiteral("keyframes")).toArray();
    serialized.envelopes =
        propertyObject.value(QStringLiteral("envelopes")).toArray();
    property->setAnimatable(true);
    ArtifactCore::PropertySerializationBridge::deserializeProperty(
        property, serialized);
  }
}
} // namespace

class ArtifactNoiseLayer::Impl
{
public:
  ArtifactCore::ProceduralTextureSettings settings_;
  bool colorMappingEnabled_ = false;
  FloatColor colorA_ = FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
  FloatColor colorB_ = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
  mutable ArtifactCore::ImageF32x4_RGBA buffer_;
  mutable QString bufferSignature_;
  mutable QImage cachedImage_;
  mutable QSize cachedSize_;
  mutable QString cachedSignature_;
  mutable ArtifactCore::GpuContext* gpuContext_ = nullptr;
  mutable ArtifactCore::ProceduralTextureComputePipeline* gpuPipeline_ = nullptr;
  mutable Diligent::RefCntAutoPtr<Diligent::ITexture> gpuTexture_;
  mutable QString gpuSignature_;
  mutable Diligent::IRenderDevice* gpuDevice_ = nullptr;

  ~Impl() {
    delete gpuPipeline_;
    delete gpuContext_;
  }

  Diligent::ITextureView* gpuView(ArtifactIRenderer* renderer,
                                 const QSize& size,
                                 const ArtifactCore::ProceduralTextureSettings& settings) const {
    if (!renderer || settings.width <= 0 || settings.height <= 0) return nullptr;
    if (settings.width != size.width() || settings.height != size.height()) return nullptr;
    const QString signature = noiseSignatureKey(settings, false, colorA_, colorB_);
    if (gpuSignature_ == signature && gpuTexture_) {
      return gpuTexture_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    }
    auto device = renderer->device();
    auto context = renderer->immediateContext();
    if (!device || !context) return nullptr;
    if (gpuDevice_ && gpuDevice_ != device.RawPtr()) {
      gpuTexture_.Release();
      delete gpuPipeline_;
      delete gpuContext_;
      gpuPipeline_ = nullptr;
      gpuContext_ = nullptr;
      gpuSignature_.clear();
    }
    gpuDevice_ = device.RawPtr();
    if (!gpuContext_) gpuContext_ = new ArtifactCore::GpuContext(device.RawPtr(), context.RawPtr());
    if (!gpuPipeline_) {
      gpuPipeline_ = new ArtifactCore::ProceduralTextureComputePipeline(*gpuContext_);
      if (!gpuPipeline_->initialize()) return nullptr;
    }
    auto* texture = ArtifactCore::ProceduralTextureComputePipeline::createOutputTexture(
        device.RawPtr(), static_cast<std::uint32_t>(settings.width),
        static_cast<std::uint32_t>(settings.height),
        Diligent::TEX_FORMAT_RGBA16_FLOAT, "ArtifactNoiseLayer/ComputeOutput");
    if (!texture) return nullptr;
    gpuTexture_.Attach(texture);
    auto* outputUAV = gpuTexture_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS);
    if (!gpuPipeline_->generate(context.RawPtr(), outputUAV, settings)) {
      gpuTexture_.Release();
      gpuSignature_.clear();
      return nullptr;
    }
    gpuSignature_ = signature;
    return gpuTexture_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
  }
};

ArtifactNoiseLayer::ArtifactNoiseLayer() : impl_(new Impl()) {
  setBuiltinLayerSourceComponentType(QStringLiteral("source-noise"));
}

ArtifactNoiseLayer::~ArtifactNoiseLayer() { delete impl_; }

void ArtifactNoiseLayer::setSize(const int width, const int height) {
  setSourceSize(Size_2D(std::clamp(width, 1, 16384),
                        std::clamp(height, 1, 16384)));
}

const ArtifactCore::ProceduralTextureSettings&
ArtifactNoiseLayer::settings() const {
  return impl_->settings_;
}

void ArtifactNoiseLayer::setSettings(
    const ArtifactCore::ProceduralTextureSettings& settings) {
  impl_->settings_ = settings;
  sanitizeNoiseSettings(impl_->settings_);
}

bool ArtifactNoiseLayer::isColorMappingEnabled() const {
  return impl_->colorMappingEnabled_;
}

void ArtifactNoiseLayer::setColorMappingEnabled(bool enabled) {
  impl_->colorMappingEnabled_ = enabled;
}

FloatColor ArtifactNoiseLayer::colorA() const { return impl_->colorA_; }

void ArtifactNoiseLayer::setColorA(const FloatColor& color) {
  impl_->colorA_ = FloatColor(clampUnit(color.r(), 0.0f),
                              clampUnit(color.g(), 0.0f),
                              clampUnit(color.b(), 0.0f),
                              clampUnit(color.a(), 1.0f));
}

FloatColor ArtifactNoiseLayer::colorB() const { return impl_->colorB_; }

void ArtifactNoiseLayer::setColorB(const FloatColor& color) {
  impl_->colorB_ = FloatColor(clampUnit(color.r(), 1.0f),
                              clampUnit(color.g(), 1.0f),
                              clampUnit(color.b(), 1.0f),
                              clampUnit(color.a(), 1.0f));
}

const ArtifactCore::ImageF32x4_RGBA*
ArtifactNoiseLayer::resolveLayerSourceOverride() const {
  const auto source = sourceSize();
  const int width = std::clamp(source.width, 1, 16384);
  const int height = std::clamp(source.height, 1, 16384);
  auto settings = evaluatedNoiseSettings(this, impl_->settings_);
  settings.width = width;
  settings.height = height;
  const QString signature = noiseSignatureKey(
      settings, impl_->colorMappingEnabled_, impl_->colorA_, impl_->colorB_);
  if (impl_->buffer_.isEmpty() || impl_->bufferSignature_ != signature) {
    impl_->buffer_ = ArtifactCore::ImageF32x4_RGBA();
    if (!ArtifactCore::ProceduralTextureGenerator::generate(
            settings, impl_->buffer_)) {
      return nullptr;
    }
    if (impl_->colorMappingEnabled_ && impl_->buffer_.rgba32fData() &&
        impl_->buffer_.width() > 0 && impl_->buffer_.height() > 0) {
      const int pixelCount =
          impl_->buffer_.width() * impl_->buffer_.height();
      float* pixels = impl_->buffer_.rgba32fData();
      const FloatColor& a = impl_->colorA_;
      const FloatColor& b = impl_->colorB_;
      for (int i = 0; i < pixelCount; ++i) {
        const float v = std::clamp(pixels[i * 4], 0.0f, 1.0f);
        pixels[i * 4 + 0] = a.r() + (b.r() - a.r()) * v;
        pixels[i * 4 + 1] = a.g() + (b.g() - a.g()) * v;
        pixels[i * 4 + 2] = a.b() + (b.b() - a.b()) * v;
        pixels[i * 4 + 3] = a.a() + (b.a() - a.a()) * v;
      }
    }
    impl_->bufferSignature_ = signature;
  }
  return &impl_->buffer_;
}

const QImage& ArtifactNoiseLayer::currentNoiseImage() const {
  const auto source = sourceSize();
  const QSize targetSize(std::clamp(source.width, 1, 16384),
                         std::clamp(source.height, 1, 16384));
  const auto* buffer = resolveLayerSourceOverride();
  const QString signature = buffer ? impl_->bufferSignature_ : QString();
  if (impl_->cachedSignature_ == signature &&
      impl_->cachedSize_ == targetSize &&
      impl_->cachedImage_.isNull() != static_cast<bool>(buffer)) {
    return impl_->cachedImage_;
  }
  impl_->cachedImage_ = buffer ? buffer->toQImage() : QImage();
  impl_->cachedSize_ = targetSize;
  impl_->cachedSignature_ = signature;
  return impl_->cachedImage_;
}

void ArtifactNoiseLayer::draw(ArtifactIRenderer* renderer) {
  if (!renderer) return;
  const auto source = sourceSize();
  const Size_2D size(std::clamp(source.width, 1, 16384),
                     std::clamp(source.height, 1, 16384));
  const QMatrix4x4 baseTransform = getGlobalTransform4x4();
  if (!impl_->colorMappingEnabled_) {
    auto gpuSettings = evaluatedNoiseSettings(this, impl_->settings_);
    gpuSettings.width = size.width;
    gpuSettings.height = size.height;
    if (auto* gpuTexture = impl_->gpuView(renderer,
                                          QSize(size.width, size.height),
                                          gpuSettings)) {
      drawWithClonerEffect(
          this, baseTransform,
          [renderer, size, gpuTexture, this]
          (const QMatrix4x4& transform, float weight) {
            renderer->drawSpriteTransformed(
                0.0f, 0.0f, static_cast<float>(size.width),
                static_cast<float>(size.height), transform, gpuTexture,
                opacity() * weight);
          });
      drawFractureOverlay(renderer, baseTransform,
                          QSizeF(size.width, size.height), opacity());
      return;
    }
  }
  if (const auto* sourceOverride = resolveLayerSourceOverride()) {
    const auto& overrideImage = *sourceOverride;
    drawWithClonerEffect(
        this, baseTransform,
        [renderer, size, &overrideImage, this]
        (const QMatrix4x4& transform, float weight) {
          renderer->drawSpriteTransformed(
              0.0f, 0.0f, static_cast<float>(size.width),
              static_cast<float>(size.height), transform, overrideImage,
              opacity() * weight);
        });
    drawFractureOverlay(renderer, baseTransform,
                        QSizeF(size.width, size.height), opacity());
    return;
  }
  drawFractureOverlay(renderer, baseTransform,
                      QSizeF(size.width, size.height), opacity());
}

QImage ArtifactNoiseLayer::toQImage() const {
  return currentNoiseImage();
}

QImage ArtifactNoiseLayer::getThumbnail(int width, int height) const {
  const QSize targetSize(std::clamp(width, 1, 16384),
                         std::clamp(height, 1, 16384));
  const QImage image = toQImage();
  return image.isNull()
      ? ArtifactAbstractLayer::getThumbnail(targetSize.width(),
                                            targetSize.height())
      : image.scaled(targetSize, Qt::KeepAspectRatio,
                     Qt::SmoothTransformation);
}

QJsonObject ArtifactNoiseLayer::toJson() const {
  QJsonObject obj = ArtifactAbstract2DLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Noise);
  obj["layerType"] = QStringLiteral("Noise");
  const auto safeSource = sourceSize();
  obj["noiseWidth"] = std::clamp(safeSource.width, 1, 16384);
  obj["noiseHeight"] = std::clamp(safeSource.height, 1, 16384);
  const auto& settings = impl_->settings_;
  const auto& p = settings.primary;
  const auto& post = settings.post;
  QJsonObject noiseObj;
  noiseObj["kind"] =
      noiseKindToString(p.kind, p.voronoiMode, p.gradientMode);
  noiseObj["seed"] = static_cast<int>(p.seed);
  noiseObj["scaleX"] = static_cast<double>(p.scale[0]);
  noiseObj["scaleY"] = static_cast<double>(p.scale[1]);
  noiseObj["offsetX"] = static_cast<double>(p.offset[0]);
  noiseObj["offsetY"] = static_cast<double>(p.offset[1]);
  noiseObj["rotation"] = static_cast<double>(p.rotation);
  noiseObj["amplitude"] = static_cast<double>(p.amplitude);
  noiseObj["octaves"] = static_cast<int>(p.octaves);
  noiseObj["lacunarity"] = static_cast<double>(p.lacunarity);
  noiseObj["gain"] = static_cast<double>(p.gain);
  noiseObj["cellJitter"] = static_cast<double>(p.cellJitter);
  noiseObj["seamless"] = settings.seamless;
  noiseObj["domainWarp"] = post.domainWarpEnabled;
  noiseObj["warpAmplitude"] = static_cast<double>(post.warpAmplitude);
  noiseObj["useSecondary"] = post.useSecondary;
  noiseObj["gamma"] = static_cast<double>(post.gamma);
  noiseObj["invert"] = post.invert;
  noiseObj["colorMapping"] = impl_->colorMappingEnabled_;
  QJsonObject colorAObj;
  colorAObj["r"] = impl_->colorA_.r();
  colorAObj["g"] = impl_->colorA_.g();
  colorAObj["b"] = impl_->colorA_.b();
  colorAObj["a"] = impl_->colorA_.a();
  noiseObj["colorA"] = colorAObj;
  QJsonObject colorBObj;
  colorBObj["r"] = impl_->colorB_.r();
  colorBObj["g"] = impl_->colorB_.g();
  colorBObj["b"] = impl_->colorB_.b();
  colorBObj["a"] = impl_->colorB_.a();
  noiseObj["colorB"] = colorBObj;
  const auto animatedProperties = serializeNoiseAnimatedProperties(this);
  if (!animatedProperties.isEmpty()) {
    noiseObj["animatedProperties"] = animatedProperties;
  }
  obj["noise"] = noiseObj;
  return obj;
}

void ArtifactNoiseLayer::fromJsonProperties(const QJsonObject& obj) {
  ArtifactAbstract2DLayer::fromJsonProperties(obj);
  if (obj.contains("noiseWidth") || obj.contains("noiseHeight")) {
    const int width = obj.value("noiseWidth").toInt(sourceSize().width);
    const int height = obj.value("noiseHeight").toInt(sourceSize().height);
    setSize(width, height);
  }
  if (obj.contains("noise") && obj["noise"].isObject()) {
    const auto noiseObj = obj["noise"].toObject();
    auto& settings = impl_->settings_;
    auto& p = settings.primary;
    noiseKindFromString(noiseObj.value("kind").toString(), p.kind,
                        p.voronoiMode, p.gradientMode);
    p.seed =
        static_cast<std::uint32_t>(std::max(0, noiseObj.value("seed").toInt(0)));
    p.scale[0] = static_cast<float>(noiseObj.value("scaleX").toDouble(8.0));
    p.scale[1] = static_cast<float>(noiseObj.value("scaleY").toDouble(8.0));
    p.offset[0] = static_cast<float>(noiseObj.value("offsetX").toDouble(0.0));
    p.offset[1] = static_cast<float>(noiseObj.value("offsetY").toDouble(0.0));
    p.rotation = static_cast<float>(noiseObj.value("rotation").toDouble(0.0));
    p.amplitude = static_cast<float>(noiseObj.value("amplitude").toDouble(1.0));
    p.octaves = static_cast<std::uint32_t>(
        std::clamp(noiseObj.value("octaves").toInt(4), 1, 12));
    p.lacunarity =
        static_cast<float>(noiseObj.value("lacunarity").toDouble(2.0));
    p.gain = static_cast<float>(noiseObj.value("gain").toDouble(0.5));
    p.cellJitter =
        static_cast<float>(noiseObj.value("cellJitter").toDouble(0.75));
    settings.seamless = noiseObj.value("seamless").toBool(true);
    settings.post.domainWarpEnabled =
        noiseObj.value("domainWarp").toBool(false);
    settings.post.warpAmplitude =
        static_cast<float>(noiseObj.value("warpAmplitude").toDouble(0.25));
    settings.post.useSecondary = noiseObj.value("useSecondary").toBool(false);
    settings.post.gamma =
        static_cast<float>(noiseObj.value("gamma").toDouble(1.0));
    settings.post.invert = noiseObj.value("invert").toBool(false);
    setColorMappingEnabled(noiseObj.value("colorMapping").toBool(false));
    if (noiseObj.contains("colorA") && noiseObj["colorA"].isObject()) {
      const auto colorObj = noiseObj["colorA"].toObject();
      setColorA(FloatColor(
          static_cast<float>(colorObj.value("r").toDouble(0.0)),
          static_cast<float>(colorObj.value("g").toDouble(0.0)),
          static_cast<float>(colorObj.value("b").toDouble(0.0)),
          static_cast<float>(colorObj.value("a").toDouble(1.0))));
    }
    if (noiseObj.contains("colorB") && noiseObj["colorB"].isObject()) {
      const auto colorObj = noiseObj["colorB"].toObject();
      setColorB(FloatColor(
          static_cast<float>(colorObj.value("r").toDouble(1.0)),
          static_cast<float>(colorObj.value("g").toDouble(1.0)),
          static_cast<float>(colorObj.value("b").toDouble(1.0)),
          static_cast<float>(colorObj.value("a").toDouble(1.0))));
    }
    if (noiseObj.contains("animatedProperties") &&
        noiseObj["animatedProperties"].isObject()) {
      restoreNoiseAnimatedProperties(
          this, noiseObj["animatedProperties"].toObject());
    }
  }
}

std::vector<ArtifactCore::PropertyGroup>
ArtifactNoiseLayer::getLayerPropertyGroups() const {
  auto groups = ArtifactAbstract2DLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup noiseGroup(QStringLiteral("Noise"));
  const auto& p = impl_->settings_.primary;
  auto kindProperty = persistentLayerProperty(
      QStringLiteral("noise.kind"), ArtifactCore::PropertyType::String,
      noiseKindToString(p.kind, p.voronoiMode, p.gradientMode), -105);
  kindProperty->setDisplayLabel(QStringLiteral("ノイズタイプ"));
  noiseGroup.addProperty(kindProperty);
  auto seedProperty = persistentLayerProperty(
      QStringLiteral("noise.seed"), ArtifactCore::PropertyType::Integer,
      static_cast<int>(p.seed), -105);
  seedProperty->setAnimatable(true);
  seedProperty->setDisplayLabel(QStringLiteral("シード"));
  noiseGroup.addProperty(seedProperty);
  auto scaleXProperty = persistentLayerProperty(
      QStringLiteral("noise.scaleX"), ArtifactCore::PropertyType::Float,
      p.scale[0], -105);
  scaleXProperty->setAnimatable(true);
  scaleXProperty->setHardRange(0.001, 10000.0);
  scaleXProperty->setDisplayLabel(QStringLiteral("スケールX"));
  noiseGroup.addProperty(scaleXProperty);
  auto scaleYProperty = persistentLayerProperty(
      QStringLiteral("noise.scaleY"), ArtifactCore::PropertyType::Float,
      p.scale[1], -105);
  scaleYProperty->setAnimatable(true);
  scaleYProperty->setHardRange(0.001, 10000.0);
  scaleYProperty->setDisplayLabel(QStringLiteral("スケールY"));
  noiseGroup.addProperty(scaleYProperty);
  auto offsetXProperty = persistentLayerProperty(
      QStringLiteral("noise.offsetX"), ArtifactCore::PropertyType::Float,
      p.offset[0], -105);
  offsetXProperty->setAnimatable(true);
  offsetXProperty->setDisplayLabel(QStringLiteral("オフセットX"));
  noiseGroup.addProperty(offsetXProperty);
  auto offsetYProperty = persistentLayerProperty(
      QStringLiteral("noise.offsetY"), ArtifactCore::PropertyType::Float,
      p.offset[1], -105);
  offsetYProperty->setAnimatable(true);
  offsetYProperty->setDisplayLabel(QStringLiteral("オフセットY"));
  noiseGroup.addProperty(offsetYProperty);
  auto rotationProperty = persistentLayerProperty(
      QStringLiteral("noise.rotation"), ArtifactCore::PropertyType::Float,
      p.rotation, -105);
  rotationProperty->setAnimatable(true);
  rotationProperty->setDisplayLabel(QStringLiteral("回転"));
  noiseGroup.addProperty(rotationProperty);
  auto amplitudeProperty = persistentLayerProperty(
      QStringLiteral("noise.amplitude"), ArtifactCore::PropertyType::Float,
      p.amplitude, -105);
  amplitudeProperty->setAnimatable(true);
  amplitudeProperty->setDisplayLabel(QStringLiteral("振幅"));
  noiseGroup.addProperty(amplitudeProperty);
  auto octavesProperty = persistentLayerProperty(
      QStringLiteral("noise.octaves"), ArtifactCore::PropertyType::Integer,
      static_cast<int>(p.octaves), -105);
  octavesProperty->setAnimatable(true);
  octavesProperty->setHardRange(1.0, 12.0);
  octavesProperty->setDisplayLabel(QStringLiteral("オクターブ"));
  noiseGroup.addProperty(octavesProperty);
  auto lacunarityProperty = persistentLayerProperty(
      QStringLiteral("noise.lacunarity"), ArtifactCore::PropertyType::Float,
      p.lacunarity, -105);
  lacunarityProperty->setAnimatable(true);
  lacunarityProperty->setHardRange(1.0, 8.0);
  lacunarityProperty->setDisplayLabel(QStringLiteral("ラクナリティ"));
  noiseGroup.addProperty(lacunarityProperty);
  auto gainProperty = persistentLayerProperty(
      QStringLiteral("noise.gain"), ArtifactCore::PropertyType::Float,
      p.gain, -105);
  gainProperty->setAnimatable(true);
  gainProperty->setHardRange(0.0, 1.0);
  gainProperty->setDisplayLabel(QStringLiteral("ゲイン"));
  noiseGroup.addProperty(gainProperty);
  auto colorMappingProperty = persistentLayerProperty(
      QStringLiteral("noise.colorMapping"), ArtifactCore::PropertyType::Boolean,
      impl_->colorMappingEnabled_, -105);
  colorMappingProperty->setDisplayLabel(QStringLiteral("カラーマッピング"));
  noiseGroup.addProperty(colorMappingProperty);
  const auto mappedA = colorA();
  auto colorAProperty = persistentLayerProperty(
      QStringLiteral("noise.colorA"), ArtifactCore::PropertyType::Color,
      QColor::fromRgbF(mappedA.r(), mappedA.g(), mappedA.b(), mappedA.a()),
      -105);
  colorAProperty->setColorValue(
      QColor::fromRgbF(mappedA.r(), mappedA.g(), mappedA.b(), mappedA.a()));
  colorAProperty->setDisplayLabel(QStringLiteral("マップ色A"));
  noiseGroup.addProperty(colorAProperty);
  const auto mappedB = colorB();
  auto colorBProperty = persistentLayerProperty(
      QStringLiteral("noise.colorB"), ArtifactCore::PropertyType::Color,
      QColor::fromRgbF(mappedB.r(), mappedB.g(), mappedB.b(), mappedB.a()),
      -105);
  colorBProperty->setColorValue(
      QColor::fromRgbF(mappedB.r(), mappedB.g(), mappedB.b(), mappedB.a()));
  colorBProperty->setDisplayLabel(QStringLiteral("マップ色B"));
  noiseGroup.addProperty(colorBProperty);
  groups.push_back(noiseGroup);
  return groups;
}

bool ArtifactNoiseLayer::setLayerPropertyValue(const QString& propertyPath,
                                               const QVariant& value) {
  auto& settings = impl_->settings_;
  auto& p = settings.primary;
  if (propertyPath == QStringLiteral("noise.kind")) {
    noiseKindFromString(value.toString(), p.kind, p.voronoiMode,
                        p.gradientMode);
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.seed")) {
    p.seed = static_cast<std::uint32_t>(std::max(0, value.toInt()));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.scaleX")) {
    p.scale[0] =
        static_cast<float>(std::clamp(value.toDouble(8.0), 0.001, 10000.0));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.scaleY")) {
    p.scale[1] =
        static_cast<float>(std::clamp(value.toDouble(8.0), 0.001, 10000.0));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.offsetX")) {
    p.offset[0] = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.offsetY")) {
    p.offset[1] = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.rotation")) {
    p.rotation = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.amplitude")) {
    p.amplitude = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.octaves")) {
    p.octaves =
        static_cast<std::uint32_t>(std::clamp(value.toInt(4), 1, 12));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.lacunarity")) {
    p.lacunarity =
        static_cast<float>(std::clamp(value.toDouble(2.0), 1.0, 8.0));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.gain")) {
    p.gain = static_cast<float>(std::clamp(value.toDouble(0.5), 0.0, 1.0));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.colorMapping")) {
    setColorMappingEnabled(value.toBool());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.colorA")) {
    const auto c = value.value<QColor>();
    setColorA(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.colorB")) {
    const auto c = value.value<QColor>();
    setColorB(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
    Q_EMIT changed();
    return true;
  }
  return ArtifactAbstract2DLayer::setLayerPropertyValue(propertyPath, value);
}

} // namespace Artifact
