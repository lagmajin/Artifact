module;
#include <QDebug>
#include <QPointF>
#include <QPolygonF>
#include <QSize>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

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
#include <opencv2/opencv.hpp>
module Artifact.Mask.Path;


import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

namespace {

void applySnapshotToPath(MaskPath& path, const MaskPathKeyframeSnapshot& snapshot)
{
    path.clearVertices();
    for (const auto& vertex : snapshot.vertices) {
        path.addVertex(vertex);
    }
    path.setClosed(snapshot.closed);
    path.setOpacity(snapshot.opacity);
    path.setFeather(snapshot.feather);
    path.setExpansion(snapshot.expansion);
    path.setInverted(snapshot.inverted);
    path.setMode(snapshot.mode);
    path.setName(snapshot.name);
}

MaskVertex lerpVertex(const MaskVertex& a, const MaskVertex& b, float t)
{
    const auto lerpPoint = [t](const QPointF& p0, const QPointF& p1) {
        return QPointF(p0.x() + (p1.x() - p0.x()) * t,
                       p0.y() + (p1.y() - p0.y()) * t);
    };
    MaskVertex out;
    out.position = lerpPoint(a.position, b.position);
    out.inTangent = lerpPoint(a.inTangent, b.inTangent);
    out.outTangent = lerpPoint(a.outTangent, b.outTangent);
    return out;
}

MaskPathKeyframeSnapshot interpolateSnapshot(const MaskPathKeyframeSnapshot& a,
                                             const MaskPathKeyframeSnapshot& b,
                                             int64_t frame)
{
    MaskPathKeyframeSnapshot out = a;
    if (b.frame <= a.frame) {
        return out;
    }

    const float t = std::clamp(
        static_cast<float>(frame - a.frame) / static_cast<float>(b.frame - a.frame),
        0.0f, 1.0f);
    out.frame = frame;
    out.opacity = a.opacity + (b.opacity - a.opacity) * t;
    out.feather = a.feather + (b.feather - a.feather) * t;
    out.expansion = a.expansion + (b.expansion - a.expansion) * t;
    out.inverted = t < 0.5f ? a.inverted : b.inverted;
    out.mode = t < 0.5f ? a.mode : b.mode;
    out.closed = t < 0.5f ? a.closed : b.closed;
    out.name = t < 0.5f ? a.name : b.name;

    if (a.vertices.size() == b.vertices.size()) {
        out.vertices.resize(a.vertices.size());
        for (size_t i = 0; i < a.vertices.size(); ++i) {
            out.vertices[i] = lerpVertex(a.vertices[i], b.vertices[i], t);
        }
    } else {
        out.vertices = t < 0.5f ? a.vertices : b.vertices;
    }
    return out;
}

} // namespace

// -- Impl --

class MaskPath::Impl {
public:
    std::vector<MaskVertex> vertices;
    std::vector<MaskPathKeyframeSnapshot> animationKeyframes;
    bool closed = true;
    float opacity = 1.0f;
    float feather = 0.0f;
    float expansion = 0.0f;
    bool inverted = false;
    MaskMode mode = MaskMode::Add;
    UniString name;

    // ベジェ曲線のサブディビジョンで近似ポリゴンを生成
    QPolygonF toPolygon(int subdivisions = 16) const;
};

QPolygonF MaskPath::Impl::toPolygon(int subdivisions) const
{
    QPolygonF poly;
    const int n = static_cast<int>(vertices.size());
    if (n < 2) {
        for (const auto& v : vertices) poly << v.position;
        return poly;
    }

    auto cubicBezier = [](const QPointF& p0, const QPointF& p1,
                          const QPointF& p2, const QPointF& p3, float t) -> QPointF {
        float u = 1.0f - t;
        float tt = t * t;
        float uu = u * u;
        float uuu = uu * u;
        float ttt = tt * t;
        return uuu * p0 + 3.0f * uu * t * p1 + 3.0f * u * tt * p2 + ttt * p3;
    };

    int segments = closed ? n : n - 1;
    for (int i = 0; i < segments; ++i) {
        const auto& v0 = vertices[i];
        const auto& v1 = vertices[(i + 1) % n];
        QPointF cp0 = v0.position + v0.outTangent;
        QPointF cp1 = v1.position + v1.inTangent;
        for (int s = 0; s < subdivisions; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(subdivisions);
            poly << cubicBezier(v0.position, cp0, cp1, v1.position, t);
        }
    }
    // close: last point
    if (!closed && n > 0) {
        poly << vertices.back().position;
    }
    return poly;
}

