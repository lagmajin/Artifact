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
import Script.Expression.Evaluator;

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
  key += QString::number(settings.width);
  key += QLatin1Char('|') + QString::number(settings.height);
  key += QLatin1Char('|') + QString::number(settings.parallel);
  key += QLatin1Char('|') + QString::number(static_cast<int>(settings.outputFormat));
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
  key += QLatin1Char('|') + QString::number(post.normalize);
  key += QLatin1Char('|') + QString::number(post.normalizeMin);
  key += QLatin1Char('|') + QString::number(post.normalizeMax);
  key += QLatin1Char('|') + QString::number(post.clampEnabled);
  key += QLatin1Char('|') + QString::number(post.clampMin);
  key += QLatin1Char('|') + QString::number(post.clampMax);
  key += QLatin1Char('|') + QString::number(post.remapEnabled);
  key += QLatin1Char('|') + QString::number(post.remapInMin);
  key += QLatin1Char('|') + QString::number(post.remapInMax);
  key += QLatin1Char('|') + QString::number(post.remapOutMin);
  key += QLatin1Char('|') + QString::number(post.remapOutMax);
  key += QLatin1Char('|') + QString::number(static_cast<int>(post.blendMode));
  key += QLatin1Char('|') + QString::number(post.blendWeight);
  const auto appendGeneratorParams = [&key](
      const ArtifactCore::ProceduralTextureGeneratorParams& params) {
    key += QLatin1Char('|') + QString::number(static_cast<int>(params.kind));
    key += QLatin1Char('|') + QString::number(static_cast<int>(params.voronoiMode));
    key += QLatin1Char('|') + QString::number(static_cast<int>(params.gradientMode));
    key += QLatin1Char('|') + QString::number(params.seed);
    for (const float value : params.scale) key += QLatin1Char('|') + QString::number(value);
    for (const float value : params.offset) key += QLatin1Char('|') + QString::number(value);
    key += QLatin1Char('|') + QString::number(params.rotation);
    key += QLatin1Char('|') + QString::number(params.amplitude);
    key += QLatin1Char('|') + QString::number(params.octaves);
    key += QLatin1Char('|') + QString::number(params.lacunarity);
    key += QLatin1Char('|') + QString::number(params.gain);
    key += QLatin1Char('|') + QString::number(params.cellJitter);
  };
  appendGeneratorParams(post.secondary);
  appendGeneratorParams(post.warp);
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
  settings.post.warpAmplitude =
      std::isfinite(settings.post.warpAmplitude)
          ? std::clamp(settings.post.warpAmplitude, 0.0f, 16.0f)
          : 0.25f;
  settings.post.gamma =
      std::isfinite(settings.post.gamma)
          ? std::clamp(settings.post.gamma, 0.01f, 8.0f)
          : 1.0f;
  settings.post.normalizeMin = std::isfinite(settings.post.normalizeMin)
                                   ? settings.post.normalizeMin
                                   : 0.0f;
  settings.post.normalizeMax = std::isfinite(settings.post.normalizeMax)
                                   ? settings.post.normalizeMax
                                   : 1.0f;
  if (settings.post.normalizeMax < settings.post.normalizeMin) {
    std::swap(settings.post.normalizeMin, settings.post.normalizeMax);
  }
  settings.post.clampMin = std::isfinite(settings.post.clampMin)
                               ? settings.post.clampMin
                               : 0.0f;
  settings.post.clampMax = std::isfinite(settings.post.clampMax)
                               ? settings.post.clampMax
                               : 1.0f;
  if (settings.post.clampMax < settings.post.clampMin) {
    std::swap(settings.post.clampMin, settings.post.clampMax);
  }
  settings.post.remapInMin = std::isfinite(settings.post.remapInMin)
                                 ? settings.post.remapInMin
                                 : 0.0f;
  settings.post.remapInMax = std::isfinite(settings.post.remapInMax)
                                 ? settings.post.remapInMax
                                 : 1.0f;
  if (settings.post.remapInMax < settings.post.remapInMin) {
    std::swap(settings.post.remapInMin, settings.post.remapInMax);
  }
  settings.post.remapOutMin = std::isfinite(settings.post.remapOutMin)
                                  ? settings.post.remapOutMin
                                  : 0.0f;
  settings.post.remapOutMax = std::isfinite(settings.post.remapOutMax)
                                  ? settings.post.remapOutMax
                                  : 1.0f;
  if (settings.post.remapOutMax < settings.post.remapOutMin) {
    std::swap(settings.post.remapOutMin, settings.post.remapOutMax);
  }
  settings.post.blendWeight = std::isfinite(settings.post.blendWeight)
                                  ? std::clamp(settings.post.blendWeight, 0.0f, 1.0f)
                                  : 0.5f;
}

