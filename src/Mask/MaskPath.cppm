module;
#include <QDebug>
#include <QPointF>
#include <QPolygonF>
#include <QSize>
#include <QPainterPath>
#include <opencv2/opencv.hpp>
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
import Shape.Path;

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
    path.setFeatherHorizontal(snapshot.featherHorizontal);
    path.setFeatherVertical(snapshot.featherVertical);
    path.setFeatherInner(snapshot.featherInner);
    path.setFeatherOuter(snapshot.featherOuter);
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
    out.featherHorizontal = a.featherHorizontal + (b.featherHorizontal - a.featherHorizontal) * t;
    out.featherVertical = a.featherVertical + (b.featherVertical - a.featherVertical) * t;
    out.featherInner = a.featherInner + (b.featherInner - a.featherInner) * t;
    out.featherOuter = a.featherOuter + (b.featherOuter - a.featherOuter) * t;
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
    float featherHorizontal = 0.0f;
    float featherVertical = 0.0f;
    float featherInner = 0.0f;
    float featherOuter = 0.0f;
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
float MaskPath::featherHorizontal() const { return impl_->featherHorizontal; }
void MaskPath::setFeatherHorizontal(float feather) { impl_->featherHorizontal = std::max(0.0f, feather); }
float MaskPath::featherVertical() const { return impl_->featherVertical; }
void MaskPath::setFeatherVertical(float feather) { impl_->featherVertical = std::max(0.0f, feather); }
float MaskPath::featherInner() const { return impl_->featherInner; }
void MaskPath::setFeatherInner(float feather) { impl_->featherInner = std::max(0.0f, feather); }
float MaskPath::featherOuter() const { return impl_->featherOuter; }
void MaskPath::setFeatherOuter(float feather) { impl_->featherOuter = std::max(0.0f, feather); }

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

std::vector<MaskPath> MaskPath::fromShapePath(const ShapePath& path)
{
    std::vector<MaskPath> result;
    MaskPath currentMask;
    bool hasSubpath = false;
    QPointF currentPoint;

    const auto flushSubpath = [&]() {
        if (hasSubpath && currentMask.vertexCount() > 0) {
            result.push_back(currentMask);
        }
        currentMask = MaskPath();
        hasSubpath = false;
        currentPoint = QPointF();
    };

    const auto appendSegment = [&](const QPointF& control1,
                                   const QPointF& control2,
                                   const QPointF& end) {
        if (!hasSubpath || currentMask.vertexCount() == 0) {
            return;
        }
        MaskVertex previous = currentMask.vertex(currentMask.vertexCount() - 1);
        previous.outTangent = control1 - previous.position;
        currentMask.setVertex(currentMask.vertexCount() - 1, previous);

        MaskVertex next;
        next.position = end;
        next.inTangent = control2 - end;
        next.outTangent = QPointF();
        currentMask.addVertex(next);
        currentPoint = end;
    };

    for (const auto& command : path.commands()) {
        switch (command.type) {
        case PathCommandType::MoveTo: {
            flushSubpath();
            MaskVertex vertex;
            vertex.position = command.points[0];
            currentMask.addVertex(vertex);
            currentMask.setClosed(false);
            currentMask.setOpacity(static_cast<float>(path.opacity()));
            currentMask.setMode(MaskMode::Add);
            currentPoint = command.points[0];
            hasSubpath = true;
            break;
        }
        case PathCommandType::LineTo:
            appendSegment(currentPoint, command.points[0], command.points[0]);
            break;
        case PathCommandType::CubicTo:
            appendSegment(command.points[0], command.points[1], command.points[2]);
            break;
        case PathCommandType::QuadTo: {
            const QPointF end = command.points[1];
            const QPointF control = command.points[0];
            const QPointF cubic1 = currentPoint + (control - currentPoint) * (2.0 / 3.0);
            const QPointF cubic2 = end + (control - end) * (2.0 / 3.0);
            appendSegment(cubic1, cubic2, end);
            break;
        }
        case PathCommandType::Close:
            if (hasSubpath) {
                const int vertexCount = currentMask.vertexCount();
                if (vertexCount > 1) {
                    const MaskVertex first = currentMask.vertex(0);
                    const MaskVertex last = currentMask.vertex(vertexCount - 1);
                    const QPointF delta = last.position - first.position;
                    if (std::hypot(delta.x(), delta.y()) <= 1.0e-9) {
                        MaskVertex mergedFirst = first;
                        mergedFirst.inTangent = last.inTangent;
                        currentMask.setVertex(0, mergedFirst);
                        currentMask.removeVertex(vertexCount - 1);
                    }
                }
                currentMask.setClosed(true);
            }
            break;
        }
    }
    flushSubpath();
    return result;
}

