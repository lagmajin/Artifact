module;

#include <QJsonObject>

export module Artifact.FxStudio.Serialization;

import Artifact.FxStudio.EventTrack;

export namespace Artifact::FxStudio {

struct EventTrackDecodeResult {
  EventTrack track;
  int skippedEvents = 0;
  bool schemaSupported = true;
};

QJsonObject eventTrackToJson(const EventTrack& track);
EventTrackDecodeResult eventTrackFromJson(const QJsonObject& object);

}
