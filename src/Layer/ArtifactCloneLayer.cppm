module;
#include <utility>
#include <QSize>
#include <QRectF>
#include <QImage>
#include <QJsonObject>
#include <QVariant>
#include <QVector3D>
#include <QMatrix4x4>
#include <QTransform>
#include <limits>
#include <vector>
#include <memory>
#include <algorithm>
#include <random>

module Artifact.Layer.Clone;

import Artifact.Layers;
import Artifact.Composition.Abstract;
import Artifact.Effect.Clone.Core;
import Artifact.Effect.Clone.Basic;
import Artifact.Effect.Abstract;
import Memory.SharedPtr;
import Color.Float;
import Utils.String.UniString;
import Artifact.Render.IRenderer;
import Property.Abstract;
import Utils.Id;
import Core.Parallel;

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
    case CloneMode::Random:
        return QStringLiteral("Random");
    case CloneMode::Spline:
        return QStringLiteral("Spline");
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
    std::vector<SharedPtr<AbstractCloneEffector>> effectors_;
};

ArtifactCloneLayer::Impl::Impl() {
    // Keep the default effector chain meaningful while preserving the
    // zero-offset default behavior. The editor can toggle it via useEffector.
    effectors_.push_back(ArtifactCore::makeShared<TransformCloneEffector>());
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

    auto clones = generateCloneData();
    if (clones.empty()) return;

    const QRectF bounds = localBounds();
    const float layerOpacity = opacity();

    for (const auto& clone : clones) {
        if (!clone.visible) continue;

        const float cloneOpacity = layerOpacity * std::clamp(clone.weight, 0.0f, 1.0f);
        if (cloneOpacity <= 0.0f) continue;

        ArtifactCore::FloatColor color = {
            clone.color.redF(),
            clone.color.greenF(),
            clone.color.blueF(),
            clone.color.alphaF() * cloneOpacity
        };

        renderer->drawSolidRectTransformed(
            static_cast<float>(bounds.x()), static_cast<float>(bounds.y()),
            static_cast<float>(bounds.width()), static_cast<float>(bounds.height()),
            clone.transform, color, cloneOpacity);
    }
}


bool ArtifactCloneLayer::isCloneLayer() const {
    return true;
}

QJsonObject ArtifactCloneLayer::toJson() const {
    QJsonObject obj = ArtifactAbstractLayer::toJson();
    const auto& settings = impl_->settings_;
    const auto writeVector = [&obj](const QString& prefix, const QVector3D& value) {
        obj[prefix + QStringLiteral(".x")] = value.x();
        obj[prefix + QStringLiteral(".y")] = value.y();
        obj[prefix + QStringLiteral(".z")] = value.z();
    };
    const auto writeStage = [&obj, &writeVector](const QString& prefix,
                                                 const ArtifactCloneLayerSettings::TransformStage& stage) {
        obj[prefix + QStringLiteral(".enabled")] = stage.enabled;
        writeVector(prefix + QStringLiteral(".offset"), stage.offset);
        writeVector(prefix + QStringLiteral(".scale"), stage.scale);
        obj[prefix + QStringLiteral(".rotation")] = stage.rotation;
    };
    obj["type"] = static_cast<int>(LayerType::Clone);
    obj["clone.mode"] = static_cast<int>(settings.mode);
    obj["clone.cloneCount"] = settings.cloneCount;
    writeVector(QStringLiteral("clone.offset"), settings.offset);
    writeVector(QStringLiteral("clone.jitter"), settings.jitter);
    obj["clone.seed"] = settings.seed;
    obj["clone.curveRadius"] = settings.curveRadius;
    obj["clone.curveStartAngle"] = settings.curveStartAngle;
    obj["clone.curveEndAngle"] = settings.curveEndAngle;
    obj["clone.columns"] = settings.columns;
    obj["clone.rows"] = settings.rows;
    obj["clone.depth"] = settings.depth;
    writeVector(QStringLiteral("clone.gridSpacing"), settings.gridSpacing);
    obj["clone.radialCount"] = settings.radialCount;
    obj["clone.radius"] = settings.radius;
    obj["clone.startAngle"] = settings.startAngle;
    obj["clone.endAngle"] = settings.endAngle;
    obj["clone.rotationStep"] = settings.rotationStep;
    obj["clone.opacityDecay"] = settings.opacityDecay;
    obj["clone.useEffector"] = settings.useEffector;
    obj["clone.sourceLayerId"] = settings.sourceLayerId.toString();
    obj["clone.sourceIndex"] = settings.sourceIndex;
    writeStage(QStringLiteral("clone.transform1"), settings.transform1);
    writeStage(QStringLiteral("clone.transform2"), settings.transform2);
    writeStage(QStringLiteral("clone.transform3"), settings.transform3);
    if (!impl_->effectors_.empty()) {
        if (const auto transformEffector =
                ArtifactCore::dynamicPointerCast<TransformCloneEffector>(
                    impl_->effectors_.front())) {
            obj["clone.effector.strength"] = transformEffector->strength;
            writeVector(QStringLiteral("clone.effector.position"),
                        transformEffector->positionOffset);
            writeVector(QStringLiteral("clone.effector.rotation"),
                        transformEffector->rotationOffset);
            writeVector(QStringLiteral("clone.effector.scale"),
                        transformEffector->scaleOffset);
            obj["clone.effector.useColor"] = transformEffector->useColor;
            obj["clone.effector.color.r"] = transformEffector->colorOffset.redF();
            obj["clone.effector.color.g"] = transformEffector->colorOffset.greenF();
            obj["clone.effector.color.b"] = transformEffector->colorOffset.blueF();
            obj["clone.effector.color.a"] = transformEffector->colorOffset.alphaF();
        }
    }
    return obj;
}

