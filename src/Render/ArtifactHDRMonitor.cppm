module;
#include <QObject>
#include <wobjectimpl.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

module Render.HDRMonitor;

import Color.Float;
import Color.GamutConversion;
import Color.Luminance;
import Color.TransferFunction;
import Core.Parallel;
import Event.Bus;
import Core.ArtifactAlgorithms;
import Core.ArtifactMath;

namespace Artifact {

class ArtifactHDRMonitor::Impl {
public:
  HDRMonitorSettings settings_;
  mutable ArtifactArray<float> luminanceCache_;
  mutable ArtifactArray<FloatColor> colorCache_;
  mutable ArtifactArray<unsigned char> clippedCache_;
  mutable ArtifactArray<unsigned char> shadowClippedCache_;
  mutable ArtifactArray<unsigned char> highlightClippedCache_;
  mutable ArtifactArray<unsigned char> outOfGamutCache_;
  mutable ArtifactArray<unsigned char> luminanceLegalViolationCache_;
  mutable ArtifactArray<unsigned char> channelLegalViolationCache_;
  mutable ArtifactArray<unsigned char> nonFiniteCache_;
};

namespace {

ScopeAnalysisDescriptor sanitizeScopeDescriptor(
    const ScopeAnalysisDescriptor &input) {
  ScopeAnalysisDescriptor descriptor = input;
  descriptor.referenceWhiteNits =
      std::isfinite(descriptor.referenceWhiteNits)
          ? std::clamp(descriptor.referenceWhiteNits, 1.0f, 10000.0f)
          : 100.0f;
  descriptor.peakLuminanceNits =
      std::isfinite(descriptor.peakLuminanceNits)
          ? std::clamp(descriptor.peakLuminanceNits,
                       descriptor.referenceWhiteNits, 10000.0f)
          : std::max(1000.0f, descriptor.referenceWhiteNits);
  descriptor.lowClipThreshold =
      std::isfinite(descriptor.lowClipThreshold)
          ? descriptor.lowClipThreshold
          : 0.0f;
  descriptor.highClipThreshold =
      std::isfinite(descriptor.highClipThreshold)
          ? std::max(descriptor.highClipThreshold,
                     descriptor.lowClipThreshold)
          : 1.0f;
  descriptor.gamutTolerance =
      std::isfinite(descriptor.gamutTolerance)
          ? std::clamp(descriptor.gamutTolerance, 0.0f, 0.01f)
          : 1.0e-5f;
  descriptor.bitDepth = std::clamp(descriptor.bitDepth, 8, 16);
  descriptor.sampleStep = std::clamp(descriptor.sampleStep, 1, 64);
  descriptor.maxOutOfGamutSamples =
      std::max(0, descriptor.maxOutOfGamutSamples);
  return descriptor;
}

float decodeScopeSignal(float value,
                        const ScopeAnalysisDescriptor &descriptor) {
  if (descriptor.domain == ScopeSignalDomain::SceneLinear ||
      descriptor.transferFunction == TransferFunction::Linear) {
    return value;
  }
  return ColorTransferFunction::decode(value, descriptor.transferFunction);
}

float encodeScopeSignal(float value,
                        const ScopeAnalysisDescriptor &descriptor) {
  if (descriptor.domain == ScopeSignalDomain::DisplayEncoded ||
      descriptor.transferFunction == TransferFunction::Linear) {
    return value;
  }
  return ColorTransferFunction::encode(value, descriptor.transferFunction);
}

float scopeNitsScale(const ScopeAnalysisDescriptor &descriptor) {
  if (descriptor.domain == ScopeSignalDomain::DisplayEncoded &&
      descriptor.transferFunction == TransferFunction::Rec2084_PQ) {
    return 10000.0f;
  }
  if (descriptor.domain == ScopeSignalDomain::DisplayEncoded &&
      descriptor.transferFunction == TransferFunction::HLG) {
    return descriptor.peakLuminanceNits;
  }
  return descriptor.referenceWhiteNits;
}

void scopeLegalLimits(const ScopeAnalysisDescriptor &descriptor,
                      float &black, float &white) {
  if (descriptor.signalRange == ScopeSignalRange::Full) {
    black = 0.0f;
    white = 1.0f;
    return;
  }
  const unsigned int shift = static_cast<unsigned int>(descriptor.bitDepth - 8);
  const unsigned int codeMax = (1u << descriptor.bitDepth) - 1u;
  black = static_cast<float>(16u << shift) / static_cast<float>(codeMax);
  white = static_cast<float>(235u << shift) / static_cast<float>(codeMax);
}

} // namespace

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
  ScopeAnalysisDescriptor descriptor;
  descriptor.domain = ScopeSignalDomain::SceneLinear;
  descriptor.signalRange = ScopeSignalRange::VideoLegal;
  descriptor.transferFunction = TransferFunction::Linear;
  descriptor.maxOutOfGamutSamples = 4096;
  return analyzeFrame(frameData, width, height, descriptor);
}