// -- MaskPath --

MaskPath::MaskPath() : impl_(new Impl()) {}

MaskPath::~MaskPath() { delete impl_; }

MaskPath::MaskPath(const MaskPath& other) : impl_(new Impl(*other.impl_)) {}

MaskPath& MaskPath::operator=(const MaskPath& other) {
    if (this != &other) {
        delete impl_;
        impl_ = new Impl(*other.impl_);
    }
    return *this;
}

void MaskPath::addVertex(const MaskVertex& vertex) { impl_->vertices.push_back(vertex); }

void MaskPath::insertVertex(int index, const MaskVertex& vertex) {
    if (index >= 0 && index <= static_cast<int>(impl_->vertices.size()))
        impl_->vertices.insert(impl_->vertices.begin() + index, vertex);
}

void MaskPath::removeVertex(int index) {
    if (index >= 0 && index < static_cast<int>(impl_->vertices.size()))
        impl_->vertices.erase(impl_->vertices.begin() + index);
}

void MaskPath::setVertex(int index, const MaskVertex& vertex) {
    if (index >= 0 && index < static_cast<int>(impl_->vertices.size()))
        impl_->vertices[index] = vertex;
}

MaskVertex MaskPath::vertex(int index) const {
    if (index >= 0 && index < static_cast<int>(impl_->vertices.size()))
        return impl_->vertices[index];
    return {};
}

int MaskPath::vertexCount() const { return static_cast<int>(impl_->vertices.size()); }

void MaskPath::clearVertices() { impl_->vertices.clear(); }

bool MaskPath::isClosed() const { return impl_->closed; }
void MaskPath::setClosed(bool closed) { impl_->closed = closed; }

float MaskPath::opacity() const { return impl_->opacity; }
void MaskPath::setOpacity(float opacity) { impl_->opacity = std::clamp(opacity, 0.0f, 1.0f); }

float MaskPath::feather() const { return impl_->feather; }
void MaskPath::setFeather(float feather) { impl_->feather = std::max(0.0f, feather); }

float MaskPath::expansion() const { return impl_->expansion; }
void MaskPath::setExpansion(float expansion) { impl_->expansion = expansion; }

bool MaskPath::isInverted() const { return impl_->inverted; }
void MaskPath::setInverted(bool inverted) { impl_->inverted = inverted; }

MaskMode MaskPath::mode() const { return impl_->mode; }
void MaskPath::setMode(MaskMode mode) { impl_->mode = mode; }

UniString MaskPath::name() const { return impl_->name; }
void MaskPath::setName(const UniString& name) { impl_->name = name; }

void MaskPath::clearAnimationKeyframes() { impl_->animationKeyframes.clear(); }

void MaskPath::setAnimationKeyframe(int64_t frame, const MaskPathKeyframeSnapshot& snapshot)
{
    MaskPathKeyframeSnapshot stored = snapshot;
    stored.frame = frame;
    stored.name = stored.name.toQString().trimmed().isEmpty() ? impl_->name : stored.name;
    auto it = std::find_if(impl_->animationKeyframes.begin(), impl_->animationKeyframes.end(),
                           [frame](const MaskPathKeyframeSnapshot& existing) {
                               return existing.frame == frame;
                           });
    if (it != impl_->animationKeyframes.end()) {
        *it = std::move(stored);
    } else {
        impl_->animationKeyframes.push_back(std::move(stored));
        std::sort(impl_->animationKeyframes.begin(), impl_->animationKeyframes.end(),
                  [](const MaskPathKeyframeSnapshot& a, const MaskPathKeyframeSnapshot& b) {
                      return a.frame < b.frame;
                  });
    }
}

bool MaskPath::removeAnimationKeyframe(int64_t frame)
{
    const auto before = impl_->animationKeyframes.size();
    impl_->animationKeyframes.erase(
        std::remove_if(impl_->animationKeyframes.begin(), impl_->animationKeyframes.end(),
                       [frame](const MaskPathKeyframeSnapshot& existing) {
                           return existing.frame == frame;
                       }),
        impl_->animationKeyframes.end());
    return impl_->animationKeyframes.size() != before;
}

bool MaskPath::hasAnimationKeyframes() const
{
    return !impl_->animationKeyframes.empty();
}

std::vector<MaskPathKeyframeSnapshot> MaskPath::animationKeyframes() const
{
    return impl_->animationKeyframes;
}

