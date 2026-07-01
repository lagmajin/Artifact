module;
#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <QJsonArray>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QString>
#include <QStringList>
#include <QVector2D>
#include <QVector3D>

export module Artifact.Layer.Component.System;

export namespace Artifact {

enum class LayerComponentPhase : std::uint8_t {
    Source = 0,
    Drive,
    Generate,
    Arrange,
    Intent,
    Dynamics,
    Topology,
    Emit,
    RenderExtraction,
};

enum class LayerComponentScope : std::uint8_t {
    Layer = 0,
    InstanceSet,
    Composition,
};

struct LayerComponentDescriptor {
    QString componentId;
    QString typeId;
    std::uint32_t version = 1;
    bool enabled = true;
    LayerComponentPhase phase = LayerComponentPhase::Source;
    LayerComponentScope scope = LayerComponentScope::Layer;
    int order = 0;
    QStringList requiredTypeIds;
    QJsonObject settings;
};

struct LayerComponentValidationIssue {
    QString componentId;
    QString message;
    bool error = false;
};

struct LayerGeneratorDescriptor {
    QString generatorId;
    QString typeId;
    std::uint32_t version = 1;
    bool enabled = true;
    int order = 0;
    QJsonObject settings;
};

struct LayerFieldDescriptor {
    QString fieldId;
    QString typeId;
    std::uint32_t version = 1;
    bool enabled = true;
    int order = 0;
    QString blendMode = QStringLiteral("normal");
    float strength = 1.0f;
    bool invert = false;
    QJsonObject settings;
};

struct LayerModifierDescriptor {
    QString modifierId;
    QString typeId;
    std::uint32_t version = 1;
    bool enabled = true;
    int order = 0;
    QJsonObject settings;
};

inline QJsonObject toJsonObject(const LayerGeneratorDescriptor& descriptor) {
    QJsonObject obj;
    obj[QStringLiteral("generatorId")] = descriptor.generatorId;
    obj[QStringLiteral("typeId")] = descriptor.typeId;
    obj[QStringLiteral("version")] = static_cast<qint64>(descriptor.version);
    obj[QStringLiteral("enabled")] = descriptor.enabled;
    obj[QStringLiteral("order")] = descriptor.order;
    obj[QStringLiteral("settings")] = descriptor.settings;
    return obj;
}

inline std::optional<LayerGeneratorDescriptor>
layerGeneratorDescriptorFromJson(const QJsonObject& obj) {
    LayerGeneratorDescriptor descriptor;
    descriptor.generatorId =
        obj.value(QStringLiteral("generatorId")).toString().trimmed();
    descriptor.typeId =
        obj.value(QStringLiteral("typeId")).toString().trimmed();
    if (descriptor.generatorId.isEmpty() || descriptor.typeId.isEmpty()) {
        return std::nullopt;
    }
    descriptor.version = static_cast<std::uint32_t>(
        std::max<qint64>(1, obj.value(QStringLiteral("version")).toInteger(1)));
    descriptor.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    descriptor.order = obj.value(QStringLiteral("order")).toInt(0);
    descriptor.settings = obj.value(QStringLiteral("settings")).toObject();
    return descriptor;
}

inline QJsonObject toJsonObject(const LayerFieldDescriptor& descriptor) {
    QJsonObject obj;
    obj[QStringLiteral("fieldId")] = descriptor.fieldId;
    obj[QStringLiteral("typeId")] = descriptor.typeId;
    obj[QStringLiteral("version")] = static_cast<qint64>(descriptor.version);
    obj[QStringLiteral("enabled")] = descriptor.enabled;
    obj[QStringLiteral("order")] = descriptor.order;
    obj[QStringLiteral("blendMode")] = descriptor.blendMode;
    obj[QStringLiteral("strength")] = descriptor.strength;
    obj[QStringLiteral("invert")] = descriptor.invert;
    obj[QStringLiteral("settings")] = descriptor.settings;
    return obj;
}

inline std::optional<LayerFieldDescriptor>
layerFieldDescriptorFromJson(const QJsonObject& obj) {
    LayerFieldDescriptor descriptor;
    descriptor.fieldId =
        obj.value(QStringLiteral("fieldId")).toString().trimmed();
    descriptor.typeId =
        obj.value(QStringLiteral("typeId")).toString().trimmed();
    if (descriptor.fieldId.isEmpty() || descriptor.typeId.isEmpty()) {
        return std::nullopt;
    }
    descriptor.version = static_cast<std::uint32_t>(
        std::max<qint64>(1, obj.value(QStringLiteral("version")).toInteger(1)));
    descriptor.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    descriptor.order = obj.value(QStringLiteral("order")).toInt(0);
    descriptor.blendMode =
        obj.value(QStringLiteral("blendMode")).toString(QStringLiteral("normal"));
    descriptor.strength =
        static_cast<float>(obj.value(QStringLiteral("strength")).toDouble(1.0));
    descriptor.invert = obj.value(QStringLiteral("invert")).toBool(false);
    descriptor.settings = obj.value(QStringLiteral("settings")).toObject();
    return descriptor;
}

inline QJsonObject toJsonObject(const LayerModifierDescriptor& descriptor) {
    QJsonObject obj;
    obj[QStringLiteral("modifierId")] = descriptor.modifierId;
    obj[QStringLiteral("typeId")] = descriptor.typeId;
    obj[QStringLiteral("version")] = static_cast<qint64>(descriptor.version);
    obj[QStringLiteral("enabled")] = descriptor.enabled;
    obj[QStringLiteral("order")] = descriptor.order;
    obj[QStringLiteral("settings")] = descriptor.settings;
    return obj;
}

inline std::optional<LayerModifierDescriptor>
layerModifierDescriptorFromJson(const QJsonObject& obj) {
    LayerModifierDescriptor descriptor;
    descriptor.modifierId =
        obj.value(QStringLiteral("modifierId")).toString().trimmed();
    descriptor.typeId =
        obj.value(QStringLiteral("typeId")).toString().trimmed();
    if (descriptor.modifierId.isEmpty() || descriptor.typeId.isEmpty()) {
        return std::nullopt;
    }
    descriptor.version = static_cast<std::uint32_t>(
        std::max<qint64>(1, obj.value(QStringLiteral("version")).toInteger(1)));
    descriptor.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    descriptor.order = obj.value(QStringLiteral("order")).toInt(0);
    descriptor.settings = obj.value(QStringLiteral("settings")).toObject();
    return descriptor;
}

class LayerComponentHost {
public:
    bool upsert(LayerComponentDescriptor descriptor);
    bool remove(const QString& componentId);
    void clear();

