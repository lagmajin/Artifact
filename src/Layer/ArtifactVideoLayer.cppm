module;
#include <QImage>
#include <QString>
#include <QJsonObject>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QVariant>
#include <QLoggingCategory>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <mutex>
#include <unordered_map>
#include <deque>

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
module Artifact.Layer.Video;




import Artifact.Layer.Video;
import Artifact.Project.Manager;
import Utils.String.UniString;
import Utils.Id;
import Property.Group;
import Property;
import MediaPlaybackController;

namespace Artifact {

Q_LOGGING_CATEGORY(videoLayerLog, "artifact.layer.video")

// ============================================================================
// FrameCache - LRU cache for decoded frames
// ============================================================================
class FrameCache {
public:
    explicit FrameCache(size_t maxFrames = 30)
        : maxFrames_(maxFrames) {}

    void put(int64_t frame, const QImage& frameData) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Remove oldest if at capacity
        while (cacheOrder_.size() >= maxFrames_) {
            int64_t oldest = cacheOrder_.back();
            cacheOrder_.pop_back();
            cache_.erase(oldest);
        }
        
        cache_[frame] = frameData.copy();
        cacheOrder_.push_front(frame);
    }

    bool get(int64_t frame, QImage& outFrame) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(frame);
        if (it != cache_.end()) {
            outFrame = it->second.copy();
            // Move to front (most recently used)
            auto orderIt = std::find(cacheOrder_.begin(), cacheOrder_.end(), frame);
            if (orderIt != cacheOrder_.end()) {
                cacheOrder_.erase(orderIt);
            }
            cacheOrder_.push_front(frame);
            return true;
        }
        return false;
    }

    bool contains(int64_t frame) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.find(frame) != cache_.end();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        cacheOrder_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<int64_t, QImage> cache_;
    std::deque<int64_t> cacheOrder_;
    size_t maxFrames_;
};

// ============================================================================
// ArtifactVideoLayer::Impl
// ============================================================================
class ArtifactVideoLayer::Impl {
public:
    std::unique_ptr<ArtifactCore::MediaPlaybackController> playbackController_;
    FrameCache frameCache_;
    VideoStreamInfo streamInfo_;
    
    // Audio buffering
    std::deque<float> audioBufferL_;
    std::deque<float> audioBufferR_;
    std::mutex audioMutex_;
    
    QString sourcePath_;
    bool isLoaded_ = false;
    
    double playbackSpeed_ = 1.0;
    bool loopEnabled_ = true;
    ProxyQuality proxyQuality_ = ProxyQuality::None;
    QString proxyPath_;
    
    double audioVolume_ = 1.0;
    bool audioMuted_ = false;
    bool audioEnabled_ = true;
    bool videoEnabled_ = true;
    
    QImage currentQImage_;
    int64_t lastDecodedFrame_ = -1;
    bool debugFrameSaved_ = false;
    
    void saveDebugFrame(const QImage& frame, int64_t frameNumber) {
        if (frameNumber != 0 || debugFrameSaved_ || frame.isNull()) {
            return;
        }
        debugFrameSaved_ = true;
        QString assetsPath = ArtifactProjectManager::getInstance().currentProjectAssetsPath();
        
        // Fallback if no project assets path is available
        if (assetsPath.isEmpty()) {
            assetsPath = QDir::currentPath();
            qDebug() << "[VideoLayer] currentProjectAssetsPath is empty, falling back to current path:" << assetsPath;
        }

        QDir assetsDir(assetsPath);
        if (!assetsDir.exists()) {
            assetsDir.mkpath(QStringLiteral("."));
        }
        
        // If we are at the root or current path, we might want to ensure an Assets folder exists
        // but let's just save it to whatever assetsDir is for now to be sure it saves somewhere.
        const QString savePath = assetsDir.filePath(QStringLiteral("debug_decode_frame_0.png"));
        if (frame.save(savePath, "PNG")) {
            qDebug() << "[VideoLayer] Debug frame 0 SUCCESS saved to:" << savePath;
        } else {
            qWarning() << "[VideoLayer] Debug frame 0 FAILED to save to:" << savePath;
        }
    }

