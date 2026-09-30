module;
#include <wobjectimpl.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QObject>
#include <QPointF>
#include <QUuid>
#include <QString>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QTransform>

module Artifact.Layer.Paint;

import Artifact.Composition.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Video;
import Artifact.Layers.SolidImage;
import Artifact.Render.IRenderer;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Core.ArtifactArray;
import FloatRGBA;
import Utils.Id;

namespace Artifact {

namespace {

QJsonObject frameBufferToJson(const ArtifactCore::ImageF32x4RGBAWithCache& buffer, int64_t frame)
{
    QJsonObject obj;
    const auto& image = buffer.image();
    obj["frame"] = static_cast<qint64>(frame);
    obj["width"] = image.width();
    obj["height"] = image.height();

    const std::size_t byteCount = image.totalPixels() * 4u * sizeof(float);
    if (byteCount > 0 && image.rgba32fData()) {
        const QByteArray bytes(
            reinterpret_cast<const char*>(image.rgba32fData()),
            static_cast<qsizetype>(byteCount));
        obj["pixels_b64"] = QString::fromLatin1(bytes.toBase64());
    }
    return obj;
}

bool frameBufferFromJson(const QJsonObject& obj, ArtifactCore::ImageF32x4RGBAWithCache& buffer)
{
    const int width = obj.value("width").toInt(0);
    const int height = obj.value("height").toInt(0);
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
        return false;
    }
    const std::size_t requiredBytes = static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height) * 4u * sizeof(float);
    if (requiredBytes > 512u * 1024u * 1024u) {
        return false;
    }

    buffer.image().resize(width, height);
    buffer.image().fill(FloatRGBA{0, 0, 0, 0});

    const QString pixelsB64 = obj.value("pixels_b64").toString();
    if (pixelsB64.isEmpty()) {
        return true;
    }

    const QByteArray bytes = QByteArray::fromBase64(pixelsB64.toLatin1());
    if (bytes.size() < static_cast<qsizetype>(requiredBytes)) {
        return false;
    }

    buffer.image().setFromRGBA32F(reinterpret_cast<const float*>(bytes.constData()), width, height);
    float* pixels = buffer.image().rgba32fData();
    const std::size_t pixelCount = buffer.image().totalPixels();
    for (std::size_t index = 0; index < pixelCount * 4u; ++index) {
        if (!std::isfinite(pixels[index])) {
            pixels[index] = 0.0f;
        }
    }
    return true;
}

} // namespace

class ArtifactPaintLayer::Impl {
public:
    struct UndoPatch {
        QRect rect;
        ArtifactCore::ImageF32x4_RGBA pixels;
    };
    struct UndoStroke {
        ArtifactCore::ArtifactArray<UndoPatch> patches;
    };
    std::map<int64_t, ArtifactCore::ImageF32x4RGBAWithCache> frames_;
    std::map<int64_t, ArtifactCore::ArtifactArray<UndoStroke>> undoStacks_;
    std::map<int64_t, ArtifactCore::ImageF32x4RGBAWithCache> clearUndoFrames_;
    int64_t activeUndoFrame_ = -1;
    bool activeUndoStroke_ = false;
    std::map<int64_t, quint64> frameVersions_;
    QUuid textureIdentity_;
    ArtifactCore::LayerID targetLayerId_{ArtifactCore::Id::Nil()};
    QSize defaultSize_{100, 100};
    ArtifactAbstractComposition* composition_ = nullptr;

    ArtifactCore::ImageF32x4RGBAWithCache& getOrCreateFrame(int64_t frame) {
        auto it = frames_.find(frame);
        if (it == frames_.end()) {
            auto& buf = frames_[frame];
            buf = ArtifactCore::ImageF32x4RGBAWithCache();
            buf.image().resize(defaultSize_.width(), defaultSize_.height());
            buf.image().fill(FloatRGBA{0,0,0,0});
            return buf;
        }
        return it->second;
    }

    void beginUndoStroke(int64_t frame) {
        auto& history = undoStacks_[frame];
        history.emplace_back();
        if (history.size() > 20) {
            history.removeAt(0);
        }
        activeUndoFrame_ = frame;
        activeUndoStroke_ = true;
    }

    void appendUndoPatch(int64_t frame, const ArtifactCore::ImageF32x4_RGBA& image,
                         const QRect& rect) {
        if (!activeUndoStroke_ || activeUndoFrame_ != frame || rect.isEmpty()) {
            return;
        }
        auto history = undoStacks_.find(frame);
        if (history == undoStacks_.end() || history->second.isEmpty()) {
            activeUndoStroke_ = false;
            return;
        }
        UndoPatch patch;
        patch.rect = rect;
        patch.pixels.resize(rect.width(), rect.height());
        const auto* source = image.rgba32fData();
        auto* destination = patch.pixels.rgba32fData();
        if (!source || !destination) return;
        const std::size_t rowBytes = static_cast<std::size_t>(rect.width()) * 4u * sizeof(float);
        for (int row = 0; row < rect.height(); ++row) {
            const auto* rowSource = source +
                (static_cast<std::size_t>(rect.y() + row) * image.width() + rect.x()) * 4u;
            auto* rowDestination = destination + static_cast<std::size_t>(row) * rect.width() * 4u;
            std::memcpy(rowDestination, rowSource, rowBytes);
        }
        history->second.back().patches.append(std::move(patch));
    }

