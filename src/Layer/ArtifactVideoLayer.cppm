module;
#include <QImage>
#include <QMatrix4x4>
#include <QSize>
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
#include <QRect>
#include <QRectF>
#include <QSizeF>
#include <opencv2/opencv.hpp>
#include <vulkan/vulkan_core.h>
#include <RenderDeviceVk.h>
#include <CommandQueueVk.h>
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
#include <cstring>
module Artifact.Layer.Video;




import Thread.Helper;
import Artifact.Composition.Abstract;
import Event.Bus;
import Artifact.Event.Types;
import CvUtils;
import Image.ImageF32x4_RGBA;
import Artifact.Layer.SourceCrop;
import Utils.String.UniString;
import Utils.Id;
import Property.Group;
import Property;
import MediaPlaybackController;

namespace Artifact {

Q_LOGGING_CATEGORY(videoLayerLog, "artifact.layer.video")

namespace {

void applyStereoPan(double pan, double& left, double& right)
{
    const double clamped = std::clamp(pan, -1.0, 1.0);
    const double angle = (clamped + 1.0) * 0.7853981633974483;
    left *= std::cos(angle);
    right *= std::sin(angle);
}

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
    return layer ? layer->currentSourceFrameValue() : 0;
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

    buffer.setFromCVMat(mat);
    return buffer;
}

QImage cpuVideoFrameToQImage(const ArtifactCore::CpuVideoFrame& frame)
{
    if (!frame.isValid() || frame.meta.width <= 0 || frame.meta.height <= 0 ||
        frame.strideBytes <= 0) {
        return QImage();
    }

    const size_t requiredBytes =
        static_cast<size_t>(frame.strideBytes) *
        static_cast<size_t>(frame.meta.height);
    if (frame.bytes.size() < requiredBytes) {
        qWarning() << "[VideoLayer] cpuVideoFrameToQImage rejected undersized buffer"
                   << "pixelFormat=" << static_cast<int>(frame.meta.pixelFormat)
                   << "width=" << frame.meta.width
                   << "height=" << frame.meta.height
                   << "strideBytes=" << frame.strideBytes
                   << "bytes=" << frame.bytes.size()
                   << "requiredBytes=" << requiredBytes;
        return QImage();
    }

    if (frame.meta.pixelFormat == ArtifactCore::VideoFramePixelFormat::RGBA32F) {
        cv::Mat wrapped(frame.meta.height, frame.meta.width, CV_32FC4,
                        const_cast<std::uint8_t*>(frame.bytes.data()),
                        frame.strideBytes);
        return ArtifactCore::CvUtils::cvMatToQImage(wrapped);
    }

    if (frame.meta.pixelFormat == ArtifactCore::VideoFramePixelFormat::BGRA8) {
        cv::Mat wrapped(frame.meta.height, frame.meta.width, CV_8UC4,
                        const_cast<std::uint8_t*>(frame.bytes.data()),
                        frame.strideBytes);
        return ArtifactCore::CvUtils::cvMatToQImage(wrapped);
    }

    const QImage::Format format =
        frame.meta.pixelFormat == ArtifactCore::VideoFramePixelFormat::RGB24
            ? QImage::Format_RGB888
            : QImage::Format_RGBA8888;
    QImage image(frame.meta.width, frame.meta.height, format);
    if (image.isNull()) {
        return QImage();
    }

    const int rowBytes = frame.strideBytes < static_cast<int>(image.bytesPerLine())
        ? frame.strideBytes
        : static_cast<int>(image.bytesPerLine());
    for (int y = 0; y < frame.meta.height; ++y) {
        std::memcpy(image.scanLine(y),
                    frame.bytes.data() + static_cast<size_t>(y) * static_cast<size_t>(frame.strideBytes),
                    static_cast<size_t>(rowBytes));
    }
    return image;
}

QImage decodedVideoFrameToQImage(const ArtifactCore::DecodedVideoFrame& decoded)
{
    if (const auto* cpu = std::get_if<ArtifactCore::CpuVideoFrame>(&decoded)) {
        return cpuVideoFrameToQImage(*cpu);
    }
    return QImage();
}

ArtifactCore::ImageF32x4_RGBA cpuVideoFrameToImageF32x4_RGBA(const ArtifactCore::CpuVideoFrame& frame)
{
    ArtifactCore::ImageF32x4_RGBA image;
    if (!frame.isValid() || frame.meta.width <= 0 || frame.meta.height <= 0) {
        return image;
    }

    if (frame.meta.pixelFormat == ArtifactCore::VideoFramePixelFormat::RGB24) {
        cv::Mat wrapped(frame.meta.height, frame.meta.width, CV_8UC3,
                        const_cast<std::uint8_t*>(frame.bytes.data()),
                        frame.strideBytes);
        cv::Mat converted;
        cv::cvtColor(wrapped, converted, cv::COLOR_RGB2BGRA);
        image.setFromCVMat(converted);
    }
    else if (frame.meta.pixelFormat == ArtifactCore::VideoFramePixelFormat::RGBA8) {
        cv::Mat wrapped(frame.meta.height, frame.meta.width, CV_8UC4,
                        const_cast<std::uint8_t*>(frame.bytes.data()),
                        frame.strideBytes);
        cv::Mat converted;
        cv::cvtColor(wrapped, converted, cv::COLOR_RGBA2BGRA);
        image.setFromCVMat(converted);
    }
    else if (frame.meta.pixelFormat == ArtifactCore::VideoFramePixelFormat::BGRA8) {
        cv::Mat wrapped(frame.meta.height, frame.meta.width, CV_8UC4,
                        const_cast<std::uint8_t*>(frame.bytes.data()),
                        frame.strideBytes);
        image.setFromCVMat(wrapped);
    }
    else if (frame.meta.pixelFormat == ArtifactCore::VideoFramePixelFormat::RGBA32F) {
        cv::Mat wrapped(frame.meta.height, frame.meta.width, CV_32FC4,
                        const_cast<std::uint8_t*>(frame.bytes.data()),
                        frame.strideBytes);
        image.setFromCVMat(wrapped);
    }
    else {
        QImage qimg = cpuVideoFrameToQImage(frame);
        if (!qimg.isNull()) {
            image = toFrameBuffer(qimg);
        }
    }
    return image;
}

ArtifactCore::ImageF32x4_RGBA decodedVideoFrameToImageF32x4_RGBA(const ArtifactCore::DecodedVideoFrame& decoded)
{
    if (const auto* cpu = std::get_if<ArtifactCore::CpuVideoFrame>(&decoded)) {
        return cpuVideoFrameToImageF32x4_RGBA(*cpu);
    }
    return ArtifactCore::ImageF32x4_RGBA();
}

ArtifactCore::GpuVideoFrame decodedVideoFrameToGpuFrame(const ArtifactCore::DecodedVideoFrame& decoded)
{
    if (const auto* gpu = std::get_if<ArtifactCore::GpuVideoFrame>(&decoded)) {
        return *gpu;
    }
    return {};
}

bool decodedVideoFrameHasGpuPayload(const ArtifactCore::DecodedVideoFrame& decoded)
{
    if (const auto* gpu = std::get_if<ArtifactCore::GpuVideoFrame>(&decoded)) {
        return gpu->isValid();
    }
    return false;
}

QRect sourceCropToRect(const Artifact::SourceCrop& crop, const QSize& sourceSize)
{
    if (!crop.enabled() || sourceSize.width() <= 0 || sourceSize.height() <= 0) {
        return {};
    }

    const QRectF cropRect = crop.effectiveCropRect(QSizeF(sourceSize));
    if (!cropRect.isValid() || cropRect.width() <= 0.0 || cropRect.height() <= 0.0) {
        return {};
    }

    return cropRect.toAlignedRect().intersected(
        QRect(0, 0, sourceSize.width(), sourceSize.height()));
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

    void put(int64_t frame, const ArtifactCore::ImageF32x4_RGBA& frameData) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(frame);
        if (it != cache_.end()) {
            it->second.data = frameData;
            touch(it);
            return;
        }

        while (cache_.size() >= maxFrames_ && !cacheOrder_.empty()) {
            const int64_t oldest = cacheOrder_.back();
            cacheOrder_.pop_back();
            cache_.erase(oldest);
        }

        cacheOrder_.push_front(frame);
        cache_.emplace(frame, CacheEntry{frameData, cacheOrder_.begin()});
    }

