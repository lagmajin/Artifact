module;

#include <cstdint>
#include <optional>
#include <string_view>

export module Artifact.FxStudio.Session;

import Artifact.FxStudio.EventTrack;
import Artifact.FxStudio.PresetCatalog;
import Artifact.FxStudio.Sequence;
import Artifact.FxStudio.ViewportModel;

export namespace Artifact::FxStudio {

class Session {
public:
  EventTrack& eventTrack() noexcept;
  const EventTrack& eventTrack() const noexcept;

  ViewportModel& viewport() noexcept;
  const ViewportModel& viewport() const noexcept;

  std::optional<EventId> selectedEventId() const noexcept;
  Event* selectedEvent() noexcept;
  const Event* selectedEvent() const noexcept;

  std::optional<EventId> insertPreset(std::string_view presetId,
                                      std::int64_t startFrame);
  bool selectEvent(EventId id) noexcept;
  void clearSelection() noexcept;
  bool removeSelectedEvent();

  bool moveSelectedEvent(std::int64_t startFrame);
  bool resizeSelectedEvent(std::int64_t durationFrames);
  bool setSelectedEventStrength(float strength);
  bool setSelectedEventEnabled(bool enabled);

private:
  EventId allocateEventId() noexcept;
  bool commitSelectedEvent(Event event);

  EventTrack eventTrack_;
  ViewportModel viewport_;
  std::optional<EventId> selectedEventId_;
  EventId nextEventId_ = 1;
};

}