    LayerComponentDescriptor* find(const QString& componentId);
    const LayerComponentDescriptor* find(const QString& componentId) const;
    LayerComponentDescriptor* findByType(const QString& typeId);
    const LayerComponentDescriptor* findByType(const QString& typeId) const;

    std::vector<LayerComponentDescriptor> components() const;
    std::vector<LayerComponentDescriptor> enabledForPhase(
        LayerComponentPhase phase) const;
    std::vector<LayerComponentValidationIssue> validate() const;

    QJsonArray toJson() const;
    void fromJson(const QJsonArray& array);

private:
    std::vector<LayerComponentDescriptor> components_;
};

struct SimulationEntityId {
    QString ownerLayerId;
    QString componentId;
    std::uint64_t localId = 0;
    std::uint32_t generation = 0;

    bool isValid() const {
        return !ownerLayerId.trimmed().isEmpty() &&
               !componentId.trimmed().isEmpty();
    }
};

struct LayerInstanceState {
    SimulationEntityId entityId;
    QMatrix4x4 transform;
    QVector3D linearVelocity{0.0f, 0.0f, 0.0f};
    float opacity = 1.0f;
    bool active = true;
};

struct LayerMotionIntent {
    SimulationEntityId entityId;
    QVector3D desiredVelocity{0.0f, 0.0f, 0.0f};
    QVector3D desiredFacing{1.0f, 0.0f, 0.0f};
    float weight = 1.0f;
};

struct LayerContactEvent {
    SimulationEntityId first;
    SimulationEntityId second;
    QVector3D position{0.0f, 0.0f, 0.0f};
    QVector3D normal{0.0f, -1.0f, 0.0f};
    float impulse = 0.0f;
};

struct LayerFractureEvent {
    SimulationEntityId source;
    QVector3D position{0.0f, 0.0f, 0.0f};
    QVector3D impulse{0.0f, 0.0f, 0.0f};
    float damage = 0.0f;
    std::uint32_t requestedFragmentCount = 0;
};

struct LayerParticleSpawnEvent {
    SimulationEntityId source;
    QVector3D position{0.0f, 0.0f, 0.0f};
    QVector3D velocity{0.0f, 0.0f, 0.0f};
    std::uint32_t count = 0;
    std::uint32_t seed = 0;
};

struct LayerEvaluationState {
    std::vector<LayerInstanceState> instances;
    std::vector<LayerMotionIntent> intents;
    std::vector<LayerContactEvent> contacts;
    std::vector<LayerFractureEvent> pendingFractures;
    std::vector<LayerParticleSpawnEvent> pendingParticleSpawns;

