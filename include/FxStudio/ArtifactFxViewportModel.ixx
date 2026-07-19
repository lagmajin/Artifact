module;

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module Artifact.FxStudio.ViewportModel;

import Artifact.FxStudio.EventTrack;
import Artifact.FxStudio.Sequence;

export namespace Artifact::FxStudio {

enum class ComparisonMode : std::uint8_t {
  Processed,
  Original,
  Split
};

enum class PreviewQuality : std::uint8_t {
  Draft,
  Interactive,
  Final
};

enum class PreviewBackground : std::uint8_t {
  Checkerboard,
  Dark,
  Light,
  Transparent
};

struct ViewportInputPair {
  std::string inputA;
  std::string inputB;
};

class ViewportModel {
public:
  ComparisonMode comparisonMode() const noexcept;
  void setComparisonMode(ComparisonMode mode) noexcept;

  float splitPosition() const noexcept;
  void setSplitPosition(float position) noexcept;

  PreviewQuality quality() const noexcept;
  void setQuality(PreviewQuality quality) noexcept;

  PreviewBackground background() const noexcept;
  void setBackground(PreviewBackground background) noexcept;

  const ViewportInputPair& inputs() const noexcept;
  void setInputs(ViewportInputPair inputs);

  bool loopEnabled() const noexcept;
  void setLoopEnabled(bool enabled) noexcept;
  bool setLoopRange(std::int64_t firstFrame, std::int64_t lastFrame) noexcept;
  bool useEventLoop(const EventTrack& track, EventId eventId) noexcept;
  std::int64_t resolvePlaybackFrame(std::int64_t requestedFrame) const noexcept;

  std::optional<CueKind> soloCue() const noexcept;
  void setSoloCue(std::optional<CueKind> cue) noexcept;
  bool isCueBypassed(CueKind cue) const noexcept;
  void setCueBypassed(CueKind cue, bool bypassed);

  std::vector<EventSample> visibleSamples(const EventTrack& track,
                                          std::int64_t frame) const;

private:
  ComparisonMode comparisonMode_ = ComparisonMode::Processed;
  PreviewQuality quality_ = PreviewQuality::Interactive;
  PreviewBackground background_ = PreviewBackground::Checkerboard;
  ViewportInputPair inputs_;
  float splitPosition_ = 0.5f;
  bool loopEnabled_ = true;
  std::int64_t loopFirstFrame_ = 0;
  std::int64_t loopLastFrame_ = 0;
  std::optional<CueKind> soloCue_;
  std::vector<CueKind> bypassedCues_;
};

}
