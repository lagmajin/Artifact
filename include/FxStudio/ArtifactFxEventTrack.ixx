module;

#include <cstdint>
#include <vector>

export module Artifact.FxStudio.EventTrack;

import Artifact.FxStudio.Sequence;

export namespace Artifact::FxStudio {

using EventId = std::uint64_t;

struct Event {
  EventId id = 0;
  std::int64_t startFrame = 0;
  Sequence sequence;
  bool enabled = true;
};

struct EventSample {
  EventId eventId = 0;
  Family family = Family::Glitch;
  std::int64_t relativeFrame = 0;
  CueSample cue;
};

class EventTrack {
public:
  const std::vector<Event>& events() const noexcept;

  bool add(Event event);
  bool update(Event event);
  bool remove(EventId id);
  void clear();

  Event* find(EventId id);
  const Event* find(EventId id) const;

  std::vector<EventSample> sample(std::int64_t frame) const;

private:
  std::vector<Event> events_;
};

}
