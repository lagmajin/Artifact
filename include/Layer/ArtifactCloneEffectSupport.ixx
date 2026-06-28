module;
#include <array>
#include <utility>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <random>
#include <vector>
#include <Graphics/InstanceData.h>
#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVariant>
#include <QVector3D>
#include <QVector4D>

export module Artifact.Layer.CloneEffectSupport;

import Artifact.Layer.Abstract;
import Artifact.Effect.Generator.Cloner;
import Property.Abstract;

export namespace Artifact {

class ArtifactAbstractLayer;

struct CloneRenderInstance {
    QMatrix4x4 transform;
    float weight = 1.0f;
    std::array<QPointF, 4> canvasCorners{};
    QRectF canvasBounds;
};

inline void populateCloneInstanceGeometry(
    const ArtifactAbstractLayer* layer,
    std::vector<CloneRenderInstance>& instances)
{
    if (!layer) {
        return;
    }
    const QRectF localBounds = layer->localBounds();
    if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
        localBounds.height() <= 0.0) {
        return;
    }
    const std::array<QPointF, 4> localCorners{
        localBounds.topLeft(), localBounds.topRight(),
        localBounds.bottomRight(), localBounds.bottomLeft()};
    for (auto& instance : instances) {
        for (std::size_t i = 0; i < localCorners.size(); ++i) {
            const QPointF& point = localCorners[i];
            const QVector3D mapped = instance.transform.map(
                QVector3D(static_cast<float>(point.x()),
                          static_cast<float>(point.y()), 0.0f));
            instance.canvasCorners[i] =
                QPointF(static_cast<qreal>(mapped.x()),
                        static_cast<qreal>(mapped.y()));
        }
        qreal minX = instance.canvasCorners[0].x();
        qreal maxX = minX;
        qreal minY = instance.canvasCorners[0].y();
        qreal maxY = minY;
        for (std::size_t i = 1; i < instance.canvasCorners.size(); ++i) {
            minX = std::min(minX, instance.canvasCorners[i].x());
            maxX = std::max(maxX, instance.canvasCorners[i].x());
            minY = std::min(minY, instance.canvasCorners[i].y());
            maxY = std::max(maxY, instance.canvasCorners[i].y());
        }
        instance.canvasBounds = QRectF(minX, minY, maxX - minX, maxY - minY);
    }
}

bool cloneComponentBoolProperty(const ArtifactAbstractLayer* layer,
                                const QString& propertyPath);
int cloneComponentIntProperty(const ArtifactAbstractLayer* layer,
                                const QString& propertyPath,
                                int fallback);
float cloneComponentFloatProperty(const ArtifactAbstractLayer* layer,
                                    const QString& propertyPath,
                                    float fallback);

inline void applyLayoutComponent(
    const ArtifactAbstractLayer* layer,
    std::vector<CloneRenderInstance>& instances)
{
    if (!layer || instances.size() < 2 ||
        !cloneComponentBoolProperty(
            layer, QStringLiteral("component.layout.enabled"))) {
        return;
    }

    const QRectF bounds = layer->layoutBounds();
    const float itemWidth = static_cast<float>(
        std::max<qreal>(1.0, bounds.width()));
    const float itemHeight = static_cast<float>(
        std::max<qreal>(1.0, bounds.height()));
    const float gap = cloneComponentFloatProperty(
        layer, QStringLiteral("component.layout.gap"), 24.0f);
    const int stackDirection = cloneComponentIntProperty(
        layer, QStringLiteral("component.layout.stackDirection"), 0);
    const int configuredMaxPerRow = cloneComponentIntProperty(
        layer, QStringLiteral("component.layout.maxPerRow"), 0);
    const int maxPerRow = configuredMaxPerRow > 0
                              ? configuredMaxPerRow
                              : static_cast<int>(instances.size());
    const QVector3D origin =
        instances.front().transform.column(3).toVector3D();

    for (std::size_t index = 0; index < instances.size(); ++index) {
        const int logicalIndex = static_cast<int>(index);
        int column = logicalIndex % std::max(1, maxPerRow);
        int row = logicalIndex / std::max(1, maxPerRow);
        if (stackDirection == 1) {
            std::swap(column, row);
        }
        const QVector3D target(
            origin.x() + static_cast<float>(column) * (itemWidth + gap),
            origin.y() + static_cast<float>(row) * (itemHeight + gap),
            origin.z());
        instances[index].transform.setColumn(
            3, QVector4D(target, 1.0f));
    }
}