    Impl() : playbackController_(std::make_unique<ArtifactCore::MediaPlaybackController>()), frameCache_(30) {}
    ~Impl() = default;
};

// ============================================================================
// ArtifactVideoLayer Implementation
// ============================================================================
ArtifactVideoLayer::ArtifactVideoLayer()
    : impl_(new Impl())
{
    qDebug() << "[VideoLayer] Created";
}

ArtifactVideoLayer::~ArtifactVideoLayer()
{
    if (impl_->playbackController_) {
        impl_->playbackController_->closeMedia();
    }
    delete impl_;
    qDebug() << "[VideoLayer] Destroyed";
}

// === Source Management ===
void ArtifactVideoLayer::setSourceFile(const QString& path)
{
    if (path.isEmpty()) return;
    (void)loadFromPath(path);
}

QString ArtifactVideoLayer::sourceFile() const
{
    return sourcePath();
}

void ArtifactVideoLayer::setHasAudio(bool hasAudio)
{
    impl_->audioEnabled_ = hasAudio;
}

void ArtifactVideoLayer::setHasVideo(bool hasVideo)
{
    impl_->videoEnabled_ = hasVideo;
}

bool ArtifactVideoLayer::loadFromPath(const QString& path)
{
    const QString normalizedPath = QFileInfo(path).absoluteFilePath();
    qDebug() << "[VideoLayer] loadFromPath:" << normalizedPath;

    if (!impl_->playbackController_->openMediaFile(normalizedPath)) {
        qWarning() << "[VideoLayer] Failed to openMediaFile:" << path;
        impl_->isLoaded_ = false;
        return false;
    }

    impl_->sourcePath_ = normalizedPath;
    impl_->streamInfo_ = VideoStreamInfo{};

    const auto playbackInfo = impl_->playbackController_->getPlaybackInfo();
    const auto metadata = impl_->playbackController_->getMetadata();
    if (const auto* videoStream = metadata.getFirstVideoStream()) {
        impl_->streamInfo_.width = videoStream->resolution.width();
        impl_->streamInfo_.height = videoStream->resolution.height();
        impl_->streamInfo_.frameRate = videoStream->frameRate > 0.0 ? videoStream->frameRate : playbackInfo.fps;
        impl_->streamInfo_.frameCount = videoStream->frameCount > 0 ? videoStream->frameCount : playbackInfo.totalFrames;
        impl_->streamInfo_.duration = videoStream->duration > 0.0 ? videoStream->duration : playbackInfo.durationSec;
        impl_->streamInfo_.codecName = videoStream->videoCodec.codecName;
        impl_->streamInfo_.bitRate = static_cast<int>(videoStream->bitrate);
    } else {
        impl_->streamInfo_.frameRate = playbackInfo.fps;
        impl_->streamInfo_.frameCount = playbackInfo.totalFrames;
        impl_->streamInfo_.duration = playbackInfo.durationSec;
    }
    if (const auto* audioStream = metadata.getFirstAudioStream()) {
        impl_->streamInfo_.hasAudio = true;
        impl_->streamInfo_.audioChannels = audioStream->audioCodec.channels;
        impl_->streamInfo_.audioSampleRate = audioStream->audioCodec.sampleRate;
    }

    impl_->isLoaded_ = true;
    
    // Set unified timeline properties
    setInPoint(0);
    setOutPoint(impl_->streamInfo_.frameCount > 0 ? impl_->streamInfo_.frameCount : 300);
    
    impl_->frameCache_.clear();

    const QImage firstFrame = impl_->playbackController_->getVideoFrameAtFrameDirect(0);
    impl_->currentQImage_ = firstFrame;
    if (!firstFrame.isNull()) {
        impl_->frameCache_.put(0, firstFrame);
        impl_->saveDebugFrame(firstFrame, 0); // Save debug frame here
        if (impl_->streamInfo_.width <= 0 || impl_->streamInfo_.height <= 0) {
            impl_->streamInfo_.width = firstFrame.width();
            impl_->streamInfo_.height = firstFrame.height();
        }
    }
    if (impl_->streamInfo_.width > 0 && impl_->streamInfo_.height > 0) {
        setSourceSize(Size_2D(impl_->streamInfo_.width, impl_->streamInfo_.height));
    }
    
    qDebug() << "[VideoLayer] Loaded:" << path
             << "Duration:" << impl_->streamInfo_.duration << "s"
             << "Frames:" << impl_->streamInfo_.frameCount;
    
    return true;
}