    void clearTransientEvents() {
        contacts.clear();
        pendingFractures.clear();
        pendingParticleSpawns.clear();
    }
};

struct LayerEvaluationContext {
    std::int64_t frame = 0;
    double timeSeconds = 0.0;
    float fixedDeltaSeconds = 1.0f / 60.0f;
    std::uint32_t deterministicSeed = 0;
    bool interactive = false;
};

inline QString layerComponentPhaseName(LayerComponentPhase phase) {
    switch (phase) {
    case LayerComponentPhase::Source:
        return QStringLiteral("source");
    case LayerComponentPhase::Drive:
        return QStringLiteral("drive");
    case LayerComponentPhase::Generate:
        return QStringLiteral("generate");
    case LayerComponentPhase::Arrange:
        return QStringLiteral("arrange");
    case LayerComponentPhase::Intent:
        return QStringLiteral("intent");
    case LayerComponentPhase::Dynamics:
        return QStringLiteral("dynamics");
    case LayerComponentPhase::Topology:
        return QStringLiteral("topology");
    case LayerComponentPhase::Emit:
        return QStringLiteral("emit");
    case LayerComponentPhase::RenderExtraction:
        return QStringLiteral("render");
    }
    return QStringLiteral("source");
}

inline std::optional<LayerComponentPhase>
layerComponentPhaseFromName(const QString& name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("source"))
        return LayerComponentPhase::Source;
    if (normalized == QStringLiteral("drive"))
        return LayerComponentPhase::Drive;
    if (normalized == QStringLiteral("generate"))
        return LayerComponentPhase::Generate;
    if (normalized == QStringLiteral("arrange"))
        return LayerComponentPhase::Arrange;
    if (normalized == QStringLiteral("intent"))
        return LayerComponentPhase::Intent;
    if (normalized == QStringLiteral("dynamics"))
        return LayerComponentPhase::Dynamics;
    if (normalized == QStringLiteral("topology"))
        return LayerComponentPhase::Topology;
    if (normalized == QStringLiteral("emit"))
        return LayerComponentPhase::Emit;
    if (normalized == QStringLiteral("render"))
        return LayerComponentPhase::RenderExtraction;
    return std::nullopt;
}

inline QString layerComponentScopeName(LayerComponentScope scope) {
    switch (scope) {
    case LayerComponentScope::Layer:
        return QStringLiteral("layer");
    case LayerComponentScope::InstanceSet:
        return QStringLiteral("instances");
    case LayerComponentScope::Composition:
        return QStringLiteral("composition");
    }
    return QStringLiteral("layer");
}

inline LayerComponentScope layerComponentScopeFromName(const QString& name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("instances"))
        return LayerComponentScope::InstanceSet;
    if (normalized == QStringLiteral("composition"))
        return LayerComponentScope::Composition;
    return LayerComponentScope::Layer;
}

inline bool LayerComponentHost::upsert(LayerComponentDescriptor descriptor) {
    descriptor.componentId = descriptor.componentId.trimmed();
    descriptor.typeId = descriptor.typeId.trimmed();
    if (descriptor.componentId.isEmpty() || descriptor.typeId.isEmpty()) {
        return false;
    }

    if (auto* existing = find(descriptor.componentId)) {
        *existing = std::move(descriptor);
        return true;
    }
    components_.push_back(std::move(descriptor));
    return true;
}

inline bool LayerComponentHost::remove(const QString& componentId) {
    const QString normalized = componentId.trimmed();
    const auto oldSize = components_.size();
    components_.erase(
        std::remove_if(
            components_.begin(), components_.end(),
            [&normalized](const LayerComponentDescriptor& descriptor) {
                return descriptor.componentId == normalized;
            }),
        components_.end());
    return components_.size() != oldSize;
}

inline void LayerComponentHost::clear() {
    components_.clear();
}

inline LayerComponentDescriptor*
LayerComponentHost::find(const QString& componentId) {
    const QString normalized = componentId.trimmed();
    for (auto& descriptor : components_) {
        if (descriptor.componentId == normalized)
            return &descriptor;
    }
    return nullptr;
}

inline const LayerComponentDescriptor*
LayerComponentHost::find(const QString& componentId) const {
    const QString normalized = componentId.trimmed();
    for (const auto& descriptor : components_) {
        if (descriptor.componentId == normalized)
            return &descriptor;
    }
    return nullptr;
}

