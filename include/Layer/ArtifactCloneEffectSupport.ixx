module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <random>
#include <vector>
#include <Graphics/InstanceData.h>
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
int cloneComponentMode(const ArtifactAbstractLayer* layer);
std::vector<CloneRenderInstance> clonerComponentInstances(
      const ArtifactAbstractLayer* layer,
      const QMatrix4x4& baseTransform);
ArtifactCore::InstanceData cloneRenderInstanceToInstanceData(
    const CloneRenderInstance& instance);

export std::vector<CloneRenderInstance> cloneRenderInstances(const ArtifactAbstractLayer* layer,
                                                             const QMatrix4x4& baseTransform)
{
    std::vector<CloneRenderInstance> instances;
    if (!layer) {
        return instances;
    }

    bool hasEnabledCloner = false;
    for (const auto& effect : layer->getEffects()) {
        const auto cloner = std::dynamic_pointer_cast<ClonerGenerator>(effect);
        if (!cloner || !cloner->isEnabled()) {
            continue;
        }
        hasEnabledCloner = true;

        const auto clones = cloner->generateCloneData();
        instances.reserve(instances.size() + clones.size());
        for (const auto& clone : clones) {
            if (!clone.visible) {
                continue;
            }

            CloneRenderInstance instance;
            instance.transform = baseTransform * clone.transform;
            instance.weight = std::clamp(clone.weight, 0.0f, 1.0f);
            instances.push_back(instance);
        }
    }

    if (hasEnabledCloner) {
        if (instances.empty()) {
            instances.push_back(CloneRenderInstance{baseTransform, 1.0f});
        }
        return instances;
    }

    instances = clonerComponentInstances(layer, baseTransform);
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

export std::vector<ArtifactCore::InstanceData> cloneRenderInstanceData(
    const ArtifactAbstractLayer* layer,
    const QMatrix4x4& baseTransform)
{
    std::vector<ArtifactCore::InstanceData> instanceData;
    const auto instances = cloneRenderInstances(layer, baseTransform);
    instanceData.reserve(instances.size());
    for (const auto& instance : instances) {
        instanceData.push_back(cloneRenderInstanceToInstanceData(instance));
    }
    return instanceData;
}

} // namespace Artifact

