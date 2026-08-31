module;

#include <algorithm>
#include <cmath>

module Artifact.Audio.Limiter;

import Audio.Segment;

namespace Artifact {

AudioLimiter::AudioLimiter() : lookAheadMs_(3.0f) {}

void AudioLimiter::process(ArtifactCore::AudioSegment& segment, int sampleRate) {
  if (sampleRate != sampleRate_) {
    const int prevSR = sampleRate_;
    sampleRate_ = sampleRate;
    attackCoeff_ = 1.0f - std::exp(-1.0f / (attackMs_ * 0.001f * sampleRate_));
    releaseCoeff_ = 1.0f - std::exp(-1.0f / (releaseMs_ * 0.001f * sampleRate_));
    if (lookAheadMs_ > 0.0f) {
      delaySize_ = std::max(1, static_cast<int>(lookAheadMs_ * 0.001f * sampleRate_));
    } else {
      delaySize_ = 0;
    }
    if (prevSR != sampleRate_) {
      delayBuf_.clear();
      delayPos_ = 0;
    }
  }

  const int channels = segment.channelCount();
  const int frames = segment.frameCount();
  if (delaySize_ > 0 && static_cast<int>(delayBuf_.size()) < channels) {
    delayBuf_.resize(channels);
    for (auto& buffer : delayBuf_) {
      buffer.assign(delaySize_, 0.0f);
    }
    delayPos_ = 0;
  }

  for (int frame = 0; frame < frames; ++frame) {
    float peak = 0.0f;
    for (int channel = 0; channel < channels; ++channel) {
      peak = std::max(peak, std::abs(segment.channelData[channel][frame]));
    }

    const float desiredGain = (peak > threshold_) ? threshold_ / peak : 1.0f;
    if (desiredGain < envelope_) {
      envelope_ += (desiredGain - envelope_) * attackCoeff_;
    } else {
      envelope_ += (desiredGain - envelope_) * releaseCoeff_;
    }

    if (delaySize_ > 0 && !delayBuf_.empty()) {
      for (int channel = 0;
           channel < channels && channel < static_cast<int>(delayBuf_.size());
           ++channel) {
        const float current = segment.channelData[channel][frame];
        const float delayed = delayBuf_[channel][delayPos_];
        segment.channelData[channel][frame] = delayed * envelope_;
        delayBuf_[channel][delayPos_] = current;
      }
      delayPos_ = (delayPos_ + 1) % delaySize_;
    } else {
      for (int channel = 0; channel < channels; ++channel) {
        segment.channelData[channel][frame] *= envelope_;
      }
    }
  }
}

void AudioLimiter::reset() {
  envelope_ = 1.0f;
  for (auto& buffer : delayBuf_) {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
  }
  delayPos_ = 0;
}

void softClipAudioSegment(ArtifactCore::AudioSegment& segment) {
  constexpr float knee = 0.85f;
  constexpr float ceiling = 0.98f;
  constexpr float inverseRange = 1.0f / (1.0f - knee);
  for (auto& channel : segment.channelData) {
    for (float& sample : channel) {
      const float absoluteValue = std::abs(sample);
      if (absoluteValue > knee) {
        const float t = (absoluteValue - knee) * inverseRange;
        const float t2 = t * t;
        const float fastTanh = t * (27.0f + t2) / (27.0f + 9.0f * t2);
        sample = (sample >= 0.0f ? 1.0f : -1.0f) *
                 (knee + (ceiling - knee) * fastTanh);
      }
    }
  }
}

}
