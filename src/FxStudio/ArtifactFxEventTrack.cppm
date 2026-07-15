module;

#include <algorithm>
#include <utility>
#include <vector>

module Artifact.FxStudio.EventTrack;

namespace Artifact::FxStudio {

namespace {

bool startsBefore(const Event& lhs, const Event& rhs) {
  if (lhs.startFrame != rhs.startFrame) {
    return lhs.startFrame < rhs.startFrame;
  }
  return lhs.id < rhs.id;
}

}

const std::vector<Event>& EventTrack::events() const noexcept {
  return events_;
}

bool EventTrack::add(Event event) {
  if (event.id == 0 || event.sequence.durationFrames <= 0 || find(event.id) != nullptr) {
    return false;
  }
  events_.push_back(std::move(event));
  std::stable_sort(events_.begin(), events_.end(), startsBefore);
  return true;
}

bool EventTrack::update(Event event) {
  if (event.id == 0 || event.sequence.durationFrames <= 0) {
    return false;
  }
  Event* current = find(event.id);
  if (current == nullptr) {
    return false;
  }
  *current = std::move(event);
  std::stable_sort(events_.begin(), events_.end(), startsBefore);
  return true;
}

bool EventTrack::remove(const EventId id) {
  const auto it = std::find_if(events_.begin(), events_.end(),
                               [id](const Event& event) { return event.id == id; });
  if (it == events_.end()) {
    return false;
  }
  events_.erase(it);
  return true;
}

void EventTrack::clear() {
  events_.clear();
}

Event* EventTrack::find(const EventId id) {
  const auto it = std::find_if(events_.begin(), events_.end(),
                               [id](const Event& event) { return event.id == id; });
  return it != events_.end() ? &*it : nullptr;
}

const Event* EventTrack::find(const EventId id) const {
  const auto it = std::find_if(events_.begin(), events_.end(),
                               [id](const Event& event) { return event.id == id; });
  return it != events_.end() ? &*it : nullptr;
}

std::vector<EventSample> EventTrack::sample(const std::int64_t frame) const {
  std::vector<EventSample> result;
  for (const Event& event : events_) {
    if (!event.enabled || frame < event.startFrame) {
      continue;
    }

    const std::int64_t relativeFrame = frame - event.startFrame;
    if (relativeFrame > event.sequence.durationFrames) {
      continue;
    }

    const std::vector<CueSample> cueSamples = event.sequence.sample(relativeFrame);
    result.reserve(result.size() + cueSamples.size());
    for (const CueSample& cue : cueSamples) {
      result.push_back({event.id, event.sequence.family, relativeFrame, cue});
    }
  }
  return result;
}

}
