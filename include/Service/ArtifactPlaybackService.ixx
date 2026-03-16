module;
#include <QObject>
#include <functional>
#include <wobjectdefs.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Service.Playback;

import Frame.Position;
import Frame.Rate;
import Frame.Range;
import Artifact.Composition.PlaybackController;
import Artifact.Composition.Abstract;
import Artifact.Playback.Engine;

W_REGISTER_ARGTYPE(Artifact::ArtifactCompositionPtr)


export namespace Artifact {

using namespace ArtifactCore;

class ArtifactPlaybackService : public QObject {
    W_OBJECT(ArtifactPlaybackService)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit ArtifactPlaybackService(QObject* parent = nullptr);
    ~ArtifactPlaybackService();
    
    static ArtifactPlaybackService* instance();
    
    // Playback control for current composition
    void play();
    void pause();
    void stop();
    void togglePlayPause();
    
    // Frame navigation
    void goToFrame(const FramePosition& position);
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
    void setCurrentFrame(const FramePosition& position);
    
    // Frame range
    FrameRange frameRange() const;
    void setFrameRange(const FrameRange& range);
    
    // Frame rate
    FrameRate frameRate() const;
    void setFrameRate(const FrameRate& rate);
    
    // Playback speed
    float playbackSpeed() const;
    void setPlaybackSpeed(float speed);
    
    // Loop settings
    bool isLooping() const;
    void setLooping(bool loop);
    
    // Real-time mode
    bool isRealTime() const;
    void setRealTime(bool realTime);
    
    // Audio clock provider (allow external modules to supply a clock)
    void setAudioClockProvider(const std::function<double()>& provider);
    // Use forward declaration to avoid importing AudioClockProvider header here
    void setPlaybackClockProvider(const std::function<double()>& provider);

    // Composition management
    void setCurrentComposition(ArtifactCompositionPtr composition);
    ArtifactCompositionPtr currentComposition() const;

    // In/Out Points
    void setInOutPoints(class ArtifactInOutPoints* inOutPoints);
    class ArtifactInOutPoints* inOutPoints() const;

    // Marker navigation
    void goToNextMarker();
    void goToPreviousMarker();
    void goToNextChapter();
    void goToPreviousChapter();

public: // signals
    void playbackStateChanged(PlaybackState state)
        W_SIGNAL(playbackStateChanged, state);
    void frameChanged(const FramePosition& position)
        W_SIGNAL(frameChanged, position);
    void playbackSpeedChanged(float speed)
        W_SIGNAL(playbackSpeedChanged, speed);
    void loopingChanged(bool loop)
        W_SIGNAL(loopingChanged, loop);
    void frameRangeChanged(const FrameRange& range)
        W_SIGNAL(frameRangeChanged, range);
    void currentCompositionChanged(ArtifactCompositionPtr composition)
        W_SIGNAL(currentCompositionChanged, composition);
};

}
