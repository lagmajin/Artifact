module;

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

// Spline用単純な曲線クラス（実装-placeholder）
class SimpleSpline
{
public:
    struct Point
    {
        QVector3D position;
        QVector3D tangent;
    };

    void addPoint(const QVector3D& pos)
    {
        points_.push_back({pos, QVector3D(1, 0, 0)});
    }

    Point getPoint(float t) const
    {
        if (points_.empty())
            return {{0, 0, 0}, {1, 0, 0}};

        int idx = static_cast<int>(t * (points_.size() - 1));
        idx = std::max(0, std::min(idx, static_cast<int>(points_.size()) - 1));

        return points_[idx];
    }

    int pointCount() const { return static_cast<int>(points_.size()); }

private:
    std::vector<Point> points_;
};

} // namespace Artifact
