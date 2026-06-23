module;
#include <utility>
#include <QSize>
#include <QRectF>
#include <QImage>
#include <QVariant>
#include <QVector3D>
#include <QMatrix4x4>
#include <QTransform>
#include <limits>
#include <vector>
#include <memory>
#include <algorithm>

module Artifact.Layer.Clone;

import Artifact.Layers;
import Artifact.Composition.Abstract;
import Artifact.Effect.Clone.Core;
import Artifact.Effect.Abstract;
import Color.Float;
import Utils.String.UniString;
import Artifact.Render.IRenderer;
import Property.Abstract;
import Utils.Id;

// Mesh instancing (Phase 2) - convert CloneData to InstanceData
import Graphics;


namespace Artifact {

// Helper function to convert CloneData to InstanceData (Mesh Instancing Phase 2)
namespace {
QString cloneModeName(CloneMode mode)
{
    switch (mode) {
    case CloneMode::Linear:
        return QStringLiteral("Linear");
    case CloneMode::LinearJitter:
        return QStringLiteral("Linear Jitter");
    case CloneMode::Curve:
        return QStringLiteral("Curve");
    case CloneMode::Grid:
        return QStringLiteral("Grid");
    case CloneMode::Radial:
        return QStringLiteral("Radial");
    }
    return QStringLiteral("Linear");
}

float jitterSample(int seed, int index, int channel)
{
    quint32 x = static_cast<quint32>(seed);
    x ^= static_cast<quint32>(index) * 0x9E3779B9u;
    x ^= static_cast<quint32>(channel) * 0x85EBCA6Bu;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    const float normalized = static_cast<float>(x) / static_cast<float>(std::numeric_limits<quint32>::max());
    return normalized * 2.0f - 1.0f;
}

    ArtifactCore::InstanceData cloneDataToInstanceData(const CloneData& clone) {
        ArtifactCore::InstanceData instance;
        
        // Convert QMatrix4x4 (row-major) to float[16] (column-major for GPU)
        const float* matPtr = clone.transform.constData();
        for (int i = 0; i < 16; ++i) {
            instance.transform[i] = matPtr[i];
        }
        
        // Convert QColor (0-255) to float[4] (0.0-1.0)
        instance.color[0] = clone.color.redF();
        instance.color[1] = clone.color.greenF();
        instance.color[2] = clone.color.blueF();
        instance.color[3] = clone.color.alphaF();
        
        // Copy numeric values
        instance.weight = std::clamp(clone.weight, 0.0f, 1.0f);
        instance.timeOffset = clone.timeOffset;
        instance.padding[0] = 0.0f;
        instance.padding[1] = 0.0f;
        
        return instance;
    }
    
    std::vector<ArtifactCore::InstanceData> cloneDataVectorToInstanceDataVector(
        const std::vector<CloneData>& clones) 
    {
        std::vector<ArtifactCore::InstanceData> instances;
        instances.reserve(clones.size());
        
        for (const auto& clone : clones) {
            if (clone.visible) {  // Only include visible clones
                instances.push_back(cloneDataToInstanceData(clone));
            }
        }
        
        return instances;
    }
} // anonymous namespace


class ArtifactCloneLayer::Impl {
public:
    Impl();
    ~Impl();
    ArtifactCloneLayerSettings settings_;
    std::vector<std::shared_ptr<AbstractCloneEffector>> effectors_;
};

ArtifactCloneLayer::Impl::Impl() {
}

ArtifactCloneLayer::Impl::~Impl() {
}

ArtifactCloneLayer::ArtifactCloneLayer() : impl_(new Impl()) {
    setSourceSize(Size_2D(1920, 1080));
    setLayerName("Clone Layer");
}

ArtifactCloneLayer::~ArtifactCloneLayer() {
    delete impl_;
}

void ArtifactCloneLayer::draw(ArtifactIRenderer* renderer) {
    if (!renderer || !isVisible() || opacity() <= 0.0f) {
        return;
    }

    const QRectF bounds = localBounds();
    if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return;
    }

    const QTransform transform = getGlobalTransform();
    const QPointF tl = transform.map(bounds.topLeft());
    const QPointF tr = transform.map(bounds.topRight());
    const QPointF br = transform.map(bounds.bottomRight());
    const QPointF bl = transform.map(bounds.bottomLeft());

