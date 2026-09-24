module;
#include <QObject>
#include <wobjectimpl.h>

import Color.Float;
import Color.Luminance;
import Core.Parallel;
import Event.Bus;

module Render.HDRMonitor;

import Core.ArtifactAlgorithms;
import Core.ArtifactMath;

namespace Artifact {

class ArtifactHDRMonitor::Impl {
public:
  HDRMonitorSettings settings_;
  mutable ArtifactArray<float> luminanceCache_;
  mutable ArtifactArray<FloatColor> colorCache_;
};

ArtifactHDRMonitor::ArtifactHDRMonitor()
    : QObject(nullptr), impl_(new Impl()) {}

ArtifactHDRMonitor::~ArtifactHDRMonitor()
{
  delete impl_;
  impl_ = nullptr;
}

void ArtifactHDRMonitor::setSettings(const HDRMonitorSettings &settings) {
  impl_->settings_ = settings;
  ArtifactCore::globalEventBus().publish<HDRMonitorSettingsChangedEvent>({});
}

HDRMonitorSettings ArtifactHDRMonitor::getSettings() const {
  return impl_->settings_;
}

HDRAnalysisResult
ArtifactHDRMonitor::analyzeFrame(const ArtifactArray<FloatColor> &frameData,
                                 int width, int height) {
  HDRAnalysisResult result;

  if (frameData.isEmpty() || width <= 0 || height <= 0) {
    impl_->luminanceCache_.removeAll();
    impl_->colorCache_.removeAll();
    return result;
  }

  // Keep the source RGB samples for scopes that need chroma information.
  impl_->colorCache_ = frameData;

  // Pre-calculate luminance values
  impl_->luminanceCache_.resize(frameData.size());
  ArtifactCore::Parallel::For(0, static_cast<int>(frameData.size()),
                              static_cast<int>(frameData.size()),
                              [&](int index) {
    const auto &color = frameData[static_cast<size_t>(index)];
    impl_->luminanceCache_[static_cast<size_t>(index)] =
        ArtifactCore::ColorLuminance::calculate(
            color.r(), color.g(), color.b(),
            ArtifactCore::LuminanceStandard::Rec709);
  });

  // Calculate statistics
  if (!impl_->luminanceCache_.isEmpty()) {
    const auto minMax = ArtifactCore::artifactMinMaxElement(
        impl_->luminanceCache_.begin(), impl_->luminanceCache_.end());
    result.minLuminance = *minMax.first;
    result.maxLuminance = *minMax.second;
    result.avgLuminance = ArtifactCore::artifactAccumulate(
                                    impl_->luminanceCache_.begin(),
                                    impl_->luminanceCache_.end(), 0.0f) /
                                impl_->luminanceCache_.size();
  }

  // Classify pixels independently, then reduce in input order so result
  // counters and the legacy out-of-gamut list remain deterministic.
  ArtifactArray<unsigned char> clipped(frameData.size(), 0);
  ArtifactArray<unsigned char> highlightClipped(frameData.size(), 0);
  ArtifactArray<unsigned char> outOfGamut(frameData.size(), 0);
  ArtifactArray<unsigned char> broadcastViolation(frameData.size(), 0);
  ArtifactCore::Parallel::For(0, static_cast<int>(frameData.size()),
                              static_cast<int>(frameData.size()),
                              [&](int index) {
    const auto &color = frameData[static_cast<size_t>(index)];
    bool rClipped = color.r() >= 1.0f || color.r() <= 0.0f;
    bool gClipped = color.g() >= 1.0f || color.g() <= 0.0f;
    bool bClipped = color.b() >= 1.0f || color.b() <= 0.0f;

    if (rClipped || gClipped || bClipped) {
      const auto i = static_cast<decltype(frameData.size())>(index);
      clipped[i] = 1;
      highlightClipped[i] = impl_->luminanceCache_[i] > 0.5f ? 1 : 0;
    }

    // Keep the legacy full-range gamut list and expose a separate legal-range
    // count for broadcast inspection. The latter is intentionally a lightweight
    // encoded-RGB check; full Y'CbCr gamut mapping belongs at export time.
    if (!isColorInGamut(color)) {
      outOfGamut[static_cast<size_t>(index)] = 1;
    }
    if (ArtifactCore::ColorLuminance::inspectBroadcastSafe(
            color.r(), color.g(), color.b(),
            ArtifactCore::LuminanceStandard::Rec709).hasViolation()) {
      broadcastViolation[static_cast<size_t>(index)] = 1;
    }
  });

  for (auto i = decltype(frameData.size()){0}; i < frameData.size(); ++i) {
    if (clipped[i]) {
      result.hasClipping = true;
      if (highlightClipped[i]) {
        ++result.clippedHighlights;
      } else {
        ++result.clippedShadows;
      }
    }
    if (outOfGamut[i]) {
      result.outOfGamutPixels.append(frameData[i]);
    }
    if (broadcastViolation[i]) {
      ++result.broadcastSafeViolations;
    }
  }

  ArtifactCore::globalEventBus().publish<HDRAnalysisCompletedEvent>({result});
  return result;
}

ArtifactArray<FloatColor>
ArtifactHDRMonitor::generateFalseColorOverlay(const HDRAnalysisResult &result,
                                              int width, int height) {
  ArtifactArray<FloatColor> overlay(width * height,
                                    FloatColor(0, 0, 0, 0)); // Transparent

  if (impl_->luminanceCache_.isEmpty())
    return overlay;

  // Create false color mapping
ArtifactCore::Parallel::For(0, height, width * height, [&](int y) {
    for (int x = 0; x < width; ++x) {
      int index = y * width + x;
      if (index >= static_cast<int>(impl_->luminanceCache_.size()))
        continue;

      float luminance = impl_->luminanceCache_[index];
      FloatColor falseColor = getFalseColorForLuminance(luminance);
      overlay[index] = FloatColor(falseColor.r(), falseColor.g(),
                                  falseColor.b(), 0.7f); // Semi-transparent
    }
  });

  return overlay;
}

ArtifactArray<FloatColor> ArtifactHDRMonitor::generateWaveformData(
    const HDRAnalysisResult &result, int waveformWidth, int waveformHeight) {
  ArtifactArray<FloatColor> waveform(waveformWidth * waveformHeight,
                                     FloatColor(0, 0, 0, 1));

  if (impl_->luminanceCache_.isEmpty())
    return waveform;

  // Simple waveform: luminance distribution across image width
  const int cacheSize = static_cast<int>(impl_->luminanceCache_.size());
  const int samplesPerColumn =
      ArtifactCore::artifactMax(1, (cacheSize + waveformWidth - 1) / waveformWidth);

  ArtifactCore::Parallel::For(0, waveformWidth, cacheSize, [&](int x) {
    int startIdx = x * samplesPerColumn;
    int endIdx = ArtifactCore::artifactMin(startIdx + samplesPerColumn, cacheSize);

    // Calculate average luminance for this column
    float avgLuminance = 0.0f;
    for (int i = startIdx; i < endIdx; ++i) {
      avgLuminance += impl_->luminanceCache_[i];
    }
    if (endIdx <= startIdx) {
      return;
    }
    avgLuminance /= static_cast<float>(endIdx - startIdx);

    // Draw vertical line at luminance level
    int yPos = static_cast<int>((1.0f - avgLuminance) * (waveformHeight - 1));
    yPos = ArtifactCore::artifactClamp(yPos, 0, waveformHeight - 1);

    waveform[yPos * waveformWidth + x] = FloatColor(1, 1, 1, 1); // White line
  });

  return waveform;
}

ArtifactArray<FloatColor>
ArtifactHDRMonitor::generateVectorscopeData(const HDRAnalysisResult &result,
                                            int scopeSize) {
  ArtifactArray<FloatColor> vectorscope(scopeSize * scopeSize,
                                        FloatColor(0, 0, 0, 1));

  if (impl_->luminanceCache_.isEmpty())
    return vectorscope;

  // Simple vectorscope: plot color points in UV space
  float centerX = scopeSize / 2.0f;
  float centerY = scopeSize / 2.0f;
  float scale = scopeSize / 2.0f * 0.8f; // Leave margin

  for (auto i = decltype(impl_->luminanceCache_.size()){0}; i < impl_->luminanceCache_.size();
       i += 100) { // Sample every 100th pixel for performance
    if (i >= impl_->luminanceCache_.size())
      break;

    // Convert RGB to YUV-like coordinates for vectorscope
    const FloatColor fallback(0.5f, 0.5f, 0.5f, 1.0f);
    const auto &color = i < impl_->colorCache_.size()
                            ? impl_->colorCache_[i]
                            : fallback;

    // Simplified UV calculation (should use proper color space conversion)
    float u = (color.b() - color.g()) * 0.5f;
    float v = (color.r() - color.g()) * 0.5f;

    int x = static_cast<int>(centerX + u * scale);
    int y = static_cast<int>(centerY + v * scale);

    x = ArtifactCore::artifactClamp(x, 0, scopeSize - 1);
    y = ArtifactCore::artifactClamp(y, 0, scopeSize - 1);

    // Preserve the sampled hue in the scope so dense regions remain legible.
    vectorscope[y * scopeSize + x] =
        FloatColor(ArtifactCore::artifactClamp(color.r(), 0.0f, 1.0f),
                   ArtifactCore::artifactClamp(color.g(), 0.0f, 1.0f),
                   ArtifactCore::artifactClamp(color.b(), 0.0f, 1.0f), 0.8f);
  }

  return vectorscope;
}

FloatColor ArtifactHDRMonitor::getFalseColorForLuminance(float luminance) {
  // Standard false color mapping for exposure
  if (luminance < 0.1f)
    return FloatColor(0, 0, 1, 1); // Deep blue (underexposed)
  if (luminance < 0.25f)
    return FloatColor(0, 0.5f, 1, 1); // Blue
  if (luminance < 0.5f)
    return FloatColor(0, 1, 1, 1); // Cyan (slightly underexposed)
  if (luminance < 0.75f)
    return FloatColor(0, 1, 0, 1); // Green (good exposure)
  if (luminance < 1.0f)
    return FloatColor(1, 1, 0, 1); // Yellow (slightly overexposed)
  if (luminance < 2.0f)
    return FloatColor(1, 0.5f, 0, 1); // Orange
  return FloatColor(1, 0, 0, 1);      // Red (overexposed)
}

bool ArtifactHDRMonitor::isColorInGamut(const FloatColor &color) {
  // Simple Rec. 709 gamut check (simplified)
  // In practice, this should use proper color space conversion
  return color.r() >= 0.0f && color.r() <= 1.0f && color.g() >= 0.0f &&
         color.g() <= 1.0f && color.b() >= 0.0f && color.b() <= 1.0f;
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactHDRMonitor)

// #include <moc_ArtifactHDRMonitor.cpp>