QString ArtifactVideoLayer::sourcePath() const
{
    return impl_->sourcePath_;
}

bool ArtifactVideoLayer::isLoaded() const
{
    return impl_->isLoaded_;
}

const VideoStreamInfo& ArtifactVideoLayer::streamInfo() const
{
    return impl_->streamInfo_;
}

// === Playback Control ===
void ArtifactVideoLayer::seekToFrame(int64_t frame)
{
    if (!impl_->isLoaded_) return;
    
    // Convert global timeline frame to source frame
    // Timeline Position at inPoint() corresponds to source frame startTime()
    const int64_t startFrameOnTimeline = inPoint();
    const int64_t endFrameOnTimeline = outPoint();
    frame = std::max(startFrameOnTimeline, std::min(frame, endFrameOnTimeline - 1));
    
    ArtifactAbstractLayer::goToFrame(frame);
    
    {
        std::lock_guard<std::mutex> lock(impl_->audioMutex_);
        impl_->audioBufferL_.clear();
        impl_->audioBufferR_.clear();
    }
    
    const int64_t sourceFrame = currentFrame();
    const bool hasKnownFrameCount = impl_->streamInfo_.frameCount > 0;
    if (sourceFrame >= 0 && (!hasKnownFrameCount || sourceFrame < impl_->streamInfo_.frameCount)) {
        impl_->playbackController_->seekToFrame(sourceFrame);
        decodeCurrentFrame();
    }
}

void ArtifactVideoLayer::seekToTime(double time)
{
    if (!impl_->isLoaded_) return;
    
    int64_t frame = static_cast<int64_t>(time * impl_->streamInfo_.frameRate);
    seekToFrame(frame);
}

int64_t ArtifactVideoLayer::currentFrame() const
{
    return ArtifactAbstractLayer::currentFrame();
}

double ArtifactVideoLayer::currentTime() const
{
    if (impl_->playbackController_ && impl_->playbackController_->isMediaOpen()) {
        return impl_->playbackController_->getCurrentPositionSeconds();
    }
    if (impl_->streamInfo_.frameRate > 0) {
        return currentFrame() / impl_->streamInfo_.frameRate;
    }
    return 0.0;
}

VideoFrameInfo ArtifactVideoLayer::currentFrameInfo() const
{
    VideoFrameInfo info;
    info.frameNumber = static_cast<int>(currentFrame());
    info.timestamp = currentTime();
    info.duration = 1.0 / (impl_->streamInfo_.frameRate > 0 ? impl_->streamInfo_.frameRate : 30.0);
    info.isKeyFrame = false; // Would need FFmpeg to determine this
    return info;
}