    bool restoreUndoStroke(int64_t frame) {
        auto history = undoStacks_.find(frame);
        auto buffer = frames_.find(frame);
        if (history == undoStacks_.end() || history->second.isEmpty() ||
            buffer == frames_.end()) {
            return false;
        }
        auto& image = buffer->second.image();
        auto* destination = image.rgba32fData();
        if (!destination) return false;
        const int width = image.width();
        auto& patches = history->second.back().patches;
        for (std::size_t patchIndex = patches.size(); patchIndex > 0; --patchIndex) {
            const auto& patch = patches[patchIndex - 1];
            const auto* source = patch.pixels.rgba32fData();
            if (!source || patch.rect.isEmpty() || patch.rect.x() < 0 || patch.rect.y() < 0 ||
                patch.rect.right() >= image.width() || patch.rect.bottom() >= image.height()) {
                continue;
            }
            const std::size_t rowBytes = static_cast<std::size_t>(patch.rect.width()) *
                4u * sizeof(float);
            for (int row = 0; row < patch.rect.height(); ++row) {
                auto* rowDestination = destination +
                    (static_cast<std::size_t>(patch.rect.y() + row) * width + patch.rect.x()) * 4u;
                const auto* rowSource = source +
                    static_cast<std::size_t>(row) * patch.rect.width() * 4u;
                std::memcpy(rowDestination, rowSource, rowBytes);
            }
        }
        history->second.removeLast();
        return true;
    }

};

ArtifactPaintLayer::ArtifactPaintLayer() : impl_(new Impl()) {
    setLayerName(QStringLiteral("Paint Layer"));
}
ArtifactPaintLayer::~ArtifactPaintLayer() { delete impl_; }

void ArtifactPaintLayer::setComposition(QObject* comp) {
    setComposition(static_cast<void*>(comp));
}

void ArtifactPaintLayer::setComposition(void* comp) {
    ArtifactAbstractLayer::setComposition(comp);
    impl_->composition_ = static_cast<ArtifactAbstractComposition*>(comp);
    if (impl_->composition_) {
        auto s = impl_->composition_->settings().compositionSize();
        impl_->defaultSize_ = QSize(s.width(), s.height());
    }
}

QRectF ArtifactPaintLayer::localBounds() const {
    return QRectF(0, 0, impl_->defaultSize_.width(), impl_->defaultSize_.height());
}

void ArtifactPaintLayer::setSurfaceSize(const QSize& size) {
    const QSize bounded(std::clamp(size.width(), 1, 16384),
                        std::clamp(size.height(), 1, 16384));
    if (impl_->defaultSize_ == bounded) return;
    impl_->defaultSize_ = bounded;
    changed();
}

void ArtifactPaintLayer::setTargetLayerId(const ArtifactCore::LayerID& layerId) {
    impl_->targetLayerId_ = layerId;
}

const ArtifactCore::LayerID& ArtifactPaintLayer::targetLayerId() const {
    return impl_->targetLayerId_;
}

bool ArtifactPaintLayer::hasTargetLayer() const {
    return !impl_->targetLayerId_.isNil();
}

FramePosition ArtifactPaintLayer::paintFramePosition() const {
    if (!impl_->targetLayerId_.isNil()) {
        if (!impl_->composition_) return FramePosition(-1);
        const auto target = impl_->composition_->layerById(impl_->targetLayerId_);
        if (!target) return FramePosition(-1);
        if (const auto* videoLayer =
                dynamic_cast<const ArtifactVideoLayer*>(target.get())) {
            return FramePosition(videoLayer->currentSourceFrameValue());
        }
        if (const auto* imageLayer =
                dynamic_cast<const ArtifactImageLayer*>(target.get());
            imageLayer && imageLayer->isImageSequence()) {
            const qint64 sequenceFrame =
                imageLayer->sequenceCachedFrameIndex();
            if (sequenceFrame >= 0) return FramePosition(sequenceFrame);
            return FramePosition(-1);
        }
        return FramePosition(target->currentFrame());
    }
    return FramePosition(currentFrame());
}