    bool get(int64_t frame, ArtifactCore::ImageF32x4_RGBA& outFrame) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(frame);
        if (it != cache_.end()) {
            outFrame = it->second.data;
            touch(it);
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
    struct CacheEntry {
        ArtifactCore::ImageF32x4_RGBA data;
        std::list<int64_t>::iterator orderIt;
    };

    mutable std::mutex mutex_;
    std::unordered_map<int64_t, CacheEntry> cache_;
    std::list<int64_t> cacheOrder_;
    size_t maxFrames_;

    void touch(std::unordered_map<int64_t, CacheEntry>::iterator it) {
        cacheOrder_.splice(cacheOrder_.begin(), cacheOrder_, it->second.orderIt);
        it->second.orderIt = cacheOrder_.begin();
    }
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
    double audioPan_ = 0.0;
    bool audioMuted_ = false;
    bool audioEnabled_ = true;
    bool videoEnabled_ = true;
    SourceCrop sourceCrop_;
    int motionTrackerId_ = 0;
    
    mutable std::mutex frameStateMutex_;
    ArtifactCore::ImageF32x4_RGBA currentFrameBuffer_;
    bool hasCurrentFrameBuffer_ = false;
    int64_t lastDecodedFrame_ = -1;
    int64_t currentTimelineFrame_ = 0;
    int64_t currentSourceFrame_ = 0;
    int64_t lastSyncFallbackSourceFrame_ = -1;
    bool lastSyncFallbackSucceeded_ = false;
    QString lastDecodeState_ = QStringLiteral("idle");
    bool vulkanDeviceConfigured_ = false;
    std::atomic<uint32_t> decodeGeneration_{0};

    Impl() : playbackController_(std::make_shared<ArtifactCore::MediaPlaybackController>()), frameCache_(120) {}
    ~Impl() {
        // バックグラウンドフューチャーが残っていても放置（デストラクタをブロックしない）
        // QFuture はスコープ離脱後も自動キャンセルされない点に注意
    }

    void cancelPendingDecode() {
        decodeGeneration_.fetch_add(1, std::memory_order_acq_rel);
        decoding_ = false;
        decodeTargetFrame_ = -1;
        decodeRetryPending_ = false;
        decodeFuture_ = QFuture<ArtifactCore::ImageF32x4_RGBA>();
    }

    // [Fix C] バックグラウンドデコード管理
    mutable QFuture<ArtifactCore::ImageF32x4_RGBA> decodeFuture_;
    mutable std::atomic<bool> decoding_{ false };
    mutable int64_t decodeTargetFrame_ = -1;
    mutable bool decodeRetryPending_ = false;
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
    if (path.isEmpty()) {
        qWarning() << "[VideoLayer] setSourceFile called with empty path"
                   << threadIdTag();
        return;
    }
    qDebug() << "[VideoLayer] setSourceFile" << QFileInfo(path).absoluteFilePath()
             << threadIdTag();
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
    qCInfo(videoLayerLog) << "[VideoLayer] load begin"
                          << "path=" << normalizedPath
                          << "thread=" << threadIdTag();

    impl_->sourcePath_ = normalizedPath;
    impl_->streamInfo_ = VideoStreamInfo{};
    {
        std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
        impl_->currentFrameBuffer_ = ArtifactCore::ImageF32x4_RGBA();
        impl_->hasCurrentFrameBuffer_ = false;
        impl_->lastDecodedFrame_ = -1;
    }
    impl_->currentTimelineFrame_ = 0;
    impl_->currentSourceFrame_ = 0;
    impl_->lastSyncFallbackSourceFrame_ = -1;
    impl_->lastSyncFallbackSucceeded_ = false;
    impl_->lastDecodeState_ = QStringLiteral("opening");
    impl_->vulkanDeviceConfigured_ = false;
    impl_->decodeTargetFrame_ = -1;
    impl_->decoding_ = false;
    impl_->decodeRetryPending_ = false;
    impl_->opening_ = true;
    impl_->isLoaded_ = false;
    impl_->lastAudioRequestTimelineFrame_ = std::numeric_limits<int64_t>::min();
    impl_->frameCache_.clear();
    setSourceSize(Size_2D(0, 0));
    impl_->sourceCrop_.clampToSource(QSizeF(0.0, 0.0));

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
            qCInfo(videoLayerLog) << "[VideoLayer] load failed"
                                  << "path=" << result.normalizedPath
                                  << "error=" << result.error
                                  << "requestId=" << requestId
                                  << "thread=" << threadIdTag();
            layer->impl_->isLoaded_ = false;
            layer->impl_->lastDecodeState_ = QStringLiteral("open-failed");
            publishVideoLayerModified(layer);
            return;
        }

        layer->impl_->playbackController_ = result.controller;
        layer->impl_->sourcePath_ = result.normalizedPath;
        layer->impl_->streamInfo_ = result.streamInfo;
        layer->impl_->isLoaded_ = true;
        {
            std::lock_guard<std::mutex> lock(layer->impl_->frameStateMutex_);
            layer->impl_->currentFrameBuffer_ = ArtifactCore::ImageF32x4_RGBA();
            layer->impl_->hasCurrentFrameBuffer_ = false;
            layer->impl_->lastDecodedFrame_ = -1;
        }
        layer->impl_->currentSourceFrame_ =
            timelineFrameToSourceFrame(layer, layer->impl_->currentTimelineFrame_);
        layer->impl_->lastSyncFallbackSourceFrame_ = -1;
        layer->impl_->lastSyncFallbackSucceeded_ = false;
        layer->impl_->lastDecodeState_ = QStringLiteral("decode-pending");
        layer->impl_->decodeTargetFrame_ = -1;
        layer->impl_->decoding_ = false;
        layer->impl_->decodeRetryPending_ = false;
        layer->impl_->lastAudioRequestTimelineFrame_ = std::numeric_limits<int64_t>::min();
        layer->impl_->frameCache_.clear();