inline void applyCrowdComponent(
    const ArtifactAbstractLayer* layer,
    std::vector<CloneRenderInstance>& instances)
{
    if (!layer || instances.size() < 2 ||
        !cloneComponentBoolProperty(
            layer, QStringLiteral("component.crowd.enabled"))) {
        return;
    }

    QVector3D centroid(0.0f, 0.0f, 0.0f);
    for (const auto& instance : instances) {
        centroid += instance.transform.column(3).toVector3D();
    }
    centroid /= static_cast<float>(instances.size());

    const float cohesion = cloneComponentFloatProperty(
        layer, QStringLiteral("component.crowd.cohesion"), 0.5f);
    const float separation = cloneComponentFloatProperty(
        layer, QStringLiteral("component.crowd.separation"), 0.5f);
    const float alignment = cloneComponentFloatProperty(
        layer, QStringLiteral("component.crowd.alignment"), 0.5f);
    const float maxSpeed = std::max(
        0.0f, cloneComponentFloatProperty(
                  layer, QStringLiteral("component.crowd.maxSpeed"), 120.0f));
    const float jitter = cloneComponentFloatProperty(
        layer, QStringLiteral("component.crowd.jitter"), 0.1f);
    const float timeSeconds =
        static_cast<float>(layer->currentFrame()) / 30.0f;
    const float maxFrameStep = maxSpeed / 30.0f;

    for (std::size_t index = 0; index < instances.size(); ++index) {
        auto& instance = instances[index];
        const QVector3D position =
            instance.transform.column(3).toVector3D();
        QVector3D towardCenter = centroid - position;
        QVector3D awayFromCenter = position - centroid;
        if (!towardCenter.isNull())
            towardCenter.normalize();
        if (!awayFromCenter.isNull())
            awayFromCenter.normalize();

        const float phase =
            static_cast<float>(index) * 1.61803398875f;
        QVector3D sharedHeading(
            std::cos(timeSeconds * 0.7f),
            std::sin(timeSeconds * 0.7f), 0.0f);
        QVector3D jitterHeading(
            std::sin(timeSeconds * 2.1f + phase),
            std::cos(timeSeconds * 1.7f + phase * 0.5f), 0.0f);

        QVector3D desired =
            towardCenter * cohesion +
            awayFromCenter * separation +
            sharedHeading * alignment +
            jitterHeading * jitter;
        if (!desired.isNull()) {
            desired.normalize();
            desired *= maxFrameStep;
        }

        QMatrix4x4 crowdDelta;
        crowdDelta.translate(desired);
        instance.transform = crowdDelta * instance.transform;
        if (desired.lengthSquared() > 0.0001f) {
            const float angle = std::atan2(desired.y(), desired.x()) *
                                180.0f / 3.1415926535f;
            instance.transform.rotate(angle, 0.0f, 0.0f, 1.0f);
        }
    }
}