ArtifactPaintSurfaceLayout ArtifactPaintLayer::paintSurfaceLayout() const {
    ArtifactPaintSurfaceLayout layout;
    layout.transform = getGlobalTransform();
    layout.drawRect = QRectF(0.0, 0.0, impl_->defaultSize_.width(),
                             impl_->defaultSize_.height());
    if (impl_->targetLayerId_.isNil()) return layout;
    if (!impl_->composition_) {
        layout.targetVisible = false;
        return layout;
    }

    const auto target = impl_->composition_->layerById(impl_->targetLayerId_);
    if (!target) {
        layout.targetVisible = false;
        return layout;
    }
    const QTransform targetTransform = target->getGlobalTransform();
    layout.transform = targetTransform;
    layout.targetOpacity = target->opacity();
    const FramePosition compositionFrame = impl_->composition_->framePosition();
    layout.targetVisible = target->isVisible() &&
        target->isActiveAt(compositionFrame);
    if (const auto* solidLayer =
            dynamic_cast<const ArtifactSolidImageLayer*>(target.get())) {
        const double pixelAspect = solidLayer->pixelAspectRatio();
        if (std::isfinite(pixelAspect) && pixelAspect > 0.0 &&
            std::abs(pixelAspect - 1.0) > 1e-6) {
            const auto toLayerLocal = [pixelAspect](const QPointF& pixel) {
                return QPointF(pixel.x() * pixelAspect, pixel.y());
            };
            const QPointF origin = targetTransform.map(toLayerLocal(QPointF()));
            const QPointF xAxis = targetTransform.map(toLayerLocal(QPointF(1, 0))) - origin;
            const QPointF yAxis = targetTransform.map(toLayerLocal(QPointF(0, 1))) - origin;
            layout.transform = QTransform(xAxis.x(), xAxis.y(), yAxis.x(), yAxis.y(),
                                          origin.x(), origin.y());
        }
    }
    if (const auto* imageLayer = dynamic_cast<const ArtifactImageLayer*>(target.get())) {
        const SourceCropDrawLayout crop = imageLayer->sourceCropDrawLayout();
        const QRectF output = crop.outputLocalRect;
        const QRect sourcePixels = crop.sourcePixelRect;
        if (output.width() > 0.0 && output.height() > 0.0 &&
            sourcePixels.width() > 0 && sourcePixels.height() > 0) {
            const double scaleX = output.width() / sourcePixels.width();
            const double scaleY = output.height() / sourcePixels.height();
            const auto toLayerLocal = [&crop, output, sourcePixels, scaleX, scaleY]
                (const QPointF& pixel) {
                const QPointF outputPoint(
                    output.left() + (pixel.x() - sourcePixels.left()) * scaleX,
                    output.top() + (pixel.y() - sourcePixels.top()) * scaleY);
                return crop.localTransform.toTransform().map(outputPoint);
            };
            const QPointF origin = targetTransform.map(toLayerLocal(QPointF()));
            const QPointF xAxis = targetTransform.map(toLayerLocal(QPointF(1, 0))) - origin;
            const QPointF yAxis = targetTransform.map(toLayerLocal(QPointF(0, 1))) - origin;
            layout.transform = QTransform(xAxis.x(), xAxis.y(), yAxis.x(), yAxis.y(),
                                          origin.x(), origin.y());
            layout.drawRect = QRectF(sourcePixels);
            const auto sourceSize = imageLayer->sourceSize();
            const int width = imageLayer->hasCurrentFrameBuffer()
                ? imageLayer->currentFrameBuffer().width()
                : (sourceSize.width > 0 ? sourceSize.width : impl_->defaultSize_.width());
            const int height = imageLayer->hasCurrentFrameBuffer()
                ? imageLayer->currentFrameBuffer().height()
                : (sourceSize.height > 0 ? sourceSize.height : impl_->defaultSize_.height());
            if (width > 0 && height > 0) {
                layout.uvRect = QRectF(static_cast<double>(sourcePixels.x()) / width,
                                       static_cast<double>(sourcePixels.y()) / height,
                                       static_cast<double>(sourcePixels.width()) / width,
                                       static_cast<double>(sourcePixels.height()) / height);
            }
        }
    }
    return layout;
}

QTransform ArtifactPaintLayer::paintSurfaceTransform() const {
    return paintSurfaceLayout().transform;
}

void ArtifactPaintLayer::draw(ArtifactIRenderer* renderer) {
    drawFrameOverlay(renderer, paintFramePosition(), opacity());
}

