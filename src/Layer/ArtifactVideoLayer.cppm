module;
#include <QImage>
#include <QMatrix4x4>
#include <QString>
#include <QJsonObject>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QVariant>
#include <QLoggingCategory>
#include <QThread>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QMetaObject>
#include <QPointer>
#include <opencv2/opencv.hpp>
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
#include <Layer/ArtifactCloneEffectSupport.hpp>
module Artifact.Layer.Video;




import Artifact.Layer.Video;
import Thread.Helper;
import Artifact.Composition.Abstract;
import Artifact.Project.Manager;
import Event.Bus;
import Artifact.Event.Types;
import CvUtils;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;
import Utils.Id;
import Property.Group;
import Property;
import MediaPlaybackController;

namespace Artifact {

Q_LOGGING_CATEGORY(videoLayerLog, "artifact.layer.video")

namespace {

int64_t timelineFrameToSourceFrame(const ArtifactVideoLayer* layer, int64_t timelineFrame)
{
    if (!layer) {
        return timelineFrame;
    }

    // Apply time remap if enabled
    if (layer->isTimeRemapEnabled()) {
        // Get composition frame from timeline frame
        const int64_t compFrame = timelineFrame;
        const double sourceFrameDouble = layer->getSourceFrameAtCompFrame(compFrame);
        return static_cast<int64_t>(sourceFrameDouble);
    }

    return timelineFrame - layer->inPoint() + layer->startTime().framePosition();
}

int64_t sourceFrameToTimelineFrame(const ArtifactVideoLayer* layer, int64_t sourceFrame)
{
    if (!layer) {
        return sourceFrame;
    }
    return sourceFrame + layer->inPoint() - layer->startTime().framePosition();
}

QString decoderBackendName(const ArtifactCore::MediaPlaybackController* controller)
{
    if (!controller) {
        return QStringLiteral("Unknown");
    }
    return controller->getDecoderBackend() == ArtifactCore::DecoderBackend::MediaFoundation
        ? QStringLiteral("MediaFoundation")
        : QStringLiteral("FFmpeg");
}

int64_t currentSourceFrame(const ArtifactVideoLayer* layer)
{
    return layer ? timelineFrameToSourceFrame(layer, layer->currentFrame()) : 0;
}

ArtifactCore::ImageF32x4_RGBA toFrameBuffer(const QImage& frame)
{
    ArtifactCore::ImageF32x4_RGBA buffer;
    if (frame.isNull()) {
        return buffer;
    }

    cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(frame, true);
    if (mat.empty()) {
        return buffer;
    }

    if (mat.type() == CV_8UC4) {
        cv::Mat converted;
        cv::cvtColor(mat, converted, cv::COLOR_BGRA2RGBA);
        mat = std::move(converted);
    }

    buffer.setFromCVMat(mat);
    return buffer;
}

void publishVideoLayerModified(ArtifactVideoLayer* layer)
{
    if (!layer) {
        return;
    }

    if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
    }
}

void publishVideoLayerModifiedAsync(ArtifactVideoLayer* layer)
{
    if (!layer) {
        return;
    }

    QPointer<ArtifactVideoLayer> guarded(layer);
    QMetaObject::invokeMethod(
        layer,
        [guarded]() {
            publishVideoLayerModified(guarded.data());
        },
        Qt::QueuedConnection);
}

QString threadIdTag()
{
    return QStringLiteral("thread=%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

QString threadDiagnosticsTag()
{
    return QStringLiteral("%1 | %2")
        .arg(ArtifactCore::sharedBackgroundThreadPoolDebugString(),
             ArtifactCore::currentProcessThreadDebugString());
}

}

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
    struct AsyncOpenResult {
        bool success = false;
        QString normalizedPath;
        QString error;
        VideoStreamInfo streamInfo;
        int64_t defaultOutPoint = 300;
        std::shared_ptr<ArtifactCore::MediaPlaybackController> controller;
    };

    std::shared_ptr<ArtifactCore::MediaPlaybackController> playbackController_;
    FrameCache frameCache_;
    VideoStreamInfo streamInfo_;
    
    // Audio buffering
    std::deque<float> audioBufferL_;
    std::deque<float> audioBufferR_;
    std::mutex audioMutex_;
    int64_t lastAudioRequestTimelineFrame_ = std::numeric_limits<int64_t>::min();
    
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
    ArtifactCore::ImageF32x4_RGBA currentFrameBuffer_;
    bool hasCurrentFrameBuffer_ = false;
    int64_t lastDecodedFrame_ = -1;
    int64_t lastSyncFallbackSourceFrame_ = -1;
    bool lastSyncFallbackSucceeded_ = false;
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

    Impl() : playbackController_(std::make_shared<ArtifactCore::MediaPlaybackController>()), frameCache_(30) {}
    ~Impl() {
        // バックグラウンドフューチャーが残っていても放置（デストラクタをブロックしない）
        // QFuture はスコープ離脱後も自動キャンセルされない点に注意
    }

    // [Fix C] バックグラウンドデコード管理
    mutable QFuture<QImage> decodeFuture_;
    mutable std::atomic<bool> decoding_{ false };
    mutable int64_t decodeTargetFrame_ = -1;
    mutable QFuture<AsyncOpenResult> openFuture_;
    mutable std::atomic<bool> opening_{ false };
    mutable int openRequestId_ = 0;
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
    qDebug() << "[VideoLayer] loadFromPath:" << normalizedPath << threadIdTag();

    impl_->sourcePath_ = normalizedPath;
    impl_->streamInfo_ = VideoStreamInfo{};
    impl_->currentQImage_ = QImage();
    impl_->currentFrameBuffer_ = ArtifactCore::ImageF32x4_RGBA();
    impl_->hasCurrentFrameBuffer_ = false;
    impl_->lastDecodedFrame_ = -1;
    impl_->lastSyncFallbackSourceFrame_ = -1;
    impl_->lastSyncFallbackSucceeded_ = false;
    impl_->decodeTargetFrame_ = -1;
    impl_->decoding_ = false;
    impl_->opening_ = true;
    impl_->isLoaded_ = false;
    impl_->lastAudioRequestTimelineFrame_ = std::numeric_limits<int64_t>::min();
    impl_->frameCache_.clear();
    setSourceSize(Size_2D(0, 0));

    const int requestId = ++impl_->openRequestId_;
    auto* layer = this;
    impl_->openFuture_ = QtConcurrent::run(&sharedBackgroundThreadPool(), [normalizedPath]() -> Impl::AsyncOpenResult {
        ArtifactCore::ScopedThreadName threadName(
            QStringLiteral("VideoLayer/open:%1").arg(QFileInfo(normalizedPath).fileName()));
        Impl::AsyncOpenResult result;
        result.normalizedPath = normalizedPath;

        auto controller = std::make_shared<ArtifactCore::MediaPlaybackController>();
        controller->setDecoderBackend(ArtifactCore::DecoderBackend::FFmpeg);
        if (!controller->openMediaFile(normalizedPath)) {
            result.error = controller->getLastError();
            return result;
        }

        result.success = true;
        result.controller = controller;

        const auto playbackInfo = controller->getPlaybackInfo();
        const auto metadata = controller->getMetadata();
        if (const auto* videoStream = metadata.getFirstVideoStream()) {
            result.streamInfo.width = videoStream->resolution.width();
            result.streamInfo.height = videoStream->resolution.height();
            result.streamInfo.frameRate = videoStream->frameRate > 0.0 ? videoStream->frameRate : playbackInfo.fps;
            result.streamInfo.frameCount = videoStream->frameCount > 0 ? videoStream->frameCount : playbackInfo.totalFrames;
            result.streamInfo.duration = videoStream->duration > 0.0 ? videoStream->duration : playbackInfo.durationSec;
            result.streamInfo.codecName = videoStream->videoCodec.codecName;
            result.streamInfo.bitRate = static_cast<int>(videoStream->bitrate);
        } else {
            result.streamInfo.frameRate = playbackInfo.fps;
            result.streamInfo.frameCount = playbackInfo.totalFrames;
            result.streamInfo.duration = playbackInfo.durationSec;
        }
        if (const auto* audioStream = metadata.getFirstAudioStream()) {
            result.streamInfo.hasAudio = true;
            result.streamInfo.audioChannels = audioStream->audioCodec.channels;
            result.streamInfo.audioSampleRate = audioStream->audioCodec.sampleRate;
        }
        result.defaultOutPoint = result.streamInfo.frameCount > 0 ? result.streamInfo.frameCount : 300;
        return result;
    });

    auto* watcher = new QFutureWatcher<Impl::AsyncOpenResult>(this);
    QObject::connect(watcher, &QFutureWatcher<Impl::AsyncOpenResult>::finished, this,
                     [layer, watcher, requestId]() {
        const Impl::AsyncOpenResult result = watcher->result();
        watcher->deleteLater();
        if (!layer || !layer->impl_) {
            return;
        }
        if (requestId != layer->impl_->openRequestId_) {
            return;
        }

        layer->impl_->opening_ = false;

        if (!result.success || !result.controller) {
            qCritical() << "[VideoLayer] openMediaFile FAILED:" << result.normalizedPath
                        << "lastError=" << result.error
                        << threadDiagnosticsTag();
            layer->impl_->isLoaded_ = false;
            publishVideoLayerModified(layer);
            return;
        }

        layer->impl_->playbackController_ = result.controller;
        layer->impl_->sourcePath_ = result.normalizedPath;
        layer->impl_->streamInfo_ = result.streamInfo;
        layer->impl_->isLoaded_ = true;
        layer->impl_->currentQImage_ = QImage();
        layer->impl_->currentFrameBuffer_ = ArtifactCore::ImageF32x4_RGBA();
        layer->impl_->hasCurrentFrameBuffer_ = false;
        layer->impl_->lastDecodedFrame_ = -1;
        layer->impl_->lastSyncFallbackSourceFrame_ = -1;
        layer->impl_->lastSyncFallbackSucceeded_ = false;
        layer->impl_->decodeTargetFrame_ = -1;
        layer->impl_->decoding_ = false;
        layer->impl_->lastAudioRequestTimelineFrame_ = std::numeric_limits<int64_t>::min();
        layer->impl_->frameCache_.clear();

        qCInfo(videoLayerLog) << "[VideoLayer] active backend:"
                              << decoderBackendName(layer->impl_->playbackController_.get())
                              << layer->impl_->playbackController_->getDebugState()
                              << threadDiagnosticsTag()
                              << threadIdTag();

        layer->setInPoint(0);
        layer->setOutPoint(result.defaultOutPoint);

        qCInfo(videoLayerLog) << "[VideoLayer] stream info"
                              << "size=" << layer->impl_->streamInfo_.width << "x" << layer->impl_->streamInfo_.height
                              << "fps=" << layer->impl_->streamInfo_.frameRate
                              << "frames=" << layer->impl_->streamInfo_.frameCount
                              << "duration=" << layer->impl_->streamInfo_.duration
                              << "hasAudio=" << layer->impl_->streamInfo_.hasAudio
                              << threadIdTag();

        if (layer->impl_->streamInfo_.width > 0 && layer->impl_->streamInfo_.height > 0) {
            layer->setSourceSize(Size_2D(layer->impl_->streamInfo_.width, layer->impl_->streamInfo_.height));
        }

        layer->impl_->decoding_ = true;
        layer->impl_->decodeTargetFrame_ = 0;
        auto* ctrl = layer->impl_->playbackController_.get();
        layer->impl_->decodeFuture_ = QtConcurrent::run(&sharedBackgroundThreadPool(), [ctrl, layer]() -> QImage {
            ArtifactCore::ScopedThreadName threadName(QStringLiteral("VideoLayer/decode"));
            QImage frame = ctrl->getVideoFrameAtFrameDirect(0);
            if (frame.isNull()) {
                qWarning() << "[VideoLayer] initial decode FAILED"
                           << "source=0"
                           << "backend=" << decoderBackendName(ctrl)
                           << "controller=" << ctrl->getDebugState()
                           << threadDiagnosticsTag();
            }
            layer->impl_->decoding_ = false;
            publishVideoLayerModifiedAsync(layer);
            return frame;
        });

        qDebug() << "[VideoLayer] Loaded:" << result.normalizedPath
                 << "Duration:" << layer->impl_->streamInfo_.duration << "s"
                 << "Frames:" << layer->impl_->streamInfo_.frameCount;
        publishVideoLayerModified(layer);
    });
    watcher->setFuture(impl_->openFuture_);

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

QString ArtifactVideoLayer::debugState() const
{
    const auto* controller = impl_ ? impl_->playbackController_.get() : nullptr;
    const int64_t timelineFrame = currentFrame();
    const int64_t sourceFrame = currentSourceFrame(this);
    return QStringLiteral(
               "loaded=%1 opening=%2 decoding=%3 timeline=%4 source=%5 "
               "decodeTarget=%6 lastDecoded=%7 cached=%8 hasQImage=%9 hasBuffer=%10 "
               "syncFallbackFrame=%11 syncFallbackOk=%12 backend=%13 lastError=%14 state={%15}")
        .arg(impl_ && impl_->isLoaded_ ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(impl_ && impl_->opening_.load() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(impl_ && impl_->decoding_.load() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(timelineFrame)
        .arg(sourceFrame)
        .arg(impl_ ? impl_->decodeTargetFrame_ : -1)
        .arg(impl_ ? impl_->lastDecodedFrame_ : -1)
        .arg(isFrameCached(timelineFrame) ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(impl_ && !impl_->currentQImage_.isNull() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(impl_ && impl_->hasCurrentFrameBuffer_ ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(impl_ ? impl_->lastSyncFallbackSourceFrame_ : -1)
        .arg(impl_ && impl_->lastSyncFallbackSucceeded_ ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(decoderBackendName(controller))
        .arg(controller ? controller->getLastError() : QStringLiteral("<no controller>"))
        .arg(controller ? controller->getDebugState() : QStringLiteral("<no controller>"));
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
    
    bool shouldDecode = false;
    {
        std::lock_guard<std::mutex> lock(impl_->audioMutex_);
        impl_->audioBufferL_.clear();
        impl_->audioBufferR_.clear();
        impl_->lastAudioRequestTimelineFrame_ = std::numeric_limits<int64_t>::min();
    }
    
    const int64_t timelineFrame = frame;
    const int64_t sourceFrame = timelineFrameToSourceFrame(this, timelineFrame);
    const bool hasKnownFrameCount = impl_->streamInfo_.frameCount > 0;
    if (sourceFrame >= 0 && (!hasKnownFrameCount || sourceFrame < impl_->streamInfo_.frameCount)) {
        shouldDecode = true;
        qCDebug(videoLayerLog) << "[VideoLayer] seekToFrame"
                               << "timeline=" << timelineFrame
                               << "source=" << sourceFrame
                               << "in=" << inPoint()
                               << "out=" << outPoint()
                               << "start=" << startTime().framePosition()
                               << threadIdTag();
    } else {
        qWarning() << "[VideoLayer] seekToFrame rejected"
                   << "timeline=" << timelineFrame
                   << "source=" << sourceFrame
                   << "knownFrames=" << impl_->streamInfo_.frameCount
                   << "in=" << inPoint()
                   << "out=" << outPoint()
                   << "start=" << startTime().framePosition();
    }

    if (shouldDecode && impl_->playbackController_) {
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

const ArtifactCore::ImageF32x4_RGBA& ArtifactVideoLayer::currentFrameBuffer() const
{
    return impl_->currentFrameBuffer_;
}

bool ArtifactVideoLayer::hasCurrentFrameBuffer() const
{
    return impl_->hasCurrentFrameBuffer_;
}

// === Frame Decoding ===
void ArtifactVideoLayer::decodeCurrentFrame()
{
    // [Fix 3] isLoaded_ = false の間はサイレントリターン。
    // レンダーループが毎フレーム呼び出すため大量のスパムログになるので抗黙する。
    if (!impl_->isLoaded_ || impl_->opening_.load()) {
        return;
    }

    const int64_t sourceFrame = currentSourceFrame(this);
    const int64_t timelineFrame = sourceFrameToTimelineFrame(this, sourceFrame);

    // 同じフレームでキャッシュ済みの場合はスキップ
    if (sourceFrame == impl_->lastDecodedFrame_ && !impl_->currentQImage_.isNull()) {
        return;
    }

    // decoding_ は lambda 内の return 直前にクリアされるため isFinished() も合わせて確認する
    const bool inFlight = impl_->decoding_.load()
        || (impl_->decodeTargetFrame_ >= 0 && !impl_->decodeFuture_.isFinished());
    if (inFlight) {
        return;
    }

    if (timelineFrame < inPoint() || timelineFrame >= outPoint() ||
        sourceFrame < 0 ||
        (impl_->streamInfo_.frameCount > 0 && sourceFrame >= impl_->streamInfo_.frameCount)) {
        qWarning() << "[VideoLayer] decodeCurrentFrame rejected"
                   << "timeline=" << timelineFrame
                   << "source=" << sourceFrame
                   << "in=" << inPoint()
                   << "out=" << outPoint()
                   << "start=" << startTime().framePosition()
                   << threadIdTag()
                   << threadDiagnosticsTag()
                   << "streamFrames=" << impl_->streamInfo_.frameCount;
        return;
    }

    // キャッシュに存在すれば即座に返す
    QImage cachedFrame;
    if (impl_->frameCache_.get(sourceFrame, cachedFrame)) {
        impl_->currentQImage_ = cachedFrame;
        impl_->currentFrameBuffer_ = toFrameBuffer(cachedFrame);
        impl_->hasCurrentFrameBuffer_ = true;
        impl_->lastDecodedFrame_ = sourceFrame;
        return;
    }

    qCDebug(videoLayerLog)
        << "[VideoLayerT] decodeCurrentFrame starting bg decode"
        << "timeline=" << timelineFrame
        << "source=" << sourceFrame
        << "hasBuffer=" << impl_->hasCurrentFrameBuffer_
        << "hasQImage=" << !impl_->currentQImage_.isNull()
        << "inPoint=" << inPoint()
        << "outPoint=" << outPoint()
        << "backend=" << decoderBackendName(impl_->playbackController_.get())
        << threadIdTag();

    // バックグラウンドでデコードを開始し、メインスレッドをブロックしない
    // 現在フレームは currentQImage_（前のフレーム）をそのまま表示し続ける
    impl_->decoding_ = true;
    impl_->decodeTargetFrame_ = sourceFrame;
    auto* ctrl = impl_->playbackController_.get();
    const QString backendName = decoderBackendName(ctrl);
    impl_->decodeFuture_ = QtConcurrent::run(&sharedBackgroundThreadPool(), [ctrl, timelineFrame, sourceFrame, backendName, this]() -> QImage {
        ArtifactCore::ScopedThreadName threadName(QStringLiteral("VideoLayer/decode"));
        QImage decoded = ctrl->getVideoFrameAtFrameDirect(sourceFrame);
        if (!decoded.isNull()) {
            impl_->frameCache_.put(sourceFrame, decoded);
            impl_->currentQImage_ = decoded;
            impl_->currentFrameBuffer_ = toFrameBuffer(decoded);
            impl_->hasCurrentFrameBuffer_ = true;
            impl_->lastDecodedFrame_ = sourceFrame;
            impl_->saveDebugFrame(decoded, sourceFrame);
            qCDebug(videoLayerLog) << "[VideoLayer] bg decoded frame"
                                   << "timeline=" << timelineFrame
                                   << "source=" << sourceFrame
                                   << "size=" << decoded.width() << "x" << decoded.height()
                                   << threadIdTag();
        } else {
            qWarning() << "[VideoLayer] DECODE FAILED"
                       << "timeline=" << timelineFrame
                       << "source=" << sourceFrame
                       << "backend=" << backendName
                       << "backendLastError=" << ctrl->getLastError()
                       << "controller=" << ctrl->getDebugState()
                       << threadDiagnosticsTag()
                       << threadIdTag();
        }
        impl_->decoding_ = false;
        if (!decoded.isNull()) {
            publishVideoLayerModifiedAsync(this);
        }
        return decoded;
    });
}

QImage ArtifactVideoLayer::currentFrameToQImage() const
{
    if (impl_->opening_.load()) {
        return impl_->currentQImage_;
    }

    const int64_t sourceFrame = currentSourceFrame(this);

    // decoding_ は lambda 内の return 直前にクリアされるため isFinished() も合わせて確認する
    const bool inFlight = impl_->decoding_.load()
        || (impl_->decodeTargetFrame_ >= 0 && !impl_->decodeFuture_.isFinished());

    // 完了したバックグラウンドデコード結果を取り込む
    if (!inFlight && impl_->decodeTargetFrame_ >= 0) {
        QImage loaded = impl_->decodeFuture_.result();
        if (!loaded.isNull()) {
            if (impl_->decodeTargetFrame_ != impl_->lastDecodedFrame_) {
                impl_->currentQImage_ = loaded;
                impl_->currentFrameBuffer_ = toFrameBuffer(loaded);
                impl_->hasCurrentFrameBuffer_ = true;
                impl_->lastDecodedFrame_ = impl_->decodeTargetFrame_;
                if (impl_->streamInfo_.width <= 0 || impl_->streamInfo_.height <= 0) {
                    impl_->streamInfo_.width  = loaded.width();
                    impl_->streamInfo_.height = loaded.height();
                    const_cast<ArtifactVideoLayer*>(this)->setSourceSize(
                        Size_2D(loaded.width(), loaded.height()));
                }
            }
        } else {
            qWarning() << "[VideoLayer] async decode null frame"
                       << "timeline=" << sourceFrameToTimelineFrame(this, impl_->decodeTargetFrame_)
                       << "source=" << impl_->decodeTargetFrame_
                       << "backend=" << decoderBackendName(impl_->playbackController_.get())
                       << "backendLastError=" << impl_->playbackController_->getLastError()
                       << "controller=" << impl_->playbackController_->getDebugState()
                       << threadDiagnosticsTag();
        }
        impl_->decodeTargetFrame_ = -1;
    }

    // 現在フレームがまだデコードされていなければ非同期デコードを起動する
    if (!inFlight && sourceFrame != impl_->lastDecodedFrame_) {
        const_cast<ArtifactVideoLayer*>(this)->decodeCurrentFrame();
    }

    return impl_->currentQImage_;
}

QImage ArtifactVideoLayer::decodeFrameToQImage(int64_t frameNumber) const
{
    if (!impl_->isLoaded_ || impl_->opening_.load()) return QImage();

    if (!impl_->playbackController_ || !impl_->playbackController_->isMediaOpen()) {
        return QImage();
    }

    const int64_t startFrameOnTimeline = inPoint();
    const int64_t endFrameOnTimeline = outPoint();
    const int64_t clampedTimelineFrame =
        std::max(startFrameOnTimeline, std::min(frameNumber, endFrameOnTimeline - 1));
    const int64_t sourceFrame = timelineFrameToSourceFrame(this, clampedTimelineFrame);

    QImage frame;
    if (impl_->frameCache_.get(sourceFrame, frame)) {
        if (sourceFrame == currentSourceFrame(this)) {
            impl_->currentQImage_ = frame;
            impl_->currentFrameBuffer_ = toFrameBuffer(frame);
            impl_->hasCurrentFrameBuffer_ = true;
            impl_->lastDecodedFrame_ = sourceFrame;
            impl_->lastSyncFallbackSourceFrame_ = sourceFrame;
            impl_->lastSyncFallbackSucceeded_ = true;
        }
        return frame;
    }

    if (impl_->lastSyncFallbackSourceFrame_ == sourceFrame &&
        !impl_->lastSyncFallbackSucceeded_) {
        return QImage();
    }

    const QImage decoded = impl_->playbackController_->getVideoFrameAtFrameDirect(sourceFrame);
    if (!decoded.isNull()) {
        impl_->frameCache_.put(sourceFrame, decoded);
        if (sourceFrame == currentSourceFrame(this)) {
            impl_->currentQImage_ = decoded;
            impl_->currentFrameBuffer_ = toFrameBuffer(decoded);
            impl_->hasCurrentFrameBuffer_ = true;
            impl_->lastDecodedFrame_ = sourceFrame;
        }
        impl_->lastSyncFallbackSourceFrame_ = sourceFrame;
        impl_->lastSyncFallbackSucceeded_ = true;
    } else {
        impl_->lastSyncFallbackSourceFrame_ = sourceFrame;
        impl_->lastSyncFallbackSucceeded_ = false;
        qWarning() << "[VideoLayer] decodeFrameToQImage failed"
                   << "timeline=" << clampedTimelineFrame
                   << "source=" << sourceFrame
                   << "backend=" << decoderBackendName(impl_->playbackController_.get())
                   << "backendLastError=" << impl_->playbackController_->getLastError()
                   << "controller=" << impl_->playbackController_->getDebugState()
                   << threadDiagnosticsTag();
    }
    return decoded;
}

bool ArtifactVideoLayer::isFrameCached(int64_t frameNumber) const
{
    return impl_->frameCache_.contains(timelineFrameToSourceFrame(this, frameNumber));
}

void ArtifactVideoLayer::preloadFrames(int64_t startFrame, int count)
{
    if (!impl_->isLoaded_ || impl_->opening_.load()) return;
    
    const int64_t startIdx = inPoint();
    const int64_t endIdx = outPoint();

    for (int i = 0; i < count; ++i) {
        int64_t frame = startFrame + i;
        const bool hasKnownFrameCount = impl_->streamInfo_.frameCount > 0;
        const int64_t sourceFrame = timelineFrameToSourceFrame(this, frame);
        if (frame >= startIdx && (endIdx < 0 || frame < endIdx) &&
            sourceFrame >= 0 &&
            (!hasKnownFrameCount || sourceFrame < impl_->streamInfo_.frameCount)) {
            if (!impl_->frameCache_.contains(sourceFrame)) {
                const QImage frameData = impl_->playbackController_->getVideoFrameAtFrameDirect(sourceFrame);
                if (!frameData.isNull()) {
                    impl_->frameCache_.put(sourceFrame, frameData);
                }
            }
        }
    }
    
    qDebug() << "[VideoLayer] Preloaded" << count << "frames from" << startFrame
             << "Cache size:" << impl_->frameCache_.size();
}

// === Proxy Workflow (Metadata Only) ===
// プロキシ生成は ProxyManager サービスの責務。レイヤーはメタデータのみ保持。

void ArtifactVideoLayer::setProxyQuality(ProxyQuality quality)
{
    impl_->proxyQuality_ = quality;
}

ProxyQuality ArtifactVideoLayer::proxyQuality() const
{
    return impl_->proxyQuality_;
}

bool ArtifactVideoLayer::hasProxy() const
{
    return !impl_->proxyPath_.isEmpty() && QFile::exists(impl_->proxyPath_);
}

QString ArtifactVideoLayer::proxyPath() const
{
    return impl_->proxyPath_;
}

bool ArtifactVideoLayer::generateProxy(ProxyQuality quality)
{
    // プロキシ生成は ProxyManager サービスに委譲
    qWarning() << "[VideoLayer] generateProxy() is deprecated. Use ArtifactProxyManager::instance()->generateProxy() instead.";
    return false;
}

void ArtifactVideoLayer::setProxyPath(const QString& path)
{
    impl_->proxyPath_ = path;
}

void ArtifactVideoLayer::clearProxy()
{
    impl_->proxyPath_.clear();
    impl_->proxyQuality_ = ProxyQuality::None;
    qDebug() << "[VideoLayer] Proxy cleared";
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
    if (!impl_->videoEnabled_ || !impl_->isLoaded_ || impl_->opening_.load()) return;
    const int64_t sourceFrame = currentSourceFrame(this);
    const int64_t timelineFrame = sourceFrameToTimelineFrame(this, sourceFrame);

    if (!impl_->hasCurrentFrameBuffer_ && impl_->currentQImage_.isNull()
        && !impl_->decoding_.load() && impl_->decodeFuture_.isFinished()) {
        qCDebug(videoLayerLog) << "[VideoLayer] draw fallback decode"
                               << "timeline=" << timelineFrame
                               << "source=" << sourceFrame
                               << "lastDecodedSource=" << impl_->lastDecodedFrame_
                               << "lastDecodedTimeline=" << sourceFrameToTimelineFrame(this, impl_->lastDecodedFrame_)
                               << threadIdTag();
        decodeCurrentFrame();
    }

    if (!impl_->hasCurrentFrameBuffer_) {
        if (!impl_->currentQImage_.isNull()) {
            impl_->currentFrameBuffer_ = toFrameBuffer(impl_->currentQImage_);
            impl_->hasCurrentFrameBuffer_ = true;
        }
    }

    if (!impl_->hasCurrentFrameBuffer_) return;

    auto size = sourceSize();
    if (size.width <= 0 || size.height <= 0) {
        size = Size_2D(impl_->currentFrameBuffer_.width(), impl_->currentFrameBuffer_.height());
    }
    const QMatrix4x4 baseTransform = getGlobalTransform4x4();
    drawWithClonerEffect(this, baseTransform, [renderer, size, this](const QMatrix4x4& transform, float weight) {
        renderer->drawSpriteTransformed(0.0f, 0.0f,
                                        static_cast<float>(size.width),
                                        static_cast<float>(size.height),
                                        transform, impl_->currentFrameBuffer_,
                                        this->opacity() * weight);
    });
}

void ArtifactVideoLayer::goToFrame(int64_t frameNumber)
{
    ArtifactAbstractLayer::goToFrame(frameNumber);
    // 先読みデコード：次の render tick より先にバックグラウンドデコードを起動しておく
    if (timelineFrameToSourceFrame(this, currentFrame()) != impl_->lastDecodedFrame_) {
        decodeCurrentFrame();
    }
}

bool ArtifactVideoLayer::getAudio(ArtifactCore::AudioSegment &outSegment, const FramePosition &start,
                                    int frameCount, int sampleRate)
{
    if (!hasAudio() || !impl_->isLoaded_ || frameCount <= 0 || sampleRate <= 0) return false;

    const int64_t requestedTimelineFrame = start.framePosition();
    const int64_t requestedSourceFrame = timelineFrameToSourceFrame(this, requestedTimelineFrame);
    if (requestedSourceFrame < 0) {
        return false;
    }

    bool shouldSeek = false;
    {
        std::lock_guard<std::mutex> lock(impl_->audioMutex_);
        if (impl_->lastAudioRequestTimelineFrame_ == std::numeric_limits<int64_t>::min() ||
            requestedTimelineFrame != impl_->lastAudioRequestTimelineFrame_) {
            impl_->audioBufferL_.clear();
            impl_->audioBufferR_.clear();
            impl_->lastAudioRequestTimelineFrame_ = requestedTimelineFrame;
            shouldSeek = true;
        }
    }

    if (shouldSeek && impl_->playbackController_) {
        impl_->playbackController_->seekToFrame(requestedSourceFrame);
    }

    if (impl_->playbackController_) {
        while (true) {
            size_t bufferedFrames = 0;
            {
                std::lock_guard<std::mutex> lock(impl_->audioMutex_);
                bufferedFrames = impl_->audioBufferL_.size();
                if (bufferedFrames >= static_cast<size_t>(frameCount)) {
                    break;
                }
            }

            QByteArray rawAudio = impl_->playbackController_->getNextAudioFrame();
            if (rawAudio.isEmpty()) {
                break;
            }

            const int16_t* samples = reinterpret_cast<const int16_t*>(rawAudio.constData());
            const int sampleCount = rawAudio.size() / sizeof(int16_t);

            std::lock_guard<std::mutex> lock(impl_->audioMutex_);
            for (int i = 0; i < sampleCount; i += 2) {
                impl_->audioBufferL_.push_back(samples[i] / 32768.0f);
                if (i + 1 < sampleCount) {
                    impl_->audioBufferR_.push_back(samples[i + 1] / 32768.0f);
                } else {
                    impl_->audioBufferR_.push_back(samples[i] / 32768.0f);
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(impl_->audioMutex_);
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

    impl_->lastAudioRequestTimelineFrame_ = requestedTimelineFrame + 1;

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

    auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type,
                           const QVariant& value, int priority = 0) {
        return persistentLayerProperty(name, type, value, priority);
    };

    videoGroup.addProperty(makeProp(QStringLiteral("video.sourcePath"),
                                    ArtifactCore::PropertyType::String, sourcePath(), -150));
    
    auto speedProp = makeProp(QStringLiteral("video.playbackSpeed"),
                              ArtifactCore::PropertyType::Float,
                              static_cast<double>(playbackSpeed()), -140);
    speedProp->setHardRange(0.1, 8.0);
    speedProp->setSoftRange(0.25, 2.0);
    speedProp->setStep(0.05);
    speedProp->setUnit(QStringLiteral("x"));
    videoGroup.addProperty(speedProp);
        
    videoGroup.addProperty(makeProp(QStringLiteral("video.loopEnabled"),
                                    ArtifactCore::PropertyType::Boolean,
                                    isLoopEnabled(), -130));
    
    auto volumeProp = makeProp(QStringLiteral("video.audioVolume"),
                               ArtifactCore::PropertyType::Float,
                               static_cast<double>(audioVolume()), -120);
    volumeProp->setHardRange(0.0, 1.0);
    volumeProp->setSoftRange(0.0, 1.0);
    volumeProp->setStep(0.01);
    volumeProp->setUnit(QStringLiteral("linear"));
    videoGroup.addProperty(volumeProp);
        
    videoGroup.addProperty(makeProp(QStringLiteral("video.audioMuted"),
                                    ArtifactCore::PropertyType::Boolean,
                                    isAudioMuted(), -110));
    videoGroup.addProperty(makeProp(QStringLiteral("video.audioEnabled"),
                                    ArtifactCore::PropertyType::Boolean,
                                    hasAudio(), -100));
    videoGroup.addProperty(makeProp(QStringLiteral("video.videoEnabled"),
                                    ArtifactCore::PropertyType::Boolean,
                                    hasVideo(), -90));
    
    auto proxyProp = makeProp(QStringLiteral("video.proxyQuality"),
                              ArtifactCore::PropertyType::Integer,
                              static_cast<qint64>(proxyQuality()), -80);
    proxyProp->setHardRange(0, 3);
    proxyProp->setTooltip(QStringLiteral("0=None, 1=Quarter, 2=Half, 3=Full"));
    videoGroup.addProperty(proxyProp);

    groups.push_back(videoGroup);
    return groups;
}

bool ArtifactVideoLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == QStringLiteral("video.sourcePath")) {
        const auto path = value.toString().trimmed();
        if (!path.isEmpty()) {
            setSourceFile(path);
        }
        return true;
    }
    if (propertyPath == QStringLiteral("video.playbackSpeed")) {
        setPlaybackSpeed(value.toDouble());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("video.loopEnabled")) {
        setLoopEnabled(value.toBool());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("video.audioVolume")) {
        setAudioVolume(value.toDouble());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("video.audioMuted")) {
        setAudioMuted(value.toBool());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("video.audioEnabled")) {
        setHasAudio(value.toBool());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("video.videoEnabled")) {
        setHasVideo(value.toBool());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("video.proxyQuality")) {
        setProxyQuality(static_cast<ProxyQuality>(value.toInt()));
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

} // namespace Artifact