inline void applyInstanceCollisionComponent(
    const ArtifactAbstractLayer* layer,
    std::vector<CloneRenderInstance>& instances)
{
    if (!layer || instances.empty() ||
        !cloneComponentBoolProperty(
            layer, QStringLiteral("component.collision.enabled"))) {
        return;
    }
    const QSizeF compositionSize = layer->compositionSizeHint();
    const QRectF localBounds = layer->localBounds();
    if (!compositionSize.isValid() || compositionSize.height() <= 0.0 ||
        !localBounds.isValid()) {
        return;
    }

    const auto boundsForInstance = [&localBounds](const CloneRenderInstance& instance) {
        const QVector3D topLeft = instance.transform.map(
            QVector3D(static_cast<float>(localBounds.left()),
                      static_cast<float>(localBounds.top()), 0.0f));
        const QVector3D topRight = instance.transform.map(
            QVector3D(static_cast<float>(localBounds.right()),
                      static_cast<float>(localBounds.top()), 0.0f));
        const QVector3D bottomLeft = instance.transform.map(
            QVector3D(static_cast<float>(localBounds.left()),
                      static_cast<float>(localBounds.bottom()), 0.0f));
        const QVector3D bottomRight = instance.transform.map(
            QVector3D(static_cast<float>(localBounds.right()),
                      static_cast<float>(localBounds.bottom()), 0.0f));
        const qreal minX = std::min(
            std::min(static_cast<qreal>(topLeft.x()), static_cast<qreal>(topRight.x())),
            std::min(static_cast<qreal>(bottomLeft.x()), static_cast<qreal>(bottomRight.x())));
        const qreal maxX = std::max(
            std::max(static_cast<qreal>(topLeft.x()), static_cast<qreal>(topRight.x())),
            std::max(static_cast<qreal>(bottomLeft.x()), static_cast<qreal>(bottomRight.x())));
        const qreal minY = std::min(
            std::min(static_cast<qreal>(topLeft.y()), static_cast<qreal>(topRight.y())),
            std::min(static_cast<qreal>(bottomLeft.y()), static_cast<qreal>(bottomRight.y())));
        const qreal maxY = std::max(
            std::max(static_cast<qreal>(topLeft.y()), static_cast<qreal>(topRight.y())),
            std::max(static_cast<qreal>(bottomLeft.y()), static_cast<qreal>(bottomRight.y())));
        return QRectF(minX, minY, maxX - minX, maxY - minY);
    };

    const float floorY = static_cast<float>(compositionSize.height());
    for (auto& instance : instances) {
        const QRectF bounds = boundsForInstance(instance);
        const float bottomY = static_cast<float>(bounds.bottom());
        if (bottomY <= floorY) {
            continue;
        }
        QMatrix4x4 correction;
        correction.translate(0.0f, floorY - bottomY, 0.0f);
        instance.transform = correction * instance.transform;
    }

    for (int pass = 0; pass < 2; ++pass) {
        bool resolvedAny = false;
        for (std::size_t i = 0; i < instances.size(); ++i) {
            const QRectF a = boundsForInstance(instances[i]);
            if (!a.isValid()) {
                continue;
            }
            for (std::size_t j = i + 1; j < instances.size(); ++j) {
                const QRectF b = boundsForInstance(instances[j]);
                if (!b.isValid() || !a.intersects(b)) {
                    continue;
                }

                const qreal overlapLeft = a.right() - b.left();
                const qreal overlapRight = b.right() - a.left();
                const qreal overlapX = std::min(overlapLeft, overlapRight);
                const qreal overlapTop = a.bottom() - b.top();
                const qreal overlapBottom = b.bottom() - a.top();
                const qreal overlapY = std::min(overlapTop, overlapBottom);

                QMatrix4x4 correction;
                if (overlapX <= overlapY) {
                    const qreal direction = a.center().x() <= b.center().x() ? 1.0 : -1.0;
                    correction.translate(
                        static_cast<float>(direction * (overlapX + 0.5)), 0.0f, 0.0f);
                } else {
                    const qreal direction = a.center().y() <= b.center().y() ? 1.0 : -1.0;
                    correction.translate(
                        0.0f, static_cast<float>(direction * (overlapY + 0.5)), 0.0f);
                }
                instances[j].transform = correction * instances[j].transform;
                resolvedAny = true;
            }
        }
        if (!resolvedAny) {
            break;
        }
    }

    for (auto& instance : instances) {
        const QRectF bounds = boundsForInstance(instance);
        const float bottomY = static_cast<float>(bounds.bottom());
        if (bottomY <= floorY) {
            continue;
        }
        QMatrix4x4 correction;
        correction.translate(0.0f, floorY - bottomY, 0.0f);
        instance.transform = correction * instance.transform;
    }
}

