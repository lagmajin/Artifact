module;
#include <functional>

#include <QObject>
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <wobjectdefs.h>

export module Artifact.Service.Playback;

import Frame.Position;
import Frame.Rate;
import Frame.Range;
import Frame.Debug;
import Playback.State;
import Artifact.Composition.PlaybackController;
import Artifact.Composition.Abstract;
import Artifact.Playback.Engine;
import Artifact.Composition.InOutPoints;
import Artifact.Event.Types;

W_REGISTER_ARGTYPE(Artifact::ArtifactCompositionPtr)
W_REGISTER_ARGTYPE(Artifact::PlaybackRangeMode)
W_REGISTER_ARGTYPE(Artifact::PlaybackSkipMode)

export namespace Artifact {

using namespace ArtifactCore;

class ArtifactPlaybackService : public QObject {
  W_OBJECT(ArtifactPlaybackService)
private:
  class Impl;
  Impl *impl_;

public:
  explicit ArtifactPlaybackService(QObject *parent = nullptr);
  ~ArtifactPlaybackService();

  static ArtifactPlaybackService *instance();

  // Playback control for current composition
  void play();
  void pause();
  void stop();
  void togglePlayPause();

  // Frame navigation
  void goToFrame(const FramePosition &position);
  void goToNextFrame();
  void goToPreviousFrame();
  void goToStartFrame();
  void goToEndFrame();

  // State queries
  bool isPlaying() const;
  bool isPaused() const;
  bool isStopped() const;
  PlaybackState state() const;

  // Frame position
  FramePosition currentFrame() const;
  void setCurrentFrame(const FramePosition &position);

  // Frame range
  FrameRange frameRange() const;
  void setFrameRange(const FrameRange &range);

  // Frame rate
  FrameRate frameRate() const;
  void setFrameRate(const FrameRate &rate);

  // Playback speed
  float playbackSpeed() const;
  void setPlaybackSpeed(float speed);

  // Loop settings
  bool isLooping() const;
  void setLooping(bool loop);

  // Real-time mode
  bool isRealTime() const;
  void setRealTime(bool realTime);

  // Playback Range Mode
  void setPlaybackRangeMode(PlaybackRangeMode mode);
  PlaybackRangeMode playbackRangeMode() const;

  void setPlaybackSkipMode(PlaybackSkipMode mode);
  PlaybackSkipMode playbackSkipMode() const;

  // Audio clock provider (allow external modules to supply a clock)
  void setAudioClockProvider(const std::function<double()> &provider);
  void setAudioMasterVolume(float volume);
  void setAudioMasterMuted(bool muted);
  // Use forward declaration to avoid importing AudioClockProvider header here
  void setPlaybackClockProvider(const std::function<double()> &provider);

  // Composition management
  void setCurrentComposition(ArtifactCompositionPtr composition);
  ArtifactCompositionPtr currentComposition() const;
  ArtifactCore::FrameDebugSnapshot frameDebugSnapshot() const;
  double audioOffsetSeconds() const;
  std::int64_t droppedFrameCount() const;

  // In/Out Points
  void setInOutPoints(ArtifactInOutPoints *inOutPoints);
  ArtifactInOutPoints *inOutPoints() const;

  // RAM preview cache
  void setRamPreviewEnabled(bool enabled);
  bool isRamPreviewEnabled() const;
  void setRamPreviewRadius(int frames);
  int ramPreviewRadius() const;
  void setRamPreviewRange(const FrameRange &range);
  FrameRange ramPreviewRange() const;
  void clearRamPreviewCache();
  void prewarmRamPreviewAroundCurrentFrame();
  float ramPreviewHitRate() const;
  int ramPreviewCachedFrameCount() const; // Updated to improve performance
  int ramPreviewRequestedFrameCount() const;
  int ramPreviewReadyFrameCountInRange() const;
  std::vector<bool> ramPreviewCacheBitmap() const;

  // Marker navigation
  void goToNextMarker();
  void goToPreviousMarker();
  void goToNextChapter();
  void goToPreviousChapter();

  // Internal access for debugging
  ArtifactCompositionPlaybackController *controller() const;

public: // signals
  void playbackStateChanged(PlaybackState state)
      W_SIGNAL(playbackStateChanged, state);
  void frameChanged(const FramePosition &position)
      W_SIGNAL(frameChanged, position);
  void playbackSpeedChanged(float speed) W_SIGNAL(playbackSpeedChanged, speed);
  void playbackRangeModeChanged(PlaybackRangeMode mode) W_SIGNAL(playbackRangeModeChanged, mode);
  void playbackSkipModeChanged(PlaybackSkipMode mode) W_SIGNAL(playbackSkipModeChanged, mode);
  void loopingChanged(bool loop) W_SIGNAL(loopingChanged, loop);
  void frameRangeChanged(const FrameRange &range)
      W_SIGNAL(frameRangeChanged, range);
  void currentCompositionChanged(ArtifactCompositionPtr composition)
      W_SIGNAL(currentCompositionChanged, composition);
  void ramPreviewStateChanged(bool enabled, const FrameRange &range)
      W_SIGNAL(ramPreviewStateChanged, enabled, range);
  void ramPreviewStatsChanged(float hitRate, int cachedFrameCount)
      W_SIGNAL(ramPreviewStatsChanged, hitRate, cachedFrameCount);
  void audioLevelChanged(float leftRms, float rightRms, float leftPeak,
                         float rightPeak)
      W_SIGNAL(audioLevelChanged, leftRms, rightRms, leftPeak, rightPeak);
};

} // namespace Artifact
