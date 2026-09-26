module;
#include <QObject>
#include <QRect>
#include <wobjectdefs.h>
export module Render.HDRMonitor;

export import Core.ArtifactArray;
import Color.Float;
export import Color.GamutConversion;
export import Color.Luminance;
export import Color.TransferFunction;

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

enum class ScopeSignalDomain {
  SceneLinear,
  DisplayEncoded
};

enum class ScopeSignalRange {
  Full,
  VideoLegal
};

// Explicit contract for scope/QC analysis. The input buffer remains float RGBA;
// this descriptor determines how its RGB values are interpreted.
struct ScopeAnalysisDescriptor {
  ScopeSignalDomain domain = ScopeSignalDomain::SceneLinear;
  ScopeSignalRange signalRange = ScopeSignalRange::Full;
  // Primaries used to interpret the incoming RGB triplet.
  Gamut primaries = Gamut::Rec709;
  // Delivery gamut used for chromaticity-gamut and legal-range inspection.
  Gamut targetGamut = Gamut::Rec709;
  LuminanceStandard luminanceStandard = LuminanceStandard::Rec709;
  TransferFunction transferFunction = TransferFunction::Linear;
  float referenceWhiteNits = 100.0f;
  float peakLuminanceNits = 1000.0f;
  float lowClipThreshold = 0.0f;
  float highClipThreshold = 1.0f;
  float gamutTolerance = 1.0e-5f;
  int bitDepth = 10;
  int sampleStep = 1;
  int maxOutOfGamutSamples = 4096;
  bool publishEvent = true;
};

struct HDRAnalysisResult {
  float minLuminance = 0.0f;
  float maxLuminance = 1.0f;
  float avgLuminance = 0.5f;
  float minLuminanceNits = 0.0f;
  float maxLuminanceNits = 0.0f;
  float avgLuminanceNits = 0.0f;
  float minRed = 0.0f;
  float maxRed = 0.0f;
  float minGreen = 0.0f;
  float maxGreen = 0.0f;
  float minBlue = 0.0f;
  float maxBlue = 0.0f;
  ArtifactArray<FloatColor> outOfGamutPixels;
  bool hasClipping = false;
  int clippedHighlights = 0;
  int clippedShadows = 0;
  int broadcastSafeViolations = 0;
  int luminanceLegalRangeViolations = 0;
  int channelLegalRangeViolations = 0;
  int nonFiniteSamples = 0;
  int analyzedSamples = 0;
  int validSamples = 0;
  int outOfGamutSamples = 0;
  int sourcePixels = 0;
  int sampleStep = 1;
  bool outOfGamutSamplesTruncated = false;
  ScopeAnalysisDescriptor descriptor;
};

struct HDRMonitorSettingsChangedEvent {
};

struct HDRAnalysisCompletedEvent {
  HDRAnalysisResult result;
};

class ArtifactHDRMonitor : public QObject {
  W_OBJECT(ArtifactHDRMonitor)

private:
  class Impl;
  Impl *impl_ = nullptr;

public:
  ArtifactHDRMonitor();
  ~ArtifactHDRMonitor();

  // Settings
  void setSettings(const HDRMonitorSettings &settings);
  HDRMonitorSettings getSettings() const;

  // Analysis
  HDRAnalysisResult analyzeFrame(const ArtifactArray<FloatColor> &frameData,
                                 int width, int height);
  HDRAnalysisResult analyzeFrame(const ArtifactArray<FloatColor> &frameData,
                                 int width, int height,
                                 const ScopeAnalysisDescriptor &descriptor);

  // Visualization
  ArtifactArray<FloatColor>
  generateFalseColorOverlay(const HDRAnalysisResult &result, int width,
                            int height);
  ArtifactArray<FloatColor> generateWaveformData(const HDRAnalysisResult &result,
                                                  int waveformWidth,
                                                  int waveformHeight);
  ArtifactArray<FloatColor>
  generateVectorscopeData(const HDRAnalysisResult &result, int scopeSize);

  // Utility
  static FloatColor getFalseColorForLuminance(float luminance);
  static bool isColorInGamut(const FloatColor &color);

};

} // namespace Artifact

W_REGISTER_ARGTYPE(Artifact::HDRAnalysisResult)