inline void applyClonerComponentTransform(const ArtifactAbstractLayer* layer,
                                          QMatrix4x4& cloneTransform)
{
    if (!layer) {
        return;
    }
    for (int index = 0;; ++index) {
        const QString prefix =
            QStringLiteral("component.cloner.transforms.%1.").arg(index);
        const auto enabledProperty = layer->getProperty(prefix + QStringLiteral("enabled"));
        const auto nameProperty = layer->getProperty(prefix + QStringLiteral("name"));
        const auto positionXProperty = layer->getProperty(prefix + QStringLiteral("positionX"));
        if (!enabledProperty && !nameProperty && !positionXProperty) {
            break;
        }
        const bool enabled = enabledProperty ? enabledProperty->getValue().toBool() : true;
        if (!enabled) {
            continue;
        }
        const float positionX = cloneComponentFloatProperty(
            layer, prefix + QStringLiteral("positionX"), 0.0f);
        const float positionY = cloneComponentFloatProperty(
            layer, prefix + QStringLiteral("positionY"), 0.0f);
        const float positionZ = cloneComponentFloatProperty(
            layer, prefix + QStringLiteral("positionZ"), 0.0f);
        const float rotationX = cloneComponentFloatProperty(
            layer, prefix + QStringLiteral("rotationX"), 0.0f);
        const float rotationY = cloneComponentFloatProperty(
            layer, prefix + QStringLiteral("rotationY"), 0.0f);
        const float rotationZ = cloneComponentFloatProperty(
            layer, prefix + QStringLiteral("rotationZ"), 0.0f);
        const float scaleX = cloneComponentFloatProperty(
            layer, prefix + QStringLiteral("scaleX"), 1.0f);
        const float scaleY = cloneComponentFloatProperty(
            layer, prefix + QStringLiteral("scaleY"), 1.0f);
        const float scaleZ = cloneComponentFloatProperty(
            layer, prefix + QStringLiteral("scaleZ"), 1.0f);

        if (positionX != 0.0f || positionY != 0.0f || positionZ != 0.0f) {
            cloneTransform.translate(positionX, positionY, positionZ);
        }
        if (rotationX != 0.0f) {
            cloneTransform.rotate(rotationX, 1.0f, 0.0f, 0.0f);
        }
        if (rotationY != 0.0f) {
            cloneTransform.rotate(rotationY, 0.0f, 1.0f, 0.0f);
        }
        if (rotationZ != 0.0f) {
            cloneTransform.rotate(rotationZ, 0.0f, 0.0f, 1.0f);
        }
        if (scaleX != 1.0f || scaleY != 1.0f || scaleZ != 1.0f) {
            cloneTransform.scale(scaleX, scaleY, scaleZ);
        }
    }
}
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
    std::vector<CloneRenderInstance> clonerInstances;
    for (const auto& effect : layer->getEffects()) {
        const auto cloner = std::dynamic_pointer_cast<ClonerGenerator>(effect);
        if (!cloner || !cloner->isEnabled()) {
            continue;
        }
        hasEnabledCloner = true;

        const auto clones = cloner->generateCloneData();
        clonerInstances.reserve(clonerInstances.size() + clones.size());
        for (const auto& clone : clones) {
            if (!clone.visible || clone.transform.isIdentity()) {
                continue;
            }

            CloneRenderInstance instance;
            instance.transform = baseTransform * clone.transform;
            instance.weight = std::clamp(clone.weight, 0.0f, 1.0f);
            clonerInstances.push_back(instance);
        }
    }

    if (hasEnabledCloner) {
        instances.reserve(clonerInstances.size() + 1U);
        instances.push_back(CloneRenderInstance{baseTransform, 1.0f});
        instances.insert(instances.end(), clonerInstances.begin(), clonerInstances.end());
        applyLayoutComponent(layer, instances);
        applyCrowdComponent(layer, instances);
        applyInstanceCollisionComponent(layer, instances);
        populateCloneInstanceGeometry(layer, instances);
        return instances;
    }

    instances = clonerComponentInstances(layer, baseTransform);
    if (!instances.empty()) {
        applyLayoutComponent(layer, instances);
        applyCrowdComponent(layer, instances);
        applyInstanceCollisionComponent(layer, instances);
        populateCloneInstanceGeometry(layer, instances);
        return instances;
    }

    instances.push_back(CloneRenderInstance{baseTransform, 1.0f});
    applyLayoutComponent(layer, instances);
    applyCrowdComponent(layer, instances);
    applyInstanceCollisionComponent(layer, instances);
    populateCloneInstanceGeometry(layer, instances);
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
    if (mode == 5) {
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
              applyClonerComponentTransform(layer, cloneTransform);
              appendCloneInstance(cloneTransform, 1.0f);
            }
          }
        }
    } else if (mode == 6) {
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
          applyClonerComponentTransform(layer, cloneTransform);
          appendCloneInstance(cloneTransform,
                              1.0f - opacityDecay * static_cast<float>(i));
        }
    } else if (mode == 3) {
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
            applyClonerComponentTransform(layer, cloneTransform);
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
        for (int i = 1; i <= count; ++i) {
            const int cloneIndex = i - 1;
            QMatrix4x4 cloneTransform;
            cloneTransform.setToIdentity();
            cloneTransform.translate(offsetX * static_cast<float>(i),
                                     offsetY * static_cast<float>(i),
                                     offsetZ * static_cast<float>(i));
            if (rotationStep != 0.0f) {
              cloneTransform.rotate(rotationStep * static_cast<float>(i), 0.0f, 0.0f, 1.0f);
            }
            applyClonerComponentTransform(layer, cloneTransform);
            appendCloneInstance(cloneTransform,
                                1.0f - opacityDecay * static_cast<float>(cloneIndex));
        }
    }
    return instances;
  }

} // namespace Artifact


