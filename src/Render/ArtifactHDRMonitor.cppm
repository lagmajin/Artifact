module;
#include <utility>
#include <QObject>
#include <wobjectimpl.h>

import std;

import Color.Float;

module Render.HDRMonitor;

namespace Artifact {

class ArtifactHDRMonitor::Impl {
public:
  HDRMonitorSettings settings_;
  mutable std::vector<float> luminanceCache_;
};

ArtifactHDRMonitor::ArtifactHDRMonitor()
    : QObject(nullptr), impl_(new Impl()) {}

ArtifactHDRMonitor::~ArtifactHDRMonitor() { delete impl_; }

void ArtifactHDRMonitor::setSettings(const HDRMonitorSettings &settings) {
  impl_->settings_ = settings;
  Q_EMIT settingsChanged();
}

HDRMonitorSettings ArtifactHDRMonitor::getSettings() const {
  return impl_->settings_;
}

HDRAnalysisResult
ArtifactHDRMonitor::analyzeFrame(const std::vector<FloatColor> &frameData,
                                 int width, int height) {
  HDRAnalysisResult result;

  if (frameData.empty() || width <= 0 || height <= 0) {
    return result;
  }

  // Pre-calculate luminance values
  impl_->luminanceCache_.clear();
  impl_->luminanceCache_.reserve(frameData.size());

  for (const auto &color : frameData) {
    // Rec. 709 luminance calculation
    float luminance =
        0.2126f * color.r() + 0.7152f * color.g() + 0.0722f * color.b();
    impl_->luminanceCache_.push_back(luminance);
  }

  // Calculate statistics
  if (!impl_->luminanceCache_.empty()) {
    auto [minIt, maxIt] = std::minmax_element(impl_->luminanceCache_.begin(),
                                              impl_->luminanceCache_.end());
    result.minLuminance = *minIt;
    result.maxLuminance = *maxIt;

    result.avgLuminance = std::accumulate(impl_->luminanceCache_.begin(),
                                          impl_->luminanceCache_.end(), 0.0f) /
                          impl_->luminanceCache_.size();
  }

  // Check for clipping (assuming 0-1 range for now)
  for (size_t i = 0; i < frameData.size(); ++i) {
    const auto &color = frameData[i];
    bool rClipped = color.r() >= 1.0f || color.r() <= 0.0f;
    bool gClipped = color.g() >= 1.0f || color.g() <= 0.0f;
    bool bClipped = color.b() >= 1.0f || color.b() <= 0.0f;

    if (rClipped || gClipped || bClipped) {
      result.hasClipping = true;
      if (impl_->luminanceCache_[i] > 0.5f) {
        result.clippedHighlights++;
      } else {
        result.clippedShadows++;
      }
    }

    // Gamut check (Rec. 709 for now)
    if (!isColorInGamut(color)) {
      result.outOfGamutPixels.push_back(color);
    }
  }

  Q_EMIT analysisComplete(result);
  return result;
}

std::vector<FloatColor>
ArtifactHDRMonitor::generateFalseColorOverlay(const HDRAnalysisResult &result,
                                              int width, int height) {
  std::vector<FloatColor> overlay(width * height,
                                  FloatColor(0, 0, 0, 0)); // Transparent

  if (impl_->luminanceCache_.empty())
    return overlay;

  // Create false color mapping
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int index = y * width + x;
      if (index >= static_cast<int>(impl_->luminanceCache_.size()))
        continue;

      float luminance = impl_->luminanceCache_[index];
      FloatColor falseColor = getFalseColorForLuminance(luminance);
      overlay[index] = FloatColor(falseColor.r(), falseColor.g(),
                                  falseColor.b(), 0.7f); // Semi-transparent
    }
  }

  return overlay;
}

std::vector<FloatColor> ArtifactHDRMonitor::generateWaveformData(
    const HDRAnalysisResult &result, int waveformWidth, int waveformHeight) {
  std::vector<FloatColor> waveform(waveformWidth * waveformHeight,
                                   FloatColor(0, 0, 0, 1));

  if (impl_->luminanceCache_.empty())
    return waveform;

  // Simple waveform: luminance distribution across image width
  int samplesPerColumn =
      static_cast<int>(impl_->luminanceCache_.size()) / waveformWidth;

  for (int x = 0; x < waveformWidth; ++x) {
    int startIdx = x * samplesPerColumn;
    int endIdx = std::min(startIdx + samplesPerColumn,
                          static_cast<int>(impl_->luminanceCache_.size()));

    // Calculate average luminance for this column
    float avgLuminance = 0.0f;
    for (int i = startIdx; i < endIdx; ++i) {
      avgLuminance += impl_->luminanceCache_[i];
    }
    avgLuminance /= (endIdx - startIdx);

    // Draw vertical line at luminance level
    int yPos = static_cast<int>((1.0f - avgLuminance) * (waveformHeight - 1));
    yPos = std::clamp(yPos, 0, waveformHeight - 1);

    waveform[yPos * waveformWidth + x] = FloatColor(1, 1, 1, 1); // White line
  }

  return waveform;
}

std::vector<FloatColor>
ArtifactHDRMonitor::generateVectorscopeData(const HDRAnalysisResult &result,
                                            int scopeSize) {
  std::vector<FloatColor> vectorscope(scopeSize * scopeSize,
                                      FloatColor(0, 0, 0, 1));

  if (impl_->luminanceCache_.empty())
    return vectorscope;

  // Simple vectorscope: plot color points in UV space
  float centerX = scopeSize / 2.0f;
  float centerY = scopeSize / 2.0f;
  float scale = scopeSize / 2.0f * 0.8f; // Leave margin

  for (size_t i = 0; i < impl_->luminanceCache_.size();
       i += 100) { // Sample every 100th pixel for performance
    if (i >= impl_->luminanceCache_.size())
      break;

    // Convert RGB to YUV-like coordinates for vectorscope
    const auto &color = result.outOfGamutPixels.empty()
                            ? FloatColor(0.5f, 0.5f, 0.5f, 1.0f)
                            :                           // Placeholder
                            result.outOfGamutPixels[0]; // This is simplified

    // Simplified UV calculation (should use proper color space conversion)
    float u = (color.b() - color.g()) * 0.5f;
    float v = (color.r() - color.g()) * 0.5f;

    int x = static_cast<int>(centerX + u * scale);
    int y = static_cast<int>(centerY + v * scale);

    x = std::clamp(x, 0, scopeSize - 1);
    y = std::clamp(y, 0, scopeSize - 1);

    vectorscope[y * scopeSize + x] = FloatColor(1, 1, 1, 0.8f);
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
