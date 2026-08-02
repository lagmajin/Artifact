module;
#include <wobjectimpl.h>
#include <algorithm>
#include <cmath>
#include <utility>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QImage>
#include <QString>
#include <QRectF>
#include <QPainter>

module Artifact.Layer.Paint;

import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import FloatRGBA;

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
    std::map<int64_t, ArtifactCore::ImageF32x4RGBAWithCache> frames_;
    std::map<int64_t, std::vector<ArtifactCore::ImageF32x4RGBAWithCache>> undoStacks_;
    std::map<int64_t, ArtifactCore::ImageF32x4RGBAWithCache> clearUndoFrames_;
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
};

ArtifactPaintLayer::ArtifactPaintLayer() : impl_(new Impl()) {
    setLayerName(QStringLiteral("Paint Layer"));
}
ArtifactPaintLayer::~ArtifactPaintLayer() { delete impl_; }

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

void ArtifactPaintLayer::draw(ArtifactIRenderer* renderer) {
    FramePosition frame(currentFrame());
    auto* buf = frameBuffer(frame);
    if (!buf || buf->isEmpty()) return;
    QImage image = buf->toQImage();
    renderer->drawSprite(0, 0,
        static_cast<float>(image.width()),
        static_cast<float>(image.height()),
        image, opacity());
}

void ArtifactPaintLayer::newFrame(const FramePosition& pos) {
    impl_->getOrCreateFrame(pos.framePosition());
}

bool ArtifactPaintLayer::hasFrame(const FramePosition& pos) const {
    return impl_->frames_.find(pos.framePosition()) != impl_->frames_.end();
}

void ArtifactPaintLayer::removeFrame(const FramePosition& pos) {
    impl_->frames_.erase(pos.framePosition());
    impl_->undoStacks_.erase(pos.framePosition());
    impl_->clearUndoFrames_.erase(pos.framePosition());
}

void ArtifactPaintLayer::duplicateFrame(const FramePosition& src, const FramePosition& dst) {
    auto srcIt = impl_->frames_.find(src.framePosition());
    if (srcIt == impl_->frames_.end()) return;
    impl_->frames_[dst.framePosition()] = srcIt->second;
}

void ArtifactPaintLayer::clearAllFrames() {
    impl_->clearUndoFrames_ = impl_->frames_;
    impl_->frames_.clear();
    impl_->undoStacks_.clear();
    markDirty(FramePosition(currentFrame()));
    changed();
}

void ArtifactPaintLayer::applyStroke(const BrushStroke& stroke) {
    applyStrokeAtFrame(stroke, FramePosition(currentFrame()));
}

void ArtifactPaintLayer::applyStrokeAtFrame(const BrushStroke& stroke, const FramePosition& frame) {
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
    if (stroke.recordUndo) {
        impl_->undoStacks_[frame.framePosition()].push_back(buf);
        if (impl_->undoStacks_[frame.framePosition()].size() > 20)
            impl_->undoStacks_[frame.framePosition()].erase(impl_->undoStacks_[frame.framePosition()].begin());
    }

    auto& img = buf.image();
    int w = img.width(), h = img.height();
    if (w <= 0 || h <= 0) return;
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
    markDirty(frame);
}

