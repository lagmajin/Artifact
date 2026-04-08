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

module Artifact.Layer.Clone;

import Artifact.Layers;
import Artifact.Effect.Clone.Core;
import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Artifact.Render.IRenderer;
import Property.Abstract;

namespace Artifact {

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

void ArtifactCloneLayer::draw(ArtifactIRenderer* /*renderer*/) {
    // TODO: Implement clone rendering using CloneData + effectors
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
    const int total = std::max(1, impl_->settings_.cloneCount);
    clones.reserve(static_cast<size_t>(total));

    for (int i = 0; i < total; ++i) {
        CloneData clone;
        clone.index = i;
        clone.transform.setToIdentity();
        clone.transform.translate(
            impl_->settings_.offset.x() * i,
            impl_->settings_.offset.y() * i,
            impl_->settings_.offset.z() * i
        );
        if (impl_->settings_.rotationStep != 0.0f) {
            clone.transform.rotate(impl_->settings_.rotationStep * i, 0.0f, 0.0f, 1.0f);
        }
        clone.weight = std::clamp(
            1.0f - impl_->settings_.opacityDecay * static_cast<float>(i),
            0.0f, 1.0f
        );
        clone.visible = true;
        clones.push_back(clone);
    }

    // Apply effectors
    for (const auto& effector : impl_->effectors_) {
        if (effector) {
            effector->applyToClones(clones);
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
    if (key == QStringLiteral("Clone Count")) {
        impl_->settings_.cloneCount = std::max(1, value.toInt());
    } else if (key == QStringLiteral("Offset X")) {
        impl_->settings_.offset.setX(value.toFloat());
    } else if (key == QStringLiteral("Offset Y")) {
        impl_->settings_.offset.setY(value.toFloat());
    } else if (key == QStringLiteral("Offset Z")) {
        impl_->settings_.offset.setZ(value.toFloat());
    } else if (key == QStringLiteral("Rotation Step")) {
        impl_->settings_.rotationStep = value.toFloat();
    } else if (key == QStringLiteral("Opacity Decay")) {
        impl_->settings_.opacityDecay = std::clamp(value.toFloat(), 0.0f, 1.0f);
    }
}

} // namespace Artifact
