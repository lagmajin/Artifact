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
        Radial = 2
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
            const float angleStep = (endAngle_ - startAngle_) / static_cast<float>(total);

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
            TransformCloneEffector effector;
            effector.positionOffset = effectorPositionOffset_;
            effector.rotationOffset = effectorRotationOffset_;
            effector.scaleOffset = effectorScaleOffset_;
            effector.applyToClones(clones);
        }

        if (useRandomEffector_) {
            RandomCloneEffector effector;
            effector.seed = randomEffectorSeed_;
            effector.positionVariance = randomPositionVariance_;
            effector.rotationVariance = randomRotationVariance_;
            effector.scaleVariance = randomScaleVariance_;
            effector.applyToClones(clones);
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
        modeProp.setTooltip(QStringLiteral("0=Linear,1=Grid,2=Radial"));
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
            setMode(static_cast<Mode>(std::clamp(value.toInt(), 0, 2)));
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
        }
    }
};

} // namespace Artifact
