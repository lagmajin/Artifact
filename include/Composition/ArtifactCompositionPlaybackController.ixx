module;
#include <QObject>
#include <QTimer>
#include <functional>
#include <wobjectdefs.h>

export module Artifact.Composition.PlaybackController;

import std;
import Frame.Position;
import Frame.Rate;
import Frame.Range;

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
        
private :
 void onTimerTick();
     W_SLOT(onTimerTick);
};

}
