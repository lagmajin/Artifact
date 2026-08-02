module;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QVariant>

module Artifact.Effect.Keying.DifferenceKey;

import Artifact.Effect.ImplBase;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;

namespace Artifact {
using namespace ArtifactCore;

void DifferenceKeyEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src,
                                          ImageF32x4RGBAWithCache& dst) {
  const auto& image = src.image();
  const float* data = image.rgba32fData();
  if (!data || image.width() <= 0 || image.height() <= 0) { dst = src; return; }
  ImageF32x4_RGBA result = image;
  float* out = result.rgba32fData();
  const float softness = std::max(0.001f, softness_);
  const float threshold = std::clamp(threshold_, 0.0f, 1.732f);
  const float rr = referenceColor_.r(), rg = referenceColor_.g(), rb = referenceColor_.b();
  Parallel::For(0, image.height(), image.width() * image.height(), [&](int y) {
    for (int x = 0; x < image.width(); ++x) {
      const int i = (y * image.width() + x) * 4;
      const float distance = std::sqrt((data[i] - rr) * (data[i] - rr) +
          (data[i + 1] - rg) * (data[i + 1] - rg) + (data[i + 2] - rb) * (data[i + 2] - rb));
      if (!std::isfinite(distance) || !std::isfinite(data[i + 3])) {
        out[i + 3] = 0.0f;
        continue;
      }
      out[i + 3] = std::clamp(out[i + 3] * std::clamp((distance - threshold) / softness, 0.0f, 1.0f), 0.0f, 1.0f);
    }
  });
  dst = ImageF32x4RGBAWithCache(result);
}

DifferenceKeyEffect::DifferenceKeyEffect() : ArtifactAbstractEffect() {
  typedCpuImpl_ = makeShared<DifferenceKeyEffectCPUImpl>();
  setCPUImpl(typedCpuImpl_);
  setDisplayName("Difference Key");
  setEffectID("Effect.Keying.DifferenceKey");
  setPipelineStage(EffectPipelineStage::Rasterizer);
}

std::vector<AbstractProperty> DifferenceKeyEffect::getProperties() const {
  std::vector<AbstractProperty> result;
  auto& color = result.emplace_back();
  color.setName("referenceColor");
  color.setType(PropertyType::Color);
  const QColor defaultColor(Qt::black);
  color.setDefaultValue(QVariant::fromValue(defaultColor));
  color.setValue(QVariant::fromValue(defaultColor));
  for (const auto& item : {std::pair{"threshold", typedCpuImpl_->threshold()},
                           std::pair{"softness", typedCpuImpl_->softness()}}) {
    auto& property = result.emplace_back(); property.setName(item.first); property.setType(PropertyType::Float);
    property.setSoftRange(item.first == "threshold" ? 0.0 : 0.001,
                          item.first == "threshold" ? 1.732 : 1.0);
    property.setHardRange(item.first == "threshold" ? 0.0 : 0.001,
                          item.first == "threshold" ? 1.732 : 1.0);
    property.setDefaultValue(QVariant(static_cast<double>(item.second)));
    property.setValue(QVariant(static_cast<double>(item.second)));
  }
  return result;
}

void DifferenceKeyEffect::setPropertyValue(const UniString& name, const QVariant& value) {
  const QString property = name.toQString();
  if (property == "referenceColor" && value.canConvert<QColor>()) {
    const QColor color = value.value<QColor>();
    if (!color.isValid()) {
      return;
    }
    typedCpuImpl_->setReferenceColor(FloatRGBA(color.redF(), color.greenF(), color.blueF(), color.alphaF()));
  } else if (property == "threshold") {
    const float raw = static_cast<float>(value.toDouble());
    typedCpuImpl_->setThreshold(std::isfinite(raw) ? std::clamp(raw, 0.0f, 1.732f) : 0.0f);
  } else if (property == "softness") {
    const float raw = static_cast<float>(value.toDouble());
    typedCpuImpl_->setSoftness(std::isfinite(raw) ? std::clamp(raw, 0.001f, 1.0f) : 0.001f);
  }
}
}
