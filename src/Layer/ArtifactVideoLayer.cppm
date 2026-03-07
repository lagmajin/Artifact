module;
#include <QImage>
#include <QString>
#include <QJsonObject>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QFileInfo>
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
import Utils.String.UniString;
import Utils.Id;

namespace Artifact {

// ============================================================================
// VideoDecoder - Internal class for video decoding using OpenCV
// ============================================================================
class VideoDecoder {
public:
    VideoDecoder() = default;
    ~VideoDecoder() = default;

    bool open(const QString& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!QFile::exists(path)) {
            qWarning() << "[VideoDecoder] File not found:" << path;
            return false;
        }

        cap_.open(path.toStdString());
        if (!cap_.isOpened()) {
            qWarning() << "[VideoDecoder] Failed to open video:" << path;
            return false;
        }

        sourcePath_ = path;
        
        // Extract video info
        info_.width = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
        info_.height = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
        info_.frameRate = cap_.get(cv::CAP_PROP_FPS);
        info_.frameCount = static_cast<int64_t>(cap_.get(cv::CAP_PROP_FRAME_COUNT));
        info_.duration = info_.frameCount / (info_.frameRate > 0 ? info_.frameRate : 30.0);
        info_.bitRate = 0; // OpenCV doesn't expose this directly
        info_.hasAudio = true; // Assume has audio (check separately if needed)
        
        // Try to get codec name
        int fourcc = static_cast<int>(cap_.get(cv::CAP_PROP_FOURCC));
        char codecChars[5] = { (char)(fourcc & 0xFF), (char)((fourcc >> 8) & 0xFF),
                               (char)((fourcc >> 16) & 0xFF), (char)((fourcc >> 24) & 0xFF), 0 };
        info_.codecName = QString::fromLatin1(codecChars);

        qDebug() << "[VideoDecoder] Opened video:" << path
                 << "Size:" << info_.width << "x" << info_.height
                 << "FPS:" << info_.frameRate
                 << "Frames:" << info_.frameCount;

        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cap_.isOpened()) {
            cap_.release();
        }
        sourcePath_.clear();
    }

    bool isOpened() const {
        return cap_.isOpened();
    }

    const VideoStreamInfo& info() const { return info_; }
    QString sourcePath() const { return sourcePath_; }

    bool seek(int64_t frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!cap_.isOpened()) return false;
        
        cap_.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(frame));
        currentFrame_ = frame;
        return true;
    }

    bool seekToTime(double timeSeconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!cap_.isOpened()) return false;
        
        cap_.set(cv::CAP_PROP_POS_MSEC, timeSeconds * 1000.0);
        currentFrame_ = static_cast<int64_t>(cap_.get(cv::CAP_PROP_POS_FRAMES));
        return true;
    }

    int64_t currentFrame() const {
        if (!cap_.isOpened()) return -1;
        return static_cast<int64_t>(cap_.get(cv::CAP_PROP_POS_FRAMES));
    }

    bool readFrame(cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!cap_.isOpened()) return false;
        
        bool success = cap_.read(frame);
        if (success) {
            currentFrame_++;
        }
        return success;
    }

    bool readFrameAt(int64_t frame, cv::Mat& outFrame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!cap_.isOpened()) return false;
        
        cap_.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(frame));
        bool success = cap_.read(outFrame);
        if (success) {
            currentFrame_ = frame + 1;
        }
        return success;
    }

private:
    cv::VideoCapture cap_;
    VideoStreamInfo info_;
    QString sourcePath_;
    int64_t currentFrame_ = 0;
    mutable std::mutex mutex_;
};

// ============================================================================
// FrameCache - LRU cache for decoded frames
// ============================================================================
class FrameCache {
public:
    explicit FrameCache(size_t maxFrames = 30)
        : maxFrames_(maxFrames) {}

    void put(int64_t frame, const cv::Mat& frameData) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Remove oldest if at capacity
        while (cacheOrder_.size() >= maxFrames_) {
            int64_t oldest = cacheOrder_.back();
            cacheOrder_.pop_back();
            cache_.erase(oldest);
        }
        