// === Frame Decoding ===
void ArtifactVideoLayer::decodeCurrentFrame()
{
    if (!impl_->isLoaded_) {
        qCDebug(videoLayerLog) << "[VideoLayer] decodeCurrentFrame: not loaded, skipping";
        return;
    }

    const int64_t targetFrame = currentFrame();
    
    // Check if we've already decoded this frame to avoid redundant work/logs
    if (targetFrame == impl_->lastDecodedFrame_ && !impl_->currentQImage_.isNull()) {
        return;
    }

    if (targetFrame < 0 || (impl_->streamInfo_.frameCount > 0 && targetFrame >= impl_->streamInfo_.frameCount)) {
        impl_->currentQImage_ = QImage();
        impl_->lastDecodedFrame_ = targetFrame;
        return;
    }
    
    // Check cache first
    QImage cachedFrame;
    if (impl_->frameCache_.get(targetFrame, cachedFrame)) {
        impl_->currentQImage_ = cachedFrame;
        impl_->lastDecodedFrame_ = targetFrame;
        impl_->saveDebugFrame(cachedFrame, targetFrame);
        return;
    }

    // Decode directly through FFmpeg-backed controller path.
    QImage decoded = impl_->playbackController_->getVideoFrameAtFrameDirect(targetFrame);

    if (!decoded.isNull()) {
        impl_->currentQImage_ = decoded;
        impl_->lastDecodedFrame_ = targetFrame;
        impl_->frameCache_.put(targetFrame, decoded);
        impl_->saveDebugFrame(decoded, targetFrame);
        qDebug() << "[VideoLayer] decoded frame" << targetFrame << "size=" << decoded.width() << "x" << decoded.height();
    } else {
        qWarning() << "[VideoLayer] DECODE FAILED for frame" << targetFrame;
        impl_->currentQImage_ = QImage();
        impl_->lastDecodedFrame_ = targetFrame; // Mark as attempted to avoid immediate retry spam
    }
}

QImage ArtifactVideoLayer::currentFrameToQImage() const
{
    return impl_->currentQImage_;
}

QImage ArtifactVideoLayer::decodeFrameToQImage(int64_t frameNumber) const
{
    if (!impl_->isLoaded_) return QImage();
    
    QImage frame;
    if (impl_->frameCache_.get(frameNumber, frame)) {
        return frame;
    }
    
    // Need to decode - non-const operation
    const_cast<ArtifactVideoLayer*>(this)->seekToFrame(frameNumber);
    return currentFrameToQImage();
}

bool ArtifactVideoLayer::isFrameCached(int64_t frameNumber) const
{
    return impl_->frameCache_.contains(frameNumber);
}

void ArtifactVideoLayer::preloadFrames(int64_t startFrame, int count)
{
    if (!impl_->isLoaded_) return;
    
    const int64_t startIdx = inPoint();
    const int64_t endIdx = outPoint();

    for (int i = 0; i < count; ++i) {
        int64_t frame = startFrame + i;
        const bool hasKnownFrameCount = impl_->streamInfo_.frameCount > 0;
        if (frame >= startIdx && (endIdx < 0 || frame < endIdx) && (!hasKnownFrameCount || frame < impl_->streamInfo_.frameCount)) {
            if (!impl_->frameCache_.contains(frame)) {
                const QImage frameData = impl_->playbackController_->getVideoFrameAtFrameDirect(frame);
                if (!frameData.isNull()) {
                    impl_->frameCache_.put(frame, frameData);
                }
            }
        }
    }
    
    qDebug() << "[VideoLayer] Preloaded" << count << "frames from" << startFrame
             << "Cache size:" << impl_->frameCache_.size();
}

// === Proxy Workflow ===
void ArtifactVideoLayer::setProxyQuality(ProxyQuality quality)
{
    impl_->proxyQuality_ = quality;
    
    // Check if proxy file exists
    if (quality != ProxyQuality::None && !impl_->proxyPath_.isEmpty()) {
        if (QFile::exists(impl_->proxyPath_)) {
            // Load proxy instead
            qDebug() << "[VideoLayer] Switching to proxy:" << impl_->proxyPath_;
        }
    }
}

ProxyQuality ArtifactVideoLayer::proxyQuality() const
{
    return impl_->proxyQuality_;
}

bool ArtifactVideoLayer::hasProxy() const
{
    return !impl_->proxyPath_.isEmpty() && QFile::exists(impl_->proxyPath_);
}