ShapePath MaskPath::toShapePath() const
{
    ShapePath path;
    const int count = vertexCount();
    if (count <= 0) {
        return path;
    }

    const MaskVertex first = vertex(0);
    path.moveTo(first.position);
    const int segmentCount = isClosed() ? count : count - 1;
    for (int index = 0; index < segmentCount; ++index) {
        const MaskVertex from = vertex(index);
        const MaskVertex to = vertex((index + 1) % count);
        path.cubicTo(from.position + from.outTangent,
                     to.position + to.inTangent,
                     to.position);
    }
    if (isClosed()) {
        path.close();
    }
    path.setOpacity(opacity());
    return path;
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

    auto blurMask = [&](const cv::Mat& srcMask, float featherPxX, float featherPxY) {
        cv::Mat blurred = srcMask.clone();
        const float fx = featherPxX * scaleX;
        const float fy = featherPxY * scaleY;
        const int kx = fx > 0.5f ? (static_cast<int>(fx * 2.0f) | 1) : 0;
        const int ky = fy > 0.5f ? (static_cast<int>(fy * 2.0f) | 1) : 0;
        if (kx > 0 || ky > 0) {
            cv::GaussianBlur(blurred, blurred, cv::Size(std::max(1, kx), std::max(1, ky)), 0);
        }
        return blurred;
    };

    cv::Mat featherMask = mask8;
    const float uniformFeather = impl_->feather * ((scaleX + scaleY) * 0.5f);
    const float featherX = (impl_->featherHorizontal > 0.0f ? impl_->featherHorizontal : uniformFeather);
    const float featherY = (impl_->featherVertical > 0.0f ? impl_->featherVertical : uniformFeather);
    if (impl_->featherOuter > 0.0f || impl_->featherInner > 0.0f) {
        cv::Mat outerMask = mask8.clone();
        cv::Mat innerMask = mask8.clone();
        const int outerK = static_cast<int>(std::max(0.0f, impl_->featherOuter * ((scaleX + scaleY) * 0.5f)) * 2.0f) | 1;
        const int innerK = static_cast<int>(std::max(0.0f, impl_->featherInner * ((scaleX + scaleY) * 0.5f)) * 2.0f) | 1;
        if (outerK > 1) {
            cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(outerK, outerK));
            cv::dilate(outerMask, outerMask, kernel);
        }
        if (innerK > 1) {
            cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(innerK, innerK));
            cv::erode(innerMask, innerMask, kernel);
        }
        outerMask = blurMask(outerMask, featherX, featherY);
        innerMask = blurMask(innerMask, featherX, featherY);
        featherMask = cv::max(innerMask, outerMask);
    } else {
        featherMask = blurMask(mask8, featherX, featherY);
    }

    // Convert to float 0~1
    featherMask.convertTo(dst, CV_32FC1, 1.0 / 255.0);

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

// -- Static: fromQPainterPath --

std::vector<MaskPath> MaskPath::fromQPainterPath(
    const QPainterPath& path, const QString& text)
{
    std::vector<MaskPath> result;
    if (path.isEmpty())
        return result;

    int subpathIndex = 0;
    int i = 0;
    const int n = path.elementCount();

    while (i < n) {
        if (path.elementAt(i).type != QPainterPath::MoveToElement) {
            ++i;
            continue;
        }

        MaskPath maskPath;
        maskPath.setName(UniString(text));
        maskPath.setMode(subpathIndex == 0 ? MaskMode::Add : MaskMode::Subtract);

        QPointF startPoint(path.elementAt(i).x, path.elementAt(i).y);
        ++i;

        struct CubicSegment {
            QPointF end;
            QPointF c1Out;
            QPointF c2In;
        };
        std::vector<CubicSegment> segments;
        QPointF prev = startPoint;

        while (i < n) {
            const auto& e = path.elementAt(i);
            if (e.type == QPainterPath::MoveToElement)
                break;

            if (e.type == QPainterPath::LineToElement) {
                QPointF pt(e.x, e.y);
                segments.push_back({pt, prev, pt});
                prev = pt;
                ++i;
            }
            else if (e.type == QPainterPath::CurveToElement) {
                QPointF c1(e.x, e.y);
                QPointF c2(path.elementAt(i + 1).x, path.elementAt(i + 1).y);
                QPointF end(path.elementAt(i + 2).x, path.elementAt(i + 2).y);
                segments.push_back({end, c1, c2});
                prev = end;
                i += 3;
            }
            else {
                ++i;
            }
        }

        if (segments.empty())
            continue;

        const auto& lastSeg = segments.back();
        const double dx = lastSeg.end.x() - startPoint.x();
        const double dy = lastSeg.end.y() - startPoint.y();
        bool isClosed = (dx * dx + dy * dy) < 0.0001;

        for (std::size_t j = 0; j < segments.size(); ++j) {
            const auto& seg = segments[j];
            QPointF pos = seg.end;

            QPointF inTan(0, 0);
            if (j > 0) {
                const auto& prevSeg = segments[j - 1];
                inTan = prevSeg.c2In - pos;
            } else if (isClosed && segments.size() > 1) {
                const auto& lastSegRef = segments.back();
                inTan = lastSegRef.c2In - pos;
            }

            QPointF segStart = (j == 0) ? startPoint : segments[j - 1].end;
            QPointF outTan = seg.c1Out - segStart;

            MaskVertex v;
            v.position = pos;
            v.inTangent = inTan;
            v.outTangent = outTan;
            maskPath.addVertex(v);
        }

        if (isClosed && maskPath.vertexCount() >= 2) {
            const auto& lastV = maskPath.vertex(maskPath.vertexCount() - 1);
            const double dx2 = lastV.position.x() - startPoint.x();
            const double dy2 = lastV.position.y() - startPoint.y();
            if (dx2 * dx2 + dy2 * dy2 < 0.0001) {
                MaskVertex firstV = maskPath.vertex(0);
                firstV.outTangent = lastV.outTangent;
                maskPath.setVertex(0, firstV);
                maskPath.removeVertex(maskPath.vertexCount() - 1);
            }
        }

        maskPath.setClosed(isClosed);
        if (maskPath.vertexCount() >= 3 && isClosed) {
            result.push_back(std::move(maskPath));
        }
        ++subpathIndex;
    }
    return result;
}