float evaluatedNoiseProperty(const ArtifactNoiseLayer* layer,
                             const QString& path, float fallback,
                             const ArtifactCore::RationalTime& time,
                             int64_t frame) {
  if (!layer) return fallback;
  float value = fallback;
  if (const auto property = layer->getProperty(path);
      property && (property->isAnimatable() || property->hasExpression() ||
                   property->hasEnvelopes() ||
                   property->hasExternalOverride())) {
    QVariant animated;
    if (property->hasExpression()) {
      ArtifactCore::ExpressionEvaluator evaluator;
      animated = property->evaluateValue(time, &evaluator);
    } else {
      animated = property->evaluateValue(time);
    }
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

bool evaluatedNoiseBoolean(const ArtifactNoiseLayer* layer,
                           const QString& path, bool fallback,
                           const ArtifactCore::RationalTime& time,
                           int64_t frame) {
  if (!layer) return fallback;
  bool value = fallback;
  if (const auto property = layer->getProperty(path);
      property && (property->isAnimatable() || property->hasExpression() ||
                   property->hasEnvelopes() ||
                   property->hasExternalOverride())) {
    QVariant animated;
    if (property->hasExpression()) {
      ArtifactCore::ExpressionEvaluator evaluator;
      animated = property->evaluateValue(time, &evaluator);
    } else {
      animated = property->evaluateValue(time);
    }
    if (animated.isValid()) value = animated.toBool();
  }
  if (const auto* stack = layer->animationLayerStack(path);
      stack && stack->layerCount() > 0) {
    value = stack->evaluateWithBase(ArtifactCore::FramePosition(frame),
                                    value ? 1.0f : 0.0f) > 0.5f;
  }
  return value;
}

QString evaluatedNoiseKind(const ArtifactNoiseLayer* layer,
                           const QString& path, const QString& fallback,
                           const ArtifactCore::RationalTime& time) {
  if (!layer) return fallback;
  if (const auto property = layer->getProperty(path);
      property && (property->isAnimatable() || property->hasExpression() ||
                   property->hasEnvelopes() ||
                   property->hasExternalOverride())) {
    QVariant animated;
    if (property->hasExpression()) {
      ArtifactCore::ExpressionEvaluator evaluator;
      animated = property->evaluateValue(time, &evaluator);
    } else {
      animated = property->evaluateValue(time);
    }
    const QString value = animated.toString().trimmed();
    if (!value.isEmpty()) return value;
  }
  return fallback;
}

FloatColor evaluatedNoiseColor(const ArtifactNoiseLayer* layer,
                               const QString& path, const FloatColor& fallback,
                               const ArtifactCore::RationalTime& time) {
  if (!layer) return fallback;
  if (const auto property = layer->getProperty(path);
      property && (property->isAnimatable() || property->hasExpression() ||
                   property->hasEnvelopes() ||
                   property->hasExternalOverride())) {
    QVariant animated;
    if (property->hasExpression()) {
      ArtifactCore::ExpressionEvaluator evaluator;
      animated = property->evaluateValue(time, &evaluator);
    } else {
      animated = property->evaluateValue(time);
    }
    if (animated.canConvert<QColor>()) {
      const auto color = animated.value<QColor>();
      return FloatColor(color.redF(), color.greenF(), color.blueF(),
                        color.alphaF());
    }
  }
  return fallback;
}

ArtifactCore::RationalTime noiseEvaluationTime(const ArtifactNoiseLayer* layer) {
  if (!layer) return ArtifactCore::RationalTime(0, 30);
  auto* composition = dynamic_cast<ArtifactAbstractCompositionAccess*>(
      layer->compositionObject());
  if (!composition) {
    return ArtifactCore::RationalTime(layer->currentFrame(), 30);
  }
  const double fps = composition->frameRate().framerate();
  const auto timeScale = std::max<int64_t>(
      1, static_cast<int64_t>(std::llround(fps > 0.0 ? fps : 30.0)));
  return ArtifactCore::RationalTime(
      composition->framePosition().framePosition(), timeScale);
}

ArtifactCore::ProceduralTextureSettings evaluatedNoiseSettings(
    const ArtifactNoiseLayer* layer,
    const ArtifactCore::ProceduralTextureSettings& base) {
  auto settings = base;
  if (!layer) return settings;
  const auto time = noiseEvaluationTime(layer);
  const int64_t frame = time.value();
  auto& p = settings.primary;
  noiseKindFromString(
      evaluatedNoiseKind(layer, QStringLiteral("noise.kind"),
                         noiseKindToString(p.kind, p.voronoiMode, p.gradientMode),
                         time),
      p.kind, p.voronoiMode, p.gradientMode);
  p.seed = static_cast<std::uint32_t>(std::max(
      0l, std::lround(evaluatedNoiseProperty(
              layer, QStringLiteral("noise.seed"),
              static_cast<float>(p.seed), time, frame))));
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
  p.cellJitter = evaluatedNoiseProperty(layer, QStringLiteral("noise.cellJitter"),
                                        p.cellJitter, time, frame);
  settings.seamless = evaluatedNoiseBoolean(
      layer, QStringLiteral("noise.seamless"), settings.seamless, time, frame);
  settings.post.domainWarpEnabled = evaluatedNoiseBoolean(
      layer, QStringLiteral("noise.domainWarp"), settings.post.domainWarpEnabled,
      time, frame);
  settings.post.useSecondary = evaluatedNoiseBoolean(
      layer, QStringLiteral("noise.useSecondary"), settings.post.useSecondary,
      time, frame);
  settings.post.invert = evaluatedNoiseBoolean(
      layer, QStringLiteral("noise.invert"), settings.post.invert, time, frame);
  settings.post.warpAmplitude = evaluatedNoiseProperty(
      layer, QStringLiteral("noise.warpAmplitude"), settings.post.warpAmplitude,
      time, frame);
  settings.post.gamma = evaluatedNoiseProperty(
      layer, QStringLiteral("noise.gamma"), settings.post.gamma, time, frame);
  settings.post.normalize = evaluatedNoiseBoolean(
      layer, QStringLiteral("noise.normalize"), settings.post.normalize, time,
      frame);
  settings.post.normalizeMin = evaluatedNoiseProperty(
      layer, QStringLiteral("noise.normalizeMin"), settings.post.normalizeMin,
      time, frame);
  settings.post.normalizeMax = evaluatedNoiseProperty(
      layer, QStringLiteral("noise.normalizeMax"), settings.post.normalizeMax,
      time, frame);
  settings.post.clampEnabled = evaluatedNoiseBoolean(
      layer, QStringLiteral("noise.clampEnabled"), settings.post.clampEnabled,
      time, frame);
  settings.post.clampMin = evaluatedNoiseProperty(
      layer, QStringLiteral("noise.clampMin"), settings.post.clampMin, time,
      frame);
  settings.post.clampMax = evaluatedNoiseProperty(
      layer, QStringLiteral("noise.clampMax"), settings.post.clampMax, time,
      frame);
  settings.post.remapEnabled = evaluatedNoiseBoolean(
      layer, QStringLiteral("noise.remapEnabled"), settings.post.remapEnabled,
      time, frame);
  settings.post.remapInMin = evaluatedNoiseProperty(
      layer, QStringLiteral("noise.remapInMin"), settings.post.remapInMin, time,
      frame);
  settings.post.remapInMax = evaluatedNoiseProperty(
      layer, QStringLiteral("noise.remapInMax"), settings.post.remapInMax, time,
      frame);
  settings.post.remapOutMin = evaluatedNoiseProperty(
      layer, QStringLiteral("noise.remapOutMin"), settings.post.remapOutMin,
      time, frame);
  settings.post.remapOutMax = evaluatedNoiseProperty(
      layer, QStringLiteral("noise.remapOutMax"), settings.post.remapOutMax,
      time, frame);
  settings.post.blendMode = static_cast<ArtifactCore::ProceduralTextureBlendMode>(
      std::clamp(std::lround(evaluatedNoiseProperty(
                   layer, QStringLiteral("noise.blendMode"),
                   static_cast<float>(settings.post.blendMode), time, frame)),
                 0l, 2l));
  settings.post.blendWeight = evaluatedNoiseProperty(
      layer, QStringLiteral("noise.blendWeight"), settings.post.blendWeight,
      time, frame);
  sanitizeNoiseSettings(settings);
  return settings;
}

const QStringList& animatedNoisePropertySuffixes() {
  static const QStringList suffixes = {
      QStringLiteral("kind"), QStringLiteral("seed"), QStringLiteral("scaleX"),
      QStringLiteral("scaleY"), QStringLiteral("offsetX"),
      QStringLiteral("offsetY"), QStringLiteral("rotation"),
      QStringLiteral("amplitude"), QStringLiteral("octaves"),
      QStringLiteral("lacunarity"), QStringLiteral("gain"),
      QStringLiteral("cellJitter"), QStringLiteral("seamless"),
      QStringLiteral("domainWarp"), QStringLiteral("warpAmplitude"),
      QStringLiteral("useSecondary"), QStringLiteral("gamma"),
      QStringLiteral("invert"),
      QStringLiteral("normalize"), QStringLiteral("normalizeMin"),
      QStringLiteral("normalizeMax"), QStringLiteral("clampEnabled"),
      QStringLiteral("clampMin"), QStringLiteral("clampMax"),
      QStringLiteral("remapEnabled"), QStringLiteral("remapInMin"),
      QStringLiteral("remapInMax"), QStringLiteral("remapOutMin"),
      QStringLiteral("remapOutMax"), QStringLiteral("blendMode"),
      QStringLiteral("blendWeight"),
      QStringLiteral("colorMapping"), QStringLiteral("colorA"),
      QStringLiteral("colorB")};
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

QJsonObject generatorParamsToJson(
    const ArtifactCore::ProceduralTextureGeneratorParams& params) {
  QJsonObject object;
  object[QStringLiteral("kind")] =
      noiseKindToString(params.kind, params.voronoiMode, params.gradientMode);
  object[QStringLiteral("seed")] = static_cast<int>(params.seed);
  object[QStringLiteral("scaleX")] = static_cast<double>(params.scale[0]);
  object[QStringLiteral("scaleY")] = static_cast<double>(params.scale[1]);
  object[QStringLiteral("offsetX")] = static_cast<double>(params.offset[0]);
  object[QStringLiteral("offsetY")] = static_cast<double>(params.offset[1]);
  object[QStringLiteral("rotation")] = static_cast<double>(params.rotation);
  object[QStringLiteral("amplitude")] = static_cast<double>(params.amplitude);
  object[QStringLiteral("octaves")] = static_cast<int>(params.octaves);
  object[QStringLiteral("lacunarity")] = static_cast<double>(params.lacunarity);
  object[QStringLiteral("gain")] = static_cast<double>(params.gain);
  object[QStringLiteral("cellJitter")] = static_cast<double>(params.cellJitter);
  return object;
}

void generatorParamsFromJson(
    const QJsonObject& object,
    ArtifactCore::ProceduralTextureGeneratorParams& params) {
  if (object.isEmpty()) return;
  if (object.contains(QStringLiteral("kind"))) {
    noiseKindFromString(object.value(QStringLiteral("kind")).toString(),
                        params.kind, params.voronoiMode, params.gradientMode);
  }
  params.seed = static_cast<std::uint32_t>(
      std::max(0, object.value(QStringLiteral("seed")).toInt(
                                  static_cast<int>(params.seed))));
  params.scale[0] = static_cast<float>(
      object.value(QStringLiteral("scaleX")).toDouble(params.scale[0]));
  params.scale[1] = static_cast<float>(
      object.value(QStringLiteral("scaleY")).toDouble(params.scale[1]));
  params.offset[0] = static_cast<float>(
      object.value(QStringLiteral("offsetX")).toDouble(params.offset[0]));
  params.offset[1] = static_cast<float>(
      object.value(QStringLiteral("offsetY")).toDouble(params.offset[1]));
  params.rotation = static_cast<float>(
      object.value(QStringLiteral("rotation")).toDouble(params.rotation));
  params.amplitude = static_cast<float>(
      object.value(QStringLiteral("amplitude")).toDouble(params.amplitude));
  params.octaves = static_cast<std::uint32_t>(std::clamp(
      object.value(QStringLiteral("octaves")).toInt(params.octaves), 1, 12));
  params.lacunarity = static_cast<float>(
      object.value(QStringLiteral("lacunarity")).toDouble(params.lacunarity));
  params.gain = static_cast<float>(
      object.value(QStringLiteral("gain")).toDouble(params.gain));
  params.cellJitter = static_cast<float>(
      object.value(QStringLiteral("cellJitter")).toDouble(params.cellJitter));
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
  const auto time = noiseEvaluationTime(this);
  const int64_t frame = time.value();
  const bool colorMapping = evaluatedNoiseBoolean(
      this, QStringLiteral("noise.colorMapping"),
      impl_->colorMappingEnabled_, time, frame);
  const auto colorA = evaluatedNoiseColor(
      this, QStringLiteral("noise.colorA"), impl_->colorA_, time);
  const auto colorB = evaluatedNoiseColor(
      this, QStringLiteral("noise.colorB"), impl_->colorB_, time);
  settings.width = width;
  settings.height = height;
  const QString signature = noiseSignatureKey(
      settings, colorMapping, colorA, colorB);
  if (impl_->buffer_.isEmpty() || impl_->bufferSignature_ != signature) {
    impl_->buffer_ = ArtifactCore::ImageF32x4_RGBA();
    if (!ArtifactCore::ProceduralTextureGenerator::generate(
            settings, impl_->buffer_)) {
      return nullptr;
    }
    if (colorMapping && impl_->buffer_.rgba32fData() &&
        impl_->buffer_.width() > 0 && impl_->buffer_.height() > 0) {
      const int pixelCount =
          impl_->buffer_.width() * impl_->buffer_.height();
      float* pixels = impl_->buffer_.rgba32fData();
      const FloatColor& a = colorA;
      const FloatColor& b = colorB;
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
  const auto time = noiseEvaluationTime(this);
  const int64_t frame = time.value();
  const bool colorMapping = evaluatedNoiseBoolean(
      this, QStringLiteral("noise.colorMapping"),
      impl_->colorMappingEnabled_, time, frame);
  if (!colorMapping) {
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

QJsonObject ArtifactNoiseLayer::sourceComponentSettingsSnapshot() const {
  QJsonObject obj;
  const auto source = sourceSize();
  obj[QStringLiteral("width")] = source.width;
  obj[QStringLiteral("height")] = source.height;
  const auto& s = impl_->settings_;
  const auto& p = s.primary;
  const auto& post = s.post;
  obj[QStringLiteral("kind")] = noiseKindToString(p.kind, p.voronoiMode, p.gradientMode);
  obj[QStringLiteral("seed")] = static_cast<int>(p.seed);
  obj[QStringLiteral("scaleX")] = static_cast<double>(p.scale[0]);
  obj[QStringLiteral("scaleY")] = static_cast<double>(p.scale[1]);
  obj[QStringLiteral("offsetX")] = static_cast<double>(p.offset[0]);
  obj[QStringLiteral("offsetY")] = static_cast<double>(p.offset[1]);
  obj[QStringLiteral("rotation")] = static_cast<double>(p.rotation);
  obj[QStringLiteral("amplitude")] = static_cast<double>(p.amplitude);
  obj[QStringLiteral("octaves")] = static_cast<int>(p.octaves);
  obj[QStringLiteral("lacunarity")] = static_cast<double>(p.lacunarity);
  obj[QStringLiteral("gain")] = static_cast<double>(p.gain);
  obj[QStringLiteral("cellJitter")] = static_cast<double>(p.cellJitter);
  obj[QStringLiteral("seamless")] = s.seamless;
  obj[QStringLiteral("domainWarp")] = post.domainWarpEnabled;
  obj[QStringLiteral("warpAmplitude")] = static_cast<double>(post.warpAmplitude);
  obj[QStringLiteral("useSecondary")] = post.useSecondary;
  obj[QStringLiteral("gamma")] = static_cast<double>(post.gamma);
  obj[QStringLiteral("invert")] = post.invert;
  obj[QStringLiteral("colorMapping")] = impl_->colorMappingEnabled_;
  QJsonObject colorAObj;
  colorAObj[QStringLiteral("r")] = impl_->colorA_.r();
  colorAObj[QStringLiteral("g")] = impl_->colorA_.g();
  colorAObj[QStringLiteral("b")] = impl_->colorA_.b();
  colorAObj[QStringLiteral("a")] = impl_->colorA_.a();
  obj[QStringLiteral("colorA")] = colorAObj;
  QJsonObject colorBObj;
  colorBObj[QStringLiteral("r")] = impl_->colorB_.r();
  colorBObj[QStringLiteral("g")] = impl_->colorB_.g();
  colorBObj[QStringLiteral("b")] = impl_->colorB_.b();
  colorBObj[QStringLiteral("a")] = impl_->colorB_.a();
  obj[QStringLiteral("colorB")] = colorBObj;
  return obj;
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
  noiseObj["parallel"] = settings.parallel;
  noiseObj["outputFormat"] = static_cast<int>(settings.outputFormat);
  noiseObj["domainWarp"] = post.domainWarpEnabled;
  noiseObj["warpAmplitude"] = static_cast<double>(post.warpAmplitude);
  noiseObj["useSecondary"] = post.useSecondary;
  noiseObj["gamma"] = static_cast<double>(post.gamma);
  noiseObj["invert"] = post.invert;
  noiseObj["normalize"] = post.normalize;
  noiseObj["normalizeMin"] = static_cast<double>(post.normalizeMin);
  noiseObj["normalizeMax"] = static_cast<double>(post.normalizeMax);
  noiseObj["clampEnabled"] = post.clampEnabled;
  noiseObj["clampMin"] = static_cast<double>(post.clampMin);
  noiseObj["clampMax"] = static_cast<double>(post.clampMax);
  noiseObj["remapEnabled"] = post.remapEnabled;
  noiseObj["remapInMin"] = static_cast<double>(post.remapInMin);
  noiseObj["remapInMax"] = static_cast<double>(post.remapInMax);
  noiseObj["remapOutMin"] = static_cast<double>(post.remapOutMin);
  noiseObj["remapOutMax"] = static_cast<double>(post.remapOutMax);
  noiseObj["blendMode"] = static_cast<int>(post.blendMode);
  noiseObj["blendWeight"] = static_cast<double>(post.blendWeight);
  noiseObj["secondary"] = generatorParamsToJson(post.secondary);
  noiseObj["warp"] = generatorParamsToJson(post.warp);
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
    settings.parallel = noiseObj.value("parallel").toBool(false);
    const int outputFormat = noiseObj.value("outputFormat").toInt(
        static_cast<int>(settings.outputFormat));
    settings.outputFormat = static_cast<ArtifactCore::ProceduralTextureOutputFormat>(
        std::clamp(outputFormat, 1, 3));
    settings.post.domainWarpEnabled =
        noiseObj.value("domainWarp").toBool(false);
    settings.post.warpAmplitude =
        static_cast<float>(noiseObj.value("warpAmplitude").toDouble(0.25));
    settings.post.useSecondary = noiseObj.value("useSecondary").toBool(false);
    settings.post.gamma =
        static_cast<float>(noiseObj.value("gamma").toDouble(1.0));
    settings.post.invert = noiseObj.value("invert").toBool(false);
    settings.post.normalize = noiseObj.value("normalize").toBool(false);
    settings.post.normalizeMin = static_cast<float>(
        noiseObj.value("normalizeMin").toDouble(0.0));
    settings.post.normalizeMax = static_cast<float>(
        noiseObj.value("normalizeMax").toDouble(1.0));
    settings.post.clampEnabled = noiseObj.value("clampEnabled").toBool(true);
    settings.post.clampMin = static_cast<float>(
        noiseObj.value("clampMin").toDouble(0.0));
    settings.post.clampMax = static_cast<float>(
        noiseObj.value("clampMax").toDouble(1.0));
    settings.post.remapEnabled = noiseObj.value("remapEnabled").toBool(false);
    settings.post.remapInMin = static_cast<float>(
        noiseObj.value("remapInMin").toDouble(0.0));
    settings.post.remapInMax = static_cast<float>(
        noiseObj.value("remapInMax").toDouble(1.0));
    settings.post.remapOutMin = static_cast<float>(
        noiseObj.value("remapOutMin").toDouble(0.0));
    settings.post.remapOutMax = static_cast<float>(
        noiseObj.value("remapOutMax").toDouble(1.0));
    settings.post.blendMode = static_cast<ArtifactCore::ProceduralTextureBlendMode>(
        std::clamp(noiseObj.value("blendMode").toInt(
                       static_cast<int>(settings.post.blendMode)),
                   0, 2));
    settings.post.blendWeight = static_cast<float>(
        noiseObj.value("blendWeight").toDouble(settings.post.blendWeight));
    if (noiseObj.value("secondary").isObject()) {
      generatorParamsFromJson(noiseObj.value("secondary").toObject(),
                              settings.post.secondary);
    }
    if (noiseObj.value("warp").isObject()) {
      generatorParamsFromJson(noiseObj.value("warp").toObject(),
                              settings.post.warp);
    }
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
  const auto& settings = impl_->settings_;
  const auto& p = settings.primary;
  auto kindProperty = persistentLayerProperty(
      QStringLiteral("noise.kind"), ArtifactCore::PropertyType::String,
      noiseKindToString(p.kind, p.voronoiMode, p.gradientMode), -105);
  kindProperty->setAnimatable(true);
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
  auto cellJitterProperty = persistentLayerProperty(
      QStringLiteral("noise.cellJitter"), ArtifactCore::PropertyType::Float,
      p.cellJitter, -105);
  cellJitterProperty->setAnimatable(true);
  cellJitterProperty->setHardRange(0.0, 1.0);
  cellJitterProperty->setDisplayLabel(QStringLiteral("セルジッター"));
  noiseGroup.addProperty(cellJitterProperty);
  auto seamlessProperty = persistentLayerProperty(
      QStringLiteral("noise.seamless"), ArtifactCore::PropertyType::Boolean,
      settings.seamless, -105);
  seamlessProperty->setAnimatable(true);
  seamlessProperty->setDisplayLabel(QStringLiteral("シームレス"));
  noiseGroup.addProperty(seamlessProperty);
  auto domainWarpProperty = persistentLayerProperty(
      QStringLiteral("noise.domainWarp"), ArtifactCore::PropertyType::Boolean,
      settings.post.domainWarpEnabled, -105);
  domainWarpProperty->setAnimatable(true);
  domainWarpProperty->setDisplayLabel(QStringLiteral("ドメインワープ"));
  noiseGroup.addProperty(domainWarpProperty);
  auto warpAmplitudeProperty = persistentLayerProperty(
      QStringLiteral("noise.warpAmplitude"), ArtifactCore::PropertyType::Float,
      settings.post.warpAmplitude, -105);
  warpAmplitudeProperty->setAnimatable(true);
  warpAmplitudeProperty->setHardRange(0.0, 16.0);
  warpAmplitudeProperty->setDisplayLabel(QStringLiteral("ワープ振幅"));
  noiseGroup.addProperty(warpAmplitudeProperty);
  auto secondaryProperty = persistentLayerProperty(
      QStringLiteral("noise.useSecondary"), ArtifactCore::PropertyType::Boolean,
      settings.post.useSecondary, -105);
  secondaryProperty->setAnimatable(true);
  secondaryProperty->setDisplayLabel(QStringLiteral("セカンダリ"));
  noiseGroup.addProperty(secondaryProperty);
  auto gammaProperty = persistentLayerProperty(
      QStringLiteral("noise.gamma"), ArtifactCore::PropertyType::Float,
      settings.post.gamma, -105);
  gammaProperty->setAnimatable(true);
  gammaProperty->setHardRange(0.01, 8.0);
  gammaProperty->setDisplayLabel(QStringLiteral("ガンマ"));
  noiseGroup.addProperty(gammaProperty);
  auto invertProperty = persistentLayerProperty(
      QStringLiteral("noise.invert"), ArtifactCore::PropertyType::Boolean,
      settings.post.invert, -105);
  invertProperty->setAnimatable(true);
  invertProperty->setDisplayLabel(QStringLiteral("反転"));
  noiseGroup.addProperty(invertProperty);
  auto normalizeProperty = persistentLayerProperty(
      QStringLiteral("noise.normalize"), ArtifactCore::PropertyType::Boolean,
      settings.post.normalize, -105);
  normalizeProperty->setAnimatable(true);
  normalizeProperty->setDisplayLabel(QStringLiteral("正規化"));
  noiseGroup.addProperty(normalizeProperty);
  auto normalizeMinProperty = persistentLayerProperty(
      QStringLiteral("noise.normalizeMin"), ArtifactCore::PropertyType::Float,
      settings.post.normalizeMin, -105);
  normalizeMinProperty->setAnimatable(true);
  normalizeMinProperty->setDisplayLabel(QStringLiteral("正規化下限"));
  noiseGroup.addProperty(normalizeMinProperty);
  auto normalizeMaxProperty = persistentLayerProperty(
      QStringLiteral("noise.normalizeMax"), ArtifactCore::PropertyType::Float,
      settings.post.normalizeMax, -105);
  normalizeMaxProperty->setAnimatable(true);
  normalizeMaxProperty->setDisplayLabel(QStringLiteral("正規化上限"));
  noiseGroup.addProperty(normalizeMaxProperty);
  auto clampProperty = persistentLayerProperty(
      QStringLiteral("noise.clampEnabled"), ArtifactCore::PropertyType::Boolean,
      settings.post.clampEnabled, -105);
  clampProperty->setAnimatable(true);
  clampProperty->setDisplayLabel(QStringLiteral("クランプ"));
  noiseGroup.addProperty(clampProperty);
  auto clampMinProperty = persistentLayerProperty(
      QStringLiteral("noise.clampMin"), ArtifactCore::PropertyType::Float,
      settings.post.clampMin, -105);
  clampMinProperty->setAnimatable(true);
  clampMinProperty->setDisplayLabel(QStringLiteral("クランプ下限"));
  noiseGroup.addProperty(clampMinProperty);
  auto clampMaxProperty = persistentLayerProperty(
      QStringLiteral("noise.clampMax"), ArtifactCore::PropertyType::Float,
      settings.post.clampMax, -105);
  clampMaxProperty->setAnimatable(true);
  clampMaxProperty->setDisplayLabel(QStringLiteral("クランプ上限"));
  noiseGroup.addProperty(clampMaxProperty);
  auto remapProperty = persistentLayerProperty(
      QStringLiteral("noise.remapEnabled"), ArtifactCore::PropertyType::Boolean,
      settings.post.remapEnabled, -105);
  remapProperty->setAnimatable(true);
  remapProperty->setDisplayLabel(QStringLiteral("リマップ"));
  noiseGroup.addProperty(remapProperty);
  auto remapInMinProperty = persistentLayerProperty(
      QStringLiteral("noise.remapInMin"), ArtifactCore::PropertyType::Float,
      settings.post.remapInMin, -105);
  remapInMinProperty->setAnimatable(true);
  remapInMinProperty->setDisplayLabel(QStringLiteral("入力下限"));
  noiseGroup.addProperty(remapInMinProperty);
  auto remapInMaxProperty = persistentLayerProperty(
      QStringLiteral("noise.remapInMax"), ArtifactCore::PropertyType::Float,
      settings.post.remapInMax, -105);
  remapInMaxProperty->setAnimatable(true);
  remapInMaxProperty->setDisplayLabel(QStringLiteral("入力上限"));
  noiseGroup.addProperty(remapInMaxProperty);
  auto remapOutMinProperty = persistentLayerProperty(
      QStringLiteral("noise.remapOutMin"), ArtifactCore::PropertyType::Float,
      settings.post.remapOutMin, -105);
  remapOutMinProperty->setAnimatable(true);
  remapOutMinProperty->setDisplayLabel(QStringLiteral("出力下限"));
  noiseGroup.addProperty(remapOutMinProperty);
  auto remapOutMaxProperty = persistentLayerProperty(
      QStringLiteral("noise.remapOutMax"), ArtifactCore::PropertyType::Float,
      settings.post.remapOutMax, -105);
  remapOutMaxProperty->setAnimatable(true);
  remapOutMaxProperty->setDisplayLabel(QStringLiteral("出力上限"));
  noiseGroup.addProperty(remapOutMaxProperty);
  auto blendModeProperty = persistentLayerProperty(
      QStringLiteral("noise.blendMode"), ArtifactCore::PropertyType::Integer,
      static_cast<int>(settings.post.blendMode), -105);
  blendModeProperty->setAnimatable(true);
  blendModeProperty->setHardRange(0.0, 2.0);
  blendModeProperty->setDisplayLabel(QStringLiteral("ブレンドモード"));
  noiseGroup.addProperty(blendModeProperty);
  auto blendWeightProperty = persistentLayerProperty(
      QStringLiteral("noise.blendWeight"), ArtifactCore::PropertyType::Float,
      settings.post.blendWeight, -105);
  blendWeightProperty->setAnimatable(true);
  blendWeightProperty->setHardRange(0.0, 1.0);
  blendWeightProperty->setDisplayLabel(QStringLiteral("ブレンド量"));
  noiseGroup.addProperty(blendWeightProperty);
  auto colorMappingProperty = persistentLayerProperty(
      QStringLiteral("noise.colorMapping"), ArtifactCore::PropertyType::Boolean,
      impl_->colorMappingEnabled_, -105);
  colorMappingProperty->setAnimatable(true);
  colorMappingProperty->setDisplayLabel(QStringLiteral("カラーマッピング"));
  noiseGroup.addProperty(colorMappingProperty);
  const auto mappedA = colorA();
  auto colorAProperty = persistentLayerProperty(
      QStringLiteral("noise.colorA"), ArtifactCore::PropertyType::Color,
      QColor::fromRgbF(mappedA.r(), mappedA.g(), mappedA.b(), mappedA.a()),
      -105);
  colorAProperty->setAnimatable(true);
  colorAProperty->setDisplayLabel(QStringLiteral("マップ色A"));
  noiseGroup.addProperty(colorAProperty);
  const auto mappedB = colorB();
  auto colorBProperty = persistentLayerProperty(
      QStringLiteral("noise.colorB"), ArtifactCore::PropertyType::Color,
      QColor::fromRgbF(mappedB.r(), mappedB.g(), mappedB.b(), mappedB.a()),
      -105);
  colorBProperty->setAnimatable(true);
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
  if (propertyPath == QStringLiteral("noise.cellJitter")) {
    p.cellJitter = static_cast<float>(
        std::clamp(value.toDouble(0.75), 0.0, 1.0));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.seamless")) {
    settings.seamless = value.toBool();
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.domainWarp")) {
    settings.post.domainWarpEnabled = value.toBool();
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.warpAmplitude")) {
    settings.post.warpAmplitude = static_cast<float>(
        std::clamp(value.toDouble(0.25), 0.0, 16.0));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.useSecondary")) {
    settings.post.useSecondary = value.toBool();
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.gamma")) {
    settings.post.gamma = static_cast<float>(
        std::clamp(value.toDouble(1.0), 0.01, 8.0));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.invert")) {
    settings.post.invert = value.toBool();
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.normalize")) {
    settings.post.normalize = value.toBool();
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.normalizeMin")) {
    settings.post.normalizeMin = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.normalizeMax")) {
    settings.post.normalizeMax = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.clampEnabled")) {
    settings.post.clampEnabled = value.toBool();
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.clampMin")) {
    settings.post.clampMin = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.clampMax")) {
    settings.post.clampMax = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.remapEnabled")) {
    settings.post.remapEnabled = value.toBool();
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.remapInMin")) {
    settings.post.remapInMin = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.remapInMax")) {
    settings.post.remapInMax = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.remapOutMin")) {
    settings.post.remapOutMin = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.remapOutMax")) {
    settings.post.remapOutMax = static_cast<float>(value.toDouble());
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.blendMode")) {
    settings.post.blendMode = static_cast<ArtifactCore::ProceduralTextureBlendMode>(
        std::clamp(value.toInt(), 0, 2));
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("noise.blendWeight")) {
    settings.post.blendWeight = static_cast<float>(
        std::clamp(value.toDouble(0.5), 0.0, 1.0));
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