bool ArtifactVideoLayer::generateProxy(ProxyQuality quality)
{
    if (!impl_->isLoaded_) return false;
    
    // Calculate proxy dimensions
    double scale = 1.0;
    switch (quality) {
        case ProxyQuality::Quarter: scale = 0.25; break;
        case ProxyQuality::Half: scale = 0.5; break;
        default: return false;
    }
    
    int proxyWidth = static_cast<int>(impl_->streamInfo_.width * scale);
    int proxyHeight = static_cast<int>(impl_->streamInfo_.height * scale);
    
    // Generate proxy path
    QFileInfo srcInfo(impl_->sourcePath_);
    QString proxyDir = srcInfo.absolutePath() + "/.proxy";
    QDir().mkpath(proxyDir);
    QString proxyName = QString("%1_proxy_%2.mp4").arg(srcInfo.baseName()).arg(static_cast<int>(quality));
    impl_->proxyPath_ = proxyDir + "/" + proxyName;
    
    // Use FFmpeg to transcode (this is a placeholder - would need actual FFmpeg integration)
    qDebug() << "[VideoLayer] Generating proxy:" << impl_->proxyPath_
             << "Scale:" << scale << "Size:" << proxyWidth << "x" << proxyHeight;
    
    // TODO: Implement actual proxy generation using FFmpeg or OpenCV
    // For now, just return false to indicate it's not implemented
    return false;
}

void ArtifactVideoLayer::clearProxy()
{
    if (!impl_->proxyPath_.isEmpty()) {
        QFile::remove(impl_->proxyPath_);
        impl_->proxyPath_.clear();
        qDebug() << "[VideoLayer] Cleared proxy";
    }
}

// === In-Point / Out-Point ===
void ArtifactVideoLayer::setInPoint(const FramePosition& pos)
{
    ArtifactAbstractLayer::setInPoint(pos);
}

void ArtifactVideoLayer::setInPoint(int64_t frame)
{
    setInPoint(FramePosition(frame));
}

void ArtifactVideoLayer::setOutPoint(const FramePosition& pos)
{
    ArtifactAbstractLayer::setOutPoint(pos);
}

void ArtifactVideoLayer::setOutPoint(int64_t frame)
{
    setOutPoint(FramePosition(frame));
}

int64_t ArtifactVideoLayer::inPoint() const
{
    return ArtifactAbstractLayer::inPoint().framePosition();
}

int64_t ArtifactVideoLayer::outPoint() const
{
    return ArtifactAbstractLayer::outPoint().framePosition();
}

int64_t ArtifactVideoLayer::effectiveFrameCount() const
{
    return outPoint() - inPoint();
}

// === Loop/Speed ===
void ArtifactVideoLayer::setPlaybackSpeed(double speed)
{
    impl_->playbackSpeed_ = std::max(0.1, std::min(speed, 10.0));
    qDebug() << "[VideoLayer] Playback speed set to" << impl_->playbackSpeed_;
}

double ArtifactVideoLayer::playbackSpeed() const
{
    return impl_->playbackSpeed_;
}

void ArtifactVideoLayer::setLoopEnabled(bool enabled)
{
    impl_->loopEnabled_ = enabled;
}

bool ArtifactVideoLayer::isLoopEnabled() const
{
    return impl_->loopEnabled_;
}

// === Audio ===
bool ArtifactVideoLayer::hasAudio() const
{
    return impl_->audioEnabled_ && impl_->streamInfo_.hasAudio;
}

double ArtifactVideoLayer::audioVolume() const
{
    return impl_->audioVolume_;
}

void ArtifactVideoLayer::setAudioVolume(double volume)
{
    impl_->audioVolume_ = std::max(0.0, std::min(volume, 1.0));
}

void ArtifactVideoLayer::setAudioMuted(bool muted)
{
    impl_->audioMuted_ = muted;
}

bool ArtifactVideoLayer::isAudioMuted() const
{
    return impl_->audioMuted_;
}

// === Serialization ===
QJsonObject ArtifactVideoLayer::toJson() const
{
    QJsonObject obj;
    obj["type"] = "VideoLayer";
    obj["sourcePath"] = impl_->sourcePath_;
    obj["inPoint"] = static_cast<qint64>(inPoint());
    obj["outPoint"] = static_cast<qint64>(outPoint());
    obj["playbackSpeed"] = impl_->playbackSpeed_;
    obj["loopEnabled"] = impl_->loopEnabled_;
    obj["audioVolume"] = impl_->audioVolume_;
    obj["audioMuted"] = impl_->audioMuted_;
    obj["audioEnabled"] = impl_->audioEnabled_;
    obj["videoEnabled"] = impl_->videoEnabled_;
    obj["proxyQuality"] = static_cast<int>(impl_->proxyQuality_);
    obj["proxyPath"] = impl_->proxyPath_;
    
    // Store layer name
    obj["layerName"] = layerName();
    
    return obj;
}

