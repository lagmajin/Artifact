module;

#include <cstdint>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

module Artifact.FxStudio.Serialization;

namespace Artifact::FxStudio {

namespace {

constexpr int kSchemaVersion = 1;

QJsonObject cueToJson(const Cue& cue) {
  QJsonObject object;
  object["kind"] = static_cast<int>(cue.kind);
  object["curve"] = static_cast<int>(cue.curve);
  object["start"] = static_cast<double>(cue.start);
  object["duration"] = static_cast<double>(cue.duration);
  object["strength"] = static_cast<double>(cue.strength);
  object["variation"] = static_cast<double>(cue.variation);
  object["seed"] = static_cast<qint64>(cue.seed);
  object["enabled"] = cue.enabled;
  return object;
}

bool cueFromJson(const QJsonObject& object, Cue& cue) {
  const int kind = object.value("kind").toInt(-1);
  const int curve = object.value("curve").toInt(-1);
  const float duration = static_cast<float>(object.value("duration").toDouble(0.0));
  if (kind < static_cast<int>(CueKind::Distort) ||
      kind > static_cast<int>(CueKind::Recovery) ||
      curve < static_cast<int>(CueCurve::Step) ||
      curve > static_cast<int>(CueCurve::Pulse) || duration <= 0.0f) {
    return false;
  }

  cue.kind = static_cast<CueKind>(kind);
  cue.curve = static_cast<CueCurve>(curve);
  cue.start = static_cast<float>(object.value("start").toDouble(0.0));
  cue.duration = duration;
  cue.strength = static_cast<float>(object.value("strength").toDouble(1.0));
  cue.variation = static_cast<float>(object.value("variation").toDouble(0.0));
  cue.seed = static_cast<std::uint32_t>(object.value("seed").toInteger(0));
  cue.enabled = object.value("enabled").toBool(true);
  return true;
}

QJsonObject sequenceToJson(const Sequence& sequence) {
  QJsonObject object;
  object["family"] = static_cast<int>(sequence.family);
  object["durationFrames"] = sequence.durationFrames;
  object["strength"] = static_cast<double>(sequence.strength);
  object["seed"] = static_cast<qint64>(sequence.seed);

  QJsonArray cues;
  for (const Cue& cue : sequence.cues) {
    cues.append(cueToJson(cue));
  }
  object["cues"] = cues;
  return object;
}

bool sequenceFromJson(const QJsonObject& object, Sequence& sequence) {
  const int family = object.value("family").toInt(-1);
  const std::int64_t durationFrames = object.value("durationFrames").toInteger(0);
  if (family < static_cast<int>(Family::Glitch) ||
      family > static_cast<int>(Family::Transition) || durationFrames <= 0) {
    return false;
  }

  sequence.family = static_cast<Family>(family);
  sequence.durationFrames = durationFrames;
  sequence.strength = static_cast<float>(object.value("strength").toDouble(1.0));
  sequence.seed = static_cast<std::uint32_t>(object.value("seed").toInteger(0));
  sequence.cues.clear();

  const QJsonArray cues = object.value("cues").toArray();
  for (const QJsonValue& value : cues) {
    if (!value.isObject()) {
      continue;
    }
    Cue cue;
    if (cueFromJson(value.toObject(), cue)) {
      sequence.cues.push_back(cue);
    }
  }
  return !sequence.cues.empty();
}

}

QJsonObject eventTrackToJson(const EventTrack& track) {
  QJsonObject root;
  root["schemaVersion"] = kSchemaVersion;

  QJsonArray events;
  for (const Event& event : track.events()) {
    QJsonObject object;
    object["id"] = static_cast<qint64>(event.id);
    object["startFrame"] = event.startFrame;
    object["enabled"] = event.enabled;
    object["sequence"] = sequenceToJson(event.sequence);
    events.append(object);
  }
  root["events"] = events;
  return root;
}

EventTrackDecodeResult eventTrackFromJson(const QJsonObject& object) {
  EventTrackDecodeResult result;
  if (object.value("schemaVersion").toInt(-1) != kSchemaVersion) {
    result.schemaSupported = false;
    return result;
  }

  const QJsonArray events = object.value("events").toArray();
  for (const QJsonValue& value : events) {
    if (!value.isObject()) {
      ++result.skippedEvents;
      continue;
    }

    const QJsonObject object = value.toObject();
    Event event;
    event.id = static_cast<EventId>(object.value("id").toInteger(0));
    event.startFrame = object.value("startFrame").toInteger(0);
    event.enabled = object.value("enabled").toBool(true);
    if (!sequenceFromJson(object.value("sequence").toObject(), event.sequence) ||
        !result.track.add(event)) {
      ++result.skippedEvents;
    }
  }
  return result;
}

}
