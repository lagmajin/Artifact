module;
#include <utility>
#include <QString>
#include <QVector3D>

export module Generator.DistributionModes;

export namespace Artifact
{

enum class DistributionMode
{
    Linear,      // 線形（デフォルト）
    Radial,      // 放射状
    Grid2D,      // 2Dグリッド
    Grid3D,      // 3Dグリッド
    Spline,      // スプライン沿い
    Random,      // ランダム
    Noise,       // ノイズ分布
    Hexagonal,   // 六角形（蜂の巣）
    Spiral,      // 螺旋
};

enum class TransformSpace
{
    Local,       // ローカル座標
    World,       // ワールド座標
};

// Spline — Catmull-Rom 補間クラス
export class SimpleSpline
{
public:
    struct Point
    {
        QVector3D position;
        QVector3D tangent;
    };

    void addPoint(const QVector3D& pos)
    {
        points_.push_back(pos);
    }

    void setPoints(const std::vector<QVector3D>& pts)
    {
        points_ = pts;
    }

    Point getPoint(float t) const
    {
        if (points_.empty())
            return {{0, 0, 0}, {1, 0, 0}};

        t = std::clamp(t, 0.0f, 1.0f);
        int n = static_cast<int>(points_.size());

        if (n == 1) return {points_[0], {1, 0, 0}};

        // Catmull-Rom: map t to segment between points[i] and points[i+1]
        int segCount = n - 1;
        float segT = t * segCount;
        int i = static_cast<int>(segT);
        if (i >= segCount) i = segCount - 1;
        float localT = segT - i;

        // 4 control points: P[i-1], P[i], P[i+1], P[i+2]
        // Edge case handling: duplicate endpoints for first/last
        auto& p0 = (i > 0) ? points_[i - 1] : points_[i];
        auto& p1 = points_[i];
        auto& p2 = (i + 1 < n) ? points_[i + 1] : points_[i];
        auto& p3 = (i + 2 < n) ? points_[i + 2] : points_[i + 1 - segCount + 1];

        float t2 = localT * localT;
        float t3 = t2 * localT;
        float h0 = 2.0f * t3 - 3.0f * t2 + 1.0f;  // Hermite basis
        float h1 = -2.0f * t3 + 3.0f * t2;
        float h2 = t3 - 2.0f * t2 + localT;
        float h3 = t3 - t2;

        // Tangent at p1 and p2 (Catmull-Rom: 0.5*(p2 - p0), 0.5*(p3 - p1))
        QVector3D t1 = (p2 - p0) * 0.5f;
        QVector3D t2_ = (p3 - p1) * 0.5f;

        QVector3D pos = p1 * h0 + p2 * h1 + t1 * h2 + t2_ * h3;
        QVector3D tan = t1 * h2 + t2_ * h3;
        float len = tan.length();
        if (len > 0.001f) tan /= len;

        return {pos, tan};
    }

    const std::vector<QVector3D>& points() const { return points_; }
    int pointCount() const { return static_cast<int>(points_.size()); }

private:
    std::vector<QVector3D> points_;
};

} // namespace Artifact
