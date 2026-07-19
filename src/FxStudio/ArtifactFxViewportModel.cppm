module;

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

module Artifact.FxStudio.ViewportModel;

import Artifact.FxStudio.EventTrack;
import Artifact.FxStudio.Sequence;

namespace Artifact::FxStudio {

ComparisonMode ViewportModel::comparisonMode() const noexcept {
  return comparisonMode_;
}

void ViewportModel::setComparisonMode(const ComparisonMode mode) noexcept {
  comparisonMode_ = mode;
}

float ViewportModel::splitPosition() const noexcept {
  return splitPosition_;
}

void ViewportModel::setSplitPosition(const float position) noexcept {
  splitPosition_ = std::clamp(position, 0.0f, 1.0f);
}

PreviewQuality ViewportModel::quality() const noexcept {
  return quality_;
}

void ViewportModel::setQuality(const PreviewQuality quality) noexcept {
  quality_ = quality;
}

PreviewBackground ViewportModel::background() const noexcept {
  return background_;
}

void ViewportModel::setBackground(const PreviewBackground background) noexcept {
  background_ = background;
}

const ViewportInputPair& ViewportModel::inputs() const noexcept {
  return inputs_;
}

void ViewportModel::setInputs(ViewportInputPair inputs) {
  inputs_ = std::move(inputs);
}

bool ViewportModel::loopEnabled() const noexcept {
  return loopEnabled_;
}

void ViewportModel::setLoopEnabled(const bool enabled) noexcept {
  loopEnabled_ = enabled;
}

bool ViewportModel::setLoopRange(const std::int64_t firstFrame,
                                 const std::int64_t lastFrame) noexcept {
  if (lastFrame < firstFrame) {
    return false;
  }
  loopFirstFrame_ = firstFrame;
  loopLastFrame_ = lastFrame;
  return true;
}

bool ViewportModel::useEventLoop(const EventTrack& track, const EventId eventId) noexcept {
  const Event* event = track.find(eventId);
  if (event == nullptr || event->sequence.durationFrames <= 0) {
    return false;
  }
  return setLoopRange(event->startFrame,
                      event->startFrame + event->sequence.durationFrames);
}

std::int64_t ViewportModel::resolvePlaybackFrame(const std::int64_t requestedFrame) const noexcept {
  if (!loopEnabled_ || loopLastFrame_ <= loopFirstFrame_) {
    return requestedFrame;
  }
  const std::int64_t loopLength = loopLastFrame_ - loopFirstFrame_ + 1;
  const std::int64_t offset = requestedFrame - loopFirstFrame_;
  const std::int64_t wrapped = ((offset % loopLength) + loopLength) % loopLength;
  return loopFirstFrame_ + wrapped;
}

std::optional<CueKind> ViewportModel::soloCue() const noexcept {
  return soloCue_;
}

void ViewportModel::setSoloCue(const std::optional<CueKind> cue) noexcept {
  soloCue_ = cue;
}

bool ViewportModel::isCueBypassed(const CueKind cue) const noexcept {
  return std::find(bypassedCues_.begin(), bypassedCues_.end(), cue) != bypassedCues_.end();
}

void ViewportModel::setCueBypassed(const CueKind cue, const bool bypassed) {
  const auto it = std::find(bypassedCues_.begin(), bypassedCues_.end(), cue);
  if (bypassed && it == bypassedCues_.end()) {
    bypassedCues_.push_back(cue);
  } else if (!bypassed && it != bypassedCues_.end()) {
    bypassedCues_.erase(it);
  }
}

std::vector<EventSample> ViewportModel::visibleSamples(const EventTrack& track,
                                                       const std::int64_t frame) const {
  std::vector<EventSample> samples = track.sample(resolvePlaybackFrame(frame));
  std::erase_if(samples, [this](const EventSample& sample) {
    return isCueBypassed(sample.cue.kind) ||
           (soloCue_.has_value() && sample.cue.kind != *soloCue_);
  });
  return samples;
}

}
