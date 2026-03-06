module;
#include <QVector>
#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <cmath>
#include <random>
#include <memory>

export module Generator.Clone;

import std;
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

    // Minimal placeholder for SimpleSpline
    class SimpleSpline {
    public:
        struct Point { QVector3D position; QVector3D tangent; };
        Point getPoint(float /*t*/) const { return Point{{0,0,0},{0,0,0}}; }
        int pointCount() const { return 0; }
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

        void setSpline(std::shared_ptr<SimpleSpline> spline);
        std::shared_ptr<SimpleSpline> spline() const;

        // 既存の QVector<QMatrix4x4> を返すメソッド（互換性のため残す）
        QVector<QMatrix4x4> generateTransforms() const;

        // 【NEW】MoGraphアーキテクチャ用：CloneDataの配列を生成する
        std::vector<CloneData> generateCloneData() const;
    };
}
