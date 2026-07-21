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
    // Explicitly destroy the current active union member
    switch (type) {
    case GradingNodeType::LiftGammaGain:
      liftGammaGain.~LiftGammaGainNode();
      break;
    case GradingNodeType::ColorWheels:
      colorWheels.~ColorWheelNode();
      break;
    case GradingNodeType::RGBCurves:
      rgbCurves.~RGBCurveNode();
      break;
    case GradingNodeType::HueSatLum:
      hueSatLum.~HueSatLumNode();
      break;
    case GradingNodeType::LogAdjust:
      logAdjust.~LogAdjustNode();
      break;
    }

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
  switch (type) {
  case GradingNodeType::LiftGammaGain:
    liftGammaGain.~LiftGammaGainNode();
    break;
  case GradingNodeType::ColorWheels:
    colorWheels.~ColorWheelNode();
    break;
  case GradingNodeType::RGBCurves:
    rgbCurves.~RGBCurveNode();
    break;
  case GradingNodeType::HueSatLum:
    hueSatLum.~HueSatLumNode();
    break;
  case GradingNodeType::LogAdjust:
    logAdjust.~LogAdjustNode();
    break;
  }
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
  // Serialize all grading nodes to a JSON preset file
  Q_UNUSED(name);
  const QString presetName = QString::fromStdString(name);
  const QString presetDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/grading_presets");
  QDir().mkpath(presetDir);

  QJsonArray nodesArray;
  for (const GradingNode& node : impl_->nodes_) {
    QJsonObject nodeObj;
    nodeObj[QStringLiteral("type")] = static_cast<int>(node.type);
    nodeObj[QStringLiteral("enabled")] = node.enabled;
    nodeObj[QStringLiteral("name")] = QString::fromStdString(node.name);

    auto wr3 = [](QJsonObject& o, const char* kr, const char* kg, const char* kb, float r, float g, float b) {
      o[QString::fromLatin1(kr)] = r; o[QString::fromLatin1(kg)] = g; o[QString::fromLatin1(kb)] = b;
    };

    switch (node.type) {
    case GradingNodeType::LiftGammaGain: {
      QJsonObject d;
      wr3(d, "lift_r", "lift_g", "lift_b", node.liftGammaGain.lift.r(), node.liftGammaGain.lift.g(), node.liftGammaGain.lift.b());
      wr3(d, "gamma_r", "gamma_g", "gamma_b", node.liftGammaGain.gamma.r(), node.liftGammaGain.gamma.g(), node.liftGammaGain.gamma.b());
      wr3(d, "gain_r", "gain_g", "gain_b", node.liftGammaGain.gain.r(), node.liftGammaGain.gain.g(), node.liftGammaGain.gain.b());
      d[QStringLiteral("pivot")] = node.liftGammaGain.pivot;
      nodeObj[QStringLiteral("data")] = d;
      break;
    }
    case GradingNodeType::ColorWheels: {
      QJsonObject d;
      wr3(d, "shadows_r", "shadows_g", "shadows_b", node.colorWheels.shadows.r(), node.colorWheels.shadows.g(), node.colorWheels.shadows.b());
      wr3(d, "midtones_r", "midtones_g", "midtones_b", node.colorWheels.midtones.r(), node.colorWheels.midtones.g(), node.colorWheels.midtones.b());
      wr3(d, "highlights_r", "highlights_g", "highlights_b", node.colorWheels.highlights.r(), node.colorWheels.highlights.g(), node.colorWheels.highlights.b());
      d[QStringLiteral("range")] = node.colorWheels.range;
      nodeObj[QStringLiteral("data")] = d;
      break;
    }
    case GradingNodeType::RGBCurves: {
      QJsonObject d;
      auto pts = [](const std::vector<std::pair<float,float>>& pts) {
        QJsonArray a;
        for (const auto& [x, y] : pts) { QJsonObject p; p[QStringLiteral("x")] = x; p[QStringLiteral("y")] = y; a.append(p); }
        return a;
      };
      d[QStringLiteral("master")] = pts(node.rgbCurves.masterCurve);
      d[QStringLiteral("red")] = pts(node.rgbCurves.redCurve);
      d[QStringLiteral("green")] = pts(node.rgbCurves.greenCurve);
      d[QStringLiteral("blue")] = pts(node.rgbCurves.blueCurve);
      nodeObj[QStringLiteral("data")] = d;
      break;
    }
    case GradingNodeType::HueSatLum: {
      QJsonObject d;
      d[QStringLiteral("hue")] = node.hueSatLum.hue;
      d[QStringLiteral("saturation")] = node.hueSatLum.saturation;
      d[QStringLiteral("luminance")] = node.hueSatLum.luminance;
      wr3(d, "range_min_r", "range_min_g", "range_min_b", node.hueSatLum.rangeMin.r(), node.hueSatLum.rangeMin.g(), node.hueSatLum.rangeMin.b());
      wr3(d, "range_max_r", "range_max_g", "range_max_b", node.hueSatLum.rangeMax.r(), node.hueSatLum.rangeMax.g(), node.hueSatLum.rangeMax.b());
      nodeObj[QStringLiteral("data")] = d;
      break;
    }
    case GradingNodeType::LogAdjust: {
      QJsonObject d;
      d[QStringLiteral("exposure")] = node.logAdjust.exposure;
      d[QStringLiteral("contrast")] = node.logAdjust.contrast;
      wr3(d, "offset_r", "offset_g", "offset_b", node.logAdjust.offset.r(), node.logAdjust.offset.g(), node.logAdjust.offset.b());
      d[QStringLiteral("pivot")] = node.logAdjust.pivot;
      nodeObj[QStringLiteral("data")] = d;
      break;
    }
    }
    nodesArray.append(nodeObj);
  }

  QJsonObject root;
  root[QStringLiteral("version")] = 1;
  root[QStringLiteral("nodes")] = nodesArray;

  QFile file(presetDir + QStringLiteral("/") + presetName + QStringLiteral(".json"));
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  }
 
}