void ArtifactPaintLayer::drawFrameOverlay(
    ArtifactIRenderer* renderer, const FramePosition& frame,
    const float opacityMultiplier) {
    if (!renderer || frame.framePosition() < 0) return;
    const ArtifactPaintSurfaceLayout surface = paintSurfaceLayout();
    if (!surface.targetVisible) return;
    auto* buf = frameBuffer(frame);
    if (!buf || buf->isEmpty()) return;
    const auto revision = impl_->frameVersions_.find(frame.framePosition());
    const quint64 sourceVersion = revision != impl_->frameVersions_.end()
        ? revision->second : 0;
    if (impl_->textureIdentity_.isNull()) {
        impl_->textureIdentity_ = QUuid(id().toString());
    }
    auto* texture = renderer->textureForImage(
        *buf, impl_->textureIdentity_, sourceVersion, frame.framePosition());
    if (!texture) return;
    const QTransform surfaceTransform = surface.transform;
    const QRectF drawRect = surface.drawRect;
    const QRectF uvRect = surface.uvRect;
    const QMatrix4x4 transform(
        static_cast<float>(surfaceTransform.m11()),
        static_cast<float>(surfaceTransform.m21()), 0.0f,
        static_cast<float>(surfaceTransform.m31()),
        static_cast<float>(surfaceTransform.m12()),
        static_cast<float>(surfaceTransform.m22()), 0.0f,
        static_cast<float>(surfaceTransform.m32()),
        0.0f, 0.0f, 1.0f, 0.0f,
        static_cast<float>(surfaceTransform.m13()),
        static_cast<float>(surfaceTransform.m23()), 0.0f,
        static_cast<float>(surfaceTransform.m33()));
    renderer->drawSpriteTransformed(
        static_cast<float>(drawRect.x()),
        static_cast<float>(drawRect.y()),
        static_cast<float>(drawRect.width()),
        static_cast<float>(drawRect.height()), transform,
        texture, opacityMultiplier * surface.targetOpacity, uvRect);
}

void ArtifactPaintLayer::newFrame(const FramePosition& pos) {
    const bool existed = hasFrame(pos);
    impl_->getOrCreateFrame(pos.framePosition());
    if (!existed) {
        markDirty(pos);
    }
}

bool ArtifactPaintLayer::hasFrame(const FramePosition& pos) const {
    return impl_->frames_.find(pos.framePosition()) != impl_->frames_.end();
}

void ArtifactPaintLayer::removeFrame(const FramePosition& pos) {
    if (impl_->activeUndoFrame_ == pos.framePosition()) {
        impl_->activeUndoStroke_ = false;
    }
    impl_->frames_.erase(pos.framePosition());
    impl_->undoStacks_.erase(pos.framePosition());
    impl_->clearUndoFrames_.erase(pos.framePosition());
    markDirty(pos);
    changed();
}

void ArtifactPaintLayer::duplicateFrame(const FramePosition& src, const FramePosition& dst) {
    auto srcIt = impl_->frames_.find(src.framePosition());
    if (srcIt == impl_->frames_.end()) return;
    impl_->frames_[dst.framePosition()] = srcIt->second;
    markDirty(dst);
    changed();
}

void ArtifactPaintLayer::clearAllFrames() {
    impl_->activeUndoStroke_ = false;
    impl_->undoStacks_.clear();
    impl_->clearUndoFrames_ = impl_->frames_;
    for (const auto& [frame, buffer] : impl_->frames_) {
        Q_UNUSED(buffer);
        markDirty(FramePosition(frame));
    }
    impl_->frames_.clear();
    impl_->undoStacks_.clear();
    markDirty(paintFramePosition());
    changed();
}

void ArtifactPaintLayer::applyStroke(const BrushStroke& stroke) {
    applyStrokeAtFrame(stroke, paintFramePosition());
}

