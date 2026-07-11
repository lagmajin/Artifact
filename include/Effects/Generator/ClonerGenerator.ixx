module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <QString>
#include <QVariant>
#include <QVector3D>
#include <QMatrix4x4>

export module Artifact.Effect.Generator.Cloner;

import Artifact.Effect.Clone.Core;
import Artifact.Effect.Clone.Basic;
import Artifact.Effect.Clone.Advanced;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class ClonerGenerator : public ArtifactAbstractEffect {
private:
    enum class Mode {
        Linear = 0,
        Grid = 1,
        Radial = 2,
        Object = 3,
        Hexagonal = 4,
        Spiral = 5,
        Spline = 6
    };

    Mode mode_ = Mode::Linear;
    int count_ = 3;
    QVector3D offset_ = QVector3D(160.0f, 48.0f, 0.0f);
    int gridColumns_ = 4;
    int gridRows_ = 3;
    int gridDepth_ = 1;
    QVector3D gridSpacing_ = QVector3D(160.0f, 120.0f, 0.0f);
    int radialCount_ = 8;
    float radius_ = 240.0f;
    float startAngle_ = 0.0f;
    float endAngle_ = 360.0f;

    // Object mode
    std::vector<QVector3D> objectVertices_;
    float objectScale_ = 1.0f;

    // Hexagonal mode
    int hexColumns_ = 5;
    int hexRows_ = 5;
    float hexRadius_ = 60.0f;

    // Spiral mode
    int spiralCount_ = 50;
    float spiralTurns_ = 3.0f;
    float spiralRadiusStart_ = 0.0f;
    float spiralRadiusEnd_ = 240.0f;
    float spiralHeight_ = 0.0f;

    // Spline mode
    std::vector<QVector3D> splinePoints_;
    int splineCount_ = 20;

    bool useTransformEffector_ = false;
    QVector3D effectorPositionOffset_ = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D effectorRotationOffset_ = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D effectorScaleOffset_ = QVector3D(0.0f, 0.0f, 0.0f);
    bool useRandomEffector_ = false;
    int randomEffectorSeed_ = 12345;
    QVector3D randomPositionVariance_ = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D randomRotationVariance_ = QVector3D(0.0f, 0.0f, 0.0f);
    float randomScaleVariance_ = 0.0f;
    float rotationStep_ = 0.0f;
    float opacityDecay_ = 0.0f;

public:
    ClonerGenerator() {
        setDisplayName(ArtifactCore::UniString("Cloner (Generator)"));
        setPipelineStage(EffectPipelineStage::Generator);
    }

    virtual ~ClonerGenerator() = default;

    Mode mode() const { return mode_; }
    void setMode(Mode mode) { mode_ = mode; }

    int count() const { return count_; }
    void setCount(int count) { count_ = std::max(1, count); }

    QVector3D offset() const { return offset_; }
    void setOffset(const QVector3D& offset) { offset_ = offset; }

    int gridColumns() const { return gridColumns_; }
    void setGridColumns(int columns) { gridColumns_ = std::max(1, columns); }

    int gridRows() const { return gridRows_; }
    void setGridRows(int rows) { gridRows_ = std::max(1, rows); }

    int gridDepth() const { return gridDepth_; }
    void setGridDepth(int depth) { gridDepth_ = std::max(1, depth); }

    QVector3D gridSpacing() const { return gridSpacing_; }
    void setGridSpacing(const QVector3D& spacing) { gridSpacing_ = spacing; }

    int radialCount() const { return radialCount_; }
    void setRadialCount(int count) { radialCount_ = std::max(1, count); }

    float radius() const { return radius_; }
    void setRadius(float radius) { radius_ = radius; }

    float startAngle() const { return startAngle_; }
    void setStartAngle(float angle) { startAngle_ = angle; }

    float endAngle() const { return endAngle_; }
    void setEndAngle(float angle) { endAngle_ = angle; }

    bool useTransformEffector() const { return useTransformEffector_; }
    void setUseTransformEffector(bool use) { useTransformEffector_ = use; }

    QVector3D effectorPositionOffset() const { return effectorPositionOffset_; }
    void setEffectorPositionOffset(const QVector3D& offset) { effectorPositionOffset_ = offset; }

    QVector3D effectorRotationOffset() const { return effectorRotationOffset_; }
    void setEffectorRotationOffset(const QVector3D& offset) { effectorRotationOffset_ = offset; }

    QVector3D effectorScaleOffset() const { return effectorScaleOffset_; }
    void setEffectorScaleOffset(const QVector3D& offset) { effectorScaleOffset_ = offset; }

    bool useRandomEffector() const { return useRandomEffector_; }
    void setUseRandomEffector(bool use) { useRandomEffector_ = use; }

    int randomEffectorSeed() const { return randomEffectorSeed_; }
    void setRandomEffectorSeed(int seed) { randomEffectorSeed_ = seed; }

    QVector3D randomPositionVariance() const { return randomPositionVariance_; }
    void setRandomPositionVariance(const QVector3D& variance) { randomPositionVariance_ = variance; }

    QVector3D randomRotationVariance() const { return randomRotationVariance_; }
    void setRandomRotationVariance(const QVector3D& variance) { randomRotationVariance_ = variance; }

    float randomScaleVariance() const { return randomScaleVariance_; }
    void setRandomScaleVariance(float variance) { randomScaleVariance_ = variance; }

    // Object mode
    const std::vector<QVector3D>& objectVertices() const { return objectVertices_; }
    void setObjectVertices(const std::vector<QVector3D>& verts) { objectVertices_ = verts; }
    float objectScale() const { return objectScale_; }
    void setObjectScale(float s) { objectScale_ = s; }

    // Hexagonal mode
    int hexColumns() const { return hexColumns_; }
    void setHexColumns(int cols) { hexColumns_ = std::max(1, cols); }
    int hexRows() const { return hexRows_; }
    void setHexRows(int rows) { hexRows_ = std::max(1, rows); }
    float hexRadius() const { return hexRadius_; }
    void setHexRadius(float r) { hexRadius_ = std::max(1.0f, r); }

    // Spiral mode
    int spiralCount() const { return spiralCount_; }
    void setSpiralCount(int c) { spiralCount_ = std::max(1, c); }
    float spiralTurns() const { return spiralTurns_; }
    void setSpiralTurns(float t) { spiralTurns_ = std::max(0.1f, t); }
    float spiralRadiusStart() const { return spiralRadiusStart_; }
    void setSpiralRadiusStart(float r) { spiralRadiusStart_ = r; }
    float spiralRadiusEnd() const { return spiralRadiusEnd_; }
    void setSpiralRadiusEnd(float r) { spiralRadiusEnd_ = r; }
    float spiralHeight() const { return spiralHeight_; }
    void setSpiralHeight(float h) { spiralHeight_ = h; }

    // Spline mode
    const std::vector<QVector3D>& splinePoints() const { return splinePoints_; }
    void setSplinePoints(const std::vector<QVector3D>& pts) { splinePoints_ = pts; }
    void addSplinePoint(const QVector3D& pt) { splinePoints_.push_back(pt); }
    int splineCount() const { return splineCount_; }
    void setSplineCount(int c) { splineCount_ = std::max(1, c); }

    float rotationStep() const { return rotationStep_; }
    void setRotationStep(float step) { rotationStep_ = step; }

    float opacityDecay() const { return opacityDecay_; }
    void setOpacityDecay(float decay) { opacityDecay_ = std::clamp(decay, 0.0f, 1.0f); }

    std::vector<CloneData> generateCloneData() const
    {
        std::vector<CloneData> clones;
        if (mode_ == Mode::Grid) {
            const int cols = std::max(1, gridColumns_);
            const int rows = std::max(1, gridRows_);
            const int depth = std::max(1, gridDepth_);
            const int total = cols * rows * depth;
            clones.reserve(static_cast<size_t>(total));

            const QVector3D startPos =
                -gridSpacing_ * QVector3D(cols - 1, rows - 1, depth - 1) * 0.5f;

            for (int z = 0; z < depth; ++z) {
                for (int y = 0; y < rows; ++y) {
                    for (int x = 0; x < cols; ++x) {
                        CloneData clone;
                        clone.index = static_cast<int>(clones.size());
                        clone.transform.setToIdentity();
                        const QVector3D pos =
                            startPos + QVector3D(x * gridSpacing_.x(),
                                                 y * gridSpacing_.y(),
                                                 z * gridSpacing_.z());
                        clone.transform.translate(pos);
                        if (rotationStep_ != 0.0f) {
                            clone.transform.rotate(rotationStep_ * clone.index, 0.0f, 0.0f, 1.0f);
                        }
                        clone.weight = std::clamp(
                            1.0f - opacityDecay_ * static_cast<float>(clone.index), 0.0f, 1.0f);
                        clone.visible = true;
                        clones.push_back(clone);
                    }
                }
            }
        } else if (mode_ == Mode::Radial) {
            const int total = std::max(1, radialCount_);
            clones.reserve(static_cast<size_t>(total));
            const float angleStep = total > 1
                                        ? (endAngle_ - startAngle_) /
                                              static_cast<float>(total - 1)
                                        : 0.0f;

            for (int i = 0; i < total; ++i) {
                CloneData clone;
                clone.index = i;
                const float angle = startAngle_ + angleStep * static_cast<float>(i);
                const float rad = angle * static_cast<float>(M_PI) / 180.0f;

                clone.transform.setToIdentity();
                clone.transform.translate(std::cos(rad) * radius_,
                                          std::sin(rad) * radius_,
                                          0.0f);
                clone.transform.rotate(angle + rotationStep_ * static_cast<float>(i),
                                       0.0f, 0.0f, 1.0f);
                clone.weight = std::clamp(
                    1.0f - opacityDecay_ * static_cast<float>(i), 0.0f, 1.0f);
                clone.visible = true;
                clones.push_back(clone);
            }
        } else if (mode_ == Mode::Object) {
            if (objectVertices_.empty()) {
                // Fallback: 8 corners of unit cube scaled by radius
                float r = radius_ * 0.5f;
                for (int i = 0; i < 8; ++i) {
                    CloneData clone;
                    clone.index = i;
                    clone.transform.setToIdentity();
                    clone.transform.translate(
                        (i & 1 ? r : -r) * objectScale_,
                        (i & 2 ? r : -r) * objectScale_,
                        (i & 4 ? r : -r) * objectScale_);
                    clone.weight = 1.0f;
                    clone.visible = true;
                    clones.push_back(clone);
                }
            } else {
                size_t total = objectVertices_.size();
                clones.reserve(total);
                for (size_t i = 0; i < total; ++i) {
                    CloneData clone;
                    clone.index = static_cast<int>(i);
                    clone.transform.setToIdentity();
                    clone.transform.translate(objectVertices_[i] * objectScale_);
                    clone.weight = 1.0f;
                    clone.visible = true;
                    clones.push_back(clone);
                }
            }
        } else if (mode_ == Mode::Hexagonal) {
            const int cols = std::max(1, hexColumns_);
            const int rows = std::max(1, hexRows_);
            const float hSpacing = hexRadius_ * 2.0f;
            const float vSpacing = hexRadius_ * 1.73205f; // sqrt(3)
            clones.reserve(static_cast<size_t>(cols) * rows);
            for (int r = 0; r < rows; ++r) {
                float xOff = (r % 2 == 0) ? 0.0f : hexRadius_;
                for (int c = 0; c < cols; ++c) {
                    CloneData clone;
                    clone.index = static_cast<int>(clones.size());
                    clone.transform.setToIdentity();
                    clone.transform.translate(
                        (c - (cols - 1) * 0.5f) * hSpacing + xOff,
                        (r - (rows - 1) * 0.5f) * vSpacing,
                        0.0f);
                    if (rotationStep_ != 0.0f) {
                        clone.transform.rotate(rotationStep_ * clone.index, 0.0f, 0.0f, 1.0f);
                    }
                    clone.weight = std::clamp(1.0f - opacityDecay_ * static_cast<float>(clone.index), 0.0f, 1.0f);
                    clone.visible = true;
                    clones.push_back(clone);
                }
            }
        } else if (mode_ == Mode::Spiral) {
            const int total = std::max(1, spiralCount_);
            clones.reserve(static_cast<size_t>(total));
            for (int i = 0; i < total; ++i) {
                CloneData clone;
                clone.index = i;
                float t = static_cast<float>(i) / static_cast<float>(total);
                float angle = t * spiralTurns_ * 2.0f * static_cast<float>(M_PI);
                float r = spiralRadiusStart_ + (spiralRadiusEnd_ - spiralRadiusStart_) * t;
                float h = spiralHeight_ * t;
                clone.transform.setToIdentity();
                clone.transform.translate(std::cos(angle) * r, std::sin(angle) * r, h);
                if (rotationStep_ != 0.0f) {
                    clone.transform.rotate(rotationStep_ * static_cast<float>(i), 0.0f, 0.0f, 1.0f);
                }
                clone.weight = std::clamp(1.0f - opacityDecay_ * static_cast<float>(i), 0.0f, 1.0f);
                clone.visible = true;
                clones.push_back(clone);
            }
        } else if (mode_ == Mode::Spline) {
            auto catmullPos = [](const std::vector<QVector3D>& pts, float t) -> QVector3D {
                int n = static_cast<int>(pts.size());
                if (n == 0) return {};
                if (n == 1) return pts[0];
                t = std::clamp(t, 0.0f, 1.0f);
                int segCount = n - 1;
                float segT = t * segCount;
                int i = static_cast<int>(segT);
                if (i >= segCount) i = segCount - 1;
                float lt = segT - i;
                auto& p0 = (i > 0) ? pts[i - 1] : pts[i];
                auto& p1 = pts[i];
                auto& p2 = (i + 1 < n) ? pts[i + 1] : pts[i];
                auto& p3 = (i + 2 < n) ? pts[i + 2] : pts[i + 1];
                float t2 = lt * lt, t3 = t2 * lt;
                QVector3D t1 = (p2 - p0) * 0.5f;
                QVector3D t2_ = (p3 - p1) * 0.5f;
                return p1 * (2.0f*t3 - 3.0f*t2 + 1.0f)
                     + p2 * (-2.0f*t3 + 3.0f*t2)
                     + t1 * (t3 - 2.0f*t2 + lt)
                     + t2_ * (t3 - t2);
            };
            const int total = std::max(1, splineCount_);
            clones.reserve(static_cast<size_t>(total));
            const float sampleDenominator =
                static_cast<float>(std::max(1, total - 1));
            if (splinePoints_.size() >= 2) {
                for (int i = 0; i < total; ++i) {
                    CloneData clone;
                    clone.index = i;
                    float t = static_cast<float>(i) / sampleDenominator;
                    QVector3D pos = catmullPos(splinePoints_, t);
                    clone.transform.setToIdentity();
                    clone.transform.translate(pos);
                    if (rotationStep_ != 0.0f) {
                        clone.transform.rotate(rotationStep_ * static_cast<float>(i), 0.0f, 0.0f, 1.0f);
                    }
                    clone.weight = std::clamp(1.0f - opacityDecay_ * static_cast<float>(i), 0.0f, 1.0f);
                    clone.visible = true;
                    clones.push_back(clone);
                }
            } else {
                // Fallback: linear along X
                for (int i = 0; i < total; ++i) {
                    CloneData clone;
                    clone.index = i;
                    float x = (static_cast<float>(i) / sampleDenominator - 0.5f) * 400.0f;
                    clone.transform.setToIdentity();
                    clone.transform.translate(x, 0, 0);
                    clone.weight = std::clamp(1.0f - opacityDecay_ * static_cast<float>(i), 0.0f, 1.0f);
                    clone.visible = true;
                    clones.push_back(clone);
                }
            }
        } else {
            const int total = std::max(1, count_);
            clones.reserve(static_cast<size_t>(total));

            for (int i = 1; i <= total; ++i) {
                CloneData clone;
                const int cloneIndex = i - 1;
                clone.index = cloneIndex;
                clone.transform.setToIdentity();
                clone.transform.translate(offset_.x() * static_cast<float>(i),
                                          offset_.y() * static_cast<float>(i),
                                          offset_.z() * static_cast<float>(i));
                if (rotationStep_ != 0.0f) {
                    clone.transform.rotate(rotationStep_ * static_cast<float>(i), 0.0f, 0.0f, 1.0f);
                }
                clone.weight = std::clamp(1.0f - opacityDecay_ * static_cast<float>(cloneIndex), 0.0f, 1.0f);
                clone.visible = true;
                clones.push_back(clone);
            }
        }

        if (useTransformEffector_) {
            auto base = clones;
            TransformCloneEffector effector;
            effector.positionOffset = effectorPositionOffset_;
            effector.rotationOffset = effectorRotationOffset_;
            effector.scaleOffset = effectorScaleOffset_;
            effector.applyToClones(clones);
            for (size_t i = 0; i < clones.size() && i < base.size(); ++i)
                blendCloneData(base[i], clones[i], effector.blendMode, effector.strength);
            clones = std::move(base);
        }

        if (useRandomEffector_) {
            auto base = clones;
            RandomCloneEffector effector;
            effector.seed = randomEffectorSeed_;
            effector.positionVariance = randomPositionVariance_;
            effector.rotationVariance = randomRotationVariance_;
            effector.scaleVariance = randomScaleVariance_;
            effector.applyToClones(clones);
            for (size_t i = 0; i < clones.size() && i < base.size(); ++i)
                blendCloneData(base[i], clones[i], effector.blendMode, effector.strength);
            clones = std::move(base);
        }

        return clones;
    }

    std::vector<AbstractProperty> getProperties() const override
    {
        std::vector<AbstractProperty> props;

        AbstractProperty modeProp;
        modeProp.setName("Clone Mode");
        modeProp.setType(PropertyType::Integer);
        modeProp.setValue(static_cast<int>(mode_));
        modeProp.setTooltip(QStringLiteral("0=Linear,1=Grid,2=Radial,3=Object,4=Hexagonal,5=Spiral,6=Spline"));
        props.push_back(modeProp);

        if (mode_ == Mode::Grid) {
            AbstractProperty columnsProp;
            columnsProp.setName("Grid Columns");
            columnsProp.setType(PropertyType::Integer);
            columnsProp.setValue(gridColumns_);
            columnsProp.setHardRange(1, 128);
            columnsProp.setSoftRange(1, 16);
            props.push_back(columnsProp);

            AbstractProperty rowsProp;
            rowsProp.setName("Grid Rows");
            rowsProp.setType(PropertyType::Integer);
            rowsProp.setValue(gridRows_);
            rowsProp.setHardRange(1, 128);
            rowsProp.setSoftRange(1, 16);
            props.push_back(rowsProp);

            AbstractProperty depthProp;
            depthProp.setName("Grid Depth");
            depthProp.setType(PropertyType::Integer);
            depthProp.setValue(gridDepth_);
            depthProp.setHardRange(1, 64);
            depthProp.setSoftRange(1, 8);
            props.push_back(depthProp);

            AbstractProperty spacingXProp;
            spacingXProp.setName("Grid Spacing X");
            spacingXProp.setType(PropertyType::Float);
            spacingXProp.setValue(gridSpacing_.x());
            props.push_back(spacingXProp);

            AbstractProperty spacingYProp;
            spacingYProp.setName("Grid Spacing Y");
            spacingYProp.setType(PropertyType::Float);
            spacingYProp.setValue(gridSpacing_.y());
            props.push_back(spacingYProp);

            AbstractProperty spacingZProp;
            spacingZProp.setName("Grid Spacing Z");
            spacingZProp.setType(PropertyType::Float);
            spacingZProp.setValue(gridSpacing_.z());
            props.push_back(spacingZProp);
        } else if (mode_ == Mode::Radial) {
            AbstractProperty radialCountProp;
            radialCountProp.setName("Clone Count");
            radialCountProp.setType(PropertyType::Integer);
            radialCountProp.setValue(radialCount_);
            radialCountProp.setHardRange(1, 512);
            radialCountProp.setSoftRange(1, 32);
            props.push_back(radialCountProp);

            AbstractProperty radiusProp;
            radiusProp.setName("Radial Radius");
            radiusProp.setType(PropertyType::Float);
            radiusProp.setValue(radius_);
            props.push_back(radiusProp);

            AbstractProperty startAngleProp;
            startAngleProp.setName("Radial Start Angle");
            startAngleProp.setType(PropertyType::Float);
            startAngleProp.setValue(startAngle_);
            props.push_back(startAngleProp);

            AbstractProperty endAngleProp;
            endAngleProp.setName("Radial End Angle");
            endAngleProp.setType(PropertyType::Float);
            endAngleProp.setValue(endAngle_);
            props.push_back(endAngleProp);
        } else if (mode_ == Mode::Object) {
            AbstractProperty objectScaleProp;
            objectScaleProp.setName("Object Scale");
            objectScaleProp.setType(PropertyType::Float);
            objectScaleProp.setValue(objectScale_);
            objectScaleProp.setHardRange(0.01f, 100.0f);
            props.push_back(objectScaleProp);
        } else if (mode_ == Mode::Hexagonal) {
            AbstractProperty hexColsProp;
            hexColsProp.setName("Hex Columns");
            hexColsProp.setType(PropertyType::Integer);
            hexColsProp.setValue(hexColumns_);
            hexColsProp.setHardRange(1, 128);
            hexColsProp.setSoftRange(1, 16);
            props.push_back(hexColsProp);

            AbstractProperty hexRowsProp;
            hexRowsProp.setName("Hex Rows");
            hexRowsProp.setType(PropertyType::Integer);
            hexRowsProp.setValue(hexRows_);
            hexRowsProp.setHardRange(1, 128);
            hexRowsProp.setSoftRange(1, 16);
            props.push_back(hexRowsProp);

            AbstractProperty hexRadiusProp;
            hexRadiusProp.setName("Hex Radius");
            hexRadiusProp.setType(PropertyType::Float);
            hexRadiusProp.setValue(hexRadius_);
            hexRadiusProp.setHardRange(1.0f, 1000.0f);
            props.push_back(hexRadiusProp);
        } else if (mode_ == Mode::Spiral) {
            AbstractProperty spiralCountProp;
            spiralCountProp.setName("Spiral Count");
            spiralCountProp.setType(PropertyType::Integer);
            spiralCountProp.setValue(spiralCount_);
            spiralCountProp.setHardRange(1, 2048);
            spiralCountProp.setSoftRange(1, 100);
            props.push_back(spiralCountProp);

            AbstractProperty spiralTurnsProp;
            spiralTurnsProp.setName("Spiral Turns");
            spiralTurnsProp.setType(PropertyType::Float);
            spiralTurnsProp.setValue(spiralTurns_);
            spiralTurnsProp.setHardRange(0.1f, 100.0f);
            props.push_back(spiralTurnsProp);

            AbstractProperty spiralRadiusStartProp;
            spiralRadiusStartProp.setName("Spiral Radius Start");
            spiralRadiusStartProp.setType(PropertyType::Float);
            spiralRadiusStartProp.setValue(spiralRadiusStart_);
            props.push_back(spiralRadiusStartProp);

            AbstractProperty spiralRadiusEndProp;
            spiralRadiusEndProp.setName("Spiral Radius End");
            spiralRadiusEndProp.setType(PropertyType::Float);
            spiralRadiusEndProp.setValue(spiralRadiusEnd_);
            props.push_back(spiralRadiusEndProp);

            AbstractProperty spiralHeightProp;
            spiralHeightProp.setName("Spiral Height");
            spiralHeightProp.setType(PropertyType::Float);
            spiralHeightProp.setValue(spiralHeight_);
            props.push_back(spiralHeightProp);
        } else if (mode_ == Mode::Spline) {
            AbstractProperty splineCountProp;
            splineCountProp.setName("Spline Clone Count");
            splineCountProp.setType(PropertyType::Integer);
            splineCountProp.setValue(splineCount_);
            splineCountProp.setHardRange(1, 2048);
            splineCountProp.setSoftRange(1, 100);
            props.push_back(splineCountProp);
        } else {
            AbstractProperty countProp;
            countProp.setName("Clone Count");
            countProp.setType(PropertyType::Integer);
            countProp.setValue(count_);
            countProp.setHardRange(1, 512);
            countProp.setSoftRange(1, 32);
            props.push_back(countProp);

            AbstractProperty offsetXProp;
            offsetXProp.setName("Linear Offset X");
            offsetXProp.setType(PropertyType::Float);
            offsetXProp.setValue(offset_.x());
            props.push_back(offsetXProp);

            AbstractProperty offsetYProp;
            offsetYProp.setName("Linear Offset Y");
            offsetYProp.setType(PropertyType::Float);
            offsetYProp.setValue(offset_.y());
            props.push_back(offsetYProp);

            AbstractProperty offsetZProp;
            offsetZProp.setName("Linear Offset Z");
            offsetZProp.setType(PropertyType::Float);
            offsetZProp.setValue(offset_.z());
            props.push_back(offsetZProp);
        }

        AbstractProperty rotationProp;
        rotationProp.setName("Rotation Step");
        rotationProp.setType(PropertyType::Float);
        rotationProp.setValue(rotationStep_);
        props.push_back(rotationProp);

        AbstractProperty opacityProp;
        opacityProp.setName("Opacity Decay");
        opacityProp.setType(PropertyType::Float);
        opacityProp.setValue(opacityDecay_);
        props.push_back(opacityProp);

        AbstractProperty useTransformEffectorProp;
        useTransformEffectorProp.setName("Use Transform Effector");
        useTransformEffectorProp.setType(PropertyType::Boolean);
        useTransformEffectorProp.setValue(useTransformEffector_);
        props.push_back(useTransformEffectorProp);

        if (useTransformEffector_) {
            AbstractProperty effectorPosXProp;
            effectorPosXProp.setName("Effector Position X");
            effectorPosXProp.setType(PropertyType::Float);
            effectorPosXProp.setValue(effectorPositionOffset_.x());
            props.push_back(effectorPosXProp);

            AbstractProperty effectorPosYProp;
            effectorPosYProp.setName("Effector Position Y");
            effectorPosYProp.setType(PropertyType::Float);
            effectorPosYProp.setValue(effectorPositionOffset_.y());
            props.push_back(effectorPosYProp);

            AbstractProperty effectorPosZProp;
            effectorPosZProp.setName("Effector Position Z");
            effectorPosZProp.setType(PropertyType::Float);
            effectorPosZProp.setValue(effectorPositionOffset_.z());
            props.push_back(effectorPosZProp);

            AbstractProperty effectorRotXProp;
            effectorRotXProp.setName("Effector Rotation X");
            effectorRotXProp.setType(PropertyType::Float);
            effectorRotXProp.setValue(effectorRotationOffset_.x());
            props.push_back(effectorRotXProp);

            AbstractProperty effectorRotYProp;
            effectorRotYProp.setName("Effector Rotation Y");
            effectorRotYProp.setType(PropertyType::Float);
            effectorRotYProp.setValue(effectorRotationOffset_.y());
            props.push_back(effectorRotYProp);

            AbstractProperty effectorRotZProp;
            effectorRotZProp.setName("Effector Rotation Z");
            effectorRotZProp.setType(PropertyType::Float);
            effectorRotZProp.setValue(effectorRotationOffset_.z());
            props.push_back(effectorRotZProp);

            AbstractProperty effectorScaleXProp;
            effectorScaleXProp.setName("Effector Scale X");
            effectorScaleXProp.setType(PropertyType::Float);
            effectorScaleXProp.setValue(effectorScaleOffset_.x());
            props.push_back(effectorScaleXProp);

            AbstractProperty effectorScaleYProp;
            effectorScaleYProp.setName("Effector Scale Y");
            effectorScaleYProp.setType(PropertyType::Float);
            effectorScaleYProp.setValue(effectorScaleOffset_.y());
            props.push_back(effectorScaleYProp);

            AbstractProperty effectorScaleZProp;
            effectorScaleZProp.setName("Effector Scale Z");
            effectorScaleZProp.setType(PropertyType::Float);
            effectorScaleZProp.setValue(effectorScaleOffset_.z());
            props.push_back(effectorScaleZProp);
        }

        AbstractProperty useRandomEffectorProp;
        useRandomEffectorProp.setName("Use Random Effector");
        useRandomEffectorProp.setType(PropertyType::Boolean);
        useRandomEffectorProp.setValue(useRandomEffector_);
        props.push_back(useRandomEffectorProp);

        if (useRandomEffector_) {
            AbstractProperty randomSeedProp;
            randomSeedProp.setName("Random Seed");
            randomSeedProp.setType(PropertyType::Integer);
            randomSeedProp.setValue(randomEffectorSeed_);
            props.push_back(randomSeedProp);

            AbstractProperty randomPosXProp;
            randomPosXProp.setName("Random Position X");
            randomPosXProp.setType(PropertyType::Float);
            randomPosXProp.setValue(randomPositionVariance_.x());
            props.push_back(randomPosXProp);

            AbstractProperty randomPosYProp;
            randomPosYProp.setName("Random Position Y");
            randomPosYProp.setType(PropertyType::Float);
            randomPosYProp.setValue(randomPositionVariance_.y());
            props.push_back(randomPosYProp);

            AbstractProperty randomPosZProp;
            randomPosZProp.setName("Random Position Z");
            randomPosZProp.setType(PropertyType::Float);
            randomPosZProp.setValue(randomPositionVariance_.z());
            props.push_back(randomPosZProp);

            AbstractProperty randomRotXProp;
            randomRotXProp.setName("Random Rotation X");
            randomRotXProp.setType(PropertyType::Float);
            randomRotXProp.setValue(randomRotationVariance_.x());
            props.push_back(randomRotXProp);

            AbstractProperty randomRotYProp;
            randomRotYProp.setName("Random Rotation Y");
            randomRotYProp.setType(PropertyType::Float);
            randomRotYProp.setValue(randomRotationVariance_.y());
            props.push_back(randomRotYProp);

            AbstractProperty randomRotZProp;
            randomRotZProp.setName("Random Rotation Z");
            randomRotZProp.setType(PropertyType::Float);
            randomRotZProp.setValue(randomRotationVariance_.z());
            props.push_back(randomRotZProp);

            AbstractProperty randomScaleProp;
            randomScaleProp.setName("Random Scale");
            randomScaleProp.setType(PropertyType::Float);
            randomScaleProp.setValue(randomScaleVariance_);
            props.push_back(randomScaleProp);
        }

        return props;
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override
    {
        const QString key = name.toQString();
        if (key == QStringLiteral("Clone Mode")) {
            setMode(static_cast<Mode>(std::clamp(value.toInt(), 0, 6)));
        } else if (key == QStringLiteral("Clone Count")) {
            if (mode_ == Mode::Radial) {
                setRadialCount(value.toInt());
            } else {
                setCount(value.toInt());
            }
        } else if (key == QStringLiteral("Linear Offset X")) {
            offset_.setX(value.toFloat());
        } else if (key == QStringLiteral("Linear Offset Y")) {
            offset_.setY(value.toFloat());
        } else if (key == QStringLiteral("Linear Offset Z")) {
            offset_.setZ(value.toFloat());
        } else if (key == QStringLiteral("Grid Columns")) {
            setGridColumns(value.toInt());
        } else if (key == QStringLiteral("Grid Rows")) {
            setGridRows(value.toInt());
        } else if (key == QStringLiteral("Grid Depth")) {
            setGridDepth(value.toInt());
        } else if (key == QStringLiteral("Grid Spacing X")) {
            gridSpacing_.setX(value.toFloat());
        } else if (key == QStringLiteral("Grid Spacing Y")) {
            gridSpacing_.setY(value.toFloat());
        } else if (key == QStringLiteral("Grid Spacing Z")) {
            gridSpacing_.setZ(value.toFloat());
        } else if (key == QStringLiteral("Radial Radius")) {
            setRadius(value.toFloat());
        } else if (key == QStringLiteral("Radial Start Angle")) {
            setStartAngle(value.toFloat());
        } else if (key == QStringLiteral("Radial End Angle")) {
            setEndAngle(value.toFloat());
        } else if (key == QStringLiteral("Rotation Step")) {
            setRotationStep(value.toFloat());
        } else if (key == QStringLiteral("Opacity Decay")) {
            setOpacityDecay(value.toFloat());
        } else if (key == QStringLiteral("Use Transform Effector")) {
            setUseTransformEffector(value.toBool());
        } else if (key == QStringLiteral("Effector Position X")) {
            effectorPositionOffset_.setX(value.toFloat());
        } else if (key == QStringLiteral("Effector Position Y")) {
            effectorPositionOffset_.setY(value.toFloat());
        } else if (key == QStringLiteral("Effector Position Z")) {
            effectorPositionOffset_.setZ(value.toFloat());
        } else if (key == QStringLiteral("Effector Rotation X")) {
            effectorRotationOffset_.setX(value.toFloat());
        } else if (key == QStringLiteral("Effector Rotation Y")) {
            effectorRotationOffset_.setY(value.toFloat());
        } else if (key == QStringLiteral("Effector Rotation Z")) {
            effectorRotationOffset_.setZ(value.toFloat());
        } else if (key == QStringLiteral("Effector Scale X")) {
            effectorScaleOffset_.setX(value.toFloat());
        } else if (key == QStringLiteral("Effector Scale Y")) {
            effectorScaleOffset_.setY(value.toFloat());
        } else if (key == QStringLiteral("Effector Scale Z")) {
            effectorScaleOffset_.setZ(value.toFloat());
        } else if (key == QStringLiteral("Use Random Effector")) {
            setUseRandomEffector(value.toBool());
        } else if (key == QStringLiteral("Random Seed")) {
            setRandomEffectorSeed(value.toInt());
        } else if (key == QStringLiteral("Random Position X")) {
            randomPositionVariance_.setX(value.toFloat());
        } else if (key == QStringLiteral("Random Position Y")) {
            randomPositionVariance_.setY(value.toFloat());
        } else if (key == QStringLiteral("Random Position Z")) {
            randomPositionVariance_.setZ(value.toFloat());
        } else if (key == QStringLiteral("Random Rotation X")) {
            randomRotationVariance_.setX(value.toFloat());
        } else if (key == QStringLiteral("Random Rotation Y")) {
            randomRotationVariance_.setY(value.toFloat());
        } else if (key == QStringLiteral("Random Rotation Z")) {
            randomRotationVariance_.setZ(value.toFloat());
        } else if (key == QStringLiteral("Random Scale")) {
            setRandomScaleVariance(value.toFloat());
        } else if (key == QStringLiteral("Object Scale")) {
            setObjectScale(value.toFloat());
        } else if (key == QStringLiteral("Hex Columns")) {
            setHexColumns(value.toInt());
        } else if (key == QStringLiteral("Hex Rows")) {
            setHexRows(value.toInt());
        } else if (key == QStringLiteral("Hex Radius")) {
            setHexRadius(value.toFloat());
        } else if (key == QStringLiteral("Spiral Count")) {
            setSpiralCount(value.toInt());
        } else if (key == QStringLiteral("Spiral Turns")) {
            setSpiralTurns(value.toFloat());
        } else if (key == QStringLiteral("Spiral Radius Start")) {
            setSpiralRadiusStart(value.toFloat());
        } else if (key == QStringLiteral("Spiral Radius End")) {
            setSpiralRadiusEnd(value.toFloat());
        } else if (key == QStringLiteral("Spiral Height")) {
            setSpiralHeight(value.toFloat());
        } else if (key == QStringLiteral("Spline Clone Count")) {
            setSplineCount(value.toInt());
        }
    }
};

} // namespace Artifact