namespace Artifact {

ArtifactCore::InstanceData cloneRenderInstanceToInstanceData(
    const CloneRenderInstance& instance)
{
    ArtifactCore::InstanceData gpuInstance{};
    const float* matPtr = instance.transform.constData();
    for (int i = 0; i < 16; ++i) {
        gpuInstance.transform[i] = matPtr[i];
    }
    gpuInstance.color[0] = 1.0f;
    gpuInstance.color[1] = 1.0f;
    gpuInstance.color[2] = 1.0f;
    gpuInstance.color[3] = 1.0f;
    gpuInstance.weight = std::clamp(instance.weight, 0.0f, 1.0f);
    gpuInstance.timeOffset = 0.0f;
    gpuInstance.padding[0] = 0.0f;
    gpuInstance.padding[1] = 0.0f;
    return gpuInstance;
}

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

int cloneComponentMode(const ArtifactAbstractLayer* layer)
{
    return cloneComponentIntProperty(layer, QStringLiteral("component.cloner.mode"), 0);
}

std::vector<CloneRenderInstance> clonerComponentInstances(
      const ArtifactAbstractLayer* layer,
      const QMatrix4x4& baseTransform)
  {
    std::vector<CloneRenderInstance> instances;
    if (!cloneComponentBoolProperty(
            layer, QStringLiteral("component.cloner.enabled"))) {
        return instances;
    }

    instances.push_back(CloneRenderInstance{baseTransform, 1.0f});
    const auto appendCloneInstance = [&](const QMatrix4x4& cloneTransform,
                                         float weight) {
        if (cloneTransform.isIdentity()) {
            return;
        }
        instances.push_back(CloneRenderInstance{
            baseTransform * cloneTransform,
            std::clamp(weight, 0.0f, 1.0f)});
    };

    const int mode = cloneComponentMode(layer);
    if (mode == 3) {
        const int cols = std::max(1, cloneComponentIntProperty(layer, QStringLiteral("component.cloner.columns"), 3));
        const int rows = std::max(1, cloneComponentIntProperty(layer, QStringLiteral("component.cloner.rows"), 3));
        const int depth = std::max(1, cloneComponentIntProperty(layer, QStringLiteral("component.cloner.depth"), 1));
        const float spacingX = cloneComponentFloatProperty(layer, QStringLiteral("component.cloner.spacingX"), 160.0f);
        const float spacingY = cloneComponentFloatProperty(layer, QStringLiteral("component.cloner.spacingY"), 48.0f);
        const float spacingZ = cloneComponentFloatProperty(layer, QStringLiteral("component.cloner.spacingZ"), 0.0f);
        const int total = cols * rows * depth;
        instances.reserve(static_cast<size_t>(total) + 1U);
        const QVector3D startPos(
            -((cols - 1) * spacingX) * 0.5f,
            -((rows - 1) * spacingY) * 0.5f,
            -((depth - 1) * spacingZ) * 0.5f);
        for (int z = 0; z < depth; ++z) {
          for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
              QMatrix4x4 cloneTransform;
              cloneTransform.setToIdentity();
              cloneTransform.translate(startPos.x() + spacingX * x,
                                       startPos.y() + spacingY * y,
                                       startPos.z() + spacingZ * z);
              appendCloneInstance(cloneTransform, 1.0f);
            }
          }
        }
    } else if (mode == 4 || mode == 6) {
        const int count = std::max(1, cloneComponentIntProperty(layer, QStringLiteral("component.cloner.radialCount"), 8));
        const float radius = cloneComponentFloatProperty(layer, QStringLiteral("component.cloner.radius"), 160.0f);
        const float startAngle = cloneComponentFloatProperty(layer, QStringLiteral("component.cloner.startAngle"), 0.0f);
        const float endAngle = cloneComponentFloatProperty(layer, QStringLiteral("component.cloner.endAngle"), 360.0f);
        const float rotationStep = cloneComponentFloatProperty(layer, QStringLiteral("component.cloner.rotationStep"), 0.0f);
        const float opacityDecay = cloneComponentFloatProperty(layer, QStringLiteral("component.cloner.opacityDecay"), 0.0f);
        const float angleStep = count > 1 ? (endAngle - startAngle) / static_cast<float>(count - 1) : 0.0f;
        instances.reserve(static_cast<size_t>(count) + 1U);
        for (int i = 0; i < count; ++i) {
          const float angle = startAngle + angleStep * static_cast<float>(i);
          const float rad = angle * 3.1415926535f / 180.0f;
          QMatrix4x4 cloneTransform;
          cloneTransform.setToIdentity();
          cloneTransform.translate(std::cos(rad) * radius, std::sin(rad) * radius, 0.0f);
          cloneTransform.rotate(angle + rotationStep * static_cast<float>(i), 0.0f, 0.0f, 1.0f);
          appendCloneInstance(cloneTransform,
                              1.0f - opacityDecay * static_cast<float>(i));
        }
    } else if (mode == 5) {
        const int count = std::max(1, cloneComponentIntProperty(
               layer, QStringLiteral("component.cloner.cloneCount"), 3));
        const float offsetX = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.offsetX"), 160.0f);
        const float offsetY = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.offsetY"), 48.0f);
        const float offsetZ = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.offsetZ"), 0.0f);
        const float jitterX = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.jitterX"), 24.0f);
        const float jitterY = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.jitterY"), 24.0f);
        const float jitterZ = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.jitterZ"), 0.0f);
        const int seed = cloneComponentIntProperty(layer, QStringLiteral("component.cloner.seed"), 1);
        const float rotationStep = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.rotationStep"), 0.0f);
        const float opacityDecay = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.opacityDecay"), 0.0f);
        std::mt19937 rng(static_cast<uint32_t>(seed));
        std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
        instances.reserve(static_cast<size_t>(count) + 1U);
        for (int i = 0; i < count; ++i) {
            QMatrix4x4 cloneTransform;
            cloneTransform.setToIdentity();
            const float mix = std::pow(static_cast<float>(i + 1) / static_cast<float>(count + 1), 0.65f);
            cloneTransform.translate(
                offsetX * static_cast<float>(i) + unit(rng) * jitterX * mix,
                offsetY * static_cast<float>(i) + unit(rng) * jitterY * mix,
                offsetZ * static_cast<float>(i) + unit(rng) * jitterZ * mix);
            cloneTransform.rotate(rotationStep * static_cast<float>(i) + unit(rng) * 30.0f * mix,
                                  0.0f, 0.0f, 1.0f);
            appendCloneInstance(cloneTransform,
                                1.0f - opacityDecay * static_cast<float>(i));
        }
    } else {
        const int count = std::max(1, cloneComponentIntProperty(
               layer, QStringLiteral("component.cloner.cloneCount"), 3));
        const float offsetX = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.offsetX"), 160.0f);
        const float offsetY = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.offsetY"), 48.0f);
        const float offsetZ = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.offsetZ"), 0.0f);
        const float rotationStep = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.rotationStep"), 0.0f);
        const float opacityDecay = cloneComponentFloatProperty(
            layer, QStringLiteral("component.cloner.opacityDecay"), 0.0f);
        instances.reserve(static_cast<size_t>(count) + 1U);
        for (int i = 0; i < count; ++i) {
            QMatrix4x4 cloneTransform;
            cloneTransform.setToIdentity();
            cloneTransform.translate(offsetX * static_cast<float>(i),
                                     offsetY * static_cast<float>(i),
                                     offsetZ * static_cast<float>(i));
            if (rotationStep != 0.0f) {
              cloneTransform.rotate(rotationStep * static_cast<float>(i), 0.0f, 0.0f, 1.0f);
            }
            appendCloneInstance(cloneTransform,
                                1.0f - opacityDecay * static_cast<float>(i));
        }
    }
    return instances;
  }

} // namespace Artifact