void ArtifactPaintLayer::applyStrokeAtFrame(const BrushStroke& stroke, const FramePosition& frame) {
    if (frame.framePosition() < 0 ||
        (!impl_->targetLayerId_.isNil() &&
         (!impl_->composition_ ||
          !impl_->composition_->layerById(impl_->targetLayerId_)))) {
        return;
    }
    if (!std::isfinite(stroke.radius) || !std::isfinite(stroke.opacity) ||
        !std::isfinite(stroke.hardness) || !std::isfinite(stroke.flow) ||
        !std::isfinite(stroke.angle) || !std::isfinite(stroke.roundness) ||
        !std::isfinite(stroke.angleJitter) || !std::isfinite(stroke.roundnessJitter) ||
        !std::isfinite(stroke.scatter) || !std::isfinite(stroke.sizeJitter) ||
        !std::isfinite(stroke.opacityJitter) || !std::isfinite(stroke.flowJitter)) {
        return;
    }
    if (stroke.recordUndo) {
        impl_->clearUndoFrames_.clear();
    }
    auto& buf = impl_->getOrCreateFrame(frame.framePosition());
    auto& img = buf.image();
    int w = img.width(), h = img.height();
    if (w <= 0 || h <= 0) return;

    double minX = static_cast<double>(w);
    double minY = static_cast<double>(h);
    double maxX = 0.0;
    double maxY = 0.0;
    bool hasFinitePoint = false;
    const double brushExtent = std::max(0.001, static_cast<double>(stroke.radius)) *
        (1.0 + std::clamp(static_cast<double>(stroke.sizeJitter), 0.0, 1.0) +
         std::clamp(static_cast<double>(stroke.scatter), 0.0, 1.0)) + 2.0;
    for (const auto& point : stroke.points) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) continue;
        hasFinitePoint = true;
        minX = std::min(minX, std::clamp(point.x() - brushExtent, 0.0, static_cast<double>(w)));
        minY = std::min(minY, std::clamp(point.y() - brushExtent, 0.0, static_cast<double>(h)));
        maxX = std::max(maxX, std::clamp(point.x() + brushExtent, 0.0, static_cast<double>(w)));
        maxY = std::max(maxY, std::clamp(point.y() + brushExtent, 0.0, static_cast<double>(h)));
    }
    const QRect undoRect = hasFinitePoint && maxX > minX && maxY > minY
        ? QRect(static_cast<int>(std::floor(minX)), static_cast<int>(std::floor(minY)),
                static_cast<int>(std::ceil(maxX)) - static_cast<int>(std::floor(minX)),
                static_cast<int>(std::ceil(maxY)) - static_cast<int>(std::floor(minY)))
        : QRect();
    if (stroke.recordUndo && hasFinitePoint) {
        impl_->beginUndoStroke(frame.framePosition());
    }
    if (hasFinitePoint) {
        const bool continueStroke = impl_->activeUndoStroke_ &&
            impl_->activeUndoFrame_ == frame.framePosition();
        impl_->appendUndoPatch(frame.framePosition(), img, undoRect);
        if (!stroke.recordUndo && !continueStroke) {
            impl_->activeUndoStroke_ = false;
        }
    }

    float* pixels = img.rgba32fData();

    const float baseRadius = std::max(0.001f, stroke.radius);
    FloatRGBA color = stroke.eraser ? FloatRGBA{0,0,0,0} : stroke.color;

    size_t pointIndex = 0;
    for (const auto& pt : stroke.points) {
        if (!std::isfinite(pt.x()) || !std::isfinite(pt.y())) {
            ++pointIndex;
            continue;
        }
        // Deterministic per-dab variation keeps an incremental stroke and its
        // undo/redo replay visually identical without storing a random engine.
        const float jitterSeed = std::sin(
            static_cast<float>(pt.x()) * 12.9898f +
            static_cast<float>(pt.y()) * 78.233f +
            static_cast<float>(pointIndex++) * 37.719f) * 43758.5453f;
        const float jitter = jitterSeed - std::floor(jitterSeed);
        const float angleVariation = (jitter * 2.0f - 1.0f) *
                                     std::clamp(stroke.angleJitter, 0.0f, 1.0f) *
                                     180.0f;
        const float pointAngle =
            (stroke.angle + angleVariation) * 0.017453292519943295f;
        const float cosAngle = std::cos(pointAngle);
        const float sinAngle = std::sin(pointAngle);
        const float pointRoundness = std::clamp(
            stroke.roundness *
                (1.0f + (jitter * 2.0f - 1.0f) *
                            std::clamp(stroke.roundnessJitter, 0.0f, 1.0f)),
            0.01f, 1.0f);
        const float scatterSeed = std::sin(
            static_cast<float>(pt.x()) * 39.425f +
            static_cast<float>(pt.y()) * 11.731f +
            static_cast<float>(pointIndex) * 17.113f) * 24634.6345f;
        const float scatterUnit = scatterSeed - std::floor(scatterSeed);
        const float scatterAngle = scatterUnit * 6.283185307179586f;
        const float scatterDistance =
            std::sqrt(std::max(0.0f, jitter)) * baseRadius *
            std::clamp(stroke.scatter, 0.0f, 1.0f);
        const float pointRadius = std::max(
            0.001f, baseRadius *
                         (1.0f + (jitter * 2.0f - 1.0f) *
                                     std::clamp(stroke.sizeJitter, 0.0f, 1.0f)));
        const float pointOpacity = std::clamp(
            stroke.opacity *
                (1.0f + (jitter * 2.0f - 1.0f) *
                            std::clamp(stroke.opacityJitter, 0.0f, 1.0f)),
            0.0f, 1.0f);
        const float pointFlow = std::clamp(
            stroke.flow *
                (1.0f + (jitter * 2.0f - 1.0f) *
                            std::clamp(stroke.flowJitter, 0.0f, 1.0f)),
            0.0f, 1.0f);
        int cx = static_cast<int>(pt.x() + std::cos(scatterAngle) * scatterDistance);
        int cy = static_cast<int>(pt.y() + std::sin(scatterAngle) * scatterDistance);
        int minX = std::max(0, cx - static_cast<int>(std::ceil(pointRadius)));
        int maxX = std::min(w - 1, cx + static_cast<int>(std::ceil(pointRadius)));
        int minY = std::max(0, cy - static_cast<int>(std::ceil(pointRadius)));
        int maxY = std::min(h - 1, cy + static_cast<int>(std::ceil(pointRadius)));

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const float dx = static_cast<float>(x - cx);
                const float dy = static_cast<float>(y - cy);
                const float rotatedX = dx * cosAngle + dy * sinAngle;
                const float rotatedY = -dx * sinAngle + dy * cosAngle;
                const float normalizedDistance = std::sqrt(
                    (rotatedX * rotatedX) / (pointRadius * pointRadius) +
                    (rotatedY * rotatedY) /
                        (pointRadius * pointRadius * pointRoundness *
                         pointRoundness));
                if (normalizedDistance <= 1.0f) {
                    const float distance = normalizedDistance;
                    const float hardness = std::clamp(stroke.hardness, 0.0f, 1.0f);
                    const float hardRadius = hardness;
                    float falloff = distance <= hardRadius
                        ? 1.0f
                        : (1.0f > hardRadius
                            ? 1.0f - (distance - hardRadius) /
                                  (1.0f - hardRadius)
                            : 0.0f);
                    float alpha = color.a() * pointOpacity * pointFlow *
                                  std::max(0.0f, falloff);
                    float* pixel = pixels +
                        (static_cast<size_t>(y) * w + x) * 4u;
                    if (stroke.eraser) {
                        pixel[3] *= 1.0f - alpha;
                    } else {
                        const float invAlpha = 1.0f - alpha;
                        pixel[0] = pixel[0] * invAlpha + color.r() * alpha;
                        pixel[1] = pixel[1] * invAlpha + color.g() * alpha;
                        pixel[2] = pixel[2] * invAlpha + color.b() * alpha;
                        pixel[3] = pixel[3] * invAlpha + alpha;
                    }
                }
            }
        }
    }
    if (stroke.finalizeUndo && impl_->activeUndoFrame_ == frame.framePosition()) {
        impl_->activeUndoStroke_ = false;
    }
    markDirty(frame);
}

