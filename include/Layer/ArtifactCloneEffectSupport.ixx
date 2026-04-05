module;
#include <QMatrix4x4>
#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

export module Artifact.Layer.CloneEffectSupport;

import Artifact.Layer.Abstract;
import Artifact.Effect.Generator.Cloner;

export namespace Artifact {

struct CloneRenderInstance {
    QMatrix4x4 transform;
    float weight = 1.0f;
};

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
