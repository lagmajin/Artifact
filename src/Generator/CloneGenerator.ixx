module;
#include <QVector>
#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <cmath>
#include <random>
#include <memory>

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
module Generator.Clone;




import Artifact.Effect.Clone.Core;

namespace Artifact {

    // ─────────────────────────────────────────────────────────
    // DistributionMode / TransformSpace definitions
    // ─────────────────────────────────────────────────────────
    enum class DistributionMode {
        Linear = 0,
        Radial,
        Grid2D,
        Grid3D,
        Spline,
        Random,
        Noise,
        Hexagonal,
        Spiral
    };

    enum class TransformSpace {
        Local = 0,
        World
    };

    class SimpleSpline {
    public:
        struct Point { QVector3D position; QVector3D tangent; };
        void setPoints(const QVector<Point>& points) { points_ = points; }
        void addPoint(const Point& point) { points_.push_back(point); }
        void clear() { points_.clear(); }
        Point getPoint(float t) const {
            if (points_.isEmpty()) return Point{{0, 0, 0}, {0, 0, 0}};
            if (points_.size() == 1) return points_.front();
            const float safeT = std::isfinite(t) ? std::clamp(t, 0.0f, 1.0f) : 0.0f;
            const float u = safeT *
                            static_cast<float>(points_.size() - 1);
            const int lastSegment = static_cast<int>(points_.size()) - 2;
            const int i1 = std::clamp(static_cast<int>(std::floor(u)), 0,
                                      lastSegment);
            const int i0 = std::max(0, i1 - 1);
            const int lastPoint = static_cast<int>(points_.size()) - 1;
            const int i2 = std::min(lastPoint, i1 + 1);
            const int i3 = std::min(lastPoint, i1 + 2);
            const float s = u - static_cast<float>(i1);
            const auto& p0 = points_[i0].position;
            const auto& p1 = points_[i1].position;
            const auto& p2 = points_[i2].position;
            const auto& p3 = points_[i3].position;
            const float s2 = s * s;
            const float s3 = s2 * s;
            const QVector3D position =
                0.5f * ((2.0f * p1) + (-p0 + p2) * s +
                        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * s2 +
                        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * s3);
            const QVector3D tangent =
                0.5f * ((-p0 + p2) +
                        2.0f * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * s +
                        3.0f * (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * s2);
            const float tangentLength = tangent.length();
            const QVector3D safeTangent = std::isfinite(tangentLength) &&
                                                   tangentLength > 1.0e-6f
                                               ? tangent / tangentLength
                                               : QVector3D(0.0f, 0.0f, 0.0f);
            return Point{position, safeTangent};
        }
        int pointCount() const { return points_.size(); }
    private:
        QVector<Point> points_;
    };

    // ─────────────────────────────────────────────────────────
    // CloneGenerator (ジェネレーターの実体)
    // ─────────────────────────────────────────────────────────
    class CloneGenerator {
    public:
        class Impl;
    private:
        Impl* impl_;
    public:
        CloneGenerator();
        ~CloneGenerator();

        void setCount(int count);
        int count() const;

        void setSpacing(float spacing);
        float spacing() const;

        void setPrototypeName(const QString& name);
        QString prototypeName() const;

        void setDistributionMode(DistributionMode mode);
        DistributionMode distributionMode() const;

        void setTransformSpace(TransformSpace space);
        TransformSpace transformSpace() const;

        void setRadius(float radius);
        float radius() const;

        void setGridColumns(int cols);
        int gridColumns() const;

        void setGridRows(int rows);
        int gridRows() const;

        void setGridDepth(int depth);
        int gridDepth() const;

        void setGridSpacingX(float spacing);
        float gridSpacingX() const;

        void setGridSpacingY(float spacing);
        float gridSpacingY() const;

        void setGridSpacingZ(float spacing);
        float gridSpacingZ() const;

        void setRandomSeed(int seed);
        int randomSeed() const;

        void setVariation(float variation);
        float variation() const;

        void setBounds(const QVector3D& bounds);
        QVector3D bounds() const;

        void setUsePoissonDisk(bool use);
        bool usePoissonDisk() const;

        void setSpiralRotations(float rotations);
        float spiralRotations() const;

        void setOffset(const QVector3D& offset);
        QVector3D offset() const;

        void setRotationStep(float degrees);
        float rotationStep() const;

        void setSpline(ArtifactCore::SharedPtr<SimpleSpline> spline);
        ArtifactCore::SharedPtr<SimpleSpline> spline() const;

        // 既存の QVector<QMatrix4x4> を返すメソッド（互換性のため残す）
        QVector<QMatrix4x4> generateTransforms() const;

        // 【NEW】Clonerアーキテクチャ用：CloneDataの配列を生成する
        std::vector<CloneData> generateCloneData() const;
    };
}
