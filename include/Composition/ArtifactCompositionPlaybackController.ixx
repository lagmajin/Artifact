module;
#include <QObject>
#include <QTimer>
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
export module Artifact.Composition.PlaybackController;




import Frame.Position;
import Frame.Rate;
import Frame.Range;
import Artifact.Composition.InOutPoints;

W_REGISTER_ARGTYPE(ArtifactCore::FramePosition)
W_REGISTER_ARGTYPE(ArtifactCore::FrameRange)
W_REGISTER_ARGTYPE(ArtifactCore::FrameRate)


export namespace Artifact {

using namespace ArtifactCore;

enum class PlaybackState {
    Stopped,
    Playing,
    Paused
};

class ArtifactCompositionPlaybackController : public QObject {
    W_OBJECT(ArtifactCompositionPlaybackController)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit ArtifactCompositionPlaybackController(QObject* parent = nullptr);
    ~ArtifactCompositionPlaybackController();
    
    // Playback control
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
    void setPlaybackSpeed(float speed); // 1.0 = normal, 2.0 = 2x, 0.5 = half speed
    
    // Loop settings
    bool isLooping() const;
    void setLooping(bool loop);
    
    // Real-time mode (sync to actual time)
    bool isRealTime() const;
    void setRealTime(bool realTime);
    void setAudioClockProvider(const std::function<double()>& provider);
    
    // In/Out Points integration
    void setInOutPoints(ArtifactInOutPoints* inOutPoints);
    ArtifactInOutPoints* inOutPoints() const;
    
    // Navigation to markers
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
        
public:
    void onTimerTick();
    W_SLOT(onTimerTick);
};

}

W_REGISTER_ARGTYPE(Artifact::PlaybackState)