        cache_[frame] = frameData.clone();
        cacheOrder_.push_front(frame);
    }

    bool get(int64_t frame, cv::Mat& outFrame) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(frame);
        if (it != cache_.end()) {
            outFrame = it->second.clone();
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
    std::unordered_map<int64_t, cv::Mat> cache_;
    std::deque<int64_t> cacheOrder_;
    size_t maxFrames_;
};

// ============================================================================
// ArtifactVideoLayer::Impl
// ============================================================================
class ArtifactVideoLayer::Impl {
public:
    std::unique_ptr<VideoDecoder> decoder_;
    FrameCache frameCache_;
    VideoStreamInfo streamInfo_;
    
    QString sourcePath_;
    bool isLoaded_ = false;
    
    int64_t currentFrame_ = 0;
    int64_t inPoint_ = 0;
    int64_t outPoint_ = -1;  // -1 means end of video
    
    double playbackSpeed_ = 1.0;
    bool loopEnabled_ = true;
    ProxyQuality proxyQuality_ = ProxyQuality::None;
    QString proxyPath_;
    
    double audioVolume_ = 1.0;
    bool audioMuted_ = false;
    bool audioEnabled_ = true;
    bool videoEnabled_ = true;
    
    cv::Mat currentFrameData_;
    QImage currentQImage_;
    
    Impl() : frameCache_(30), decoder_(std::make_unique<VideoDecoder>()) {}
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
    if (impl_->decoder_) {
        impl_->decoder_->close();
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
    if (!impl_->decoder_->open(path)) {
        qWarning() << "[VideoLayer] Failed to load video:" << path;
        impl_->isLoaded_ = false;
        return false;
    }
    
    impl_->sourcePath_ = path;
    impl_->streamInfo_ = impl_->decoder_->info();
    impl_->isLoaded_ = true;
    impl_->currentFrame_ = 0;
    impl_->inPoint_ = 0;
    impl_->outPoint_ = impl_->streamInfo_.frameCount - 1;
    impl_->frameCache_.clear();
    
    // Set source size for parent class
    // setSourceSize({impl_->streamInfo_.width, impl_->streamInfo_.height});
    
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
    
    // Clamp to valid range
    frame = std::max(impl_->inPoint_, std::min(frame, impl_->outPoint_));
    
    impl_->currentFrame_ = frame;
    impl_->decoder_->seek(frame);
    decodeCurrentFrame();
}

void ArtifactVideoLayer::seekToTime(double time)
{
    if (!impl_->isLoaded_) return;
    
    int64_t frame = static_cast<int64_t>(time * impl_->streamInfo_.frameRate);
    seekToFrame(frame);
}

int64_t ArtifactVideoLayer::currentFrame() const
{
    return impl_->currentFrame_;
}

double ArtifactVideoLayer::currentTime() const
{
    if (impl_->streamInfo_.frameRate > 0) {
        return impl_->currentFrame_ / impl_->streamInfo_.frameRate;
    }
    return 0.0;
}

VideoFrameInfo ArtifactVideoLayer::currentFrameInfo() const
{
    VideoFrameInfo info;
    info.frameNumber = static_cast<int>(impl_->currentFrame_);
    info.timestamp = currentTime();
    info.duration = 1.0 / (impl_->streamInfo_.frameRate > 0 ? impl_->streamInfo_.frameRate : 30.0);
    info.isKeyFrame = false; // Would need FFmpeg to determine this
    return info;
}

// === Frame Decoding ===
void ArtifactVideoLayer::decodeCurrentFrame()
{
    if (!impl_->isLoaded_) return;
    
    // Check cache first
    if (impl_->frameCache_.get(impl_->currentFrame_, impl_->currentFrameData_)) {
        qDebug() << "[VideoLayer] Frame" << impl_->currentFrame_ << "from cache";
        return;
    }
    
    // Decode from video
    if (impl_->decoder_->readFrameAt(impl_->currentFrame_, impl_->currentFrameData_)) {
        // Cache the frame
        impl_->frameCache_.put(impl_->currentFrame_, impl_->currentFrameData_);
        qDebug() << "[VideoLayer] Decoded frame" << impl_->currentFrame_;
    }
}

QImage ArtifactVideoLayer::currentFrameToQImage() const
{
    if (!impl_->isLoaded_ || impl_->currentFrameData_.empty()) {
        return QImage();
    }
    
    cv::Mat rgb;
    if (impl_->currentFrameData_.channels() == 3) {
        cv::cvtColor(impl_->currentFrameData_, rgb, cv::COLOR_BGR2RGB);
    } else if (impl_->currentFrameData_.channels() == 4) {
        cv::cvtColor(impl_->currentFrameData_, rgb, cv::COLOR_BGRA2RGB);
    } else {
        rgb = impl_->currentFrameData_;
    }
    
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                  QImage::Format_RGB888).copy();
}

