module;
#include <QPointF>
#include <QPolygonF>
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

// -- Impl --

class MaskPath::Impl {
public:
    std::vector<MaskVertex> vertices;
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

void MaskPath::rasterizeToAlpha(int width, int height, void* outMat) const
{
    cv::Mat& dst = *static_cast<cv::Mat*>(outMat);
    dst = cv::Mat::zeros(height, width, CV_32FC1);

    QPolygonF poly = impl_->toPolygon(16);
    if (poly.isEmpty()) return;

    // QPolygonF -> cv::Point array for fillPoly
    std::vector<cv::Point> pts;
    pts.reserve(poly.size());
    for (const auto& p : poly) {
        pts.emplace_back(static_cast<int>(std::round(p.x())),
                         static_cast<int>(std::round(p.y())));
    }

    // fillPoly on 8-bit image, then convert to float
    cv::Mat mask8(height, width, CV_8UC1, cv::Scalar(0));
    std::vector<std::vector<cv::Point>> contours = { pts };
    cv::fillPoly(mask8, contours, cv::Scalar(255));

    // Expansion: morphological dilate(+) or erode(-)
    float exp = impl_->expansion;
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
    if (impl_->feather > 0.5f) {
        int ksize = static_cast<int>(impl_->feather * 2.0f) | 1;
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
}

}