    const ArtifactCore::FloatColor outerColor{0.32f, 0.74f, 0.98f, 0.92f};
    const ArtifactCore::FloatColor innerColor{0.08f, 0.18f, 0.28f, 0.70f};

    renderer->drawSolidLine({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                            {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                            outerColor, 1.8f);
    renderer->drawSolidLine({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                            {static_cast<float>(br.x()), static_cast<float>(br.y())},
                            outerColor, 1.8f);
    renderer->drawSolidLine({static_cast<float>(br.x()), static_cast<float>(br.y())},
                            {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                            outerColor, 1.8f);
    renderer->drawSolidLine({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                            {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                            outerColor, 1.8f);
    renderer->drawSolidLine({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                            {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                            innerColor, 0.8f);
    renderer->drawSolidLine({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                            {static_cast<float>(br.x()), static_cast<float>(br.y())},
                            innerColor, 0.8f);
    renderer->drawSolidLine({static_cast<float>(br.x()), static_cast<float>(br.y())},
                            {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                            innerColor, 0.8f);
    renderer->drawSolidLine({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                            {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                            innerColor, 0.8f);
}


bool ArtifactCloneLayer::isCloneLayer() const {
    return true;
}

ArtifactCloneLayerSettings ArtifactCloneLayer::cloneSettings() const {
    return impl_->settings_;
}

void ArtifactCloneLayer::setCloneSettings(const ArtifactCloneLayerSettings& settings) {
    impl_->settings_ = settings;
}

std::vector<CloneData> ArtifactCloneLayer::generateCloneData() const {
    std::vector<CloneData> clones;
    
    if (impl_->settings_.mode == CloneMode::Linear ||
        impl_->settings_.mode == CloneMode::LinearJitter) {
        const int total = std::max(1, impl_->settings_.cloneCount);
        clones.reserve(static_cast<size_t>(total));
        for (int i = 0; i < total; ++i) {
            CloneData clone;
            clone.index = i;
            clone.transform.setToIdentity();
            QVector3D offset = impl_->settings_.offset * static_cast<float>(i);
            if (impl_->settings_.mode == CloneMode::LinearJitter) {
                offset.setX(offset.x() + jitterSample(impl_->settings_.seed, i, 0) * impl_->settings_.jitter.x());
                offset.setY(offset.y() + jitterSample(impl_->settings_.seed, i, 1) * impl_->settings_.jitter.y());
                offset.setZ(offset.z() + jitterSample(impl_->settings_.seed, i, 2) * impl_->settings_.jitter.z());
            }
            clone.transform.translate(offset);
            if (impl_->settings_.rotationStep != 0.0f) {
                clone.transform.rotate(impl_->settings_.rotationStep * i, 0.0f, 0.0f, 1.0f);
            }
            clone.weight = std::clamp(1.0f - impl_->settings_.opacityDecay * static_cast<float>(i), 0.0f, 1.0f);
            clone.visible = true;
            clones.push_back(clone);
        }
    } else if (impl_->settings_.mode == CloneMode::Curve) {
        const int total = std::max(1, impl_->settings_.cloneCount);
        clones.reserve(static_cast<size_t>(total));
        const float start = impl_->settings_.curveStartAngle;
        const float end = impl_->settings_.curveEndAngle;
        const float step = total > 1 ? (end - start) / static_cast<float>(total - 1) : 0.0f;
        for (int i = 0; i < total; ++i) {
            CloneData clone;
            clone.index = i;
            const float angle = start + step * static_cast<float>(i);
            const float rad = angle * static_cast<float>(M_PI) / 180.0f;
            const float x = std::cos(rad) * impl_->settings_.curveRadius;
            const float y = std::sin(rad) * impl_->settings_.curveRadius;
            clone.transform.setToIdentity();
            clone.transform.translate(x, y, 0.0f);
            if (impl_->settings_.rotationStep != 0.0f) {
                clone.transform.rotate(angle + impl_->settings_.rotationStep * i, 0.0f, 0.0f, 1.0f);
            }
            clone.weight = std::clamp(1.0f - impl_->settings_.opacityDecay * static_cast<float>(i), 0.0f, 1.0f);
            clone.visible = true;
            clones.push_back(clone);
        }
    } else if (impl_->settings_.mode == CloneMode::Grid) {
        const int cols = std::max(1, impl_->settings_.columns);
        const int rows = std::max(1, impl_->settings_.rows);
        const int depth = std::max(1, impl_->settings_.depth);
        const int total = cols * rows * depth;
        clones.reserve(static_cast<size_t>(total));

        QVector3D startPos = -impl_->settings_.gridSpacing * QVector3D(cols - 1, rows - 1, depth - 1) * 0.5f;

        for (int z = 0; z < depth; ++z) {
            for (int y = 0; y < rows; ++y) {
                for (int x = 0; x < cols; ++x) {
                    CloneData clone;
                    clone.index = static_cast<int>(clones.size());
                    clone.transform.setToIdentity();
                    QVector3D pos = startPos + QVector3D(x * impl_->settings_.gridSpacing.x(),
                                                       y * impl_->settings_.gridSpacing.y(),
                                                       z * impl_->settings_.gridSpacing.z());
                    clone.transform.translate(pos);
                    clone.weight = 1.0f;
                    clone.visible = true;
                    clones.push_back(clone);
                }
            }
        }
    } else if (impl_->settings_.mode == CloneMode::Radial) {
        const int total = std::max(1, impl_->settings_.radialCount);
        clones.reserve(static_cast<size_t>(total));
        float angleStep = (impl_->settings_.endAngle - impl_->settings_.startAngle) / total;

        for (int i = 0; i < total; ++i) {
            CloneData clone;
            clone.index = i;
            float angle = impl_->settings_.startAngle + angleStep * i;
            float rad = angle * M_PI / 180.0f;
            
            clone.transform.setToIdentity();
            clone.transform.translate(std::cos(rad) * impl_->settings_.radius,
                                    std::sin(rad) * impl_->settings_.radius,
                                    0.0f);
            clone.transform.rotate(angle, 0.0f, 0.0f, 1.0f);
            clone.weight = 1.0f;
            clone.visible = true;
            clones.push_back(clone);
        }
    }

    // Apply effectors
    if (impl_->settings_.useEffector) {
        for (const auto& effector : impl_->effectors_) {
            if (effector) {
                effector->applyToClones(clones);
            }
        }
    }

    for (auto& clone : clones) {
        QMatrix4x4 transform;
        transform.setToIdentity();
        const auto applyStage = [&transform](const ArtifactCloneLayerSettings::TransformStage& stage) {
            if (!stage.enabled) {
                return;
            }
            transform.translate(stage.offset);
            transform.rotate(stage.rotation, 0.0f, 0.0f, 1.0f);
            transform.scale(stage.scale);
        };
        applyStage(impl_->settings_.transform1);
        applyStage(impl_->settings_.transform2);
        applyStage(impl_->settings_.transform3);
        clone.transform = transform * clone.transform;
    }

    return clones;
}

void ArtifactCloneLayer::addEffector(std::shared_ptr<AbstractCloneEffector> effector) {
    if (effector) {
        impl_->effectors_.push_back(effector);
    }
}

void ArtifactCloneLayer::removeEffector(int index) {
    if (index >= 0 && index < static_cast<int>(impl_->effectors_.size())) {
        impl_->effectors_.erase(impl_->effectors_.begin() + index);
    }
}

void ArtifactCloneLayer::clearEffectors() {
    impl_->effectors_.clear();
}

int ArtifactCloneLayer::effectorCount() const {
    return static_cast<int>(impl_->effectors_.size());
}

std::shared_ptr<AbstractCloneEffector> ArtifactCloneLayer::effectorAt(int index) const {
    if (index >= 0 && index < static_cast<int>(impl_->effectors_.size())) {
        return impl_->effectors_[static_cast<size_t>(index)];
    }
    return nullptr;
}

QSize ArtifactCloneLayer::sourceSize() const {
    return QSize(1920, 1080);
}

QRectF ArtifactCloneLayer::localBounds() const {
    return QRectF(0, 0, 1920, 1080);
}

QImage ArtifactCloneLayer::toQImage() const {
    return QImage();
}

std::vector<AbstractProperty> ArtifactCloneLayer::getProperties() const {
    std::vector<AbstractProperty> props;

    const auto includeMode = [this](CloneMode mode) {
        return impl_->settings_.mode == mode;
    };
    const auto includeLinear = [&]() {
        return includeMode(CloneMode::Linear) || includeMode(CloneMode::LinearJitter) ||
               includeMode(CloneMode::Curve);
    };
    const auto includeLinearJitter = [&]() {
        return includeMode(CloneMode::LinearJitter);
    };
    const auto includeCurve = [&]() {
        return includeMode(CloneMode::Curve);
    };
    const auto includeGrid = [&]() { return includeMode(CloneMode::Grid); };
    const auto includeRadial = [&]() { return includeMode(CloneMode::Radial); };

    // Mode property - use ObjectReference since Enum type doesn't exist
    AbstractProperty modeProp;
    modeProp.setName("Mode");
    modeProp.setType(PropertyType::Integer);  // Use Integer to represent the enum value
    modeProp.setValue(static_cast<int>(impl_->settings_.mode));
    modeProp.setTooltip(QStringLiteral("0=Linear, 1=Linear Jitter, 2=Curve, 3=Grid, 4=Radial (current: %1)")
                        .arg(cloneModeName(impl_->settings_.mode)));
    props.push_back(modeProp);

    AbstractProperty transform1EnabledProp;
    transform1EnabledProp.setName("Transform 1 Enabled");
    transform1EnabledProp.setType(PropertyType::Boolean);
    transform1EnabledProp.setValue(impl_->settings_.transform1.enabled);
    props.push_back(transform1EnabledProp);

    AbstractProperty transform1XProp;
    transform1XProp.setName("Transform 1 X");
    transform1XProp.setType(PropertyType::Float);
    transform1XProp.setValue(impl_->settings_.transform1.offset.x());
    props.push_back(transform1XProp);

    AbstractProperty transform1YProp;
    transform1YProp.setName("Transform 1 Y");
    transform1YProp.setType(PropertyType::Float);
    transform1YProp.setValue(impl_->settings_.transform1.offset.y());
    props.push_back(transform1YProp);

    AbstractProperty transform1ZProp;
    transform1ZProp.setName("Transform 1 Z");
    transform1ZProp.setType(PropertyType::Float);
    transform1ZProp.setValue(impl_->settings_.transform1.offset.z());
    props.push_back(transform1ZProp);

    AbstractProperty transform1ScaleXProp;
    transform1ScaleXProp.setName("Transform 1 Scale X");
    transform1ScaleXProp.setType(PropertyType::Float);
    transform1ScaleXProp.setValue(impl_->settings_.transform1.scale.x());
    props.push_back(transform1ScaleXProp);

    AbstractProperty transform1ScaleYProp;
    transform1ScaleYProp.setName("Transform 1 Scale Y");
    transform1ScaleYProp.setType(PropertyType::Float);
    transform1ScaleYProp.setValue(impl_->settings_.transform1.scale.y());
    props.push_back(transform1ScaleYProp);

    AbstractProperty transform1ScaleZProp;
    transform1ScaleZProp.setName("Transform 1 Scale Z");
    transform1ScaleZProp.setType(PropertyType::Float);
    transform1ScaleZProp.setValue(impl_->settings_.transform1.scale.z());
    props.push_back(transform1ScaleZProp);

    AbstractProperty transform1RotProp;
    transform1RotProp.setName("Transform 1 Rotation");
    transform1RotProp.setType(PropertyType::Float);
    transform1RotProp.setValue(impl_->settings_.transform1.rotation);
    props.push_back(transform1RotProp);

    AbstractProperty transform2EnabledProp;
    transform2EnabledProp.setName("Transform 2 Enabled");
    transform2EnabledProp.setType(PropertyType::Boolean);
    transform2EnabledProp.setValue(impl_->settings_.transform2.enabled);
    props.push_back(transform2EnabledProp);

    AbstractProperty transform2XProp;
    transform2XProp.setName("Transform 2 X");
    transform2XProp.setType(PropertyType::Float);
    transform2XProp.setValue(impl_->settings_.transform2.offset.x());
    props.push_back(transform2XProp);

    AbstractProperty transform2YProp;
    transform2YProp.setName("Transform 2 Y");
    transform2YProp.setType(PropertyType::Float);
    transform2YProp.setValue(impl_->settings_.transform2.offset.y());
    props.push_back(transform2YProp);

    AbstractProperty transform2ZProp;
    transform2ZProp.setName("Transform 2 Z");
    transform2ZProp.setType(PropertyType::Float);
    transform2ZProp.setValue(impl_->settings_.transform2.offset.z());
    props.push_back(transform2ZProp);

    AbstractProperty transform2ScaleXProp;
    transform2ScaleXProp.setName("Transform 2 Scale X");
    transform2ScaleXProp.setType(PropertyType::Float);
    transform2ScaleXProp.setValue(impl_->settings_.transform2.scale.x());
    props.push_back(transform2ScaleXProp);

    AbstractProperty transform2ScaleYProp;
    transform2ScaleYProp.setName("Transform 2 Scale Y");
    transform2ScaleYProp.setType(PropertyType::Float);
    transform2ScaleYProp.setValue(impl_->settings_.transform2.scale.y());
    props.push_back(transform2ScaleYProp);

    AbstractProperty transform2ScaleZProp;
    transform2ScaleZProp.setName("Transform 2 Scale Z");
    transform2ScaleZProp.setType(PropertyType::Float);
    transform2ScaleZProp.setValue(impl_->settings_.transform2.scale.z());
    props.push_back(transform2ScaleZProp);

    AbstractProperty transform2RotProp;
    transform2RotProp.setName("Transform 2 Rotation");
    transform2RotProp.setType(PropertyType::Float);
    transform2RotProp.setValue(impl_->settings_.transform2.rotation);
    props.push_back(transform2RotProp);

    AbstractProperty transform3EnabledProp;
    transform3EnabledProp.setName("Transform 3 Enabled");
    transform3EnabledProp.setType(PropertyType::Boolean);
    transform3EnabledProp.setValue(impl_->settings_.transform3.enabled);
    props.push_back(transform3EnabledProp);

    AbstractProperty transform3XProp;
    transform3XProp.setName("Transform 3 X");
    transform3XProp.setType(PropertyType::Float);
    transform3XProp.setValue(impl_->settings_.transform3.offset.x());
    props.push_back(transform3XProp);

    AbstractProperty transform3YProp;
    transform3YProp.setName("Transform 3 Y");
    transform3YProp.setType(PropertyType::Float);
    transform3YProp.setValue(impl_->settings_.transform3.offset.y());
    props.push_back(transform3YProp);

    AbstractProperty transform3ZProp;
    transform3ZProp.setName("Transform 3 Z");
    transform3ZProp.setType(PropertyType::Float);
    transform3ZProp.setValue(impl_->settings_.transform3.offset.z());
    props.push_back(transform3ZProp);

    AbstractProperty transform3ScaleXProp;
    transform3ScaleXProp.setName("Transform 3 Scale X");
    transform3ScaleXProp.setType(PropertyType::Float);
    transform3ScaleXProp.setValue(impl_->settings_.transform3.scale.x());
    props.push_back(transform3ScaleXProp);

    AbstractProperty transform3ScaleYProp;
    transform3ScaleYProp.setName("Transform 3 Scale Y");
    transform3ScaleYProp.setType(PropertyType::Float);
    transform3ScaleYProp.setValue(impl_->settings_.transform3.scale.y());
    props.push_back(transform3ScaleYProp);

    AbstractProperty transform3ScaleZProp;
    transform3ScaleZProp.setName("Transform 3 Scale Z");
    transform3ScaleZProp.setType(PropertyType::Float);
    transform3ScaleZProp.setValue(impl_->settings_.transform3.scale.z());
    props.push_back(transform3ScaleZProp);

    AbstractProperty transform3RotProp;
    transform3RotProp.setName("Transform 3 Rotation");
    transform3RotProp.setType(PropertyType::Float);
    transform3RotProp.setValue(impl_->settings_.transform3.rotation);
    props.push_back(transform3RotProp);

    if (includeLinear()) {
        AbstractProperty countProp;
        countProp.setName("Clone Count");
        countProp.setType(PropertyType::Integer);
        countProp.setValue(impl_->settings_.cloneCount);
        props.push_back(countProp);

        AbstractProperty offsetXProp;
        offsetXProp.setName("Offset X");
        offsetXProp.setType(PropertyType::Float);
        offsetXProp.setValue(impl_->settings_.offset.x());
        props.push_back(offsetXProp);

        AbstractProperty offsetYProp;
        offsetYProp.setName("Offset Y");
        offsetYProp.setType(PropertyType::Float);
        offsetYProp.setValue(impl_->settings_.offset.y());
        props.push_back(offsetYProp);

        AbstractProperty offsetZProp;
        offsetZProp.setName("Offset Z");
        offsetZProp.setType(PropertyType::Float);
        offsetZProp.setValue(impl_->settings_.offset.z());
        props.push_back(offsetZProp);
    } else if (includeGrid()) {
        AbstractProperty colsProp;
        colsProp.setName("Columns");
        colsProp.setType(PropertyType::Integer);
        colsProp.setValue(impl_->settings_.columns);
        props.push_back(colsProp);

        AbstractProperty rowsProp;
        rowsProp.setName("Rows");
        rowsProp.setType(PropertyType::Integer);
        rowsProp.setValue(impl_->settings_.rows);
        props.push_back(rowsProp);

        AbstractProperty depthProp;
        depthProp.setName("Depth");
        depthProp.setType(PropertyType::Integer);
        depthProp.setValue(impl_->settings_.depth);
        props.push_back(depthProp);

        AbstractProperty spXProp;
        spXProp.setName("Spacing X");
        spXProp.setType(PropertyType::Float);
        spXProp.setValue(impl_->settings_.gridSpacing.x());
        props.push_back(spXProp);

        AbstractProperty spYProp;
        spYProp.setName("Spacing Y");
        spYProp.setType(PropertyType::Float);
        spYProp.setValue(impl_->settings_.gridSpacing.y());
        props.push_back(spYProp);

        AbstractProperty spZProp;
        spZProp.setName("Spacing Z");
        spZProp.setType(PropertyType::Float);
        spZProp.setValue(impl_->settings_.gridSpacing.z());
        props.push_back(spZProp);
    } else if (includeRadial()) {
        AbstractProperty radCountProp;
        radCountProp.setName("Radial Count");
        radCountProp.setType(PropertyType::Integer);
        radCountProp.setValue(impl_->settings_.radialCount);
        props.push_back(radCountProp);

        AbstractProperty radiusProp;
        radiusProp.setName("Radius");
        radiusProp.setType(PropertyType::Float);
        radiusProp.setValue(impl_->settings_.radius);
        props.push_back(radiusProp);

        AbstractProperty startAngleProp;
        startAngleProp.setName("Start Angle");
        startAngleProp.setType(PropertyType::Float);
        startAngleProp.setValue(impl_->settings_.startAngle);
        props.push_back(startAngleProp);

        AbstractProperty endAngleProp;
        endAngleProp.setName("End Angle");
        endAngleProp.setType(PropertyType::Float);
        endAngleProp.setValue(impl_->settings_.endAngle);
        props.push_back(endAngleProp);
    }

    AbstractProperty sourceProp;
    sourceProp.setName("Source Layer");
    sourceProp.setType(PropertyType::ObjectReference);
    PropertyMetadata meta;
    meta.referenceTypeName = "LayerID";
    sourceProp.setMetadata(meta);
    sourceProp.setValue(impl_->settings_.sourceLayerId.toString());
    props.push_back(sourceProp);

    if (includeLinear() || includeGrid() || includeRadial()) {
        AbstractProperty rotationProp;
        rotationProp.setName("Rotation Step");
        rotationProp.setType(PropertyType::Float);
        rotationProp.setValue(impl_->settings_.rotationStep);
        props.push_back(rotationProp);

        AbstractProperty opacityProp;
        opacityProp.setName("Opacity Decay");
        opacityProp.setType(PropertyType::Float);
        opacityProp.setValue(impl_->settings_.opacityDecay);
        props.push_back(opacityProp);
    }

    if (includeLinearJitter()) {
        AbstractProperty jitterXProp;
        jitterXProp.setName("Jitter X");
        jitterXProp.setType(PropertyType::Float);
        jitterXProp.setValue(impl_->settings_.jitter.x());
        props.push_back(jitterXProp);

        AbstractProperty jitterYProp;
        jitterYProp.setName("Jitter Y");
        jitterYProp.setType(PropertyType::Float);
        jitterYProp.setValue(impl_->settings_.jitter.y());
        props.push_back(jitterYProp);

        AbstractProperty jitterZProp;
        jitterZProp.setName("Jitter Z");
        jitterZProp.setType(PropertyType::Float);
        jitterZProp.setValue(impl_->settings_.jitter.z());
        props.push_back(jitterZProp);

        AbstractProperty seedProp;
        seedProp.setName("Seed");
        seedProp.setType(PropertyType::Integer);
        seedProp.setValue(impl_->settings_.seed);
        props.push_back(seedProp);
    }

    if (includeCurve()) {
        AbstractProperty curveRadiusProp;
        curveRadiusProp.setName("Curve Radius");
        curveRadiusProp.setType(PropertyType::Float);
        curveRadiusProp.setValue(impl_->settings_.curveRadius);
        props.push_back(curveRadiusProp);

        AbstractProperty curveStartProp;
        curveStartProp.setName("Curve Start Angle");
        curveStartProp.setType(PropertyType::Float);
        curveStartProp.setValue(impl_->settings_.curveStartAngle);
        props.push_back(curveStartProp);

        AbstractProperty curveEndProp;
        curveEndProp.setName("Curve End Angle");
        curveEndProp.setType(PropertyType::Float);
        curveEndProp.setValue(impl_->settings_.curveEndAngle);
        props.push_back(curveEndProp);
    }

    return props;
}

void ArtifactCloneLayer::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Mode")) {
        impl_->settings_.mode = static_cast<CloneMode>(value.toInt());
    } else if (key == QStringLiteral("Clone Count")) {
        impl_->settings_.cloneCount = std::max(1, value.toInt());
    } else if (key == QStringLiteral("Offset X")) {
        impl_->settings_.offset.setX(value.toFloat());
    } else if (key == QStringLiteral("Offset Y")) {
        impl_->settings_.offset.setY(value.toFloat());
    } else if (key == QStringLiteral("Offset Z")) {
        impl_->settings_.offset.setZ(value.toFloat());
    } else if (key == QStringLiteral("Jitter X")) {
        impl_->settings_.jitter.setX(value.toFloat());
    } else if (key == QStringLiteral("Jitter Y")) {
        impl_->settings_.jitter.setY(value.toFloat());
    } else if (key == QStringLiteral("Jitter Z")) {
        impl_->settings_.jitter.setZ(value.toFloat());
    } else if (key == QStringLiteral("Seed")) {
        impl_->settings_.seed = value.toInt();
    } else if (key == QStringLiteral("Curve Radius")) {
        impl_->settings_.curveRadius = std::max(0.0f, value.toFloat());
    } else if (key == QStringLiteral("Curve Start Angle")) {
        impl_->settings_.curveStartAngle = value.toFloat();
    } else if (key == QStringLiteral("Curve End Angle")) {
        impl_->settings_.curveEndAngle = value.toFloat();
    } else if (key == QStringLiteral("Columns")) {
        impl_->settings_.columns = std::max(1, value.toInt());
    } else if (key == QStringLiteral("Rows")) {
        impl_->settings_.rows = std::max(1, value.toInt());
    } else if (key == QStringLiteral("Depth")) {
        impl_->settings_.depth = std::max(1, value.toInt());
    } else if (key == QStringLiteral("Spacing X")) {
        impl_->settings_.gridSpacing.setX(value.toFloat());
    } else if (key == QStringLiteral("Spacing Y")) {
        impl_->settings_.gridSpacing.setY(value.toFloat());
    } else if (key == QStringLiteral("Spacing Z")) {
        impl_->settings_.gridSpacing.setZ(value.toFloat());
    } else if (key == QStringLiteral("Source Layer")) {
        impl_->settings_.sourceLayerId = LayerID(value.toString());
    } else if (key == QStringLiteral("Radial Count")) {
        impl_->settings_.radialCount = std::max(1, value.toInt());
    } else if (key == QStringLiteral("Radius")) {
        impl_->settings_.radius = value.toFloat();
    } else if (key == QStringLiteral("Start Angle")) {
        impl_->settings_.startAngle = value.toFloat();
    } else if (key == QStringLiteral("End Angle")) {
        impl_->settings_.endAngle = value.toFloat();
    } else if (key == QStringLiteral("Rotation Step")) {
        impl_->settings_.rotationStep = value.toFloat();
    } else if (key == QStringLiteral("Opacity Decay")) {
        impl_->settings_.opacityDecay = std::clamp(value.toFloat(), 0.0f, 1.0f);
    } else if (key == QStringLiteral("Transform 1 Enabled")) {
        impl_->settings_.transform1.enabled = value.toBool();
    } else if (key == QStringLiteral("Transform 1 X")) {
        impl_->settings_.transform1.offset.setX(value.toFloat());
    } else if (key == QStringLiteral("Transform 1 Y")) {
        impl_->settings_.transform1.offset.setY(value.toFloat());
    } else if (key == QStringLiteral("Transform 1 Z")) {
        impl_->settings_.transform1.offset.setZ(value.toFloat());
    } else if (key == QStringLiteral("Transform 1 Scale X")) {
        impl_->settings_.transform1.scale.setX(value.toFloat());
    } else if (key == QStringLiteral("Transform 1 Scale Y")) {
        impl_->settings_.transform1.scale.setY(value.toFloat());
    } else if (key == QStringLiteral("Transform 1 Scale Z")) {
        impl_->settings_.transform1.scale.setZ(value.toFloat());
    } else if (key == QStringLiteral("Transform 1 Rotation")) {
        impl_->settings_.transform1.rotation = value.toFloat();
    } else if (key == QStringLiteral("Transform 2 Enabled")) {
        impl_->settings_.transform2.enabled = value.toBool();
    } else if (key == QStringLiteral("Transform 2 X")) {
        impl_->settings_.transform2.offset.setX(value.toFloat());
    } else if (key == QStringLiteral("Transform 2 Y")) {
        impl_->settings_.transform2.offset.setY(value.toFloat());
    } else if (key == QStringLiteral("Transform 2 Z")) {
        impl_->settings_.transform2.offset.setZ(value.toFloat());
    } else if (key == QStringLiteral("Transform 2 Scale X")) {
        impl_->settings_.transform2.scale.setX(value.toFloat());
    } else if (key == QStringLiteral("Transform 2 Scale Y")) {
        impl_->settings_.transform2.scale.setY(value.toFloat());
    } else if (key == QStringLiteral("Transform 2 Scale Z")) {
        impl_->settings_.transform2.scale.setZ(value.toFloat());
    } else if (key == QStringLiteral("Transform 2 Rotation")) {
        impl_->settings_.transform2.rotation = value.toFloat();
    } else if (key == QStringLiteral("Transform 3 Enabled")) {
        impl_->settings_.transform3.enabled = value.toBool();
    } else if (key == QStringLiteral("Transform 3 X")) {
        impl_->settings_.transform3.offset.setX(value.toFloat());
    } else if (key == QStringLiteral("Transform 3 Y")) {
        impl_->settings_.transform3.offset.setY(value.toFloat());
    } else if (key == QStringLiteral("Transform 3 Z")) {
        impl_->settings_.transform3.offset.setZ(value.toFloat());
    } else if (key == QStringLiteral("Transform 3 Scale X")) {
        impl_->settings_.transform3.scale.setX(value.toFloat());
    } else if (key == QStringLiteral("Transform 3 Scale Y")) {
        impl_->settings_.transform3.scale.setY(value.toFloat());
    } else if (key == QStringLiteral("Transform 3 Scale Z")) {
        impl_->settings_.transform3.scale.setZ(value.toFloat());
    } else if (key == QStringLiteral("Transform 3 Rotation")) {
        impl_->settings_.transform3.rotation = value.toFloat();
    }
}

// Mesh Instancing Phase 2: Convert CloneData to InstanceData for GPU submission
std::vector<ArtifactCore::InstanceData> ArtifactCloneLayer::getInstanceData() const {
    // Get current clone configuration
    auto clones = generateCloneData();
    
    // Convert to InstanceData format
    return cloneDataVectorToInstanceDataVector(clones);
}

} // namespace Artifact