std::shared_ptr<ArtifactVideoLayer> ArtifactVideoLayer::fromJson(const QJsonObject& obj)
{
    auto layer = std::make_shared<ArtifactVideoLayer>();
    
    if (obj.contains("sourcePath")) {
        layer->loadFromPath(obj["sourcePath"].toString());
    }
    
    if (obj.contains("inPoint")) {
        layer->setInPoint(obj["inPoint"].toVariant().toLongLong());
    }
    if (obj.contains("outPoint")) {
        layer->setOutPoint(obj["outPoint"].toVariant().toLongLong());
    }
    if (obj.contains("playbackSpeed")) {
        layer->setPlaybackSpeed(obj["playbackSpeed"].toDouble());
    }
    if (obj.contains("loopEnabled")) {
        layer->setLoopEnabled(obj["loopEnabled"].toBool());
    }
    if (obj.contains("audioVolume")) {
        layer->setAudioVolume(obj["audioVolume"].toDouble());
    }
    if (obj.contains("audioMuted")) {
        layer->setAudioMuted(obj["audioMuted"].toBool());
    }
    if (obj.contains("audioEnabled")) {
        layer->setHasAudio(obj["audioEnabled"].toBool());
    }
    if (obj.contains("videoEnabled")) {
        layer->setHasVideo(obj["videoEnabled"].toBool());
    }
    if (obj.contains("proxyQuality")) {
        layer->setProxyQuality(static_cast<ProxyQuality>(obj["proxyQuality"].toInt()));
    }
    if (obj.contains("layerName")) {
        layer->setLayerName(obj["layerName"].toString());
    }
    
    return layer;
}

// === Overrides ===
void ArtifactVideoLayer::draw(ArtifactIRenderer* renderer)
{
    if (!impl_->videoEnabled_ || !impl_->isLoaded_) return;
    
    // Decode current frame if needed
    if (impl_->currentQImage_.isNull()) {
        decodeCurrentFrame();
    }
    
    if (impl_->currentQImage_.isNull()) return;
    
    auto size = sourceSize();
    // Use drawSprite with identity transform or drawSpriteTransformed with global transform
    // Note: drawLayerForCompositionView usually handles the transform, but 
    // if drawn directly this ensures it still honors its position.
    renderer->drawSpriteTransformed(0.0f, 0.0f, (float)size.width, (float)size.height, 
                                     getGlobalTransform(), impl_->currentQImage_, this->opacity());
}

void ArtifactVideoLayer::goToFrame(int64_t frameNumber)
{
    ArtifactAbstractLayer::goToFrame(frameNumber);
    decodeCurrentFrame();
}

