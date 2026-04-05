module;
#include <QString>
#include <QVariant>
#include <QVector3D>
#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

export module Artifact.Effect.Generator.Cloner;

import Artifact.Effect.Clone.Core;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class ClonerGenerator : public ArtifactAbstractEffect {
private:
    int count_ = 3;
    QVector3D offset_ = QVector3D(160.0f, 48.0f, 0.0f);
    float rotationStep_ = 0.0f;
    float opacityDecay_ = 0.0f;

public:
    ClonerGenerator() {
        setDisplayName(ArtifactCore::UniString("Cloner (Generator)"));
        setPipelineStage(EffectPipelineStage::Generator);
    }

    virtual ~ClonerGenerator() = default;

    int count() const { return count_; }
    void setCount(int count) { count_ = std::max(1, count); }

    QVector3D offset() const { return offset_; }
    void setOffset(const QVector3D& offset) { offset_ = offset; }

    float rotationStep() const { return rotationStep_; }
    void setRotationStep(float step) { rotationStep_ = step; }

    float opacityDecay() const { return opacityDecay_; }
    void setOpacityDecay(float decay) { opacityDecay_ = std::clamp(decay, 0.0f, 1.0f); }

    std::vector<CloneData> generateCloneData() const
    {
        std::vector<CloneData> clones;
        const int total = std::max(1, count_);
        clones.reserve(static_cast<size_t>(total));

        for (int i = 0; i < total; ++i) {
            CloneData clone;
            clone.index = i;
            clone.transform.setToIdentity();
            clone.transform.translate(offset_.x() * i, offset_.y() * i, offset_.z() * i);
            if (rotationStep_ != 0.0f) {
                clone.transform.rotate(rotationStep_ * i, 0.0f, 0.0f, 1.0f);
            }
            clone.weight = std::clamp(1.0f - opacityDecay_ * static_cast<float>(i), 0.0f, 1.0f);
            clone.visible = true;
            clones.push_back(clone);
        }

        return clones;
    }

    std::vector<AbstractProperty> getProperties() const override
    {
        std::vector<AbstractProperty> props;

        AbstractProperty countProp;
        countProp.setName("Clone Count");
        countProp.setType(PropertyType::Integer);
        countProp.setValue(count_);
        props.push_back(countProp);

        AbstractProperty offsetXProp;
        offsetXProp.setName("Offset X");
        offsetXProp.setType(PropertyType::Float);
        offsetXProp.setValue(offset_.x());
        props.push_back(offsetXProp);

        AbstractProperty offsetYProp;
        offsetYProp.setName("Offset Y");
        offsetYProp.setType(PropertyType::Float);
        offsetYProp.setValue(offset_.y());
        props.push_back(offsetYProp);

        AbstractProperty offsetZProp;
        offsetZProp.setName("Offset Z");
        offsetZProp.setType(PropertyType::Float);
        offsetZProp.setValue(offset_.z());
        props.push_back(offsetZProp);

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

        return props;
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override
    {
        const QString key = name.toQString();
        if (key == QStringLiteral("Clone Count")) {
            setCount(value.toInt());
        } else if (key == QStringLiteral("Offset X")) {
            offset_.setX(value.toFloat());
        } else if (key == QStringLiteral("Offset Y")) {
            offset_.setY(value.toFloat());
        } else if (key == QStringLiteral("Offset Z")) {
            offset_.setZ(value.toFloat());
        } else if (key == QStringLiteral("Rotation Step")) {
            setRotationStep(value.toFloat());
        } else if (key == QStringLiteral("Opacity Decay")) {
            setOpacityDecay(value.toFloat());
        }
    }
};

} // namespace Artifact