void ArtifactCloneLayer::fromJsonProperties(const QJsonObject& obj) {
    ArtifactAbstractLayer::fromJsonProperties(obj);
    auto& settings = impl_->settings_;
    const auto readVector = [&obj](const QString& prefix, QVector3D& value) {
        if (obj.contains(prefix + QStringLiteral(".x")))
            value.setX(static_cast<float>(obj.value(prefix + QStringLiteral(".x")).toDouble(value.x())));
        if (obj.contains(prefix + QStringLiteral(".y")))
            value.setY(static_cast<float>(obj.value(prefix + QStringLiteral(".y")).toDouble(value.y())));
        if (obj.contains(prefix + QStringLiteral(".z")))
            value.setZ(static_cast<float>(obj.value(prefix + QStringLiteral(".z")).toDouble(value.z())));
    };
    const auto readStage = [&obj, &readVector](const QString& prefix,
                                               ArtifactCloneLayerSettings::TransformStage& stage) {
        if (obj.contains(prefix + QStringLiteral(".enabled")))
            stage.enabled = obj.value(prefix + QStringLiteral(".enabled")).toBool(stage.enabled);
        readVector(prefix + QStringLiteral(".offset"), stage.offset);
        readVector(prefix + QStringLiteral(".scale"), stage.scale);
        if (obj.contains(prefix + QStringLiteral(".rotation")))
            stage.rotation = static_cast<float>(obj.value(prefix + QStringLiteral(".rotation")).toDouble(stage.rotation));
    };
    if (obj.contains("clone.mode")) {
        settings.mode = static_cast<CloneMode>(obj.value("clone.mode").toInt(static_cast<int>(settings.mode)));
    }
    if (obj.contains("clone.cloneCount")) settings.cloneCount = std::max(1, obj.value("clone.cloneCount").toInt(settings.cloneCount));
    readVector(QStringLiteral("clone.offset"), settings.offset);
    readVector(QStringLiteral("clone.jitter"), settings.jitter);
    if (obj.contains("clone.seed")) settings.seed = obj.value("clone.seed").toInt(settings.seed);
    if (obj.contains("clone.curveRadius")) settings.curveRadius = static_cast<float>(obj.value("clone.curveRadius").toDouble(settings.curveRadius));
    if (obj.contains("clone.curveStartAngle")) settings.curveStartAngle = static_cast<float>(obj.value("clone.curveStartAngle").toDouble(settings.curveStartAngle));
    if (obj.contains("clone.curveEndAngle")) settings.curveEndAngle = static_cast<float>(obj.value("clone.curveEndAngle").toDouble(settings.curveEndAngle));
    if (obj.contains("clone.columns")) settings.columns = std::max(1, obj.value("clone.columns").toInt(settings.columns));
    if (obj.contains("clone.rows")) settings.rows = std::max(1, obj.value("clone.rows").toInt(settings.rows));
    if (obj.contains("clone.depth")) settings.depth = std::max(1, obj.value("clone.depth").toInt(settings.depth));
    readVector(QStringLiteral("clone.gridSpacing"), settings.gridSpacing);
    if (obj.contains("clone.radialCount")) settings.radialCount = std::max(1, obj.value("clone.radialCount").toInt(settings.radialCount));
    if (obj.contains("clone.radius")) settings.radius = static_cast<float>(obj.value("clone.radius").toDouble(settings.radius));
    if (obj.contains("clone.startAngle")) settings.startAngle = static_cast<float>(obj.value("clone.startAngle").toDouble(settings.startAngle));
    if (obj.contains("clone.endAngle")) settings.endAngle = static_cast<float>(obj.value("clone.endAngle").toDouble(settings.endAngle));
    if (obj.contains("clone.rotationStep")) settings.rotationStep = static_cast<float>(obj.value("clone.rotationStep").toDouble(settings.rotationStep));
    if (obj.contains("clone.opacityDecay")) settings.opacityDecay = std::clamp(static_cast<float>(obj.value("clone.opacityDecay").toDouble(settings.opacityDecay)), 0.0f, 1.0f);
    readStage(QStringLiteral("clone.transform1"), settings.transform1);
    readStage(QStringLiteral("clone.transform2"), settings.transform2);
    readStage(QStringLiteral("clone.transform3"), settings.transform3);
    if (!impl_->effectors_.empty()) {
        if (const auto transformEffector =
                ArtifactCore::dynamicPointerCast<TransformCloneEffector>(
                    impl_->effectors_.front())) {
            if (obj.contains("clone.effector.strength"))
                transformEffector->strength = std::clamp(static_cast<float>(obj.value("clone.effector.strength").toDouble(transformEffector->strength)), 0.0f, 1.0f);
            readVector(QStringLiteral("clone.effector.position"), transformEffector->positionOffset);
            readVector(QStringLiteral("clone.effector.rotation"), transformEffector->rotationOffset);
            readVector(QStringLiteral("clone.effector.scale"), transformEffector->scaleOffset);
            if (obj.contains("clone.effector.useColor"))
                transformEffector->useColor = obj.value("clone.effector.useColor").toBool(transformEffector->useColor);
            const auto colorComponent = [&obj](const QString& key, float fallback) {
                return static_cast<float>(obj.value(key).toDouble(fallback));
            };
            if (obj.contains("clone.effector.color.a")) {
                QColor color;
                color.setRgbF(colorComponent("clone.effector.color.r", transformEffector->colorOffset.redF()),
                               colorComponent("clone.effector.color.g", transformEffector->colorOffset.greenF()),
                               colorComponent("clone.effector.color.b", transformEffector->colorOffset.blueF()),
                               colorComponent("clone.effector.color.a", transformEffector->colorOffset.alphaF()));
                transformEffector->colorOffset = color;
            }
        }
    }
    if (obj.contains("clone.sourceLayerId")) {
        impl_->settings_.sourceLayerId = LayerID(obj.value("clone.sourceLayerId").toString());
    }
    impl_->settings_.sourceIndex = std::max(0, obj.value("clone.sourceIndex").toInt(0));
    if (obj.contains("clone.useEffector")) {
        impl_->settings_.useEffector = obj.value("clone.useEffector").toBool(true);
    }
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
            clone.sourceIndex = impl_->settings_.sourceIndex;
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
    } else if (impl_->settings_.mode == CloneMode::Curve ||
               impl_->settings_.mode == CloneMode::Spline) {
        const int total = std::max(1, impl_->settings_.cloneCount);
        clones.reserve(static_cast<size_t>(total));
        const float start = impl_->settings_.curveStartAngle;
        const float end = impl_->settings_.curveEndAngle;
        const float step = total > 1 ? (end - start) / static_cast<float>(total - 1) : 0.0f;
        for (int i = 0; i < total; ++i) {
            CloneData clone;
            clone.index = i;
            clone.sourceIndex = impl_->settings_.sourceIndex;
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
    } else if (impl_->settings_.mode == CloneMode::Random) {
        const int total = std::max(1, impl_->settings_.cloneCount);
        clones.reserve(static_cast<size_t>(total));
        std::mt19937 rng(static_cast<uint32_t>(impl_->settings_.seed));
        std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
        std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
        std::uniform_real_distribution<float> rotDist(0.0f, 360.0f);
        std::uniform_real_distribution<float> scaleDist(0.85f, 1.15f);
        const QVector3D spread = impl_->settings_.jitter;
        const float centerFalloff = 0.72f;

        for (int i = 0; i < total; ++i) {
            CloneData clone;
            clone.index = i;
            clone.sourceIndex = impl_->settings_.sourceIndex;
            clone.transform.setToIdentity();

            const float rx = unit(rng);
            const float ry = unit(rng);
            const float rz = unit(rng);
            const float radialMix = std::pow(zeroOne(rng), 0.65f);
            QVector3D offset = impl_->settings_.offset +
                               QVector3D(rx * spread.x() * radialMix * centerFalloff,
                                         ry * spread.y() * radialMix * centerFalloff,
                                         rz * spread.z() * radialMix * centerFalloff);
            clone.transform.translate(offset);

            const float randomRotation = rotDist(rng);
            if (impl_->settings_.rotationStep != 0.0f) {
                clone.transform.rotate(randomRotation + impl_->settings_.rotationStep * static_cast<float>(i),
                                       0.0f, 0.0f, 1.0f);
            } else {
                clone.transform.rotate(randomRotation * 0.15f, 0.0f, 0.0f, 1.0f);
            }

            const float scale = std::clamp(scaleDist(rng) * (1.0f - (1.0f - radialMix) * 0.10f), 0.5f, 1.5f);
            clone.transform.scale(scale);
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
                    clone.sourceIndex = impl_->settings_.sourceIndex;
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
        const QRectF bounds = localBounds();
        const QPointF center = bounds.isValid() ? bounds.center() : QPointF(0.0, 0.0);

        for (int i = 0; i < total; ++i) {
            CloneData clone;
            clone.index = i;
            clone.sourceIndex = impl_->settings_.sourceIndex;
            float angle = impl_->settings_.startAngle + angleStep * i;
            float rad = angle * M_PI / 180.0f;

            clone.transform.setToIdentity();
            clone.transform.translate(center.x() + std::cos(rad) * impl_->settings_.radius,
                                      center.y() + std::sin(rad) * impl_->settings_.radius,
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

    ArtifactCore::Parallel::For(0, static_cast<int>(clones.size()),
                                static_cast<int>(clones.size()),
                                [&](int index) {
        auto& clone = clones[static_cast<size_t>(index)];
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
    });

    return clones;
}

void ArtifactCloneLayer::addEffector(SharedPtr<AbstractCloneEffector> effector) {
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

SharedPtr<AbstractCloneEffector> ArtifactCloneLayer::effectorAt(int index) const {
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

std::vector<ArtifactCore::PropertyGroup>
ArtifactCloneLayer::getLayerPropertyGroups() const {
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup cloneGroup(QStringLiteral("Clone"));

    for (const auto& property : getProperties()) {
        auto cloneProperty =
            ArtifactCore::makeShared<ArtifactCore::AbstractProperty>(property);
        const QString propertyName = property.getName();
        cloneProperty->setName(QStringLiteral("clone.") + propertyName);
        cloneProperty->setDisplayLabel(propertyName);
        cloneGroup.addProperty(cloneProperty);
    }

    groups.push_back(std::move(cloneGroup));
    return groups;
}

bool ArtifactCloneLayer::setLayerPropertyValue(const QString& propertyPath,
                                               const QVariant& value) {
    const auto prefix = QStringLiteral("clone.");
    if (propertyPath.startsWith(prefix, Qt::CaseInsensitive)) {
        setPropertyValue(ArtifactCore::UniString::fromQString(
            propertyPath.mid(prefix.size())), value);
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
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
    modeProp.setTooltip(QStringLiteral("0=Linear, 1=Linear Jitter, 2=Curve, 3=Grid, 4=Radial, 5=Random, 6=Spline (current: %1)")
                        .arg(cloneModeName(impl_->settings_.mode)));
    props.push_back(modeProp);

    AbstractProperty useEffectorProp;
    useEffectorProp.setName("Use Effector");
    useEffectorProp.setType(PropertyType::Boolean);
    useEffectorProp.setValue(impl_->settings_.useEffector);
    useEffectorProp.setTooltip(
        QStringLiteral("Apply the configured clone effectors to generated clones."));
    props.push_back(useEffectorProp);

    if (!impl_->effectors_.empty()) {
        if (const auto transformEffector =
                ArtifactCore::dynamicPointerCast<TransformCloneEffector>(
                    impl_->effectors_.front())) {
            AbstractProperty strengthProp;
            strengthProp.setName("Effector Strength");
            strengthProp.setType(PropertyType::Float);
            strengthProp.setValue(transformEffector->strength);
            strengthProp.setTooltip(QStringLiteral("Overall Transform Effector strength."));
            props.push_back(strengthProp);

            const auto addEffectorVector = [&props](
                                                const QString& prefix,
                                                const QVector3D& value) {
                for (const auto& axis : {QStringLiteral("X"), QStringLiteral("Y"),
                                         QStringLiteral("Z")}) {
                    AbstractProperty property;
                    property.setName(prefix + QStringLiteral(" ") + axis);
                    property.setType(PropertyType::Float);
                    const float component = axis == QStringLiteral("X")
                                                ? value.x()
                                                : axis == QStringLiteral("Y")
                                                      ? value.y()
                                                      : value.z();
                    property.setValue(component);
                    props.push_back(property);
                }
            };
            addEffectorVector(QStringLiteral("Effector Position"),
                              transformEffector->positionOffset);
            addEffectorVector(QStringLiteral("Effector Rotation"),
                              transformEffector->rotationOffset);
            addEffectorVector(QStringLiteral("Effector Scale"),
                              transformEffector->scaleOffset);

            AbstractProperty useColorProp;
            useColorProp.setName("Effector Use Color");
            useColorProp.setType(PropertyType::Boolean);
            useColorProp.setValue(transformEffector->useColor);
            props.push_back(useColorProp);
        }
    }

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

    AbstractProperty sourceIndexProp;
    sourceIndexProp.setName("Source Index");
    sourceIndexProp.setType(PropertyType::Integer);
    sourceIndexProp.setValue(impl_->settings_.sourceIndex);
    props.push_back(sourceIndexProp);

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

    if (impl_->settings_.mode == CloneMode::Random) {
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
    } else if (impl_->settings_.mode == CloneMode::Spline || includeCurve()) {
        AbstractProperty countProp;
        countProp.setName("Clone Count");
        countProp.setType(PropertyType::Integer);
        countProp.setValue(impl_->settings_.cloneCount);
        props.push_back(countProp);

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
    } else if (key == QStringLiteral("Use Effector")) {
        impl_->settings_.useEffector = value.toBool();
    } else if (!impl_->effectors_.empty() &&
               (key == QStringLiteral("Effector Strength") ||
               key == QStringLiteral("Effector Use Color") ||
               key.startsWith(QStringLiteral("Effector Position")) ||
               key.startsWith(QStringLiteral("Effector Rotation")) ||
               key.startsWith(QStringLiteral("Effector Scale")))) {
        if (const auto transformEffector =
                ArtifactCore::dynamicPointerCast<TransformCloneEffector>(
                    impl_->effectors_.front())) {
            if (key == QStringLiteral("Effector Strength")) {
                transformEffector->strength = std::clamp(value.toFloat(), 0.0f, 1.0f);
            } else if (key == QStringLiteral("Effector Use Color")) {
                transformEffector->useColor = value.toBool();
            } else {
                const auto setVectorComponent = [&value](QVector3D& vector,
                                                          const QString& suffix,
                                                          const QString& propertyName) {
                    if (propertyName.endsWith(suffix)) {
                        if (suffix == QStringLiteral(" X")) vector.setX(value.toFloat());
                        if (suffix == QStringLiteral(" Y")) vector.setY(value.toFloat());
                        if (suffix == QStringLiteral(" Z")) vector.setZ(value.toFloat());
                        return true;
                    }
                    return false;
                };
                if (key.startsWith(QStringLiteral("Effector Position"))) {
                    setVectorComponent(transformEffector->positionOffset, key.right(2), key);
                } else if (key.startsWith(QStringLiteral("Effector Rotation"))) {
                    setVectorComponent(transformEffector->rotationOffset, key.right(2), key);
                } else if (key.startsWith(QStringLiteral("Effector Scale"))) {
                    setVectorComponent(transformEffector->scaleOffset, key.right(2), key);
                }
            }
        }
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
    } else if (key == QStringLiteral("Source Index")) {
        impl_->settings_.sourceIndex = std::max(0, value.toInt());
    } else if (key == QStringLiteral("Radial Count")) {
        impl_->settings_.radialCount = std::max(1, value.toInt());
    } else if (key == QStringLiteral("Radius")) {
        impl_->settings_.radius = value.toFloat();
    } else if (key == QStringLiteral("Start Angle")) {
        impl_->settings_.startAngle = value.toFloat();
    } else if (key == QStringLiteral("End Angle")) {
        impl_->settings_.endAngle = value.toFloat();
    } else if (key == QStringLiteral("Curve Radius")) {
        impl_->settings_.curveRadius = std::max(0.0f, value.toFloat());
    } else if (key == QStringLiteral("Curve Start Angle")) {
        impl_->settings_.curveStartAngle = value.toFloat();
    } else if (key == QStringLiteral("Curve End Angle")) {
        impl_->settings_.curveEndAngle = value.toFloat();
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
