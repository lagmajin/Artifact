module;
#include <wobjectimpl.h>
#include <algorithm>
#include <cmath>
#include <QJsonArray>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QRectF>
#include <QPainter>

module Artifact.Layer.Paint;

import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Image.ImageF32x4RGBAWithCache;
import FloatRGBA;

namespace Artifact {

class ArtifactPaintLayer::Impl {
public:
    std::map<int64_t, ArtifactCore::ImageF32x4RGBAWithCache> frames_;
    std::map<int64_t, std::vector<ArtifactCore::ImageF32x4RGBAWithCache>> undoStacks_;
    QSize defaultSize_{100, 100};
    ArtifactAbstractComposition* composition_ = nullptr;

    ArtifactCore::ImageF32x4RGBAWithCache& getOrCreateFrame(int64_t frame) {
        auto it = frames_.find(frame);
        if (it == frames_.end()) {
            auto& buf = frames_[frame];
            buf = ArtifactCore::ImageF32x4RGBAWithCache(
                ArtifactCore::ImageF32x4_RGBA(defaultSize_.width(), defaultSize_.height()));
            buf.image().clear(FloatRGBA{0,0,0,0});
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
    auto frame = currentFrame();
    auto* buf = frameBuffer(frame);
    if (!buf || buf->isEmpty()) return;
    renderer->drawSprite(0, 0,
        static_cast<float>(buf->width()),
        static_cast<float>(buf->height()),
        *buf, opacity());
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
}

void ArtifactPaintLayer::duplicateFrame(const FramePosition& src, const FramePosition& dst) {
    auto srcIt = impl_->frames_.find(src.framePosition());
    if (srcIt == impl_->frames_.end()) return;
    impl_->frames_[dst.framePosition()] = srcIt->second;
}

void ArtifactPaintLayer::clearAllFrames() {
    impl_->frames_.clear();
    impl_->undoStacks_.clear();
}

void ArtifactPaintLayer::applyStroke(const BrushStroke& stroke) {
    applyStrokeAtFrame(stroke, currentFrame());
}

void ArtifactPaintLayer::applyStrokeAtFrame(const BrushStroke& stroke, const FramePosition& frame) {
    auto& buf = impl_->getOrCreateFrame(frame.framePosition());
    impl_->undoStacks_[frame.framePosition()].push_back(buf);
    if (impl_->undoStacks_[frame.framePosition()].size() > 20)
        impl_->undoStacks_[frame.framePosition()].erase(impl_->undoStacks_[frame.framePosition()].begin());

    auto& img = buf.image();
    int w = img.width(), h = img.height();
    if (w <= 0 || h <= 0) return;

    float r2 = stroke.radius * stroke.radius;
    FloatRGBA color = stroke.eraser ? FloatRGBA{0,0,0,0} : stroke.color;

    for (const auto& pt : stroke.points) {
        int cx = static_cast<int>(pt.x());
        int cy = static_cast<int>(pt.y());
        int minX = std::max(0, cx - static_cast<int>(stroke.radius));
        int maxX = std::min(w - 1, cx + static_cast<int>(stroke.radius));
        int minY = std::max(0, cy - static_cast<int>(stroke.radius));
        int maxY = std::min(h - 1, cy + static_cast<int>(stroke.radius));

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                float dx = static_cast<float>(x - cx);
                float dy = static_cast<float>(y - cy);
                if (dx * dx + dy * dy <= r2) {
                    float falloff = 1.0f - std::sqrt(dx*dx + dy*dy) / stroke.radius;
                    float alpha = color.a * stroke.opacity * std::max(0.0f, falloff);
                    if (stroke.eraser) {
                        auto* px = img.pixel(x, y);
                        px->a *= (1.0f - alpha);
                    } else {
                        auto* px = img.pixel(x, y);
                        px->r = px->r * (1.0f - alpha) + color.r * alpha;
                        px->g = px->g * (1.0f - alpha) + color.g * alpha;
                        px->b = px->b * (1.0f - alpha) + color.b * alpha;
                        px->a = px->a * (1.0f - alpha) + alpha;
                    }
                }
            }
        }
    }
    markDirty(frame);
}

void ArtifactPaintLayer::undoLastStroke() {
    auto frame = currentFrame().framePosition();
    auto it = impl_->undoStacks_.find(frame);
    if (it == impl_->undoStacks_.end() || it->second.empty()) return;
    impl_->frames_[frame] = it->second.back();
    it->second.pop_back();
}

bool ArtifactPaintLayer::canUndo() const {
    auto it = impl_->undoStacks_.find(currentFrame().framePosition());
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
    QJsonObject obj;
    QJsonArray framesArr;
    for (const auto& [frame, buf] : impl_->frames_) {
        QJsonObject fObj;
        fObj["frame"] = static_cast<double>(frame);
        fObj["width"] = buf.image().width();
        fObj["height"] = buf.image().height();
        framesArr.append(fObj);
    }
    obj["frames"] = framesArr;
    obj["defaultWidth"] = impl_->defaultSize_.width();
    obj["defaultHeight"] = impl_->defaultSize_.height();
    return obj;
}

void ArtifactPaintLayer::fromJson(const QJsonObject& obj) {
    impl_->frames_.clear();
    impl_->defaultSize_.setWidth(obj.value("defaultWidth").toInt(100));
    impl_->defaultSize_.setHeight(obj.value("defaultHeight").toInt(100));
}

} // namespace Artifact

W_OBJECT_IMPL(ArtifactPaintLayer)
