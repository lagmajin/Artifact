module;

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>
#include <QJsonArray>

module Artifact.FxStudio.Session;

import Serialization.Registry;
import Serialization.SchemaMigration;

import Core.ArtifactString;
import Artifact.FxStudio.Serialization;

namespace Artifact::FxStudio {

namespace {
const bool registeredFxStudioSession = [] {
    ArtifactCore::Serialization::registerSerializableType<Session>();
    ArtifactCore::Serialization::SchemaMigrationRegistry::instance().registerMigration(
        QStringLiteral("ArtifactFxStudioSession"), 0, 1,
        [](const QJsonObject& legacy) { return legacy; });
    return true;
}();

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

ArtifactCore::Optional<EventId> Session::selectedEventId() const noexcept {
  return selectedEventId_;
}

Event* Session::selectedEvent() noexcept {
  return selectedEventId_.has_value() ? eventTrack_.find(*selectedEventId_) : nullptr;
}

const Event* Session::selectedEvent() const noexcept {
  return selectedEventId_.has_value() ? eventTrack_.find(*selectedEventId_) : nullptr;
}

ArtifactCore::Optional<EventId> Session::insertPreset(const ArtifactCore::String& presetId,
                                             const std::int64_t startFrame) {
  ArtifactCore::Optional<Sequence> sequence = PresetCatalog::create(presetId);
  if (!sequence.has_value()) {
    return {};
  }

  Event event;
  const EventId insertedId = allocateEventId();
  event.id = insertedId;
  event.startFrame = startFrame;
  event.sequence = std::move(*sequence);
  if (!eventTrack_.add(std::move(event))) {
    return {};
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

QJsonObject Session::toJson() const {
  QJsonObject object;
  object[QStringLiteral("schemaVersion")] = 1;
  object[QStringLiteral("eventTrack")] = eventTrackToJson(eventTrack_);
  object[QStringLiteral("selectedEventId")] = selectedEventId_.has_value()
      ? QJsonValue(static_cast<qint64>(*selectedEventId_)) : QJsonValue();
  object[QStringLiteral("comparisonMode")] = static_cast<int>(viewport_.comparisonMode());
  object[QStringLiteral("splitPosition")] = viewport_.splitPosition();
  object[QStringLiteral("quality")] = static_cast<int>(viewport_.quality());
  object[QStringLiteral("background")] = static_cast<int>(viewport_.background());
  object[QStringLiteral("loopEnabled")] = viewport_.loopEnabled();
  object[QStringLiteral("loopFirstFrame")] = viewport_.loopFirstFrame();
  object[QStringLiteral("loopLastFrame")] = viewport_.loopLastFrame();
  object[QStringLiteral("soloCue")] = viewport_.soloCue().has_value()
      ? QJsonValue(static_cast<int>(*viewport_.soloCue())) : QJsonValue();
  QJsonArray bypassed;
  for (const CueKind cue : viewport_.bypassedCues()) bypassed.append(static_cast<int>(cue));
  object[QStringLiteral("bypassedCues")] = bypassed;
  object[QStringLiteral("inputA")] = QString::fromStdString(
      ArtifactCore::toStdString(viewport_.inputs().inputA));
  object[QStringLiteral("inputB")] = QString::fromStdString(
      ArtifactCore::toStdString(viewport_.inputs().inputB));
  return object;
}

bool Session::fromJson(const QJsonObject& object) {
  if (object.value(QStringLiteral("schemaVersion")).toInt(-1) != 1) return false;
  const EventTrackDecodeResult decoded = eventTrackFromJson(
      object.value(QStringLiteral("eventTrack")).toObject());
  if (!decoded.schemaSupported) return false;
  eventTrack_ = decoded.track;
  selectedEventId_.reset();
  if (object.value(QStringLiteral("selectedEventId")).isDouble()) {
    const EventId id = static_cast<EventId>(object.value(QStringLiteral("selectedEventId")).toInteger());
    if (eventTrack_.find(id)) selectedEventId_ = id;
  }
  viewport_.setComparisonMode(static_cast<ComparisonMode>(std::clamp(
      object.value(QStringLiteral("comparisonMode")).toInt(0), 0, 2)));
  viewport_.setSplitPosition(static_cast<float>(object.value(QStringLiteral("splitPosition")).toDouble(0.5)));
  viewport_.setQuality(static_cast<PreviewQuality>(std::clamp(
      object.value(QStringLiteral("quality")).toInt(1), 0, 2)));
  viewport_.setBackground(static_cast<PreviewBackground>(std::clamp(
      object.value(QStringLiteral("background")).toInt(0), 0, 3)));
  viewport_.setLoopEnabled(object.value(QStringLiteral("loopEnabled")).toBool(true));
  viewport_.setLoopRange(object.value(QStringLiteral("loopFirstFrame")).toInteger(0),
                          object.value(QStringLiteral("loopLastFrame")).toInteger(0));
  if (!object.value(QStringLiteral("soloCue")).isNull()) {
    const int cue = object.value(QStringLiteral("soloCue")).toInt(-1);
    if (cue >= static_cast<int>(CueKind::Flash) && cue <= static_cast<int>(CueKind::Recovery))
      viewport_.setSoloCue(static_cast<CueKind>(cue));
  } else {
    viewport_.setSoloCue({});
  }
  viewport_.clearBypassedCues();
  for (const QJsonValue& value : object.value(QStringLiteral("bypassedCues")).toArray()) {
    const int cue = value.toInt(-1);
    if (cue >= static_cast<int>(CueKind::Flash) && cue <= static_cast<int>(CueKind::Recovery))
      viewport_.setCueBypassed(static_cast<CueKind>(cue), true);
  }
  viewport_.setInputs({ArtifactCore::String(object.value(QStringLiteral("inputA")).toString().toStdString()),
                       ArtifactCore::String(object.value(QStringLiteral("inputB")).toString().toStdString())});
  if (selectedEventId_.has_value()) viewport_.useEventLoop(eventTrack_, *selectedEventId_);
  return true;
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
