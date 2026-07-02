module;
#include <algorithm>
#include <cstdint>
#include <functional>
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

enum class LayerComponentRuntimeOutcome : std::uint8_t {
    Skipped = 0,
    Applied,
    Failed,
};

struct LayerComponentRuntimeRecord {
    QString componentId;
    QString typeId;
    LayerComponentPhase phase = LayerComponentPhase::Source;
    LayerComponentRuntimeOutcome outcome = LayerComponentRuntimeOutcome::Skipped;
    QString message;
};

struct LayerComponentRuntimeLog {
    std::vector<LayerComponentRuntimeRecord> records;

    void clear() {
        records.clear();
    }

    bool hasFailures() const {
        return std::any_of(
            records.begin(), records.end(),
            [](const LayerComponentRuntimeRecord& record) {
                return record.outcome == LayerComponentRuntimeOutcome::Failed;
            });
    }
};

struct LayerComponentDefinition {
    QString typeId;
    QString displayName;
    QString description;
    bool enabledByDefault = true;
    LayerComponentPhase phase = LayerComponentPhase::Source;
    LayerComponentScope scope = LayerComponentScope::Layer;
    int order = 0;
};

using LayerComponentProcessor = std::function<bool(
    const LayerComponentDescriptor& descriptor,
    const LayerComponentDefinition& definition,
    const LayerEvaluationContext& context,
    LayerEvaluationState& state,
    LayerComponentRuntimeLog& log)>;

class LayerComponentRegistry {
public:
    bool upsert(LayerComponentDefinition definition);
    bool remove(const QString& typeId);
    void clear();

    LayerComponentDefinition* find(const QString& typeId);
    const LayerComponentDefinition* find(const QString& typeId) const;

    std::vector<LayerComponentDefinition> definitions() const;

private:
    std::vector<LayerComponentDefinition> definitions_;
};

class LayerComponentRuntime {
public:
    bool registerProcessor(QString typeId, LayerComponentProcessor processor);
    bool removeProcessor(const QString& typeId);
    void clearProcessors();

    const LayerComponentRegistry* registry() const;
    void setRegistry(const LayerComponentRegistry* registry);