void ArtifactPaintLayer::undoLastStroke() {
    impl_->activeUndoStroke_ = false;
    if (!impl_->clearUndoFrames_.empty()) {
        impl_->frames_ = std::move(impl_->clearUndoFrames_);
        impl_->clearUndoFrames_.clear();
        markDirty(paintFramePosition());
        changed();
        return;
    }
    const FramePosition frame = paintFramePosition();
    if (frame.framePosition() < 0) return;
    if (!impl_->restoreUndoStroke(frame.framePosition())) return;
    markDirty(frame);
    changed();
}

void ArtifactPaintLayer::applyCloneStampAtFrame(
    const QPointF& sourcePos, const QPointF& destinationPos, float radius,
    float opacity, float hardness, bool recordUndo, const FramePosition& frame) {
    applyCloneStampFromLayerAtFrame(this, sourcePos, destinationPos, radius,
                                    opacity, hardness, recordUndo, frame, frame);
}

void ArtifactPaintLayer::applyCloneStampFromLayerAtFrame(
    ArtifactPaintLayer* sourceLayer, const QPointF& sourcePos,
    const QPointF& destinationPos, float radius, float opacity, float hardness,
    bool recordUndo, const FramePosition& sourceFrame,
    const FramePosition& targetFrameInput, bool finalizeUndo) {
    const FramePosition targetFrame = targetFrameInput.framePosition() >= 0
        ? targetFrameInput
        : paintFramePosition();
    const FramePosition resolvedSourceFrame = sourceFrame.framePosition() >= 0
        ? sourceFrame
        : (sourceLayer ? sourceLayer->paintFramePosition()
                       : paintFramePosition());
    if (targetFrame.framePosition() < 0 ||
        resolvedSourceFrame.framePosition() < 0) {
        return;
    }
    auto& buffer = impl_->getOrCreateFrame(targetFrame.framePosition());
    auto& image = buffer.image();
    const int width = image.width();
    const int height = image.height();
    if (width <= 0 || height <= 0) return;
    auto* sourceBuffer = sourceLayer
        ? sourceLayer->frameBuffer(resolvedSourceFrame)
        : nullptr;
    if (!sourceBuffer) return;
    const auto& sourceImage = *sourceBuffer;
    const int sourceWidth = sourceImage.width();
    const int sourceHeight = sourceImage.height();
    if (sourceWidth <= 0 || sourceHeight <= 0) return;
    if (!std::isfinite(sourcePos.x()) || !std::isfinite(sourcePos.y()) ||
        !std::isfinite(destinationPos.x()) || !std::isfinite(destinationPos.y()) ||
        !std::isfinite(radius) || !std::isfinite(opacity) || !std::isfinite(hardness)) {
        return;
    }
    radius = std::clamp(radius, 0.5f, 10000.0f);
    const int diameter = std::max(1, static_cast<int>(std::ceil(radius * 2.0f)));
    const float* sourceData = sourceImage.rgba32fData();
    if (!sourceData) return;

    const int destLeft = static_cast<int>(std::clamp(
        std::floor(destinationPos.x() - radius), -static_cast<double>(diameter),
        static_cast<double>(width)));
    const int destTop = static_cast<int>(std::clamp(
        std::floor(destinationPos.y() - radius), -static_cast<double>(diameter),
        static_cast<double>(height)));
    const int sourceLeft = static_cast<int>(std::clamp(
        std::floor(sourcePos.x() - radius), -static_cast<double>(diameter),
        static_cast<double>(sourceWidth)));
    const int sourceTop = static_cast<int>(std::clamp(
        std::floor(sourcePos.y() - radius), -static_cast<double>(diameter),
        static_cast<double>(sourceHeight)));
    const int patchLeft = std::clamp(destLeft, 0, width);
    const int patchTop = std::clamp(destTop, 0, height);
    const int patchRight = static_cast<int>(std::clamp(
        static_cast<int64_t>(destLeft) + diameter, int64_t{0}, static_cast<int64_t>(width)));
    const int patchBottom = static_cast<int>(std::clamp(
        static_cast<int64_t>(destTop) + diameter, int64_t{0}, static_cast<int64_t>(height)));
    const int patchWidth = std::max(0, patchRight - patchLeft);
    const int patchHeight = std::max(0, patchBottom - patchTop);
    if (patchWidth == 0 || patchHeight == 0) return;

    if (recordUndo) {
        impl_->clearUndoFrames_.clear();
        impl_->activeUndoStroke_ = false;
        impl_->beginUndoStroke(targetFrame.framePosition());
    }

    const float safeRadius = std::max(0.5f, radius);
    const float safeHardness = std::clamp(hardness, 0.0f, 1.0f);
    const float safeOpacity = std::clamp(opacity, 0.0f, 1.0f);
    const bool hasActiveUndoStroke = impl_->activeUndoStroke_ &&
        impl_->activeUndoFrame_ == targetFrame.framePosition();
    if (recordUndo || hasActiveUndoStroke) {
        impl_->appendUndoPatch(targetFrame.framePosition(), image,
            QRect(patchLeft, patchTop, std::max(0, patchRight - patchLeft),
                  std::max(0, patchBottom - patchTop)));
    }
    float* pixels = image.rgba32fData();
    const bool readsSameFrame = sourceLayer == this &&
        resolvedSourceFrame.framePosition() == targetFrame.framePosition();
    const int sourceOffsetX = sourceLeft - destLeft;
    const int sourceOffsetY = sourceTop - destTop;
    // If source and target alias, traverse like memmove so an overlapping
    // destination write cannot replace source pixels before they are sampled.
    const int yStep = readsSameFrame && sourceOffsetY < 0 ? -1 : 1;
    const int xStep = readsSameFrame && sourceOffsetX < 0 ? -1 : 1;
    const int firstPatchY = yStep > 0 ? 0 : patchHeight - 1;
    const int endPatchY = yStep > 0 ? patchHeight : -1;
    const int firstPatchX = xStep > 0 ? 0 : patchWidth - 1;
    const int endPatchX = xStep > 0 ? patchWidth : -1;
    for (int patchY = firstPatchY; patchY != endPatchY; patchY += yStep) {
        const int destinationY = patchTop + patchY;
        const int y = destinationY - destTop;
        const int sy = std::clamp(sourceTop + y, 0, sourceHeight - 1);
        for (int patchX = firstPatchX; patchX != endPatchX; patchX += xStep) {
            const int destinationX = patchLeft + patchX;
            const int x = destinationX - destLeft;
            const int sx = std::clamp(sourceLeft + x, 0, sourceWidth - 1);
            const float dx = static_cast<float>(x) - radius;
            const float dy = static_cast<float>(y) - radius;
            const float distance = std::sqrt(dx * dx + dy * dy) / safeRadius;
            if (distance > 1.0f) continue;
            const float falloff = distance <= safeHardness
                ? 1.0f
                : (safeHardness < 1.0f
                    ? 1.0f - (distance - safeHardness) / (1.0f - safeHardness)
                    : 0.0f);
            const float* src = sourceData +
                (static_cast<size_t>(sy) * sourceWidth + sx) * 4u;
            float* dst = pixels +
                (static_cast<size_t>(destinationY) * width + destinationX) * 4u;
            const float alpha = std::clamp(src[3] * safeOpacity * falloff, 0.0f, 1.0f);
            const float inverse = 1.0f - alpha;
            dst[0] = dst[0] * inverse + src[0] * alpha;
            dst[1] = dst[1] * inverse + src[1] * alpha;
            dst[2] = dst[2] * inverse + src[2] * alpha;
            dst[3] = dst[3] * inverse + alpha;
        }
    }
    if (finalizeUndo && impl_->activeUndoFrame_ == targetFrame.framePosition()) {
        impl_->activeUndoStroke_ = false;
    }
    markDirty(targetFrame);
    changed();
}