inline LayerComponentDescriptor*
LayerComponentHost::findByType(const QString& typeId) {
    const QString normalized = typeId.trimmed();
    for (auto& descriptor : components_) {
        if (descriptor.typeId == normalized)
            return &descriptor;
    }
    return nullptr;
}

inline const LayerComponentDescriptor*
LayerComponentHost::findByType(const QString& typeId) const {
    const QString normalized = typeId.trimmed();
    for (const auto& descriptor : components_) {
        if (descriptor.typeId == normalized)
            return &descriptor;
    }
    return nullptr;
}

inline std::vector<LayerComponentDescriptor>
LayerComponentHost::components() const {
    return components_;
}

inline std::vector<LayerComponentDescriptor>
LayerComponentHost::enabledForPhase(LayerComponentPhase phase) const {
    std::vector<LayerComponentDescriptor> result;
    for (const auto& descriptor : components_) {
        if (descriptor.enabled && descriptor.phase == phase) {
            result.push_back(descriptor);
        }
    }
    std::stable_sort(
        result.begin(), result.end(),
        [](const LayerComponentDescriptor& a,
           const LayerComponentDescriptor& b) {
            if (a.order != b.order)
                return a.order < b.order;
            return a.componentId < b.componentId;
        });
    return result;
}

inline std::vector<LayerComponentValidationIssue>
LayerComponentHost::validate() const {
    std::vector<LayerComponentValidationIssue> issues;
    for (std::size_t i = 0; i < components_.size(); ++i) {
        const auto& descriptor = components_[i];
        const QString componentLabel = descriptor.componentId.trimmed().isEmpty()
                                           ? descriptor.typeId.trimmed()
                                           : descriptor.componentId.trimmed();
        if (descriptor.componentId.trimmed().isEmpty() ||
            descriptor.typeId.trimmed().isEmpty()) {
            issues.push_back(
                {descriptor.componentId,
                 QStringLiteral("Component id and type id must be non-empty."),
                 true});
            continue;
        }
        for (std::size_t j = i + 1; j < components_.size(); ++j) {
            if (components_[j].componentId == descriptor.componentId) {
                issues.push_back(
                    {descriptor.componentId,
                     QStringLiteral("Duplicate component id."),
                     true});
            }
        }
        for (const auto& requiredTypeId : descriptor.requiredTypeIds) {
            const auto* dependency = findByType(requiredTypeId);
            if (!dependency) {
                issues.push_back(
                    {descriptor.componentId,
                     QStringLiteral(
                         "%1 requires %2, but that component is missing.")
                         .arg(componentLabel, requiredTypeId),
                     true});
            } else if (descriptor.enabled && !dependency->enabled) {
                issues.push_back(
                    {descriptor.componentId,
                     QStringLiteral(
                         "%1 requires %2 to be enabled first.")
                         .arg(componentLabel, requiredTypeId),
                     true});
            } else if (static_cast<int>(dependency->phase) >
                       static_cast<int>(descriptor.phase)) {
                issues.push_back(
                    {descriptor.componentId,
                     QStringLiteral(
                         "%1 depends on %2, but %2 evaluates later (%3 -> %4).")
                         .arg(componentLabel, requiredTypeId,
                              layerComponentPhaseName(descriptor.phase),
                              layerComponentPhaseName(dependency->phase)),
                     true});
            }
        }
    }
    return issues;
}

inline QJsonArray LayerComponentHost::toJson() const {
    QJsonArray array;
    for (const auto& descriptor : components_) {
        QJsonObject obj;
        obj[QStringLiteral("componentId")] = descriptor.componentId;
        obj[QStringLiteral("typeId")] = descriptor.typeId;
        obj[QStringLiteral("version")] =
            static_cast<qint64>(descriptor.version);
        obj[QStringLiteral("enabled")] = descriptor.enabled;
        obj[QStringLiteral("phase")] =
            layerComponentPhaseName(descriptor.phase);
        obj[QStringLiteral("scope")] =
            layerComponentScopeName(descriptor.scope);
        obj[QStringLiteral("order")] = descriptor.order;
        QJsonArray dependencies;
        for (const auto& dependency : descriptor.requiredTypeIds) {
            dependencies.append(dependency);
        }
        obj[QStringLiteral("requires")] = dependencies;
        obj[QStringLiteral("settings")] = descriptor.settings;
        array.append(obj);
    }
    return array;
}