void ArtifactPaintLayer::undoLastStroke() {
    if (!impl_->clearUndoFrames_.empty()) {
        impl_->frames_ = std::move(impl_->clearUndoFrames_);
        impl_->clearUndoFrames_.clear();
        markDirty(FramePosition(currentFrame()));
        changed();
        return;
    }
    FramePosition frame(currentFrame());
    auto it = impl_->undoStacks_.find(frame.framePosition());
    if (it == impl_->undoStacks_.end() || it->second.empty()) return;
    impl_->frames_[frame.framePosition()] = it->second.back();
    it->second.pop_back();
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
    const FramePosition& targetFrameInput) {
    const FramePosition targetFrame = targetFrameInput.framePosition() >= 0
        ? targetFrameInput
        : FramePosition(currentFrame());
    const FramePosition resolvedSourceFrame = sourceFrame.framePosition() >= 0
        ? sourceFrame
        : FramePosition(sourceLayer ? sourceLayer->currentFrame() : currentFrame());
    auto& buffer = impl_->getOrCreateFrame(targetFrame.framePosition());
    auto& image = buffer.image();
    const int width = image.width();
    const int height = image.height();
    if (width <= 0 || height <= 0) return;
    auto* sourceBuffer = sourceLayer
        ? sourceLayer->frameBuffer(resolvedSourceFrame)
        : nullptr;
    if (!sourceBuffer) return;
    auto& sourceImage = sourceBuffer->image();
    const int sourceWidth = sourceImage.width();
    const int sourceHeight = sourceImage.height();
    if (sourceWidth <= 0 || sourceHeight <= 0) return;
    if (!std::isfinite(sourcePos.x()) || !std::isfinite(sourcePos.y()) ||
        !std::isfinite(destinationPos.x()) || !std::isfinite(destinationPos.y()) ||
        !std::isfinite(radius) || !std::isfinite(opacity) || !std::isfinite(hardness)) {
        return;
    }
    radius = std::clamp(radius, 0.5f, 10000.0f);
    const float* sourceData = sourceImage.rgba32fData();
    if (recordUndo) {
        impl_->clearUndoFrames_.clear();
        impl_->undoStacks_[targetFrame.framePosition()].push_back(buffer);
        auto& history = impl_->undoStacks_[targetFrame.framePosition()];
        if (history.size() > 20) {
            history.erase(history.begin());
        }
    }

    const int diameter = std::max(1, static_cast<int>(std::ceil(radius * 2.0f)));
    std::vector<float> sourcePixels(static_cast<size_t>(diameter) * diameter * 4u,
                                    0.0f);
    float* pixels = image.rgba32fData();
    const int sourceLeft = static_cast<int>(std::floor(sourcePos.x() - radius));
    const int sourceTop = static_cast<int>(std::floor(sourcePos.y() - radius));
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const int sx = std::clamp(sourceLeft + x, 0, sourceWidth - 1);
            const int sy = std::clamp(sourceTop + y, 0, sourceHeight - 1);
            const float* src = sourceData +
                (static_cast<size_t>(sy) * sourceWidth + sx) * 4u;
            float* dst = sourcePixels.data() +
                         (static_cast<size_t>(y) * diameter + x) * 4u;
            std::copy(src, src + 4, dst);
        }
    }
    const float safeRadius = std::max(0.5f, radius);
    const float safeHardness = std::clamp(hardness, 0.0f, 1.0f);
    const float safeOpacity = std::clamp(opacity, 0.0f, 1.0f);
    const int destLeft = static_cast<int>(std::floor(destinationPos.x() - radius));
    const int destTop = static_cast<int>(std::floor(destinationPos.y() - radius));
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const float dx = static_cast<float>(x) - radius;
            const float dy = static_cast<float>(y) - radius;
            const float distance = std::sqrt(dx * dx + dy * dy) / safeRadius;
            if (distance > 1.0f) continue;
            const float falloff = distance <= safeHardness
                ? 1.0f
                : (safeHardness < 1.0f
                    ? 1.0f - (distance - safeHardness) / (1.0f - safeHardness)
                    : 0.0f);
            const int dxp = destLeft + x;
            const int dyp = destTop + y;
            if (dxp < 0 || dxp >= width || dyp < 0 || dyp >= height) continue;
            const float* src = sourcePixels.data() +
                (static_cast<size_t>(y) * diameter + x) * 4u;
            float* dst = pixels + (static_cast<size_t>(dyp) * width + dxp) * 4u;
            const float alpha = std::clamp(src[3] * safeOpacity * falloff, 0.0f, 1.0f);
            const float inverse = 1.0f - alpha;
            dst[0] = dst[0] * inverse + src[0] * alpha;
            dst[1] = dst[1] * inverse + src[1] * alpha;
            dst[2] = dst[2] * inverse + src[2] * alpha;
            dst[3] = dst[3] * inverse + alpha;
        }
    }
    markDirty(targetFrame);
    changed();
}

bool ArtifactPaintLayer::canUndo() const {
    FramePosition frame(currentFrame());
    auto it = impl_->undoStacks_.find(frame.framePosition());
    return it != impl_->undoStacks_.end() && !it->second.empty();
}

ArtifactCore::ImageF32x4_RGBA* ArtifactPaintLayer::frameBuffer(const FramePosition& pos) {
    auto it = impl_->frames_.find(pos.framePosition());
    return (it != impl_->frames_.end()) ? &it->second.image() : nullptr;
}

void ArtifactPaintLayer::markDirty(const FramePosition& pos) {
    Q_UNUSED(pos);
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
    QJsonArray framesArr;
    for (const auto& [frame, buf] : impl_->frames_) {
        framesArr.append(frameBufferToJson(buf, frame));
    }
    obj["frames"] = framesArr;
    obj["defaultWidth"] = impl_->defaultSize_.width();
    obj["defaultHeight"] = impl_->defaultSize_.height();
    return obj;
}

void ArtifactPaintLayer::fromJsonProperties(const QJsonObject& obj) {
    ArtifactAbstract2DLayer::fromJsonProperties(obj);
    impl_->frames_.clear();
    impl_->undoStacks_.clear();
    impl_->defaultSize_.setWidth(std::clamp(obj.value("defaultWidth").toInt(100), 1, 100000));
    impl_->defaultSize_.setHeight(std::clamp(obj.value("defaultHeight").toInt(100), 1, 100000));

    const QJsonArray framesArr = obj.value("frames").toArray();
    const int frameCount = std::min(framesArr.size(), 10000);
    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        const auto val = framesArr.at(frameIndex);
        const QJsonObject fObj = val.toObject();
        const int64_t frame = fObj.value("frame").toVariant().toLongLong();
        auto& buffer = impl_->frames_[frame];
        if (!frameBufferFromJson(fObj, buffer)) {
            impl_->frames_.erase(frame);
        }
    }
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactPaintLayer)