void ArtifactColorGradingEngine::loadPreset(const std::string &name) {
  // Deserialize grading nodes from a JSON preset file
  Q_UNUSED(name);
  const QString presetName = QString::fromStdString(name);
  const QString presetDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/grading_presets");

  QFile file(presetDir + QStringLiteral("/") + presetName + QStringLiteral(".json"));
  if (!file.open(QIODevice::ReadOnly)) return;

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) return;

  QJsonObject root = doc.object();
  QJsonArray nodesArray = root[QStringLiteral("nodes")].toArray();

  impl_->nodes_.clear();
  for (const QJsonValue& val : nodesArray) {
    if (!val.isObject()) continue;
    QJsonObject nodeObj = val.toObject();

    auto tp = static_cast<GradingNodeType>(nodeObj[QStringLiteral("type")].toInt(0));
    bool on = nodeObj[QStringLiteral("enabled")].toBool(true);
    std::string nm = nodeObj[QStringLiteral("name")].toString().toStdString();

    GradingNode node(tp, nm);
    node.enabled = on;

    QJsonObject d = nodeObj[QStringLiteral("data")].toObject();
    switch (tp) {
    case GradingNodeType::LiftGammaGain:
      node.liftGammaGain.lift = FloatColor(d[QStringLiteral("lift_r")].toDouble(), d[QStringLiteral("lift_g")].toDouble(), d[QStringLiteral("lift_b")].toDouble(), 0);
      node.liftGammaGain.gamma = FloatColor(d[QStringLiteral("gamma_r")].toDouble(), d[QStringLiteral("gamma_g")].toDouble(), d[QStringLiteral("gamma_b")].toDouble(), 0);
      node.liftGammaGain.gain = FloatColor(d[QStringLiteral("gain_r")].toDouble(), d[QStringLiteral("gain_g")].toDouble(), d[QStringLiteral("gain_b")].toDouble(), 0);
      node.liftGammaGain.pivot = d[QStringLiteral("pivot")].toDouble(0.5);
      break;
    case GradingNodeType::ColorWheels:
      node.colorWheels.shadows = FloatColor(d[QStringLiteral("shadows_r")].toDouble(), d[QStringLiteral("shadows_g")].toDouble(), d[QStringLiteral("shadows_b")].toDouble(), 0);
      node.colorWheels.midtones = FloatColor(d[QStringLiteral("midtones_r")].toDouble(), d[QStringLiteral("midtones_g")].toDouble(), d[QStringLiteral("midtones_b")].toDouble(), 0);
      node.colorWheels.highlights = FloatColor(d[QStringLiteral("highlights_r")].toDouble(), d[QStringLiteral("highlights_g")].toDouble(), d[QStringLiteral("highlights_b")].toDouble(), 0);
      node.colorWheels.range = d[QStringLiteral("range")].toDouble(0.5);
      break;
    case GradingNodeType::RGBCurves: {
      auto rd = [](const QJsonValue& v) {
        std::vector<std::pair<float,float>> pts;
        for (const QJsonValue& pv : v.toArray())
          pts.emplace_back(pv[QStringLiteral("x")].toDouble(), pv[QStringLiteral("y")].toDouble());
        return pts;
      };
      node.rgbCurves.masterCurve = rd(d[QStringLiteral("master")]);
      node.rgbCurves.redCurve = rd(d[QStringLiteral("red")]);
      node.rgbCurves.greenCurve = rd(d[QStringLiteral("green")]);
      node.rgbCurves.blueCurve = rd(d[QStringLiteral("blue")]);
      break;
    }
    case GradingNodeType::HueSatLum:
      node.hueSatLum.hue = d[QStringLiteral("hue")].toDouble();
      node.hueSatLum.saturation = d[QStringLiteral("saturation")].toDouble(1.0);
      node.hueSatLum.luminance = d[QStringLiteral("luminance")].toDouble(1.0);
      node.hueSatLum.rangeMin = FloatColor(d[QStringLiteral("range_min_r")].toDouble(), d[QStringLiteral("range_min_g")].toDouble(), d[QStringLiteral("range_min_b")].toDouble(), 0);
      node.hueSatLum.rangeMax = FloatColor(d[QStringLiteral("range_max_r")].toDouble(), d[QStringLiteral("range_max_g")].toDouble(), d[QStringLiteral("range_max_b")].toDouble(), 0);
      break;
    case GradingNodeType::LogAdjust:
      node.logAdjust.exposure = d[QStringLiteral("exposure")].toDouble();
      node.logAdjust.contrast = d[QStringLiteral("contrast")].toDouble(1.0);
      node.logAdjust.offset = FloatColor(d[QStringLiteral("offset_r")].toDouble(), d[QStringLiteral("offset_g")].toDouble(), d[QStringLiteral("offset_b")].toDouble(), 0);
      node.logAdjust.pivot = d[QStringLiteral("pivot")].toDouble(0.5);
      break;
    }
    impl_->nodes_.push_back(node);
  }
 
}

std::vector<std::string>
ArtifactColorGradingEngine::getAvailablePresets() const {
  // Return list of available preset files in the grading_presets directory
  const QString presetDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/grading_presets");
  QDir dir(presetDir);
  QStringList filters; filters << QStringLiteral("*.json");
  QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);
  std::vector<std::string> result;
  for (const QString& f : files) {
    QString name = f;
    name.chop(5); // remove ".json"
    result.push_back(name.toStdString());
  }
  return result;
}

void ArtifactColorGradingEngine::resetAll() { impl_->nodes_.clear(); }

bool ArtifactColorGradingEngine::hasActiveNodes() const {
  return std::any_of(impl_->nodes_.begin(), impl_->nodes_.end(),
                     [](const GradingNode &node) { return node.enabled; });
}

} // namespace Artifact