inline void LayerComponentHost::fromJson(const QJsonArray& array) {
    components_.clear();
    for (const auto& value : array) {
        if (!value.isObject())
            continue;
        const auto obj = value.toObject();
        LayerComponentDescriptor descriptor;
        descriptor.componentId =
            obj.value(QStringLiteral("componentId")).toString();
        descriptor.typeId = obj.value(QStringLiteral("typeId")).toString();
        descriptor.version = static_cast<std::uint32_t>(
            std::max<qint64>(
                1, obj.value(QStringLiteral("version")).toInteger(1)));
        descriptor.enabled =
            obj.value(QStringLiteral("enabled")).toBool(true);
        descriptor.phase =
            layerComponentPhaseFromName(
                obj.value(QStringLiteral("phase")).toString())
                .value_or(LayerComponentPhase::Source);
        descriptor.scope = layerComponentScopeFromName(
            obj.value(QStringLiteral("scope")).toString());
        descriptor.order =
            obj.value(QStringLiteral("order")).toInt(0);
        const auto dependencies =
            obj.value(QStringLiteral("requires")).toArray();
        for (const auto& dependency : dependencies) {
            const QString typeId = dependency.toString().trimmed();
            if (!typeId.isEmpty())
                descriptor.requiredTypeIds.push_back(typeId);
        }
        descriptor.settings =
            obj.value(QStringLiteral("settings")).toObject();
        upsert(std::move(descriptor));
    }
}

inline LayerComponentDescriptor makeClonerComponentDescriptor(bool enabled) {
    return {
        QStringLiteral("builtin.cloner"),
        QStringLiteral("artifact.component.cloner"),
        1,
        enabled,
        LayerComponentPhase::Generate,
        LayerComponentScope::InstanceSet,
        100,
        {},
        {},
    };
}

inline LayerComponentDescriptor makeLayoutComponentDescriptor(bool enabled) {
    return {
        QStringLiteral("builtin.layout"),
        QStringLiteral("artifact.component.layout"),
        1,
        enabled,
        LayerComponentPhase::Arrange,
        LayerComponentScope::InstanceSet,
        200,
        {QStringLiteral("artifact.component.cloner")},
        {},
    };
}

inline LayerComponentDescriptor makeCrowdComponentDescriptor(bool enabled) {
    return {
        QStringLiteral("builtin.crowd"),
        QStringLiteral("artifact.component.crowd"),
        1,
        enabled,
        LayerComponentPhase::Intent,
        LayerComponentScope::InstanceSet,
        300,
        {QStringLiteral("artifact.component.cloner")},
        {},
    };
}

inline LayerComponentDescriptor makeMotionDynamicsComponentDescriptor(
    bool enabled) {
    return {
        QStringLiteral("builtin.motion-dynamics"),
        QStringLiteral("artifact.component.motion-dynamics"),
        1,
        enabled,
        LayerComponentPhase::Drive,
        LayerComponentScope::Layer,
        400,
        {},
        {},
    };
}

inline LayerComponentDescriptor makeCollisionComponentDescriptor(bool enabled) {
    return {
        QStringLiteral("builtin.collision"),
        QStringLiteral("artifact.component.collision"),
        1,
        enabled,
        LayerComponentPhase::Dynamics,
        LayerComponentScope::Composition,
        500,
        {QStringLiteral("artifact.component.cloner")},
        {},
    };
}

inline LayerComponentDescriptor makeFractureComponentDescriptor(bool enabled) {
    return {
        QStringLiteral("builtin.fracture"),
        QStringLiteral("artifact.component.fracture"),
        1,
        enabled,
        LayerComponentPhase::Topology,
        LayerComponentScope::InstanceSet,
        600,
        {QStringLiteral("artifact.component.collision")},
        {},
    };
}

inline LayerComponentDescriptor makeParticleEmitterComponentDescriptor(
    bool enabled) {
    return {
        QStringLiteral("builtin.particle-emitter"),
        QStringLiteral("artifact.component.particle-emitter"),
        1,
        enabled,
        LayerComponentPhase::Emit,
        LayerComponentScope::InstanceSet,
        700,
        {QStringLiteral("artifact.component.cloner")},
        {},
    };
}

inline LayerComponentDescriptor makeFluidComponentDescriptor(bool enabled) {
    return {
        QStringLiteral("builtin.fluid"),
        QStringLiteral("artifact.component.fluid"),
        1,
        enabled,
        LayerComponentPhase::Dynamics,
        LayerComponentScope::Composition,
        750,
        {},
        {},
    };
}

} // namespace Artifact