void ArtifactPaintLayer::finalizeUndoStroke(const FramePosition& frame) {
    if (impl_->activeUndoFrame_ == frame.framePosition()) {
        impl_->activeUndoStroke_ = false;
    }
}

bool ArtifactPaintLayer::cancelActiveUndoStroke(const FramePosition& frame) {
    if (!impl_->activeUndoStroke_ || impl_->activeUndoFrame_ != frame.framePosition()) {
        return false;
    }
    impl_->activeUndoStroke_ = false;
    if (!impl_->restoreUndoStroke(frame.framePosition())) return false;
    markDirty(frame);
    changed();
    return true;
}

bool ArtifactPaintLayer::canUndo() const {
    const FramePosition frame = paintFramePosition();
    if (frame.framePosition() < 0) return false;
    auto it = impl_->undoStacks_.find(frame.framePosition());
    return it != impl_->undoStacks_.end() && !it->second.isEmpty();
}

ArtifactCore::ImageF32x4_RGBA* ArtifactPaintLayer::frameBuffer(const FramePosition& pos) {
    auto it = impl_->frames_.find(pos.framePosition());
    return (it != impl_->frames_.end()) ? &it->second.image() : nullptr;
}

void ArtifactPaintLayer::markDirty(const FramePosition& pos) {
    auto& revision = impl_->frameVersions_[pos.framePosition()];
    if (++revision == 0) {
        revision = 1;
    }
}