std::vector<MaskPath> MaskPath::fromAlphaMask(
    const void* outMat, const MaskConversionParams& params)
{
    std::vector<MaskPath> result;
    if (!outMat) return result;
    const auto& input = *static_cast<const cv::Mat*>(outMat);
    if (input.empty() || input.channels() != 1 ||
        (input.depth() != CV_8U && input.depth() != CV_32F)) {
        return result;
    }

    cv::Mat binary;
    if (input.depth() == CV_8U) {
        cv::threshold(input, binary, 127, 255, cv::THRESH_BINARY);
    } else {
        cv::threshold(input, binary, 0.5, 255, cv::THRESH_BINARY);
        binary.convertTo(binary, CV_8U);
    }

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(binary, contours, hierarchy, cv::RETR_CCOMP,
                     cv::CHAIN_APPROX_NONE);
    const double tolerance = std::max(0.0f, params.simplificationTolerance);
    const int minimum = std::max(3, params.minPathVertices);
    for (std::size_t index = 0; index < contours.size(); ++index) {
        if (contours[index].size() < static_cast<std::size_t>(minimum)) continue;
        std::vector<cv::Point> simplified;
        cv::approxPolyDP(contours[index], simplified, tolerance,
                         params.closedPath);
        if (simplified.size() < static_cast<std::size_t>(minimum)) continue;

        MaskPath path;
        path.setClosed(params.closedPath);
        path.setMode(hierarchy.empty() || hierarchy[index][3] < 0
                         ? MaskMode::Add : MaskMode::Subtract);
        const float scaleX = 1.0f / static_cast<float>(input.cols);
        const float scaleY = 1.0f / static_cast<float>(input.rows);
        for (std::size_t vertex = 0; vertex < simplified.size(); ++vertex) {
            const auto& previous = simplified[(vertex + simplified.size() - 1) % simplified.size()];
            const auto& current = simplified[vertex];
            const auto& next = simplified[(vertex + 1) % simplified.size()];
            const QPointF position(current.x * scaleX, current.y * scaleY);
            const double inX = static_cast<double>(current.x - previous.x);
            const double inY = static_cast<double>(current.y - previous.y);
            const double outX = static_cast<double>(next.x - current.x);
            const double outY = static_cast<double>(next.y - current.y);
            const double inLength = std::hypot(inX, inY);
            const double outLength = std::hypot(outX, outY);
            const double cosine = (inLength > 0.0 && outLength > 0.0)
                ? std::clamp((inX * outX + inY * outY) /
                             (inLength * outLength), -1.0, 1.0) : 1.0;
            constexpr double kRadiansToDegrees = 57.29577951308232;
            const double angle = std::acos(cosine) * kRadiansToDegrees;
            const bool corner = angle >= std::max(0.0f, params.cornerThreshold);
            const QPointF tangent = corner
                ? QPointF(0.0, 0.0)
                : QPointF((next.x - previous.x) * scaleX / 6.0,
                          (next.y - previous.y) * scaleY / 6.0);
            path.addVertex({position, -tangent, tangent});
        }
        result.push_back(std::move(path));
    }
    return result;
}

}