    std::vector<LayerComponentRuntimeRecord> evaluate(
        const LayerComponentHost& host,
        const LayerEvaluationContext& context,
        LayerEvaluationState& state) const;

private:
    const LayerComponentRegistry* registry_ = nullptr;
    std::vector<std::pair<QString, LayerComponentProcessor>> processors_;
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

inline bool LayerComponentRegistry::upsert(LayerComponentDefinition definition) {
    definition.typeId = definition.typeId.trimmed();
    definition.displayName = definition.displayName.trimmed();
    if (definition.typeId.isEmpty()) {
        return false;
    }

    if (auto* existing = find(definition.typeId)) {
        *existing = std::move(definition);
        return true;
    }

    definitions_.push_back(std::move(definition));
    return true;
}

inline bool LayerComponentRegistry::remove(const QString& typeId) {
    const QString normalized = typeId.trimmed();
    const auto oldSize = definitions_.size();
    definitions_.erase(
        std::remove_if(
            definitions_.begin(), definitions_.end(),
            [&normalized](const LayerComponentDefinition& definition) {
                return definition.typeId == normalized;
            }),
        definitions_.end());
    return definitions_.size() != oldSize;
}

inline void LayerComponentRegistry::clear() {
    definitions_.clear();
}

inline LayerComponentDefinition*
LayerComponentRegistry::find(const QString& typeId) {
    const QString normalized = typeId.trimmed();
    for (auto& definition : definitions_) {
        if (definition.typeId == normalized)
            return &definition;
    }
    return nullptr;
}

inline const LayerComponentDefinition*
LayerComponentRegistry::find(const QString& typeId) const {
    const QString normalized = typeId.trimmed();
    for (const auto& definition : definitions_) {
        if (definition.typeId == normalized)
            return &definition;
    }
    return nullptr;
}

inline std::vector<LayerComponentDefinition>
LayerComponentRegistry::definitions() const {
    return definitions_;
}

inline bool LayerComponentRuntime::registerProcessor(
    QString typeId, LayerComponentProcessor processor) {
    typeId = typeId.trimmed();
    if (typeId.isEmpty() || !processor) {
        return false;
    }

    for (auto& entry : processors_) {
        if (entry.first == typeId) {
            entry.second = std::move(processor);
            return true;
        }
    }

    processors_.emplace_back(std::move(typeId), std::move(processor));
    return true;
}

inline bool LayerComponentRuntime::removeProcessor(const QString& typeId) {
    const QString normalized = typeId.trimmed();
    const auto oldSize = processors_.size();
    processors_.erase(
        std::remove_if(
            processors_.begin(), processors_.end(),
            [&normalized](const auto& entry) { return entry.first == normalized; }),
        processors_.end());
    return processors_.size() != oldSize;
}

inline void LayerComponentRuntime::clearProcessors() {
    processors_.clear();
}

inline const LayerComponentRegistry* LayerComponentRuntime::registry() const {
    return registry_;
}

inline void LayerComponentRuntime::setRegistry(
    const LayerComponentRegistry* registry) {
    registry_ = registry;
}

inline std::vector<LayerComponentRuntimeRecord>
LayerComponentRuntime::evaluate(const LayerComponentHost& host,
                                const LayerEvaluationContext& context,
                                LayerEvaluationState& state) const {
    LayerComponentRuntimeLog log;
    std::vector<LayerComponentRuntimeRecord> records;
    for (int phaseIndex = static_cast<int>(LayerComponentPhase::Source);
         phaseIndex <= static_cast<int>(LayerComponentPhase::RenderExtraction);
         ++phaseIndex) {
        const auto phase = static_cast<LayerComponentPhase>(phaseIndex);
        const auto components = host.enabledForPhase(phase);
        for (const auto& descriptor : components) {
            LayerComponentRuntimeRecord record;
            record.componentId = descriptor.componentId;
            record.typeId = descriptor.typeId;
            record.phase = descriptor.phase;

            const auto* definition = registry_ ? registry_->find(descriptor.typeId)
                                               : nullptr;
            if (!definition) {
                record.outcome = LayerComponentRuntimeOutcome::Skipped;
                record.message = QStringLiteral("No registered component definition.");
                records.push_back(record);
                continue;
            }

            const auto processorIt = std::find_if(
                processors_.begin(), processors_.end(),
                [&descriptor](const auto& entry) {
                    return entry.first == descriptor.typeId;
                });

            if (processorIt == processors_.end() || !processorIt->second) {
                record.outcome = LayerComponentRuntimeOutcome::Skipped;
                record.message = QStringLiteral("No registered component processor.");
                records.push_back(record);
                continue;
            }

            const bool applied = processorIt->second(
                descriptor, *definition, context, state, log);
            record.outcome = applied ? LayerComponentRuntimeOutcome::Applied
                                     : LayerComponentRuntimeOutcome::Failed;
            if (record.message.isEmpty()) {
                record.message = applied
                    ? QStringLiteral("Applied.")
                    : QStringLiteral("Processor reported failure.");
            }
            records.push_back(record);
        }
    }
    for (const auto& note : log.records) {
        records.push_back(note);
    }
    return records;
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
                     QStringLiteral("Missing required component type: %1")
                         .arg(requiredTypeId),
                     true});
            } else if (static_cast<int>(dependency->phase) >
                       static_cast<int>(descriptor.phase)) {
                issues.push_back(
                    {descriptor.componentId,
                     QStringLiteral(
                         "Required component %1 evaluates in a later phase.")
                         .arg(requiredTypeId),
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
        {},
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
        {},
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
        {},
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
        {},
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
        {},
        {},
    };
}

inline LayerComponentDescriptor makeScriptComponentDescriptor(bool enabled) {
    return {
        QStringLiteral("builtin.script"),
        QStringLiteral("artifact.component.script"),
        1,
        enabled,
        LayerComponentPhase::Drive,
        LayerComponentScope::Layer,
        50,
        {},
        {},
    };
}

inline LayerComponentDefinition makeBuiltinClonerComponentDefinition() {
    return {
        QStringLiteral("artifact.component.cloner"),
        QStringLiteral("Cloner"),
        QStringLiteral("Duplicates layer instances across a generated set."),
        true,
        LayerComponentPhase::Generate,
        LayerComponentScope::InstanceSet,
        100,
    };
}

inline LayerComponentDefinition makeBuiltinLayoutComponentDefinition() {
    return {
        QStringLiteral("artifact.component.layout"),
        QStringLiteral("Layout"),
        QStringLiteral("Arranges generated instances in a grid or stack."),
        true,
        LayerComponentPhase::Arrange,
        LayerComponentScope::InstanceSet,
        200,
    };
}

inline LayerComponentDefinition makeBuiltinCrowdComponentDefinition() {
    return {
        QStringLiteral("artifact.component.crowd"),
        QStringLiteral("Crowd"),
        QStringLiteral("Blends motion intent across a crowd of instances."),
        true,
        LayerComponentPhase::Intent,
        LayerComponentScope::InstanceSet,
        300,
    };
}

inline LayerComponentDefinition makeBuiltinMotionDynamicsComponentDefinition() {
    return {
        QStringLiteral("artifact.component.motion-dynamics"),
        QStringLiteral("Motion Dynamics"),
        QStringLiteral("Applies layer-level drive dynamics before instance generation."),
        true,
        LayerComponentPhase::Drive,
        LayerComponentScope::Layer,
        400,
    };
}

inline LayerComponentDefinition makeBuiltinScriptComponentDefinition() {
    return {
        QStringLiteral("artifact.component.script"),
        QStringLiteral("Script"),
        QStringLiteral("Runs Unity-like lifecycle hooks for the layer."),
        true,
        LayerComponentPhase::Drive,
        LayerComponentScope::Layer,
        50,
    };
}

inline LayerComponentDefinition makeBuiltinCollisionComponentDefinition() {
    return {
        QStringLiteral("artifact.component.collision"),
        QStringLiteral("Collision"),
        QStringLiteral("Registers composition-wide collision resolution."),
        true,
        LayerComponentPhase::Dynamics,
        LayerComponentScope::Composition,
        500,
    };
}

inline LayerComponentDefinition makeBuiltinFractureComponentDefinition() {
    return {
        QStringLiteral("artifact.component.fracture"),
        QStringLiteral("Fracture"),
        QStringLiteral("Generates fracture events from high-energy impacts."),
        true,
        LayerComponentPhase::Topology,
        LayerComponentScope::InstanceSet,
        600,
    };
}

inline LayerComponentDefinition makeBuiltinParticleEmitterComponentDefinition() {
    return {
        QStringLiteral("artifact.component.particle-emitter"),
        QStringLiteral("Particle Emitter"),
        QStringLiteral("Spawns particles during the emit phase."),
        true,
        LayerComponentPhase::Emit,
        LayerComponentScope::InstanceSet,
        700,
    };
}

inline LayerComponentRegistry makeBuiltinLayerComponentRegistry() {
    LayerComponentRegistry registry;
    registry.upsert(makeBuiltinScriptComponentDefinition());
    registry.upsert(makeBuiltinMotionDynamicsComponentDefinition());
    registry.upsert(makeBuiltinClonerComponentDefinition());
    registry.upsert(makeBuiltinLayoutComponentDefinition());
    registry.upsert(makeBuiltinCrowdComponentDefinition());
    registry.upsert(makeBuiltinCollisionComponentDefinition());
    registry.upsert(makeBuiltinFractureComponentDefinition());
    registry.upsert(makeBuiltinParticleEmitterComponentDefinition());
    return registry;
}

} // namespace Artifact
