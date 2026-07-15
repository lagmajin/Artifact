module;

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

module Artifact.FxStudio.Session;

namespace Artifact::FxStudio {

EventTrack& Session::eventTrack() noexcept {
  return eventTrack_;
}

const EventTrack& Session::eventTrack() const noexcept {
  return eventTrack_;
}

ViewportModel& Session::viewport() noexcept {
  return viewport_;
}

const ViewportModel& Session::viewport() const noexcept {
  return viewport_;
}

std::optional<EventId> Session::selectedEventId() const noexcept {
  return selectedEventId_;
}

Event* Session::selectedEvent() noexcept {
  return selectedEventId_.has_value() ? eventTrack_.find(*selectedEventId_) : nullptr;
}

const Event* Session::selectedEvent() const noexcept {
  return selectedEventId_.has_value() ? eventTrack_.find(*selectedEventId_) : nullptr;
}

std::optional<EventId> Session::insertPreset(const std::string_view presetId,
                                             const std::int64_t startFrame) {
  std::optional<Sequence> sequence = PresetCatalog::create(presetId);
  if (!sequence.has_value()) {
    return std::nullopt;
  }

  Event event;
  const EventId insertedId = allocateEventId();
  event.id = insertedId;
  event.startFrame = startFrame;
  event.sequence = std::move(*sequence);
  if (!eventTrack_.add(std::move(event))) {
    return std::nullopt;
  }

  selectEvent(insertedId);
  return insertedId;
}

bool Session::selectEvent(const EventId id) noexcept {
  if (eventTrack_.find(id) == nullptr) {
    return false;
  }
  selectedEventId_ = id;
  viewport_.useEventLoop(eventTrack_, id);
  return true;
}

void Session::clearSelection() noexcept {
  selectedEventId_.reset();
}

bool Session::removeSelectedEvent() {
  if (!selectedEventId_.has_value()) {
    return false;
  }
  const bool removed = eventTrack_.remove(*selectedEventId_);
  if (removed) {
    selectedEventId_.reset();
  }
  return removed;
}

bool Session::moveSelectedEvent(const std::int64_t startFrame) {
  const Event* current = selectedEvent();
  if (current == nullptr) {
    return false;
  }
  Event updated = *current;
  updated.startFrame = startFrame;
  return commitSelectedEvent(std::move(updated));
}

bool Session::resizeSelectedEvent(const std::int64_t durationFrames) {
  const Event* current = selectedEvent();
  if (current == nullptr || durationFrames <= 0) {
    return false;
  }
  Event updated = *current;
  updated.sequence.durationFrames = durationFrames;
  return commitSelectedEvent(std::move(updated));
}

bool Session::setSelectedEventStrength(const float strength) {
  const Event* current = selectedEvent();
  if (current == nullptr) {
    return false;
  }
  Event updated = *current;
  updated.sequence.strength = std::clamp(strength, 0.0f, 1.0f);
  return commitSelectedEvent(std::move(updated));
}

bool Session::setSelectedEventEnabled(const bool enabled) {
  const Event* current = selectedEvent();
  if (current == nullptr) {
    return false;
  }
  Event updated = *current;
  updated.enabled = enabled;
  return commitSelectedEvent(std::move(updated));
}

EventId Session::allocateEventId() noexcept {
  while (nextEventId_ == 0 || eventTrack_.find(nextEventId_) != nullptr) {
    ++nextEventId_;
  }
  return nextEventId_++;
}

bool Session::commitSelectedEvent(Event event) {
  const EventId id = event.id;
  if (!eventTrack_.update(std::move(event))) {
    return false;
  }
  viewport_.useEventLoop(eventTrack_, id);
  return true;
}

}
