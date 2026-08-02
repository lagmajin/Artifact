module;
#include <algorithm>
#include <cmath>
#include <QVariant>

module Artifact.Effect.Keying.LumaKey;

import Artifact.Effect.ImplBase;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;

namespace Artifact {
using namespace ArtifactCore;

void LumaKeyEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src,
                                    ImageF32x4RGBAWithCache& dst) {
  const auto& image = src.image();
  const float* data = image.rgba32fData();
  if (!data || image.width() <= 0 || image.height() <= 0) { dst = src; return; }
  ImageF32x4_RGBA result = image;
  float* out = result.rgba32fData();
  const float safeLow = std::isfinite(lowThreshold_) ? lowThreshold_ : 0.15f;
  const float safeHigh = std::isfinite(highThreshold_) ? highThreshold_ : 0.85f;
  const float safeSoftness = std::isfinite(softness_) ? softness_ : 0.08f;
  const float softness = std::max(0.001f, safeSoftness);
  const float low = std::clamp(std::min(safeLow, safeHigh), 0.0f, 1.0f);
  const float high = std::clamp(std::max(safeLow, safeHigh), 0.0f, 1.0f);
  Parallel::For(0, image.height(), image.width() * image.height(), [&](int y) {
    for (int x = 0; x < image.width(); ++x) {
      const int i = (y * image.width() + x) * 4;
      const float luma = data[i] * 0.2126f + data[i + 1] * 0.7152f + data[i + 2] * 0.0722f;
      if (!std::isfinite(luma) || !std::isfinite(data[i + 3])) {
        out[i + 3] = 0.0f;
        continue;
      }
      const float lower = std::clamp((luma - low) / softness, 0.0f, 1.0f);
      const float upper = std::clamp((high - luma) / softness, 0.0f, 1.0f);
      out[i + 3] = std::clamp(out[i + 3] * std::min(lower, upper), 0.0f, 1.0f);
    }
  });
  dst = ImageF32x4RGBAWithCache(result);
}

LumaKeyEffect::LumaKeyEffect() : ArtifactAbstractEffect() {
  typedCpuImpl_ = makeShared<LumaKeyEffectCPUImpl>();
  setCPUImpl(typedCpuImpl_);
  setDisplayName("Luma Key");
  setEffectID("Effect.Keying.LumaKey");
  setPipelineStage(EffectPipelineStage::Rasterizer);
}

std::vector<AbstractProperty> LumaKeyEffect::getProperties() const {
  std::vector<AbstractProperty> properties;
  for (const auto& item : {std::pair{"lowThreshold", typedCpuImpl_->lowThreshold()},
                           std::pair{"highThreshold", typedCpuImpl_->highThreshold()},
                           std::pair{"softness", typedCpuImpl_->softness()}}) {
    auto& property = properties.emplace_back();
    property.setName(item.first);
    property.setType(PropertyType::Float);
    property.setSoftRange(0.0, 1.0);
    property.setHardRange(0.0, 1.0);
    property.setDefaultValue(QVariant(static_cast<double>(item.second)));
    property.setValue(QVariant(static_cast<double>(item.second)));
  }
  return properties;
}

void LumaKeyEffect::setPropertyValue(const UniString& name, const QVariant& value) {
  const QString property = name.toQString();
  const float raw = static_cast<float>(value.toDouble());
  const float v = std::isfinite(raw) ? raw : 0.0f;
  if (property == "lowThreshold") typedCpuImpl_->setLowThreshold(std::clamp(v, 0.0f, 1.0f));
  else if (property == "highThreshold") typedCpuImpl_->setHighThreshold(std::clamp(v, 0.0f, 1.0f));
  else if (property == "softness") typedCpuImpl_->setSoftness(std::clamp(v, 0.001f, 1.0f));
}
}
