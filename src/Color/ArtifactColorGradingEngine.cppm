module;
#include <utility>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>


import Color.Float;

module Color.GradingEngine;

namespace Artifact {

using namespace ArtifactCore;

// GradingNode copy/move helpers
GradingNode::GradingNode(const GradingNode &other)
    : type(other.type), enabled(other.enabled), name(other.name) {
  switch (type) {
  case GradingNodeType::LiftGammaGain:
    new (&liftGammaGain) LiftGammaGainNode(other.liftGammaGain);
    break;
  case GradingNodeType::ColorWheels:
    new (&colorWheels) ColorWheelNode(other.colorWheels);
    break;
  case GradingNodeType::RGBCurves:
    new (&rgbCurves) RGBCurveNode(other.rgbCurves);
    break;
  case GradingNodeType::HueSatLum:
    new (&hueSatLum) HueSatLumNode(other.hueSatLum);
    break;
  case GradingNodeType::LogAdjust:
    new (&logAdjust) LogAdjustNode(other.logAdjust);
    break;
  }
}

GradingNode &GradingNode::operator=(const GradingNode &other) {
  if (this != &other) {
    // Destroy current
    this->~GradingNode();

    // Copy new
    type = other.type;
    enabled = other.enabled;
    name = other.name;

    switch (type) {
    case GradingNodeType::LiftGammaGain:
      new (&liftGammaGain) LiftGammaGainNode(other.liftGammaGain);
      break;
    case GradingNodeType::ColorWheels:
      new (&colorWheels) ColorWheelNode(other.colorWheels);
      break;
    case GradingNodeType::RGBCurves:
      new (&rgbCurves) RGBCurveNode(other.rgbCurves);
      break;
    case GradingNodeType::HueSatLum:
      new (&hueSatLum) HueSatLumNode(other.hueSatLum);
      break;
    case GradingNodeType::LogAdjust:
      new (&logAdjust) LogAdjustNode(other.logAdjust);
      break;
    }
  }
  return *this;
}

GradingNode::~GradingNode() {
  // Union destructor - no action needed for POD types
}

class ArtifactColorGradingEngine::Impl {
public:
  std::vector<GradingNode> nodes_;
};

ArtifactColorGradingEngine::ArtifactColorGradingEngine() : impl_(new Impl()) {}

ArtifactColorGradingEngine::~ArtifactColorGradingEngine() { delete impl_; }

void ArtifactColorGradingEngine::addGradingNode(const GradingNode &node) {
  impl_->nodes_.push_back(node);
}

void ArtifactColorGradingEngine::removeGradingNode(size_t index) {
  if (index < impl_->nodes_.size()) {
    impl_->nodes_.erase(impl_->nodes_.begin() + index);
  }
}

void ArtifactColorGradingEngine::moveGradingNode(size_t fromIndex,
                                                 size_t toIndex) {
  if (fromIndex < impl_->nodes_.size() && toIndex < impl_->nodes_.size()) {
    auto node = std::move(impl_->nodes_[fromIndex]);
    impl_->nodes_.erase(impl_->nodes_.begin() + fromIndex);
    impl_->nodes_.insert(impl_->nodes_.begin() + toIndex, std::move(node));
  }
}

GradingNode &ArtifactColorGradingEngine::getGradingNode(size_t index) {
  return impl_->nodes_.at(index);
}

const GradingNode &
ArtifactColorGradingEngine::getGradingNode(size_t index) const {
  return impl_->nodes_.at(index);
}

size_t ArtifactColorGradingEngine::getNodeCount() const {
  return impl_->nodes_.size();
}

FloatColor
ArtifactColorGradingEngine::applyGrading(const FloatColor &input) const {
  FloatColor result = input;

  for (const auto &node : impl_->nodes_) {
    if (!node.enabled)
      continue;

    switch (node.type) {
    case GradingNodeType::LiftGammaGain:
      result = applyLiftGammaGain(result, node.liftGammaGain);
      break;
    case GradingNodeType::ColorWheels:
      result = applyColorWheels(result, node.colorWheels);
      break;
    case GradingNodeType::RGBCurves:
      result = applyRGBCurves(result, node.rgbCurves);
      break;
    case GradingNodeType::HueSatLum:
      result = applyHueSatLum(result, node.hueSatLum);
      break;
    case GradingNodeType::LogAdjust:
      result = applyLogAdjust(result, node.logAdjust);
      break;
    }
  }

  return result;
}

void ArtifactColorGradingEngine::applyGradingToBuffer(
    std::vector<FloatColor> &buffer) const {
  for (auto &color : buffer) {
    color = applyGrading(color);
  }
}

FloatColor ArtifactColorGradingEngine::applyLiftGammaGain(
    const FloatColor &color, const LiftGammaGainNode &node) const {
  // Calculate luminance for pivot-based adjustment
  float luminance =
      0.2126f * color.r() + 0.7152f * color.g() + 0.0722f * color.b();

  FloatColor result = color;

  // Apply lift/gamma/gain based on luminance
  if (luminance < node.pivot) {
    // Shadows region - apply lift
    float factor = luminance / node.pivot;
    result = FloatColor(color.r() + node.lift.r() * (1.0f - factor),
                        color.g() + node.lift.g() * (1.0f - factor),
                        color.b() + node.lift.b() * (1.0f - factor), color.a());
  } else {
    // Highlights region - apply gain
    float factor = (luminance - node.pivot) / (1.0f - node.pivot);
    result = FloatColor(
        color.r() * node.gain.r() * factor + color.r() * (1.0f - factor),
        color.g() * node.gain.g() * factor + color.g() * (1.0f - factor),
        color.b() * node.gain.b() * factor + color.b() * (1.0f - factor),
        color.a());
  }

  // Apply gamma to midtones
  float gammaFactor = (luminance - node.pivot) / (1.0f - node.pivot);
  if (gammaFactor > 0) {
    result =
        FloatColor(std::pow(result.r(), 1.0f / node.gamma.r()),
                   std::pow(result.g(), 1.0f / node.gamma.g()),
                   std::pow(result.b(), 1.0f / node.gamma.b()), result.a());
  }

  return result;
}

FloatColor
ArtifactColorGradingEngine::applyColorWheels(const FloatColor &color,
                                             const ColorWheelNode &node) const {
  float luminance =
      0.2126f * color.r() + 0.7152f * color.g() + 0.0722f * color.b();

  FloatColor tint = FloatColor(0, 0, 0, 0);

  // Blend between shadows/midtones/highlights based on luminance
  if (luminance < 0.5f) {
    // Shadows to midtones blend
    float factor = luminance / 0.5f;
    tint = FloatColor(
        node.shadows.r() * (1.0f - factor) + node.midtones.r() * factor,
        node.shadows.g() * (1.0f - factor) + node.midtones.g() * factor,
        node.shadows.b() * (1.0f - factor) + node.midtones.b() * factor, 0);
  } else {
    // Midtones to highlights blend
    float factor = (luminance - 0.5f) / 0.5f;
    tint = FloatColor(
        node.midtones.r() * (1.0f - factor) + node.highlights.r() * factor,
        node.midtones.g() * (1.0f - factor) + node.highlights.g() * factor,
        node.midtones.b() * (1.0f - factor) + node.highlights.b() * factor, 0);
  }

  return FloatColor(color.r() + tint.r(), color.g() + tint.g(),
                    color.b() + tint.b(), color.a());
}

FloatColor
ArtifactColorGradingEngine::applyRGBCurves(const FloatColor &color,
                                           const RGBCurveNode &node) const {
  return FloatColor(interpolateCurve(color.r(), node.redCurve),
                    interpolateCurve(color.g(), node.greenCurve),
                    interpolateCurve(color.b(), node.blueCurve), color.a());
}

FloatColor
ArtifactColorGradingEngine::applyHueSatLum(const FloatColor &color,
                                           const HueSatLumNode &node) const {
  // Check if color is in the adjustment range
  bool inRange =
      color.r() >= node.rangeMin.r() && color.r() <= node.rangeMax.r() &&
      color.g() >= node.rangeMin.g() && color.g() <= node.rangeMax.g() &&
      color.b() >= node.rangeMin.b() && color.b() <= node.rangeMax.b();

  if (!inRange)
    return color;

  // Convert RGB to HSL for adjustment
  // Simplified HSL conversion (could use proper color space conversion)
  float max = std::max({color.r(), color.g(), color.b()});
  float min = std::min({color.r(), color.g(), color.b()});
  float delta = max - min;

  float h = 0, s = 0, l = (max + min) / 2;

  if (delta != 0) {
    s = l > 0.5f ? delta / (2 - max - min) : delta / (max + min);

    if (max == color.r())
      h = (color.g() - color.b()) / delta + (color.g() < color.b() ? 6 : 0);
    else if (max == color.g())
      h = (color.b() - color.r()) / delta + 2;
    else
      h = (color.r() - color.g()) / delta + 4;

    h /= 6;
  }

  // Apply adjustments
  h = std::fmod(h + node.hue / 360.0f, 1.0f);
  s *= node.saturation;
  l *= node.luminance;

  // Convert back to RGB (simplified)
  auto hueToRgb = [](float p, float q, float t) {
    if (t < 0)
      t += 1;
    if (t > 1)
      t -= 1;
    if (t < 1.0f / 6)
      return p + (q - p) * 6 * t;
    if (t < 1.0f / 2)
      return q;
    if (t < 2.0f / 3)
      return p + (q - p) * (2.0f / 3 - t) * 6;
    return p;
  };

  if (s == 0) {
    return FloatColor(l, l, l, color.a());
  }

  float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
  float p = 2 * l - q;

  return FloatColor(hueToRgb(p, q, h + 1.0f / 3), hueToRgb(p, q, h),
                    hueToRgb(p, q, h - 1.0f / 3), color.a());
}

FloatColor
ArtifactColorGradingEngine::applyLogAdjust(const FloatColor &color,
                                           const LogAdjustNode &node) const {
  // Apply log-space adjustments
  FloatColor result = color;

  // Exposure adjustment (log space)
  if (node.exposure != 0) {
    result = FloatColor(result.r() * std::pow(2, node.exposure),
                        result.g() * std::pow(2, node.exposure),
                        result.b() * std::pow(2, node.exposure), result.a());
  }

  // Contrast adjustment
  if (node.contrast != 1.0f) {
    result = FloatColor(std::pow(result.r(), 1.0f / node.contrast),
                        std::pow(result.g(), 1.0f / node.contrast),
                        std::pow(result.b(), 1.0f / node.contrast), result.a());
  }

  // RGB offset
  result =
      FloatColor(result.r() + node.offset.r(), result.g() + node.offset.g(),
                 result.b() + node.offset.b(), result.a());

  return result;
}

float ArtifactColorGradingEngine::interpolateCurve(
    float input, const std::vector<std::pair<float, float>> &curve) const {
  if (curve.empty())
    return input;
  if (curve.size() == 1)
    return curve[0].second;

  // Find the appropriate segment
  for (size_t i = 0; i < curve.size() - 1; ++i) {
    if (input >= curve[i].first && input <= curve[i + 1].first) {
      float t =
          (input - curve[i].first) / (curve[i + 1].first - curve[i].first);
      return curve[i].second + t * (curve[i + 1].second - curve[i].second);
    }
  }

  // Extrapolate
  if (input < curve[0].first) {
    return curve[0].second;
  }
  return curve.back().second;
}

void ArtifactColorGradingEngine::savePreset(const std::string &name) {
  // Implementation for saving presets to JSON
  // (Simplified - would save node configurations)
}

void ArtifactColorGradingEngine::loadPreset(const std::string &name) {
  // Implementation for loading presets from JSON
  // (Simplified - would load and recreate nodes)
}

std::vector<std::string>
ArtifactColorGradingEngine::getAvailablePresets() const {
  // Return list of available preset files
  return {};
}

void ArtifactColorGradingEngine::resetAll() { impl_->nodes_.clear(); }

bool ArtifactColorGradingEngine::hasActiveNodes() const {
  return std::any_of(impl_->nodes_.begin(), impl_->nodes_.end(),
                     [](const GradingNode &node) { return node.enabled; });
}

} // namespace Artifact
