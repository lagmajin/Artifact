module;

#include <array>
#include <memory>
#include <string>
#include <vector>


export module Color.GradingEngine;

import Color.Float;

export namespace Artifact {

using namespace ArtifactCore;

// Color grading node types
enum class GradingNodeType {
  LiftGammaGain, // Primary correction
  ColorWheels,   // Shadows/Midtones/Highlights
  RGBCurves,     // Channel curves
  HueSatLum,     // Secondary correction
  LogAdjust      // Log space adjustments
};

// Primary color correction (Lift/Gamma/Gain)
struct LiftGammaGainNode {
  FloatColor lift = FloatColor(0, 0, 0, 0);  // Shadows adjustment
  FloatColor gamma = FloatColor(1, 1, 1, 1); // Midtones adjustment
  FloatColor gain = FloatColor(1, 1, 1, 1);  // Highlights adjustment
  float pivot = 0.5f;                        // Midtone pivot point
};

// Color wheel adjustments
struct ColorWheelNode {
  FloatColor shadows = FloatColor(0, 0, 0, 0);    // Shadow tint
  FloatColor midtones = FloatColor(0, 0, 0, 0);   // Midtone tint
  FloatColor highlights = FloatColor(0, 0, 0, 0); // Highlight tint
  float range = 0.5f;                             // Wheel influence range
};

// RGB curve node
struct RGBCurveNode {
  std::vector<std::pair<float, float>> redCurve; // Input->Output mapping
  std::vector<std::pair<float, float>> greenCurve;
  std::vector<std::pair<float, float>> blueCurve;
  std::vector<std::pair<float, float>> masterCurve;
};

// Secondary color correction
struct HueSatLumNode {
  float hue = 0.0f;                             // Hue shift in degrees
  float saturation = 1.0f;                      // Saturation multiplier
  float luminance = 1.0f;                       // Luminance multiplier
  FloatColor rangeMin = FloatColor(0, 0, 0, 0); // Color range minimum
  FloatColor rangeMax = FloatColor(1, 1, 1, 1); // Color range maximum
};

// Log adjustment node
struct LogAdjustNode {
  float exposure = 0.0f;                      // Exposure adjustment
  float contrast = 1.0f;                      // Contrast adjustment
  FloatColor offset = FloatColor(0, 0, 0, 0); // RGB offset
  float pivot = 0.5f;                         // Adjustment pivot
};

// Generic grading node
struct GradingNode {
  GradingNodeType type;
  bool enabled = true;
  std::string name;

  // Node data (union-like with std::variant in C++17)
  union {
    LiftGammaGainNode liftGammaGain;
    ColorWheelNode colorWheels;
    RGBCurveNode rgbCurves;
    HueSatLumNode hueSatLum;
    LogAdjustNode logAdjust;
  };

  GradingNode(GradingNodeType t, const std::string &n = "") : type(t), name(n) {
    // Initialize union members
    new (&liftGammaGain) LiftGammaGainNode();
  }

  GradingNode(const GradingNode &other);
  GradingNode &operator=(const GradingNode &other);
  ~GradingNode();
};

class ArtifactColorGradingEngine {
private:
  class Impl;
  Impl *impl_;

public:
  ArtifactColorGradingEngine();
  ~ArtifactColorGradingEngine();

  // Node management
  void addGradingNode(const GradingNode &node);
  void removeGradingNode(size_t index);
  void moveGradingNode(size_t fromIndex, size_t toIndex);
  GradingNode &getGradingNode(size_t index);
  const GradingNode &getGradingNode(size_t index) const;
  size_t getNodeCount() const;

  // Processing
  FloatColor applyGrading(const FloatColor &input) const;
  void applyGradingToBuffer(std::vector<FloatColor> &buffer) const;

  // Preset management
  void savePreset(const std::string &name);
  void loadPreset(const std::string &name);
  std::vector<std::string> getAvailablePresets() const;

  // Utility
  void resetAll();
  bool hasActiveNodes() const;

private:
  FloatColor applyLiftGammaGain(const FloatColor &color,
                                const LiftGammaGainNode &node) const;
  FloatColor applyColorWheels(const FloatColor &color,
                              const ColorWheelNode &node) const;
  FloatColor applyRGBCurves(const FloatColor &color,
                            const RGBCurveNode &node) const;
  FloatColor applyHueSatLum(const FloatColor &color,
                            const HueSatLumNode &node) const;
  FloatColor applyLogAdjust(const FloatColor &color,
                            const LogAdjustNode &node) const;

  float
  interpolateCurve(float input,
                   const std::vector<std::pair<float, float>> &curve) const;
};

} // namespace Artifact
