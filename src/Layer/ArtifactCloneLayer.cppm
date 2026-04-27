module;
#include <utility>
#include <QSize>
#include <QRectF>
#include <QImage>
#include <QVariant>
#include <QVector3D>
#include <QMatrix4x4>
#include <vector>
#include <memory>
#include <algorithm>

module Artifact.Layer.Clone;

import Artifact.Layers;
import Artifact.Composition.Abstract;
import Artifact.Effect.Clone.Core;
import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Artifact.Render.IRenderer;
import Property.Abstract;
import Utils.Id;

// Mesh instancing (Phase 2) - convert CloneData to InstanceData
import Graphics;


namespace Artifact {

// Helper function to convert CloneData to InstanceData (Mesh Instancing Phase 2)
namespace {
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
    // TODO: Implement proper clone layer rendering
    // Current stub prevents crashes while full implementation is completed
    if (!isVisible() || opacity() <= 0.0f) return;
    if (!renderer) return;
    // Placeholder: clone rendering not yet implemented
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
    
    if (impl_->settings_.mode == CloneMode::Linear) {
        const int total = std::max(1, impl_->settings_.cloneCount);
        clones.reserve(static_cast<size_t>(total));
        for (int i = 0; i < total; ++i) {
            CloneData clone;
            clone.index = i;
            clone.transform.setToIdentity();
            clone.transform.translate(impl_->settings_.offset * i);
            if (impl_->settings_.rotationStep != 0.0f) {
                clone.transform.rotate(impl_->settings_.rotationStep * i, 0.0f, 0.0f, 1.0f);
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

    // Mode property - use ObjectReference since Enum type doesn't exist
    AbstractProperty modeProp;
    modeProp.setName("Mode");
    modeProp.setType(PropertyType::Integer);  // Use Integer to represent the enum value
    modeProp.setValue(static_cast<int>(impl_->settings_.mode));
    props.push_back(modeProp);

    if (impl_->settings_.mode == CloneMode::Linear) {
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
    } else if (impl_->settings_.mode == CloneMode::Grid) {
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
    } else if (impl_->settings_.mode == CloneMode::Radial) {
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
    }
 else if (key == QStringLiteral("Radial Count")) {
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
