module;

#include <cstdint>
#include <QJsonObject>
#include <QString>

export module Artifact.FxStudio.Session;

import Artifact.FxStudio.EventTrack;
import Artifact.FxStudio.PresetCatalog;
import Artifact.FxStudio.Sequence;
import Artifact.FxStudio.ViewportModel;
import Utils.Optional;
import Core.ArtifactString;
import Serialization.ISerializable;

export namespace Artifact::FxStudio {

class Session : public ArtifactCore::Serialization::ISerializable {
public:
  EventTrack& eventTrack() noexcept;
  const EventTrack& eventTrack() const noexcept;

  ViewportModel& viewport() noexcept;
  const ViewportModel& viewport() const noexcept;

  ArtifactCore::Optional<EventId> selectedEventId() const noexcept;
  Event* selectedEvent() noexcept;
  const Event* selectedEvent() const noexcept;

  ArtifactCore::Optional<EventId> insertPreset(const ArtifactCore::String& presetId,
                                      std::int64_t startFrame);
  bool selectEvent(EventId id) noexcept;
  void clearSelection() noexcept;
  bool removeSelectedEvent();

  bool moveSelectedEvent(std::int64_t startFrame);
  bool resizeSelectedEvent(std::int64_t durationFrames);
  bool setSelectedEventStrength(float strength);
  bool setSelectedEventEnabled(bool enabled);

  QJsonObject toJson() const;
  bool fromJson(const QJsonObject& object);
  QJsonObject serialize() const override { return toJson(); }
  bool deserialize(const QJsonObject& object) override { return fromJson(object); }
  QString typeName() const override { return QStringLiteral("ArtifactFxStudioSession"); }
  int schemaVersion() const override { return 1; }

private:
  EventId allocateEventId() noexcept;
  bool commitSelectedEvent(Event event);

  EventTrack eventTrack_;
  ViewportModel viewport_;
  ArtifactCore::Optional<EventId> selectedEventId_;
  EventId nextEventId_ = 1;
};

}