MaskPath MaskPath::sampleAtFrame(int64_t frame) const
{
    if (impl_->animationKeyframes.empty()) {
        return *this;
    }

    if (impl_->animationKeyframes.size() == 1) {
        MaskPath sampled;
        applySnapshotToPath(sampled, impl_->animationKeyframes.front());
        return sampled;
    }

    const auto upper = std::lower_bound(
        impl_->animationKeyframes.begin(), impl_->animationKeyframes.end(), frame,
        [](const MaskPathKeyframeSnapshot& snapshot, int64_t value) {
            return snapshot.frame < value;
        });
    if (upper == impl_->animationKeyframes.begin()) {
        MaskPath sampled;
        applySnapshotToPath(sampled, *upper);
        return sampled;
    }
    if (upper == impl_->animationKeyframes.end()) {
        MaskPath sampled;
        applySnapshotToPath(sampled, impl_->animationKeyframes.back());
        return sampled;
    }

    const auto& after = *upper;
    const auto& before = *(upper - 1);
    if (before.frame == frame) {
        MaskPath sampled;
        applySnapshotToPath(sampled, before);
        return sampled;
    }
    if (after.frame == frame) {
        MaskPath sampled;
        applySnapshotToPath(sampled, after);
        return sampled;
    }

    MaskPath sampled;
    applySnapshotToPath(sampled, interpolateSnapshot(before, after, frame));
    return sampled;
}

void MaskPath::rasterizeToAlpha(int width, int height, void* outMat,
                                float offsetX, float offsetY,
                                float scaleX, float scaleY) const
{
    cv::Mat& dst = *static_cast<cv::Mat*>(outMat);
    dst = cv::Mat::zeros(height, width, CV_32FC1);

    QPolygonF poly = impl_->toPolygon(16);
    if (poly.isEmpty()) {
        qWarning() << "[MaskPath] rasterizeToAlpha: empty polygon"
                   << "name=" << impl_->name.toQString()
                   << "closed=" << impl_->closed
                   << "vertexCount=" << static_cast<int>(impl_->vertices.size())
                   << "size=" << QSize(width, height)
                   << "offset=" << QPointF(offsetX, offsetY);
        return;
    }

    // QPolygonF -> cv::Point array for fillPoly
    // offsetX/offsetY translates from layer-local space to image pixel space
    std::vector<cv::Point> pts;
    pts.reserve(poly.size());
    for (const auto& p : poly) {
        pts.emplace_back(static_cast<int>(std::round(p.x() * scaleX + offsetX)),
                         static_cast<int>(std::round(p.y() * scaleY + offsetY)));
    }

    // fillPoly on 8-bit image, then convert to float
    cv::Mat mask8(height, width, CV_8UC1, cv::Scalar(0));
    std::vector<std::vector<cv::Point>> contours = { pts };
    cv::fillPoly(mask8, contours, cv::Scalar(255));

    // Expansion: morphological dilate(+) or erode(-)
    float exp = impl_->expansion * ((scaleX + scaleY) * 0.5f);
    if (std::abs(exp) > 0.5f) {
        int ksize = static_cast<int>(std::abs(exp) * 2.0f) | 1; // ensure odd
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(ksize, ksize));
        if (exp > 0.0f) {
            cv::dilate(mask8, mask8, kernel);
        } else {
            cv::erode(mask8, mask8, kernel);
        }
    }

    // Feather: Gaussian blur
    float featherVal = impl_->feather * ((scaleX + scaleY) * 0.5f);
    if (featherVal > 0.5f) {
        int ksize = static_cast<int>(featherVal * 2.0f) | 1;
        cv::GaussianBlur(mask8, mask8, cv::Size(ksize, ksize), 0);
    }

    // Convert to float 0~1
    mask8.convertTo(dst, CV_32FC1, 1.0 / 255.0);

    // Apply opacity
    if (impl_->opacity < 1.0f) {
        dst *= impl_->opacity;
    }

    // Invert
    if (impl_->inverted) {
        dst = cv::Scalar(1.0f) - dst;
    }

    double minValue = 0.0;
    double maxValue = 0.0;
    cv::minMaxLoc(dst, &minValue, &maxValue);
    if (maxValue <= 0.0) {
        qWarning() << "[MaskPath] rasterizeToAlpha: zero coverage"
                   << "name=" << impl_->name.toQString()
                   << "closed=" << impl_->closed
                   << "vertexCount=" << static_cast<int>(impl_->vertices.size())
                   << "size=" << QSize(width, height)
                   << "offset=" << QPointF(offsetX, offsetY);
    }
}

}