std::vector<ArtifactCore::PropertyGroup> ArtifactPaintLayer::getLayerPropertyGroups() const {
    auto groups = ArtifactAbstract2DLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup paintGrp(QStringLiteral("Paint"));
    paintGrp.addProperty(persistentLayerProperty(
        QStringLiteral("paint.frameCount"),
        ArtifactCore::PropertyType::Integer,
        static_cast<int>(impl_->frames_.size()), -100));
    paintGrp.addProperty(persistentLayerProperty(
        QStringLiteral("paint.width"),
        ArtifactCore::PropertyType::Integer, impl_->defaultSize_.width(), -99));
    paintGrp.addProperty(persistentLayerProperty(
        QStringLiteral("paint.height"),
        ArtifactCore::PropertyType::Integer, impl_->defaultSize_.height(), -98));
    groups.push_back(paintGrp);
    return groups;
}

QJsonObject ArtifactPaintLayer::toJson() const {
    QJsonObject obj = ArtifactAbstract2DLayer::toJson();
    obj["type"] = static_cast<int>(LayerType::Paint);
    QJsonArray framesArr;
    for (const auto& [frame, buf] : impl_->frames_) {
        framesArr.append(frameBufferToJson(buf, frame));
    }
    obj["frames"] = framesArr;
    obj["defaultWidth"] = impl_->defaultSize_.width();
    obj["defaultHeight"] = impl_->defaultSize_.height();
    if (!impl_->targetLayerId_.isNil()) {
        obj["targetLayerId"] = impl_->targetLayerId_.toString();
    }
    return obj;
}

void ArtifactPaintLayer::fromJsonProperties(const QJsonObject& obj) {
    ArtifactAbstract2DLayer::fromJsonProperties(obj);
    for (const auto& [frame, buffer] : impl_->frames_) {
        Q_UNUSED(buffer);
        markDirty(FramePosition(frame));
    }
    impl_->frames_.clear();
    impl_->undoStacks_.clear();
    impl_->activeUndoStroke_ = false;
    impl_->defaultSize_.setWidth(std::clamp(obj.value("defaultWidth").toInt(100), 1, 100000));
    impl_->defaultSize_.setHeight(std::clamp(obj.value("defaultHeight").toInt(100), 1, 100000));
    const QString targetLayerId = obj.value("targetLayerId").toString().trimmed();
    impl_->targetLayerId_ = targetLayerId.isEmpty()
        ? ArtifactCore::LayerID(ArtifactCore::Id::Nil())
        : ArtifactCore::LayerID(targetLayerId);
    const QJsonArray framesArr = obj.value("frames").toArray();
    const int frameCount = std::min(static_cast<int>(framesArr.size()), 10000);
    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        const auto val = framesArr.at(frameIndex);
        const QJsonObject fObj = val.toObject();
        const int64_t frame = fObj.value("frame").toVariant().toLongLong();
        auto& buffer = impl_->frames_[frame];
        if (!frameBufferFromJson(fObj, buffer)) {
            impl_->frames_.erase(frame);
        } else {
            markDirty(FramePosition(frame));
        }
    }
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactPaintLayer)
