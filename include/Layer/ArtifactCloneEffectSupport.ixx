module;
#include <utility>
#include <algorithm>
#include <functional>
#include <memory>
#include <vector>
#include <QMatrix4x4>
#include <QString>
#include <QVariant>

export module Artifact.Layer.CloneEffectSupport;

import Artifact.Layer.Abstract;
import Artifact.Effect.Generator.Cloner;
import Property.Abstract;

export namespace Artifact {

class ArtifactAbstractLayer;

struct CloneRenderInstance {
    QMatrix4x4 transform;
    float weight = 1.0f;
};

bool cloneComponentBoolProperty(const ArtifactAbstractLayer* layer,
                                const QString& propertyPath);
int cloneComponentIntProperty(const ArtifactAbstractLayer* layer,
                              const QString& propertyPath,
                              int fallback);
float cloneComponentFloatProperty(const ArtifactAbstractLayer* layer,
                                  const QString& propertyPath,
                                  float fallback);
std::vector<CloneRenderInstance> mographComponentInstances(
    const ArtifactAbstractLayer* layer,
    const QMatrix4x4& baseTransform);

export std::vector<CloneRenderInstance> cloneRenderInstances(const ArtifactAbstractLayer* layer,
                                                             const QMatrix4x4& baseTransform)
{
    std::vector<CloneRenderInstance> instances;
    if (!layer) {
        return instances;
    }

    for (const auto& effect : layer->getEffects()) {
        const auto cloner = std::dynamic_pointer_cast<ClonerGenerator>(effect);
        if (!cloner || !cloner->isEnabled()) {
            continue;
        }

        const auto clones = cloner->generateCloneData();
        instances.reserve(clones.size());
        for (const auto& clone : clones) {
            if (!clone.visible) {
                continue;
            }

            CloneRenderInstance instance;
            instance.transform = baseTransform * clone.transform;
            instance.weight = std::clamp(clone.weight, 0.0f, 1.0f);
            instances.push_back(instance);
        }
        if (!instances.empty()) {
            return instances;
        }
        break;
    }

    instances = mographComponentInstances(layer, baseTransform);
    if (!instances.empty()) {
        return instances;
    }

    instances.push_back(CloneRenderInstance{baseTransform, 1.0f});
    return instances;
}

export void drawWithClonerEffect(const ArtifactAbstractLayer* layer,
                                 const QMatrix4x4& baseTransform,
                                 const std::function<void(const QMatrix4x4&, float)>& drawFn)
{
    const auto instances = cloneRenderInstances(layer, baseTransform);
    for (const auto& instance : instances) {
        drawFn(instance.transform, instance.weight);
    }
}

} // namespace Artifact

namespace Artifact {

bool cloneComponentBoolProperty(const ArtifactAbstractLayer* layer,
                                const QString& propertyPath)
{
    if (!layer) {
        return false;
    }
    const auto property = layer->getProperty(propertyPath);
    return property ? property->getValue().toBool() : false;
}

int cloneComponentIntProperty(const ArtifactAbstractLayer* layer,
                              const QString& propertyPath,
                              int fallback)
{
    if (!layer) {
        return fallback;
    }
    const auto property = layer->getProperty(propertyPath);
    return property ? property->getValue().toInt() : fallback;
}

float cloneComponentFloatProperty(const ArtifactAbstractLayer* layer,
                                  const QString& propertyPath,
                                  float fallback)
{
    if (!layer) {
        return fallback;
    }
    const auto property = layer->getProperty(propertyPath);
    return property ? property->getValue().toFloat() : fallback;
}

std::vector<CloneRenderInstance> mographComponentInstances(
    const ArtifactAbstractLayer* layer,
    const QMatrix4x4& baseTransform)
{
    std::vector<CloneRenderInstance> instances;
    if (!cloneComponentBoolProperty(
            layer, QStringLiteral("component.mograph.enabled"))) {
        return instances;
    }

    const int count = std::max(
        1, cloneComponentIntProperty(
               layer, QStringLiteral("component.mograph.cloneCount"), 3));
    const float offsetX = cloneComponentFloatProperty(
        layer, QStringLiteral("component.mograph.offsetX"), 160.0f);
    const float offsetY = cloneComponentFloatProperty(
        layer, QStringLiteral("component.mograph.offsetY"), 48.0f);

    instances.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        QMatrix4x4 cloneTransform;
        cloneTransform.setToIdentity();
        cloneTransform.translate(offsetX * static_cast<float>(i),
                                 offsetY * static_cast<float>(i), 0.0f);
        instances.push_back(CloneRenderInstance{baseTransform * cloneTransform,
                                                1.0f});
    }
    return instances;
}

} // namespace Artifact