bool ArtifactVideoLayer::getAudio(ArtifactCore::AudioSegment &outSegment, const FramePosition &start,
                                    int frameCount, int sampleRate)
{
    if (!hasAudio() || !impl_->isLoaded_) return false;

    std::lock_guard<std::mutex> lock(impl_->audioMutex_);

    // Check if we need to fetch more data
    while (impl_->audioBufferL_.size() < (size_t)frameCount) {
        QByteArray rawAudio = impl_->playbackController_->getNextAudioFrame();
        if (rawAudio.isEmpty()) break;

        // Convert raw audio (S16 Stereo 44100Hz assumed for now) to float and push to buffer
        // TODO: Use actual format from MediaAudioDecoder
        const int16_t* samples = reinterpret_cast<const int16_t*>(rawAudio.constData());
        int sampleCount = rawAudio.size() / sizeof(int16_t);
        
        for (int i = 0; i < sampleCount; i += 2) {
            impl_->audioBufferL_.push_back(samples[i] / 32768.0f);
            if (i + 1 < sampleCount) {
                impl_->audioBufferR_.push_back(samples[i + 1] / 32768.0f);
            } else {
                impl_->audioBufferR_.push_back(samples[i] / 32768.0f);
            }
        }
    }

    if (impl_->audioBufferL_.empty()) return false;

    // Fill outSegment
    int actualFrames = std::min((int)impl_->audioBufferL_.size(), frameCount);
    outSegment.sampleRate = sampleRate;
    outSegment.layout = ArtifactCore::AudioChannelLayout::Stereo;
    outSegment.channelData.resize(2);
    outSegment.channelData[0].resize(actualFrames);
    outSegment.channelData[1].resize(actualFrames);

    for (int i = 0; i < actualFrames; ++i) {
        outSegment.channelData[0][i] = impl_->audioBufferL_.front() * (float)impl_->audioVolume_;
        outSegment.channelData[1][i] = impl_->audioBufferR_.front() * (float)impl_->audioVolume_;
        impl_->audioBufferL_.pop_front();
        impl_->audioBufferR_.pop_front();
    }

    return true;
}

bool ArtifactVideoLayer::hasVideo() const
{
    return impl_->videoEnabled_ && impl_->isLoaded_ && impl_->streamInfo_.width > 0 && impl_->streamInfo_.height > 0;
}

std::vector<ArtifactCore::PropertyGroup> ArtifactVideoLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup videoGroup(QStringLiteral("Video"));

    videoGroup.addProperty(ArtifactCore::Property(QStringLiteral("video.sourcePath"), sourcePath()));
    
    videoGroup.addProperty(ArtifactCore::Property(QStringLiteral("video.playbackSpeed"), (float)playbackSpeed())
        .setHardRange(0.1, 8.0)
        .setSoftRange(0.25, 2.0)
        .setStep(0.05)
        .setUnit(QStringLiteral("x")));
        
    videoGroup.addProperty(ArtifactCore::Property(QStringLiteral("video.loopEnabled"), isLoopEnabled()));
    
    videoGroup.addProperty(ArtifactCore::Property(QStringLiteral("video.audioVolume"), (float)audioVolume())
        .setHardRange(0.0, 1.0)
        .setSoftRange(0.0, 1.0)
        .setStep(0.01)
        .setUnit(QStringLiteral("linear")));
        
    videoGroup.addProperty(ArtifactCore::Property(QStringLiteral("video.audioMuted"), isAudioMuted()));
    videoGroup.addProperty(ArtifactCore::Property(QStringLiteral("video.audioEnabled"), hasAudio()));
    videoGroup.addProperty(ArtifactCore::Property(QStringLiteral("video.videoEnabled"), hasVideo()));
    
    videoGroup.addProperty(ArtifactCore::Property(QStringLiteral("video.proxyQuality"), (int)proxyQuality())
        .setHardRange(0, 3)
        .setTooltip(QStringLiteral("0=None, 1=Quarter, 2=Half, 3=Full")));

    groups.push_back(videoGroup);
    return groups;
}

bool ArtifactVideoLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == QStringLiteral("video.sourcePath")) {
        const auto path = value.toString().trimmed();
        if (!path.isEmpty()) {
            setSourceFile(path);
            Q_EMIT changed();
        }
        return true;
    }
    if (propertyPath == QStringLiteral("video.playbackSpeed")) {
        setPlaybackSpeed(value.toDouble());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("video.loopEnabled")) {
        setLoopEnabled(value.toBool());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("video.audioVolume")) {
        setAudioVolume(value.toDouble());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("video.audioMuted")) {
        setAudioMuted(value.toBool());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("video.audioEnabled")) {
        setHasAudio(value.toBool());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("video.videoEnabled")) {
        setHasVideo(value.toBool());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("video.proxyQuality")) {
        setProxyQuality(static_cast<ProxyQuality>(value.toInt()));
        Q_EMIT changed();
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

} // namespace Artifact
