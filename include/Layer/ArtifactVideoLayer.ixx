module;
#include <QImage>
#include <QString>
#include <QJsonObject>
#include <memory>
#include <chrono>

export module Artifact.Layer.Video;

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


import Artifact.Layer.Abstract;


export namespace Artifact {

// Forward declarations
class VideoDecoder;
class ProxyManager;

/// Video frame information
struct VideoFrameInfo {
    int frameNumber = 0;
    double timestamp = 0.0;      // seconds
    double duration = 0.0;       // frame duration in seconds
    bool isKeyFrame = false;
};

/// Video stream metadata
struct VideoStreamInfo {
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    int64_t frameCount = 0;
    double duration = 0.0;       // total duration in seconds
    QString codecName;
    int bitRate = 0;
    bool hasAudio = false;
    int audioChannels = 0;
    int audioSampleRate = 0;
};

/// Proxy quality levels for video playback
enum class ProxyQuality {
    None = 0,       // Full resolution
    Quarter = 1,    // 1/4 resolution
    Half = 2,       // 1/2 resolution
    Full = 3        // Full resolution (same as None, for clarity)
};

/// Video layer for timeline-based video playback
/// Supports:
/// - Frame-accurate seeking and playback
/// - Proxy workflow for large video files
/// - Audio stream handling
/// - Integration with effect pipeline
class ArtifactVideoLayer : public ArtifactAbstractLayer {
private:
    class Impl;
    Impl* impl_;

public:
    ArtifactVideoLayer();
    ~ArtifactVideoLayer() override;

    // === Source Management ===
    // Compatibility API shared with ArtifactMediaLayer.
    void setSourceFile(const QString& path);
    QString sourceFile() const;
    void setHasAudio(bool hasAudio);
    void setHasVideo(bool hasVideo);
    
    /// Load video from file path
    /// @param path Absolute or relative path to video file
    /// @return true if successful
    bool loadFromPath(const QString& path);
    
    /// Get current source file path
    QString sourcePath() const;
    
    /// Check if video is loaded
    bool isLoaded() const;
    
    /// Get video stream information
    const VideoStreamInfo& streamInfo() const;

    // === Playback Control ===
    
    /// Seek to specific frame
    /// @param frame Frame number (0-based)
    void seekToFrame(int64_t frame);
    
    /// Seek to specific time
    /// @param time Time in seconds
    void seekToTime(double time);
    
    /// Get current frame number
    int64_t currentFrame() const;
    
    /// Get current timestamp in seconds
    double currentTime() const;
    
    /// Get current frame info
    VideoFrameInfo currentFrameInfo() const;

    // === Frame Decoding ===
    
    /// Decode current frame to QImage
    /// @return Decoded frame as QImage
    QImage currentFrameToQImage() const;
    
    /// Decode specific frame to QImage
    /// @param frameNumber Frame to decode
    /// @return Decoded frame as QImage
    QImage decodeFrameToQImage(int64_t frameNumber) const;
    
    /// Check if frame is available in cache
    bool isFrameCached(int64_t frameNumber) const;
    
    /// Preload frames into cache
    /// @param startFrame Start frame
    /// @param count Number of frames to preload
    void preloadFrames(int64_t startFrame, int count);

    // === Proxy Workflow ===
    
    /// Set proxy quality level
    void setProxyQuality(ProxyQuality quality);
    
    /// Get current proxy quality
    ProxyQuality proxyQuality() const;
    
    /// Check if proxy is available
    bool hasProxy() const;
    
    /// Generate proxy files
    /// @param quality Target quality level
    /// @return true if successful
    bool generateProxy(ProxyQuality quality);
    
    /// Clear proxy files
    void clearProxy();

    // === In-Point / Out-Point ===
    
    /// Set in-point (start frame for playback)
    void setInPoint(int64_t frame);
    
    /// Set out-point (end frame for playback)
    void setOutPoint(int64_t frame);
    
    /// Get in-point
    int64_t inPoint() const;
    
    /// Get out-point
    int64_t outPoint() const;
    
    /// Get effective duration in frames
    int64_t effectiveFrameCount() const;

    // === Loop/Speed ===
    
    /// Set playback speed (1.0 = normal, 0.5 = half speed, 2.0 = double speed)
    void setPlaybackSpeed(double speed);
    
    /// Get playback speed
    double playbackSpeed() const;
    
    /// Enable/disable loop
    void setLoopEnabled(bool enabled);
    
    /// Check if loop is enabled
    bool isLoopEnabled() const;

    // === Audio ===
    
    /// Check if video has audio stream
    bool hasAudio() const override;
    
    /// Get audio volume (0.0 - 1.0)
    double audioVolume() const;
    
    /// Set audio volume
    void setAudioVolume(double volume);
    
    /// Mute/unmute audio
    void setAudioMuted(bool muted);
    
    /// Check if audio is muted
    bool isAudioMuted() const;

    // === Serialization ===
    
    /// Export to JSON for project save
    QJsonObject toJson() const;
    
    /// Import from JSON for project load
    static std::shared_ptr<ArtifactVideoLayer> fromJson(const QJsonObject& obj);

    // === Overrides ===
    
    void draw(ArtifactIRenderer* renderer) override;
    bool hasVideo() const override;

private:
    /// Internal frame decoding
    void decodeCurrentFrame();
};

} // namespace Artifact
