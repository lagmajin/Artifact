module;

#include <wobjectdefs.h>
#include <QObject>
#include <QRect>
#include <memory>
#include <vector>

export module Render.HDRMonitor;

import Color.Float;

export namespace Artifact {

using namespace ArtifactCore;

enum class HDRMonitorMode {
  None,        // No monitoring
  FalseColor,  // False color overlay for exposure
  Waveform,    // Waveform monitor
  Vectorscope, // Vectorscope
  GamutWarning // Color gamut warnings
};

struct HDRMonitorSettings {
  HDRMonitorMode mode = HDRMonitorMode::None;
  bool enabled = false;
  float exposureMin = -4.0f; // Stops below middle gray
  float exposureMax = 4.0f;  // Stops above middle gray
  bool showGamutWarnings = true;
  QRect monitorRect; // Screen position for overlay
};

struct HDRAnalysisResult {
  float minLuminance = 0.0f;
  float maxLuminance = 1.0f;
  float avgLuminance = 0.5f;
  std::vector<FloatColor> outOfGamutPixels;
  bool hasClipping = false;
  int clippedHighlights = 0;
  int clippedShadows = 0;
};

class ArtifactHDRMonitor : public QObject {
  W_OBJECT(ArtifactHDRMonitor)

private:
  class Impl;
  Impl *impl_;

public:
  ArtifactHDRMonitor();
  ~ArtifactHDRMonitor();

  // Settings
  void setSettings(const HDRMonitorSettings &settings);
  HDRMonitorSettings getSettings() const;

  // Analysis
  HDRAnalysisResult analyzeFrame(const std::vector<FloatColor> &frameData,
                                 int width, int height);

  // Visualization
  std::vector<FloatColor>
  generateFalseColorOverlay(const HDRAnalysisResult &result, int width,
                            int height);
  std::vector<FloatColor> generateWaveformData(const HDRAnalysisResult &result,
                                               int waveformWidth,
                                               int waveformHeight);
  std::vector<FloatColor>
  generateVectorscopeData(const HDRAnalysisResult &result, int scopeSize);

  // Utility
  static FloatColor getFalseColorForLuminance(float luminance);
  static bool isColorInGamut(const FloatColor &color);

  // Signals
  void analysisComplete(const HDRAnalysisResult &result)
      W_SIGNAL(analysisComplete, result);
  void settingsChanged() W_SIGNAL(settingsChanged);
};

} // namespace Artifact

W_REGISTER_ARGTYPE(Artifact::HDRAnalysisResult)