        qCInfo(videoLayerLog) << "[VideoLayer] active backend:"
                              << decoderBackendName(layer->impl_->playbackController_.get())
                              << layer->impl_->playbackController_->getDebugState()
                              << threadDiagnosticsTag()
                              << threadIdTag();
        qCInfo(videoLayerLog) << "[VideoLayer] load success"
                              << "path=" << layer->impl_->sourcePath_
                              << "requestId=" << requestId
                              << "stream=" << layer->impl_->streamInfo_.width << "x"
                              << layer->impl_->streamInfo_.height
                              << "fps=" << layer->impl_->streamInfo_.frameRate
                              << "frames=" << layer->impl_->streamInfo_.frameCount
                              << "thread=" << threadIdTag();

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
            layer->impl_->sourceCrop_.clampToSource(
                QSizeF(layer->impl_->streamInfo_.width, layer->impl_->streamInfo_.height));
        }

        const int64_t initialSourceFrame = std::max<int64_t>(0, layer->impl_->currentSourceFrame_);
        layer->impl_->decoding_ = true;
        layer->impl_->decodeTargetFrame_ = initialSourceFrame;
        auto* ctrl = layer->impl_->playbackController_.get();
        layer->impl_->decodeFuture_ = QtConcurrent::run(&sharedBackgroundThreadPool(), [ctrl, layer, initialSourceFrame]() -> ArtifactCore::ImageF32x4_RGBA {
            ArtifactCore::ScopedThreadName threadName(QStringLiteral("VideoLayer/decode"));
            const ArtifactCore::ImageF32x4_RGBA frame =
                decodedVideoFrameToImageF32x4_RGBA(ctrl->getVideoFrameAtFrameDirectRaw(initialSourceFrame));
            if (frame.isEmpty()) {
                qWarning() << "[VideoLayer] initial decode FAILED"
                           << "source=" << initialSourceFrame
                           << "backend=" << decoderBackendName(ctrl)
                           << "controller=" << ctrl->getDebugState()
                           << threadDiagnosticsTag();
                layer->impl_->lastDecodeState_ = QStringLiteral("decode-failed");
            } else {
                layer->impl_->frameCache_.put(initialSourceFrame, frame);
                {
                    std::lock_guard<std::mutex> lock(layer->impl_->frameStateMutex_);
                    layer->impl_->currentFrameBuffer_ = frame;
                    layer->impl_->hasCurrentFrameBuffer_ = true;
                    layer->impl_->lastDecodedFrame_ = initialSourceFrame;
                }
                layer->impl_->lastDecodeState_ = QStringLiteral("ready");
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

void ArtifactVideoLayer::setStreamFrameRate(double fps)
{
    if (!impl_->isLoaded_) return;
    impl_->streamInfo_.frameRate = fps;
    {
        std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
        impl_->currentFrameBuffer_ = ArtifactCore::ImageF32x4_RGBA();
        impl_->hasCurrentFrameBuffer_ = false;
        impl_->lastDecodedFrame_ = -1;
    }
    impl_->decodeTargetFrame_ = -1;
    impl_->lastDecodeState_ = QStringLiteral("rate_changed");
    Q_EMIT changed();
}

QString ArtifactVideoLayer::debugState() const
{
    const auto* controller = impl_ ? impl_->playbackController_.get() : nullptr;
    const int64_t timelineFrame = impl_ ? impl_->currentTimelineFrame_ : 0;
    const int64_t sourceFrame = currentSourceFrame(this);
    QSize currentBufferSize;
    int64_t lastDecodedFrame = -1;
    bool hasCurrentFrameBuffer = false;
    if (impl_) {
        std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
        currentBufferSize = QSize(impl_->currentFrameBuffer_.width(), impl_->currentFrameBuffer_.height());
        lastDecodedFrame = impl_->lastDecodedFrame_;
        hasCurrentFrameBuffer = impl_->hasCurrentFrameBuffer_;
    }
    return QStringLiteral(
        "state=%1 loaded=%2 opening=%3 decoding=%4 timeline=%5 source=%6 "
        "decodeTarget=%7 lastDecoded=%8 cachedFrames=%9 "
        "hasBuffer=%10 bufferSize=%11 syncFallbackFrame=%12 syncFallbackOk=%13 "
        "backend=%14 lastError=%15 detail={%16} bounds={%17}")
        .arg(impl_ ? impl_->lastDecodeState_ : QStringLiteral("idle"))
        .arg(impl_ && impl_->isLoaded_ ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(impl_ && impl_->opening_.load() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(impl_ && impl_->decoding_.load() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(timelineFrame)
        .arg(sourceFrame)
        .arg(impl_ ? impl_->decodeTargetFrame_ : -1)
        .arg(lastDecodedFrame)
        .arg(impl_ ? impl_->frameCache_.size() : 0)
        .arg(hasCurrentFrameBuffer ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(currentBufferSize.isValid() ? QStringLiteral("%1x%2").arg(currentBufferSize.width()).arg(currentBufferSize.height()) : QStringLiteral("0x0"))
        .arg(impl_ ? impl_->lastSyncFallbackSourceFrame_ : -1)
        .arg(impl_ && impl_->lastSyncFallbackSucceeded_ ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(decoderBackendName(controller))
        .arg(controller ? controller->getLastError() : QStringLiteral("<no controller>"))
        .arg(controller ? controller->getDebugState() : QStringLiteral("<no controller>"))
        .arg(contentBoundsSummary());
}

QString ArtifactVideoLayer::decodeState() const
{
    const auto* controller = impl_ ? impl_->playbackController_.get() : nullptr;
    const int64_t sourceFrame = currentSourceFrame(this);
    int64_t lastDecodedFrame = -1;
    bool hasCurrentFrameBuffer = false;
    if (impl_) {
        std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
        lastDecodedFrame = impl_->lastDecodedFrame_;
        hasCurrentFrameBuffer = impl_->hasCurrentFrameBuffer_;
    }
    return QStringLiteral("state=%1 loaded=%2 opening=%3 decoding=%4 source=%5 target=%6 last=%7 hasBuffer=%8 syncOk=%9 backend=%10 err=%11")
        .arg(impl_ ? impl_->lastDecodeState_ : QStringLiteral("idle"))
        .arg(impl_ && impl_->isLoaded_ ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(impl_ && impl_->opening_.load() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(impl_ && impl_->decoding_.load() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(sourceFrame)
        .arg(impl_ ? impl_->decodeTargetFrame_ : -1)
        .arg(lastDecodedFrame)
        .arg(hasCurrentFrameBuffer ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(impl_ && impl_->lastSyncFallbackSucceeded_ ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(decoderBackendName(controller))
        .arg(controller ? controller->getLastError() : QStringLiteral("<no controller>"));
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
    
    goToFrame(frame);
    
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
        impl_->cancelPendingDecode();
        impl_->playbackController_->seekToFrame(sourceFrame);
        decodeCurrentFrame();
    }
}

void ArtifactVideoLayer::stop()
{
    if (!impl_) {
        return;
    }

    impl_->cancelPendingDecode();
    {
        std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
        impl_->currentFrameBuffer_ = ArtifactCore::ImageF32x4_RGBA();
        impl_->hasCurrentFrameBuffer_ = false;
        impl_->lastDecodedFrame_ = -1;
        impl_->lastDecodeState_ = QStringLiteral("stopped");
    }
    {
        std::lock_guard<std::mutex> lock(impl_->audioMutex_);
        impl_->audioBufferL_.clear();
        impl_->audioBufferR_.clear();
        impl_->lastAudioRequestTimelineFrame_ = std::numeric_limits<int64_t>::min();
    }
    if (impl_->playbackController_) {
        impl_->playbackController_->stop();
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

int64_t ArtifactVideoLayer::currentSourceFrameValue() const
{
    return impl_ ? impl_->currentSourceFrame_ : 0;
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
    std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
    return impl_->hasCurrentFrameBuffer_;
}

// === Frame Decoding ===
void ArtifactVideoLayer::decodeCurrentFrame()
{
    if (!impl_->isLoaded_) {
        impl_->lastDecodeState_ = QStringLiteral("not-loaded");
        return;
    }
    if (impl_->opening_.load()) {
        impl_->lastDecodeState_ = QStringLiteral("opening");
        return;
    }

    const int64_t sourceFrame = currentSourceFrame(this);
    const int64_t timelineFrame = impl_->currentTimelineFrame_;

    // 同じフレームでキャッシュ済みの場合はスキップ
    {
        std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
        if (sourceFrame == impl_->lastDecodedFrame_ && impl_->hasCurrentFrameBuffer_) {
            impl_->lastDecodeState_ = QStringLiteral("cached");
            return;
        }
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
        impl_->lastDecodeState_ = QStringLiteral("range-rejected");
        return;
    }

    // キャッシュに存在すれば即座に返す。再生中に別フレームの decode が
    // 走っていても、目的フレームが既にあるなら stale buffer に留まらない。
    ArtifactCore::ImageF32x4_RGBA cachedFrame;
    if (impl_->frameCache_.get(sourceFrame, cachedFrame)) {
        {
            std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
            impl_->currentFrameBuffer_ = cachedFrame;
            impl_->hasCurrentFrameBuffer_ = true;
            impl_->lastDecodedFrame_ = sourceFrame;
        }
        impl_->lastDecodeState_ = QStringLiteral("cached");
        return;
    }

// decoding_ は lambda 内の return 直前にクリアされるため isFinished() も合わせて確認する
    const bool inFlight = impl_->decoding_.load()
        || (impl_->decodeTargetFrame_ >= 0 && !impl_->decodeFuture_.isFinished());
    if (inFlight) {
        // If another frame is being decoded, cancel it if it's not our target
        if (impl_->decodeTargetFrame_ != sourceFrame) {
            impl_->cancelPendingDecode();
            impl_->lastDecodeState_ = QStringLiteral("decode-preempted");
        } else {
            impl_->decodeRetryPending_ = true;
            impl_->lastDecodeState_ = QStringLiteral("decode-pending");
        }
        return;
    }

    bool hasBufferForLog = false;
    {
        std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
        hasBufferForLog = impl_->hasCurrentFrameBuffer_;
    }

    qCDebug(videoLayerLog)
        << "[VideoLayerT] decodeCurrentFrame starting bg decode"
        << "timeline=" << timelineFrame
        << "source=" << sourceFrame
        << "hasBuffer=" << hasBufferForLog
        << "inPoint=" << inPoint()
        << "outPoint=" << outPoint()
        << "backend=" << decoderBackendName(impl_->playbackController_.get())
        << threadIdTag();

    // バックグラウンドでデコードを開始し、メインスレッドをブロックしない
    impl_->decoding_ = true;
    impl_->decodeRetryPending_ = false;
    impl_->decodeTargetFrame_ = sourceFrame;
    impl_->lastDecodeState_ = QStringLiteral("decode-pending");
    auto* ctrl = impl_->playbackController_.get();
    const QString backendName = decoderBackendName(ctrl);
    const uint32_t decodeGeneration =
        impl_->decodeGeneration_.load(std::memory_order_acquire);
    impl_->decodeFuture_ = QtConcurrent::run(
        &sharedBackgroundThreadPool(),
        [ctrl, timelineFrame, sourceFrame, backendName, decodeGeneration,
         this]() -> ArtifactCore::ImageF32x4_RGBA {
        ArtifactCore::ScopedThreadName threadName(QStringLiteral("VideoLayer/decode"));
        const ArtifactCore::DecodedVideoFrame rawDecoded =
            ctrl->getVideoFrameAtFrameDirectRaw(sourceFrame);
        const ArtifactCore::ImageF32x4_RGBA decoded =
            decodedVideoFrameToImageF32x4_RGBA(rawDecoded);
        if (decodeGeneration !=
            impl_->decodeGeneration_.load(std::memory_order_acquire)) {
            return ArtifactCore::ImageF32x4_RGBA();
        }
        if (!decoded.isEmpty()) {
            impl_->frameCache_.put(sourceFrame, decoded);
            {
                std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
                impl_->currentFrameBuffer_ = decoded;
                impl_->hasCurrentFrameBuffer_ = true;
                impl_->lastDecodedFrame_ = sourceFrame;
            }
            impl_->lastDecodeState_ = QStringLiteral("ready");
            qCDebug(videoLayerLog) << "[VideoLayer] bg decoded frame"
                                   << "timeline=" << timelineFrame
                                   << "source=" << sourceFrame
                                   << "size=" << decoded.width() << "x" << decoded.height()
                                   << threadIdTag();
        } else if (decodedVideoFrameHasGpuPayload(rawDecoded)) {
            impl_->lastDecodeState_ = QStringLiteral("gpu-ready");
        } else {
            impl_->lastDecodeState_ = QStringLiteral("decode-failed");
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
        QPointer<ArtifactVideoLayer> guarded(this);
        QMetaObject::invokeMethod(
            this,
            [guarded]() {
                if (!guarded || !guarded->impl_) {
                    return;
                }
                publishVideoLayerModified(guarded.data());
                bool shouldRetry = false;
                {
                    std::lock_guard<std::mutex> lock(guarded->impl_->frameStateMutex_);
                    shouldRetry = guarded->impl_->decodeRetryPending_
                        && currentSourceFrame(guarded.data()) != guarded->impl_->lastDecodedFrame_;
                }
                if (shouldRetry) {
                    guarded->decodeCurrentFrame();
                }
            },
            Qt::QueuedConnection);
        return decoded;
    });
}

QImage ArtifactVideoLayer::currentFrameToQImage() const
{
    const ArtifactCore::ImageF32x4_RGBA frame = currentFrameImageBuffer();
    return frame.isEmpty() ? QImage() : frame.toQImage();
}

ArtifactCore::ImageF32x4_RGBA ArtifactVideoLayer::currentFrameImageBuffer() const
{
    if (impl_->opening_.load()) {
        impl_->lastDecodeState_ = QStringLiteral("opening");
        std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
        return impl_->currentFrameBuffer_;
    }

    const int64_t sourceFrame = currentSourceFrame(this);

    // decoding_ は lambda 内の return 直前にクリアされるため isFinished() も合わせて確認する
    const bool inFlight = impl_->decoding_.load()
        || (impl_->decodeTargetFrame_ >= 0 && !impl_->decodeFuture_.isFinished());

    // 完了したバックグラウンドデコード結果を取り込む
    if (!inFlight && impl_->decodeTargetFrame_ >= 0) {
        ArtifactCore::ImageF32x4_RGBA loaded = impl_->decodeFuture_.result();
        if (!loaded.isEmpty()) {
            impl_->lastDecodeState_ = QStringLiteral("ready");
            bool updateSourceSize = false;
            {
                std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
                if (impl_->decodeTargetFrame_ != impl_->lastDecodedFrame_) {
                    impl_->currentFrameBuffer_ = loaded;
                    impl_->hasCurrentFrameBuffer_ = true;
                    impl_->lastDecodedFrame_ = impl_->decodeTargetFrame_;
                    updateSourceSize = impl_->streamInfo_.width <= 0 || impl_->streamInfo_.height <= 0;
                }
            }
            if (updateSourceSize) {
                if (impl_->streamInfo_.width <= 0 || impl_->streamInfo_.height <= 0) {
                    impl_->streamInfo_.width  = loaded.width();
                    impl_->streamInfo_.height = loaded.height();
                    const_cast<ArtifactVideoLayer*>(this)->setSourceSize(
                        Size_2D(loaded.width(), loaded.height()));
                    const_cast<ArtifactVideoLayer*>(this)->impl_->sourceCrop_.clampToSource(
                        QSizeF(loaded.width(), loaded.height()));
                }
            }
        } else {
            if (impl_->lastDecodeState_ != QStringLiteral("gpu-ready")) {
                qWarning() << "[VideoLayer] async decode null frame"
                           << "timeline=" << sourceFrameToTimelineFrame(this, impl_->decodeTargetFrame_)
                           << "source=" << impl_->decodeTargetFrame_
                           << "backend=" << decoderBackendName(impl_->playbackController_.get())
                           << "backendLastError=" << impl_->playbackController_->getLastError()
                           << "controller=" << impl_->playbackController_->getDebugState()
                           << threadDiagnosticsTag();
                impl_->lastDecodeState_ = QStringLiteral("sync-fallback-miss");
            }
        }
        impl_->decodeTargetFrame_ = -1;
    }

    // 現在フレームがまだデコードされていなければ非同期デコードを起動する
    bool needsDecode = false;
    {
        std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
        needsDecode = sourceFrame != impl_->lastDecodedFrame_;
    }
    if (!inFlight && needsDecode) {
        impl_->lastDecodeState_ = QStringLiteral("decode-pending");
        const_cast<ArtifactVideoLayer*>(this)->decodeCurrentFrame();
    }

    std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
    return impl_->currentFrameBuffer_;
}

QImage ArtifactVideoLayer::decodeFrameToQImage(int64_t frameNumber) const
{
    const ArtifactCore::ImageF32x4_RGBA frame = decodeFrameToImageBuffer(frameNumber);
    return frame.isEmpty() ? QImage() : frame.toQImage();
}

ArtifactCore::ImageF32x4_RGBA ArtifactVideoLayer::cachedFrameImageBuffer(int64_t frameNumber) const
{
    ArtifactCore::ImageF32x4_RGBA frame;
    if (!impl_) {
        return frame;
    }

    impl_->frameCache_.get(timelineFrameToSourceFrame(this, frameNumber), frame);
    return frame;
}

ArtifactCore::ImageF32x4_RGBA ArtifactVideoLayer::decodeFrameToImageBuffer(int64_t frameNumber) const
{
    if (!impl_->isLoaded_) {
        impl_->lastDecodeState_ = QStringLiteral("not-loaded");
        return ArtifactCore::ImageF32x4_RGBA();
    }
    if (impl_->opening_.load()) {
        impl_->lastDecodeState_ = QStringLiteral("opening");
        return ArtifactCore::ImageF32x4_RGBA();
    }

    if (!impl_->playbackController_ || !impl_->playbackController_->isMediaOpen()) {
        return ArtifactCore::ImageF32x4_RGBA();
    }
    
    const int64_t sourceFrame = timelineFrameToSourceFrame(this, frameNumber);
    if (sourceFrame < 0 || (impl_->streamInfo_.frameCount > 0 && sourceFrame >= impl_->streamInfo_.frameCount)) {
        return ArtifactCore::ImageF32x4_RGBA();
    }

    // キャッシュにあれば返す
    ArtifactCore::ImageF32x4_RGBA frame;
    if (impl_->frameCache_.get(sourceFrame, frame)) {
        impl_->lastDecodeState_ = QStringLiteral("cached");
        if (sourceFrame == currentSourceFrame(this)) {
            std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
            impl_->currentFrameBuffer_ = frame;
            impl_->hasCurrentFrameBuffer_ = true;
            impl_->lastDecodedFrame_ = sourceFrame;
        }
        return frame;
    }

    {
        std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
        if (sourceFrame == impl_->lastDecodedFrame_ && impl_->hasCurrentFrameBuffer_) {
            impl_->lastDecodeState_ = QStringLiteral("ready");
            return impl_->currentFrameBuffer_;
        }
    }
    if (impl_->decoding_.load()) {
        impl_->lastDecodeState_ = QStringLiteral("decode-pending");
        return ArtifactCore::ImageF32x4_RGBA();
    }

    const ArtifactCore::DecodedVideoFrame rawDecoded =
        impl_->playbackController_->getVideoFrameAtFrameDirectRaw(sourceFrame);
    const ArtifactCore::ImageF32x4_RGBA decoded =
        decodedVideoFrameToImageF32x4_RGBA(rawDecoded);
    if (!decoded.isEmpty()) {
        impl_->frameCache_.put(sourceFrame, decoded);
        if (sourceFrame == currentSourceFrame(this)) {
            std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
            impl_->currentFrameBuffer_ = decoded;
            impl_->hasCurrentFrameBuffer_ = true;
            impl_->lastDecodedFrame_ = sourceFrame;
        }
        impl_->lastDecodeState_ = QStringLiteral("sync-fallback-ready");
        return decoded;
    } else if (decodedVideoFrameHasGpuPayload(rawDecoded)) {
        impl_->lastDecodeState_ = QStringLiteral("gpu-ready");
        return ArtifactCore::ImageF32x4_RGBA();
    } else {
        impl_->lastDecodeState_ = QStringLiteral("sync-fallback-miss");
        qWarning() << "[VideoLayer] decodeFrameToImageBuffer failed"
                   << "source=" << sourceFrame
                   << "backend=" << decoderBackendName(impl_->playbackController_.get())
                   << "backendLastError=" << impl_->playbackController_->getLastError()
                   << "controller=" << impl_->playbackController_->getDebugState()
                   << threadDiagnosticsTag();
    }
    return decoded;
}

ArtifactCore::GpuVideoFrame ArtifactVideoLayer::decodeFrameToGpuFrame(int64_t frameNumber) const
{
    if (!impl_->isLoaded_ || impl_->opening_.load()) {
        return {};
    }
    if (!impl_->playbackController_ || !impl_->playbackController_->isMediaOpen()) {
        return {};
    }

    const int64_t sourceFrame = timelineFrameToSourceFrame(this, frameNumber);
    if (sourceFrame < 0 || (impl_->streamInfo_.frameCount > 0 && sourceFrame >= impl_->streamInfo_.frameCount)) {
        return {};
    }

    return decodedVideoFrameToGpuFrame(
        impl_->playbackController_->getVideoFrameAtFrameDirectRaw(sourceFrame));
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
                const ArtifactCore::ImageF32x4_RGBA frameData = decodedVideoFrameToImageF32x4_RGBA(
                    impl_->playbackController_->getVideoFrameAtFrameDirectRaw(sourceFrame));
                if (!frameData.isEmpty()) {
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
    impl_->audioVolume_ = std::max(0.0, std::min(volume, 2.0));
}

void ArtifactVideoLayer::setAudioPan(double pan)
{
    impl_->audioPan_ = std::max(-1.0, std::min(pan, 1.0));
}

double ArtifactVideoLayer::audioPan() const
{
    return impl_->audioPan_;
}

void ArtifactVideoLayer::setAudioMuted(bool muted)
{
    impl_->audioMuted_ = muted;
}

bool ArtifactVideoLayer::isAudioMuted() const
{
    return impl_->audioMuted_;
}

int ArtifactVideoLayer::motionTrackerId() const
{
    return impl_->motionTrackerId_;
}

void ArtifactVideoLayer::setMotionTrackerId(int trackerId)
{
    impl_->motionTrackerId_ = std::max(0, trackerId);
}

// === Serialization ===
QJsonObject ArtifactVideoLayer::toJson() const
{
    QJsonObject obj;
    obj["type"] = static_cast<int>(LayerType::Video);
    obj["layerType"] = QStringLiteral("Video");
    obj["sourcePath"] = impl_->sourcePath_;
    obj["video.sourcePath"] = impl_->sourcePath_;
    obj["inPoint"] = static_cast<qint64>(inPoint());
    obj["outPoint"] = static_cast<qint64>(outPoint());
    obj["playbackSpeed"] = impl_->playbackSpeed_;
    obj["video.playbackSpeed"] = impl_->playbackSpeed_;
    obj["loopEnabled"] = impl_->loopEnabled_;
    obj["video.loopEnabled"] = impl_->loopEnabled_;
    obj["audioVolume"] = impl_->audioVolume_;
    obj["video.audioVolume"] = impl_->audioVolume_;
    obj["audioPan"] = impl_->audioPan_;
    obj["video.audioPan"] = impl_->audioPan_;
    obj["audioMuted"] = impl_->audioMuted_;
    obj["video.audioMuted"] = impl_->audioMuted_;
    obj["audioEnabled"] = impl_->audioEnabled_;
    obj["video.audioEnabled"] = impl_->audioEnabled_;
    obj["videoEnabled"] = impl_->videoEnabled_;
    obj["video.videoEnabled"] = impl_->videoEnabled_;
    obj["proxyQuality"] = static_cast<int>(impl_->proxyQuality_);
    obj["video.proxyQuality"] = static_cast<int>(impl_->proxyQuality_);
    obj["proxyPath"] = impl_->proxyPath_;
    obj["video.proxyPath"] = impl_->proxyPath_;
    obj["video.width"] = impl_->streamInfo_.width;
    obj["video.height"] = impl_->streamInfo_.height;
    obj["video.frameRate"] = impl_->streamInfo_.frameRate;
    obj["video.hasAudio"] = impl_->streamInfo_.hasAudio;
    obj["video.hasVideo"] = impl_->videoEnabled_;
    obj["sourceCrop.enabled"] = impl_->sourceCrop_.enabled();
    obj["sourceCrop.cropX"] = impl_->sourceCrop_.cropRect().x();
    obj["sourceCrop.cropY"] = impl_->sourceCrop_.cropRect().y();
    obj["sourceCrop.cropWidth"] = impl_->sourceCrop_.cropRect().width();
    obj["sourceCrop.cropHeight"] = impl_->sourceCrop_.cropRect().height();
    obj["sourceCrop.panX"] = impl_->sourceCrop_.pan().x();
    obj["sourceCrop.panY"] = impl_->sourceCrop_.pan().y();
    obj["sourceCrop.zoom"] = impl_->sourceCrop_.zoom();
    obj["sourceCrop.rotation"] = impl_->sourceCrop_.rotation();
    obj["sourceCrop.anchorX"] = impl_->sourceCrop_.anchor().x();
    obj["sourceCrop.anchorY"] = impl_->sourceCrop_.anchor().y();
    obj["sourceCrop.preserveAspect"] = impl_->sourceCrop_.preserveAspect();
    obj["sourceCrop"] = impl_->sourceCrop_.toJson();
    obj["tracking.motionTrackerId"] = impl_->motionTrackerId_;
    obj["motionTrackerId"] = impl_->motionTrackerId_;
    
    // Store layer name
    obj["layerName"] = layerName();
    
    return obj;
}

std::shared_ptr<ArtifactVideoLayer> ArtifactVideoLayer::fromJson(const QJsonObject& obj)
{
    auto layer = std::make_shared<ArtifactVideoLayer>();
    
    const QString sourcePath = obj.contains("video.sourcePath")
        ? obj["video.sourcePath"].toString()
        : obj.value("sourcePath").toString();
    if (!sourcePath.isEmpty()) {
        layer->loadFromPath(sourcePath);
    }
    
    if (obj.contains("inPoint")) {
        layer->setInPoint(obj["inPoint"].toVariant().toLongLong());
    }
    if (obj.contains("outPoint")) {
        layer->setOutPoint(obj["outPoint"].toVariant().toLongLong());
    }
    if (obj.contains("video.playbackSpeed") || obj.contains("playbackSpeed")) {
        layer->setPlaybackSpeed(obj.value("video.playbackSpeed").toDouble(obj.value("playbackSpeed").toDouble(1.0)));
    }
    if (obj.contains("video.loopEnabled") || obj.contains("loopEnabled")) {
        layer->setLoopEnabled(obj.value("video.loopEnabled").toBool(obj.value("loopEnabled").toBool(true)));
    }
    if (obj.contains("video.audioVolume") || obj.contains("audioVolume")) {
        layer->setAudioVolume(obj.value("video.audioVolume").toDouble(obj.value("audioVolume").toDouble(1.0)));
    }
    if (obj.contains("video.audioPan") || obj.contains("audioPan")) {
        layer->setAudioPan(obj.value("video.audioPan").toDouble(obj.value("audioPan").toDouble(0.0)));
    }
    if (obj.contains("video.audioMuted") || obj.contains("audioMuted")) {
        layer->setAudioMuted(obj.value("video.audioMuted").toBool(obj.value("audioMuted").toBool(false)));
    }
    if (obj.contains("video.audioEnabled") || obj.contains("audioEnabled")) {
        layer->setHasAudio(obj.value("video.audioEnabled").toBool(obj.value("audioEnabled").toBool(true)));
    }
    if (obj.contains("video.videoEnabled") || obj.contains("videoEnabled")) {
        layer->setHasVideo(obj.value("video.videoEnabled").toBool(obj.value("videoEnabled").toBool(true)));
    }
    if (obj.contains("video.proxyQuality") || obj.contains("proxyQuality")) {
        layer->setProxyQuality(static_cast<ProxyQuality>(obj.value("video.proxyQuality").toInt(obj.value("proxyQuality").toInt(0))));
    }
    if (obj.contains("video.proxyPath") || obj.contains("proxyPath")) {
        layer->setProxyPath(obj.value("video.proxyPath").toString(obj.value("proxyPath").toString()));
    }
    if (obj.contains("sourceCrop") && obj.value("sourceCrop").isObject()) {
        layer->impl_->sourceCrop_.fromJson(obj.value("sourceCrop").toObject());
        layer->impl_->sourceCrop_.clampToSource(
            QSizeF(layer->impl_->streamInfo_.width, layer->impl_->streamInfo_.height));
    }
    if (obj.contains("tracking.motionTrackerId") || obj.contains("motionTrackerId")) {
        layer->setMotionTrackerId(
            obj.value("tracking.motionTrackerId").toInt(obj.value("motionTrackerId").toInt(0)));
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
    if (renderer && !impl_->vulkanDeviceConfigured_ && impl_->playbackController_) {
        auto device = renderer->device();
        auto context = renderer->immediateContext();
        if (device && context && device->GetDeviceInfo().Type == Diligent::RENDER_DEVICE_TYPE_VULKAN) {
            auto commandQueue = context->LockCommandQueue();
            if (commandQueue) {
                Diligent::RefCntAutoPtr<Diligent::ICommandQueueVk> queueVk{commandQueue, Diligent::IID_CommandQueueVk};
                if (queueVk) {
                    auto deviceVk = Diligent::RefCntAutoPtr<Diligent::IRenderDeviceVk>{device, Diligent::IID_RenderDeviceVk};
                    if (deviceVk) {
                        impl_->playbackController_->setVulkanDevice(
                            deviceVk->GetVkInstance(),
                            deviceVk->GetVkPhysicalDevice(),
                            deviceVk->GetVkDevice(),
                            queueVk->GetQueueFamilyIndex());
                        impl_->vulkanDeviceConfigured_ = true;
                    }
                }
                context->UnlockCommandQueue();
            }
        }
    }
    const int64_t sourceFrame = currentSourceFrame(this);
    const int64_t timelineFrame = impl_->currentTimelineFrame_;

    const bool hasFrameBuffer = hasCurrentFrameBuffer();
    if (!hasFrameBuffer && !impl_->decoding_.load() && impl_->decodeFuture_.isFinished()) {
        int64_t lastDecodedFrame = -1;
        {
            std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
            lastDecodedFrame = impl_->lastDecodedFrame_;
        }
        qCDebug(videoLayerLog) << "[VideoLayer] draw fallback decode"
                               << "timeline=" << timelineFrame
                               << "source=" << sourceFrame
                               << "lastDecodedSource=" << lastDecodedFrame
                               << "lastDecodedTimeline=" << sourceFrameToTimelineFrame(this, lastDecodedFrame)
                               << threadIdTag();
        decodeCurrentFrame();
    }

    ArtifactCore::ImageF32x4_RGBA frameBuffer = cachedFrameImageBuffer(timelineFrame);
    if (frameBuffer.isEmpty()) {
        frameBuffer = currentFrameImageBuffer();
    }
    if (frameBuffer.isEmpty()) return;

    auto size = sourceSize();
    if (size.width <= 0 || size.height <= 0) {
        size = Size_2D(frameBuffer.width(), frameBuffer.height());
    }
    const QRect cropRect = sourceCropToRect(impl_->sourceCrop_, QSize(size.width, size.height));
    const bool useCrop = cropRect.isValid() && cropRect.width() > 0 && cropRect.height() > 0;
    if (useCrop) {
        size = Size_2D(cropRect.width(), cropRect.height());
    }
    const QMatrix4x4 baseTransform = getGlobalTransform4x4();
    const ArtifactCore::ImageF32x4_RGBA renderBuffer =
        useCrop ? frameBuffer.crop(cropRect.x(), cropRect.y(), cropRect.width(), cropRect.height())
                : frameBuffer;
    drawWithClonerEffect(this, baseTransform, [renderer, renderBuffer, size, this](const QMatrix4x4& transform, float weight) {
        renderer->drawSpriteTransformed(0.0f, 0.0f,
                                        static_cast<float>(size.width),
                                        static_cast<float>(size.height),
                                        transform, renderBuffer,
                                        this->opacity() * weight);
    });
}

QRectF ArtifactVideoLayer::localBounds() const
{
    auto size = sourceSize();
    if (size.width <= 0 || size.height <= 0) {
        const ArtifactCore::ImageF32x4_RGBA frameBuffer = currentFrameImageBuffer();
        size = Size_2D(frameBuffer.width(), frameBuffer.height());
    }
    if (size.width <= 0 || size.height <= 0) {
        return QRectF();
    }

    const QRect cropRect = sourceCropToRect(impl_->sourceCrop_, QSize(size.width, size.height));
    if (cropRect.isValid() && cropRect.width() > 0 && cropRect.height() > 0) {
        return QRectF(0.0, 0.0, static_cast<qreal>(cropRect.width()), static_cast<qreal>(cropRect.height()));
    }

    return QRectF(0.0, 0.0, static_cast<qreal>(size.width), static_cast<qreal>(size.height));
}

void ArtifactVideoLayer::goToFrame(int64_t frameNumber)
{
    impl_->currentTimelineFrame_ = frameNumber;
    ArtifactAbstractLayer::goToFrame(frameNumber);
    impl_->currentSourceFrame_ = timelineFrameToSourceFrame(this, frameNumber);
    // 先読みデコード：次の render tick より先にバックグラウンドデコードを起動しておく
    bool needsDecode = false;
    {
        std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
        needsDecode = impl_->currentSourceFrame_ != impl_->lastDecodedFrame_;
    }
    if (needsDecode) {
        decodeCurrentFrame();
    }
    // 再生中の先行デコード（Look-ahead prefetch）
    if (impl_->playbackSpeed_ > 1.0 && impl_->playbackController_ && needsDecode) {
        const int64_t currentSource = impl_->currentSourceFrame_;
        const int64_t nextSource = currentSource + 2; // 2フレーム先をプリフェッチ
        if (nextSource < impl_->streamInfo_.frameCount) {
            // 非同期で次フレームを先読み
            QtConcurrent::run(&sharedBackgroundThreadPool(), 
                [this, nextSource]() {
                    if (!impl_->playbackController_ || !impl_->isLoaded_) return;
                    const auto rawFrame = impl_->playbackController_->getVideoFrameAtFrameDirectRaw(nextSource);
                    const auto decoded = decodedVideoFrameToImageF32x4_RGBA(rawFrame);
                    if (!decoded.isEmpty()) {
                        impl_->frameCache_.put(nextSource, decoded);
                    }
                });
        }
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
        applyStereoPan(impl_->audioPan_, outSegment.channelData[0][i], outSegment.channelData[1][i]);
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
    volumeProp->setHardRange(0.0, 2.0);
    volumeProp->setSoftRange(0.0, 2.0);
    volumeProp->setStep(0.01);
    volumeProp->setUnit(QStringLiteral("linear"));
    videoGroup.addProperty(volumeProp);

    auto panProp = makeProp(QStringLiteral("video.audioPan"),
                            ArtifactCore::PropertyType::Float,
                            audioPan(), -115);
    panProp->setHardRange(-1.0, 1.0);
    panProp->setSoftRange(-1.0, 1.0);
    panProp->setStep(0.01);
    panProp->setUnit(QStringLiteral("pan"));
    videoGroup.addProperty(panProp);
        
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

    ArtifactCore::PropertyGroup trackingGroup(QStringLiteral("Motion Tracking"));
    auto trackerProp = makeProp(QStringLiteral("tracking.motionTrackerId"),
                                ArtifactCore::PropertyType::Integer,
                                static_cast<qint64>(motionTrackerId()), -70);
    trackerProp->setDisplayLabel(QStringLiteral("Tracker ID"));
    trackerProp->setHardRange(0, std::numeric_limits<qint64>::max());
    trackerProp->setTooltip(QStringLiteral("Linked ArtifactCore::Tracking::MotionTracker ID"));
    trackingGroup.addProperty(trackerProp);
    groups.push_back(trackingGroup);

    ArtifactCore::PropertyGroup sourceCropGroup(QStringLiteral("Source Reframe"));
    auto enabledProp = makeProp(QStringLiteral("sourceCrop.enabled"),
                                ArtifactCore::PropertyType::Boolean,
                                impl_->sourceCrop_.enabled(), -240);
    enabledProp->setDisplayLabel(QStringLiteral("Enabled"));
    sourceCropGroup.addProperty(enabledProp);

    auto cropXProp = makeProp(QStringLiteral("sourceCrop.cropX"),
                              ArtifactCore::PropertyType::Float,
                              impl_->sourceCrop_.cropRect().x(), -239);
    cropXProp->setDisplayLabel(QStringLiteral("Crop X"));
    cropXProp->setUnit(QStringLiteral("px"));
    cropXProp->setSoftRange(-10000.0, 10000.0);
    sourceCropGroup.addProperty(cropXProp);

    auto cropYProp = makeProp(QStringLiteral("sourceCrop.cropY"),
                              ArtifactCore::PropertyType::Float,
                              impl_->sourceCrop_.cropRect().y(), -238);
    cropYProp->setDisplayLabel(QStringLiteral("Crop Y"));
    cropYProp->setUnit(QStringLiteral("px"));
    cropYProp->setSoftRange(-10000.0, 10000.0);
    sourceCropGroup.addProperty(cropYProp);

    auto cropWProp = makeProp(QStringLiteral("sourceCrop.cropWidth"),
                              ArtifactCore::PropertyType::Float,
                              impl_->sourceCrop_.cropRect().width(), -237);
    cropWProp->setDisplayLabel(QStringLiteral("Crop W"));
    cropWProp->setUnit(QStringLiteral("px"));
    cropWProp->setSoftRange(0.0, 10000.0);
    sourceCropGroup.addProperty(cropWProp);

    auto cropHProp = makeProp(QStringLiteral("sourceCrop.cropHeight"),
                              ArtifactCore::PropertyType::Float,
                              impl_->sourceCrop_.cropRect().height(), -236);
    cropHProp->setDisplayLabel(QStringLiteral("Crop H"));
    cropHProp->setUnit(QStringLiteral("px"));
    cropHProp->setSoftRange(0.0, 10000.0);
    sourceCropGroup.addProperty(cropHProp);

    auto panXProp = makeProp(QStringLiteral("sourceCrop.panX"),
                             ArtifactCore::PropertyType::Float,
                             impl_->sourceCrop_.pan().x(), -235);
    panXProp->setDisplayLabel(QStringLiteral("Pan X"));
    panXProp->setUnit(QStringLiteral("px"));
    panXProp->setSoftRange(-10000.0, 10000.0);
    sourceCropGroup.addProperty(panXProp);

    auto panYProp = makeProp(QStringLiteral("sourceCrop.panY"),
                             ArtifactCore::PropertyType::Float,
                             impl_->sourceCrop_.pan().y(), -234);
    panYProp->setDisplayLabel(QStringLiteral("Pan Y"));
    panYProp->setUnit(QStringLiteral("px"));
    panYProp->setSoftRange(-10000.0, 10000.0);
    sourceCropGroup.addProperty(panYProp);

    auto zoomProp = makeProp(QStringLiteral("sourceCrop.zoom"),
                             ArtifactCore::PropertyType::Float,
                             impl_->sourceCrop_.zoom(), -233);
    zoomProp->setDisplayLabel(QStringLiteral("Zoom"));
    zoomProp->setUnit(QStringLiteral("x"));
    zoomProp->setSoftRange(0.1, 8.0);
    zoomProp->setStep(0.05);
    sourceCropGroup.addProperty(zoomProp);

    auto rotationProp = makeProp(QStringLiteral("sourceCrop.rotation"),
                                 ArtifactCore::PropertyType::Float,
                                 impl_->sourceCrop_.rotation(), -232);
    rotationProp->setDisplayLabel(QStringLiteral("Rotation"));
    rotationProp->setUnit(QStringLiteral("deg"));
    rotationProp->setSoftRange(-360.0, 360.0);
    rotationProp->setStep(0.5);
    sourceCropGroup.addProperty(rotationProp);

    auto anchorXProp = makeProp(QStringLiteral("sourceCrop.anchorX"),
                                ArtifactCore::PropertyType::Float,
                                impl_->sourceCrop_.anchor().x(), -231);
    anchorXProp->setDisplayLabel(QStringLiteral("Anchor X"));
    anchorXProp->setSoftRange(0.0, 1.0);
    anchorXProp->setStep(0.01);
    sourceCropGroup.addProperty(anchorXProp);

    auto anchorYProp = makeProp(QStringLiteral("sourceCrop.anchorY"),
                                ArtifactCore::PropertyType::Float,
                                impl_->sourceCrop_.anchor().y(), -230);
    anchorYProp->setDisplayLabel(QStringLiteral("Anchor Y"));
    anchorYProp->setSoftRange(0.0, 1.0);
    anchorYProp->setStep(0.01);
    sourceCropGroup.addProperty(anchorYProp);

    auto preserveProp = makeProp(QStringLiteral("sourceCrop.preserveAspect"),
                                 ArtifactCore::PropertyType::Boolean,
                                 impl_->sourceCrop_.preserveAspect(), -229);
    preserveProp->setDisplayLabel(QStringLiteral("Preserve Aspect"));
    sourceCropGroup.addProperty(preserveProp);

    groups.push_back(sourceCropGroup);
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
    if (propertyPath == QStringLiteral("video.audioPan")) {
        setAudioPan(value.toDouble());
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
    if (propertyPath == QStringLiteral("tracking.motionTrackerId")) {
        setMotionTrackerId(value.toInt());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.enabled")) {
        impl_->sourceCrop_.setEnabled(value.toBool());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.cropX") ||
        propertyPath == QStringLiteral("sourceCrop.cropY") ||
        propertyPath == QStringLiteral("sourceCrop.cropWidth") ||
        propertyPath == QStringLiteral("sourceCrop.cropHeight")) {
        const auto size = sourceSize();
        QRectF rect = impl_->sourceCrop_.cropRect();
        if (!rect.isValid() || rect.width() <= 0.0 || rect.height() <= 0.0) {
            rect = QRectF(0.0, 0.0, static_cast<qreal>(size.width), static_cast<qreal>(size.height));
        }
        if (propertyPath == QStringLiteral("sourceCrop.cropX")) {
            rect.moveLeft(value.toDouble());
        } else if (propertyPath == QStringLiteral("sourceCrop.cropY")) {
            rect.moveTop(value.toDouble());
        } else if (propertyPath == QStringLiteral("sourceCrop.cropWidth")) {
            rect.setWidth(std::max(1.0, value.toDouble()));
        } else {
            rect.setHeight(std::max(1.0, value.toDouble()));
        }
        impl_->sourceCrop_.setCropRect(rect);
        impl_->sourceCrop_.clampToSource(QSizeF(size.width, size.height));
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.panX") ||
        propertyPath == QStringLiteral("sourceCrop.panY")) {
        QPointF pan = impl_->sourceCrop_.pan();
        if (propertyPath == QStringLiteral("sourceCrop.panX")) {
            pan.setX(value.toDouble());
        } else {
            pan.setY(value.toDouble());
        }
        impl_->sourceCrop_.setPan(pan);
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.zoom")) {
        impl_->sourceCrop_.setZoom(value.toDouble());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.rotation")) {
        impl_->sourceCrop_.setRotation(value.toDouble());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.anchorX") ||
        propertyPath == QStringLiteral("sourceCrop.anchorY")) {
        QPointF anchor = impl_->sourceCrop_.anchor();
        if (propertyPath == QStringLiteral("sourceCrop.anchorX")) {
            anchor.setX(value.toDouble());
        } else {
            anchor.setY(value.toDouble());
        }
        impl_->sourceCrop_.setAnchor(anchor);
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.preserveAspect")) {
        impl_->sourceCrop_.setPreserveAspect(value.toBool());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

} // namespace Artifact