QImage ArtifactVideoLayer::decodeFrameToQImage(int64_t frameNumber) const
{
    if (!impl_->isLoaded_) return QImage();
    
    cv::Mat frame;
    if (impl_->frameCache_.get(frameNumber, frame)) {
        cv::Mat rgb;
        if (frame.channels() == 3) {
            cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
        } else {
            rgb = frame;
        }
        return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                      QImage::Format_RGB888).copy();
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
    
    for (int i = 0; i < count; ++i) {
        int64_t frame = startFrame + i;
        if (frame >= impl_->inPoint_ && frame <= impl_->outPoint_) {
            if (!impl_->frameCache_.contains(frame)) {
                cv::Mat frameData;
                if (impl_->decoder_->readFrameAt(frame, frameData)) {
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
void ArtifactVideoLayer::setInPoint(int64_t frame)
{
    impl_->inPoint_ = std::max(0LL, std::min(frame, impl_->streamInfo_.frameCount - 1));
    if (impl_->outPoint_ < impl_->inPoint_) {
        impl_->outPoint_ = impl_->inPoint_;
    }
    qDebug() << "[VideoLayer] In-point set to" << impl_->inPoint_;
}

void ArtifactVideoLayer::setOutPoint(int64_t frame)
{
    impl_->outPoint_ = std::max(impl_->inPoint_, std::min(frame, impl_->streamInfo_.frameCount - 1));
    qDebug() << "[VideoLayer] Out-point set to" << impl_->outPoint_;
}

int64_t ArtifactVideoLayer::inPoint() const
{
    return impl_->inPoint_;
}

int64_t ArtifactVideoLayer::outPoint() const
{
    return impl_->outPoint_ >= 0 ? impl_->outPoint_ : impl_->streamInfo_.frameCount - 1;
}

int64_t ArtifactVideoLayer::effectiveFrameCount() const
{
    return outPoint() - inPoint() + 1;
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
    obj["inPoint"] = static_cast<qint64>(impl_->inPoint_);
    obj["outPoint"] = static_cast<qint64>(impl_->outPoint_);
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
        layer->setInPoint(obj["inPoint"].toInteger());
    }
    if (obj.contains("outPoint")) {
        layer->setOutPoint(obj["outPoint"].toInteger());
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
    if (impl_->currentFrameData_.empty() || 
        impl_->currentFrame_ != impl_->decoder_->currentFrame()) {
        decodeCurrentFrame();
    }
    
    if (impl_->currentFrameData_.empty()) return;

    cv::Mat bgra;
    cv::cvtColor(impl_->currentFrameData_, bgra, cv::COLOR_BGR2BGRA);
    QImage img((uchar*)bgra.data, bgra.cols, bgra.rows, bgra.step, QImage::Format_RGBA8888);
    
    auto size = sourceSize();
    renderer->drawSprite(0.0f, 0.0f, (float)size.width, (float)size.height, img);
}

bool ArtifactVideoLayer::hasVideo() const
{
    return impl_->videoEnabled_ && impl_->isLoaded_;
}

} // namespace Artifact