HDRAnalysisResult ArtifactHDRMonitor::analyzeFrame(
    const ArtifactArray<FloatColor> &frameData, int width, int height,
    const ScopeAnalysisDescriptor &requestedDescriptor) {
  HDRAnalysisResult result;
  const ScopeAnalysisDescriptor descriptor =
      sanitizeScopeDescriptor(requestedDescriptor);
  result.descriptor = descriptor;
  result.sourcePixels = static_cast<int>(std::min<size_t>(
      frameData.size(), static_cast<size_t>(std::numeric_limits<int>::max())));
  result.sampleStep = descriptor.sampleStep;

  if (frameData.isEmpty() || width <= 0 || height <= 0) {
    impl_->luminanceCache_.removeAll();
    impl_->colorCache_.removeAll();
    return result;
  }

  const std::int64_t sampleColumns64 =
      (static_cast<std::int64_t>(width) + descriptor.sampleStep - 1) /
      descriptor.sampleStep;
  const std::int64_t sampleRows64 =
      (static_cast<std::int64_t>(height) + descriptor.sampleStep - 1) /
      descriptor.sampleStep;
  if (sampleColumns64 > std::numeric_limits<int>::max() ||
      sampleRows64 > std::numeric_limits<int>::max()) {
    result.nonFiniteSamples = result.sourcePixels;
    return result;
  }
  const int sampleColumns = static_cast<int>(sampleColumns64);
  const int sampleRows = static_cast<int>(sampleRows64);
  if (static_cast<size_t>(sampleColumns) >
      std::numeric_limits<size_t>::max() / static_cast<size_t>(sampleRows)) {
    result.nonFiniteSamples = result.sourcePixels;
    return result;
  }
  const size_t sampleCountSize =
      static_cast<size_t>(sampleColumns) * static_cast<size_t>(sampleRows);
  if (sampleCountSize > static_cast<size_t>(std::numeric_limits<int>::max())) {
    result.nonFiniteSamples = result.sourcePixels;
    return result;
  }
  const int sampleCount = static_cast<int>(sampleCountSize);
  result.analyzedSamples = sampleCount;

  impl_->luminanceCache_.resize(sampleCount);
  impl_->colorCache_.resize(sampleCount);
  impl_->clippedCache_.resize(sampleCount);
  impl_->shadowClippedCache_.resize(sampleCount);
  impl_->highlightClippedCache_.resize(sampleCount);
  impl_->outOfGamutCache_.resize(sampleCount);
  impl_->luminanceLegalViolationCache_.resize(sampleCount);
  impl_->channelLegalViolationCache_.resize(sampleCount);
  impl_->nonFiniteCache_.resize(sampleCount);

  float legalBlack = 0.0f;
  float legalWhite = 1.0f;
  scopeLegalLimits(descriptor, legalBlack, legalWhite);
  const ArtifactCore::Matrix3x3 sourceToTarget =
      ArtifactCore::ColorGamutConversion::getConversionMatrix(
          descriptor.primaries, descriptor.targetGamut);

  ArtifactCore::Parallel::For(0, sampleCount, sampleCount, [&](int sampleIndex) {
    const int sampleX = (sampleIndex % sampleColumns) * descriptor.sampleStep;
    const int sampleY = (sampleIndex / sampleColumns) * descriptor.sampleStep;
    const size_t sourceIndex =
        static_cast<size_t>(sampleY) * static_cast<size_t>(width) +
        static_cast<size_t>(sampleX);

    FloatColor color(0.0f, 0.0f, 0.0f, 0.0f);
    bool finite = sourceIndex < frameData.size();
    if (finite) {
      color = frameData[sourceIndex];
      finite = std::isfinite(color.r()) && std::isfinite(color.g()) &&
               std::isfinite(color.b()) && std::isfinite(color.a());
    }

    float linearR = 0.0f;
    float linearG = 0.0f;
    float linearB = 0.0f;
    if (finite) {
      linearR = decodeScopeSignal(color.r(), descriptor);
      linearG = decodeScopeSignal(color.g(), descriptor);
      linearB = decodeScopeSignal(color.b(), descriptor);
      finite = std::isfinite(linearR) && std::isfinite(linearG) &&
               std::isfinite(linearB);
    }

    float targetLinearR = 0.0f;
    float targetLinearG = 0.0f;
    float targetLinearB = 0.0f;
    if (finite) {
      ArtifactCore::multiply(sourceToTarget, linearR, linearG, linearB,
                             targetLinearR, targetLinearG, targetLinearB);
      finite = std::isfinite(targetLinearR) &&
               std::isfinite(targetLinearG) &&
               std::isfinite(targetLinearB);
    }

    const float luminance = finite
        ? ArtifactCore::ColorLuminance::calculate(
              targetLinearR, targetLinearG, targetLinearB,
              descriptor.luminanceStandard)
        : 0.0f;
    const float encodedR =
        finite ? encodeScopeSignal(targetLinearR, descriptor) : 0.0f;
    const float encodedG =
        finite ? encodeScopeSignal(targetLinearG, descriptor) : 0.0f;
    const float encodedB =
        finite ? encodeScopeSignal(targetLinearB, descriptor) : 0.0f;
    const auto legal = ArtifactCore::ColorLuminance::inspectBroadcastSafe(
        encodedR, encodedG, encodedB, descriptor.luminanceStandard,
        legalBlack, legalWhite, legalBlack, legalWhite);

    impl_->colorCache_[sampleIndex] = color;
    impl_->luminanceCache_[sampleIndex] = luminance;
    impl_->nonFiniteCache_[sampleIndex] = finite ? 0 : 1;
    impl_->clippedCache_[sampleIndex] =
        finite && (color.r() <= descriptor.lowClipThreshold ||
                   color.g() <= descriptor.lowClipThreshold ||
                   color.b() <= descriptor.lowClipThreshold ||
                   color.r() >= descriptor.highClipThreshold ||
                   color.g() >= descriptor.highClipThreshold ||
                   color.b() >= descriptor.highClipThreshold)
            ? 1
            : 0;
    impl_->shadowClippedCache_[sampleIndex] =
        finite && (color.r() <= descriptor.lowClipThreshold ||
                   color.g() <= descriptor.lowClipThreshold ||
                   color.b() <= descriptor.lowClipThreshold)
            ? 1
            : 0;
    impl_->highlightClippedCache_[sampleIndex] =
        finite && (color.r() >= descriptor.highClipThreshold ||
                   color.g() >= descriptor.highClipThreshold ||
                   color.b() >= descriptor.highClipThreshold)
            ? 1
            : 0;
    const float gamutMin = -descriptor.gamutTolerance;
    const float gamutMax = 1.0f + descriptor.gamutTolerance;
    impl_->outOfGamutCache_[sampleIndex] =
        finite && (targetLinearR < gamutMin || targetLinearR > gamutMax ||
                   targetLinearG < gamutMin || targetLinearG > gamutMax ||
                   targetLinearB < gamutMin || targetLinearB > gamutMax)
            ? 1
            : 0;
    impl_->luminanceLegalViolationCache_[sampleIndex] =
        legal.luminanceViolation ? 1 : 0;
    impl_->channelLegalViolationCache_[sampleIndex] =
        legal.gamutViolation ? 1 : 0;
  });

  float minLuminance = std::numeric_limits<float>::max();
  float maxLuminance = std::numeric_limits<float>::lowest();
  float minRed = std::numeric_limits<float>::max();
  float maxRed = std::numeric_limits<float>::lowest();
  float minGreen = std::numeric_limits<float>::max();
  float maxGreen = std::numeric_limits<float>::lowest();
  float minBlue = std::numeric_limits<float>::max();
  float maxBlue = std::numeric_limits<float>::lowest();
  double luminanceSum = 0.0;
  int validSamples = 0;
  result.outOfGamutPixels.reserve(
      static_cast<size_t>(descriptor.maxOutOfGamutSamples));

  for (int i = 0; i < sampleCount; ++i) {
    if (impl_->nonFiniteCache_[i]) {
      ++result.nonFiniteSamples;
      continue;
    }

    const float luminance = impl_->luminanceCache_[i];
    const auto &color = impl_->colorCache_[i];
    minLuminance = std::min(minLuminance, luminance);
    maxLuminance = std::max(maxLuminance, luminance);
    minRed = std::min(minRed, color.r());
    maxRed = std::max(maxRed, color.r());
    minGreen = std::min(minGreen, color.g());
    maxGreen = std::max(maxGreen, color.g());
    minBlue = std::min(minBlue, color.b());
    maxBlue = std::max(maxBlue, color.b());
    luminanceSum += luminance;
    ++validSamples;

    if (impl_->clippedCache_[i]) {
      result.hasClipping = true;
    }
    if (impl_->shadowClippedCache_[i]) {
      ++result.clippedShadows;
    }
    if (impl_->highlightClippedCache_[i]) {
      ++result.clippedHighlights;
    }
    if (impl_->outOfGamutCache_[i]) {
      ++result.outOfGamutSamples;
      if (static_cast<int>(result.outOfGamutPixels.size()) <
          descriptor.maxOutOfGamutSamples) {
        result.outOfGamutPixels.append(color);
      } else {
        result.outOfGamutSamplesTruncated = true;
      }
    }
    if (impl_->luminanceLegalViolationCache_[i]) {
      ++result.luminanceLegalRangeViolations;
    }
    if (impl_->channelLegalViolationCache_[i]) {
      ++result.channelLegalRangeViolations;
    }
    if (impl_->luminanceLegalViolationCache_[i] ||
        impl_->channelLegalViolationCache_[i]) {
      ++result.broadcastSafeViolations;
    }
  }

  if (validSamples > 0) {
    result.validSamples = validSamples;
    result.minLuminance = minLuminance;
    result.maxLuminance = maxLuminance;
    result.avgLuminance =
        static_cast<float>(luminanceSum / static_cast<double>(validSamples));
    result.minRed = minRed;
    result.maxRed = maxRed;
    result.minGreen = minGreen;
    result.maxGreen = maxGreen;
    result.minBlue = minBlue;
    result.maxBlue = maxBlue;
    const float nitsScale = scopeNitsScale(descriptor);
    result.minLuminanceNits = result.minLuminance * nitsScale;
    result.maxLuminanceNits = result.maxLuminance * nitsScale;
    result.avgLuminanceNits = result.avgLuminance * nitsScale;
  }

  if (descriptor.publishEvent) {
    ArtifactCore::globalEventBus().publish<HDRAnalysisCompletedEvent>({result});
  }
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
