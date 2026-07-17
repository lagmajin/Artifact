module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <random>
#include <vector>
#include <Graphics/InstanceData.h>
#include <QMatrix4x4>
#include <QJsonArray>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVariant>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

export module Artifact.Layer.CloneEffectSupport;

import Artifact.Layer.Abstract;
import Artifact.Layer.Component.System;
import Artifact.Effect.Generator.Cloner;
import Property.Abstract;

export namespace Artifact {

class ArtifactAbstractLayer;

struct CloneRenderInstance {
    QMatrix4x4 transform;
    float weight = 1.0f;
    float timeOffset = 0.0f;
    std::array<QPointF, 4> canvasCorners{};
    QRectF canvasBounds;
};

struct FragmentRenderInstance {
    SimulationEntityId entityId;
    QString geometryHandle;
    QString materialHandle;
    std::vector<QVector2D> localPolygon;
    std::vector<QVector2D> localUV;
    QMatrix4x4 transform;
    float weight = 1.0f;
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
    const auto updateGeometry = [&localCorners](CloneRenderInstance& instance) {
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
    };
    for (std::size_t instanceIndex = 0; instanceIndex < instances.size();
         ++instanceIndex) {
        auto& instance = instances[instanceIndex];
        updateGeometry(instance);
        if (instanceIndex > 0) {
            const auto channels = layer->compositionFieldChannelsAtCanvasPoint(
                instance.canvasBounds.center());
            if (channels.affected) {
                instance.weight = std::clamp(
                    instance.weight * channels.weight, 0.0f, 1.0f);
                if (std::abs(channels.scaleMultiplier - 1.0f) > 0.0001f) {
                    const QPointF localCenter = localBounds.center();
                    instance.transform.translate(
                        static_cast<float>(localCenter.x()),
                        static_cast<float>(localCenter.y()));
                    instance.transform.scale(channels.scaleMultiplier);
                    instance.transform.translate(
                        static_cast<float>(-localCenter.x()),
                        static_cast<float>(-localCenter.y()));
                    updateGeometry(instance);
                }
                instance.timeOffset += channels.timeOffsetSeconds;
            }
        }
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
bool generatorSettingBool(const QJsonObject& settings,
                          const QString& key,
                          bool fallback);
int generatorSettingInt(const QJsonObject& settings,
                        const QString& key,
                        int fallback);
float generatorSettingFloat(const QJsonObject& settings,
                            const QString& key,
                            float fallback);

inline QRectF collisionLocalBounds(const ArtifactAbstractLayer* layer)
{
    if (!layer) {
        return QRectF();
    }

    const QRectF localBounds = layer->localBounds();
    if (!localBounds.isValid()) {
        return QRectF();
    }

    const int shape = cloneComponentIntProperty(
        layer, QStringLiteral("component.collision.shape"), 0);
    const float width = std::max(
        0.0f, cloneComponentFloatProperty(
                  layer, QStringLiteral("component.collision.width"), 0.0f));
    const float height = std::max(
        0.0f, cloneComponentFloatProperty(
                  layer, QStringLiteral("component.collision.height"), 0.0f));
    const float radius = std::max(
        0.0f, cloneComponentFloatProperty(
                  layer, QStringLiteral("component.collision.radius"), 0.0f));
    const float offsetX = cloneComponentFloatProperty(
        layer, QStringLiteral("component.collision.offsetX"), 0.0f);
    const float offsetY = cloneComponentFloatProperty(
        layer, QStringLiteral("component.collision.offsetY"), 0.0f);
    const QPointF center = localBounds.center() +
                           QPointF(static_cast<qreal>(offsetX),
                                   static_cast<qreal>(offsetY));

    if (shape == 1) {
        const qreal boxWidth = width > 0.0f ? static_cast<qreal>(width)
                                            : localBounds.width();
        const qreal boxHeight = height > 0.0f ? static_cast<qreal>(height)
                                              : localBounds.height();
        return QRectF(center.x() - boxWidth * 0.5, center.y() - boxHeight * 0.5,
                      boxWidth, boxHeight);
    }

    if (shape == 2) {
        const qreal circleRadius =
            radius > 0.0f
                ? static_cast<qreal>(radius)
                : static_cast<qreal>(
                      std::max(localBounds.width(), localBounds.height()) * 0.5);
        return QRectF(center.x() - circleRadius, center.y() - circleRadius,
                      circleRadius * 2.0, circleRadius * 2.0);
    }

    return localBounds.translated(static_cast<qreal>(offsetX),
                                  static_cast<qreal>(offsetY));
}

inline void applyLayoutComponent(
    const ArtifactAbstractLayer* layer,
    std::vector<CloneRenderInstance>& instances)
{
    // Layout is an authoring-stage arrangement pass: it seeds an initial
    // placement for instances, but it is not the authoritative dynamics
    // solver for collision or long-lived simulation state.
    if (!layer || instances.empty() ||
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
    // This preview path applies immediate steering-like offsets so editor
    // feedback stays responsive. The long-term contract is that crowd logic
    // produces motion intent, while authoritative transforms come from the
    // dynamics/simulation path.
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
    // This is a lightweight preview-only fallback that prevents obvious clone
    // overlap in the editor. Composition-wide collision ownership belongs to
    // the authoritative dynamics/simulation path.
    if (!layer || instances.empty() ||
        !cloneComponentBoolProperty(
            layer, QStringLiteral("component.collision.enabled"))) {
        return;
    }
    const QSizeF compositionSize = layer->compositionSizeHint();
    const QRectF localBounds = collisionLocalBounds(layer);
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

    const float configuredFloorY = cloneComponentFloatProperty(
        layer, QStringLiteral("component.collision.floorY"), 0.0f);
    const float floorY = configuredFloorY > 0.0f
                             ? configuredFloorY
                             : static_cast<float>(compositionSize.height());
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

inline void applyClonePhysicsTiming(
    const ArtifactAbstractLayer* layer,
    std::vector<CloneRenderInstance>& instances)
{
    if (!layer || instances.size() < 2 ||
        !cloneComponentBoolProperty(layer, QStringLiteral("physics.enabled"))) {
        return;
    }

    const float gravityY = cloneComponentFloatProperty(
        layer, QStringLiteral("physics.gravityY"), 980.0f);
    if (gravityY <= 0.0f) {
        return;
    }
    const double frameRate = std::max(1.0, layer->compositionFrameRate());
    const float timeSeconds = std::max(
        0.0f, static_cast<float>(static_cast<double>(layer->currentFrame()) /
                                  frameRate));
    const float restitution = std::clamp(cloneComponentFloatProperty(
        layer, QStringLiteral("physics.restitution"), 0.35f), 0.0f, 1.0f);
    const float initialVelocityY = cloneComponentFloatProperty(
        layer, QStringLiteral("physics.initialVelocityY"), 0.0f);
    const int maxBounces = std::clamp(cloneComponentIntProperty(
        layer, QStringLiteral("physics.maxBounces"), 4), 0, 32);
    const QSizeF compositionSize = layer->compositionSizeHint();
    const float configuredFloorY = cloneComponentFloatProperty(
        layer, QStringLiteral("component.collision.floorY"), 0.0f);
    const float maxFall = configuredFloorY > 0.0f
                              ? configuredFloorY
                              : (compositionSize.isValid()
                                     ? static_cast<float>(compositionSize.height()) +
                                           static_cast<float>(std::max<qreal>(
                                               0.0, layer->localBounds().height()))
                                     : 10000.0f);
    const auto fallDistance = [&](const float seconds) {
        float remaining = seconds;
        float position = 0.0f;
        float velocity = initialVelocityY;
        for (int bounce = 0; bounce <= maxBounces && remaining > 0.0f; ++bounce) {
            const float discriminant = velocity * velocity +
                2.0f * gravityY * std::max(0.0f, maxFall - position);
            const float timeToFloor =
                (-velocity + std::sqrt(std::max(0.0f, discriminant))) / gravityY;
            if (timeToFloor <= 0.0001f || remaining <= timeToFloor) {
                return std::clamp(position + velocity * remaining +
                                      0.5f * gravityY * remaining * remaining,
                                  0.0f, maxFall);
            }
            position = maxFall;
            remaining -= timeToFloor;
            velocity = -velocity - gravityY * timeToFloor;
            velocity *= restitution;
            if (std::abs(velocity) < 0.5f) {
                return maxFall;
            }
        }
        return maxFall;
    };
    const float currentFall = fallDistance(timeSeconds);
    for (auto& instance : instances) {
        const float delaySeconds = std::max(0.0f, instance.timeOffset);
        if (delaySeconds <= 0.0f) {
            continue;
        }
        const float localTime = std::max(0.0f, timeSeconds - delaySeconds);
        const float delayedFall = fallDistance(localTime);
        QMatrix4x4 timingDelta;
        timingDelta.translate(0.0f, delayedFall - currentFall, 0.0f);
        instance.transform = timingDelta * instance.transform;
    }
}

inline void applyCloneFields(
    const ArtifactAbstractLayer* layer,
    std::vector<CloneRenderInstance>& instances)
{
    if (!layer || instances.empty()) {
        return;
    }
    const auto fields = layer->layerFields();
    if (fields.empty()) {
        return;
    }

    const float timeSeconds =
        static_cast<float>(layer->currentFrame()) / 30.0f;
    const QVector3D fieldOrigin =
        instances.front().transform.column(3).toVector3D();
    for (std::size_t index = 0; index < instances.size(); ++index) {
        auto& instance = instances[index];
        QVector3D position = instance.transform.column(3).toVector3D();
        QVector3D accumulated(0.0f, 0.0f, 0.0f);
        for (const auto& field : fields) {
            if (!field.enabled || field.strength == 0.0f) {
                continue;
            }
            const float direction = field.invert ? -1.0f : 1.0f;
            const float force = field.strength * direction;
            const float centerX = static_cast<float>(
                field.settings.value(QStringLiteral("centerX")).toDouble(0.0));
            const float centerY = static_cast<float>(
                field.settings.value(QStringLiteral("centerY")).toDouble(0.0));
            QVector3D fromCenter(position.x() - fieldOrigin.x() - centerX,
                                 position.y() - fieldOrigin.y() - centerY,
                                 0.0f);

            if (field.typeId == QStringLiteral("artifact.field.radial") ||
                field.typeId == QStringLiteral("artifact.field.sphere")) {
                const float outerRadius = std::max(
                    1.0f, static_cast<float>(field.settings
                        .value(QStringLiteral("outerRadius"))
                        .toDouble(field.settings.value(QStringLiteral("radius"))
                                      .toDouble(160.0))));
                const float distance = fromCenter.length();
                if (distance <= outerRadius && distance > 0.0001f) {
                    fromCenter.normalize();
                    accumulated += fromCenter * force *
                                   (1.0f - distance / outerRadius) * 80.0f;
                }
            } else if (field.typeId == QStringLiteral("artifact.field.linear")) {
                const float angle = static_cast<float>(
                    field.settings.value(QStringLiteral("angle")).toDouble(0.0)) *
                    3.1415926535f / 180.0f;
                accumulated += QVector3D(std::cos(angle), std::sin(angle), 0.0f) *
                               force * 80.0f;
            } else if (field.typeId == QStringLiteral("artifact.field.box")) {
                const float halfX = std::max(1.0f, static_cast<float>(
                    field.settings.value(QStringLiteral("halfX")).toDouble(120.0)));
                const float halfY = std::max(1.0f, static_cast<float>(
                    field.settings.value(QStringLiteral("halfY")).toDouble(120.0)));
                if (std::abs(fromCenter.x()) <= halfX &&
                    std::abs(fromCenter.y()) <= halfY) {
                    accumulated += QVector3D(0.0f, force * 80.0f, 0.0f);
                }
            } else if (field.typeId == QStringLiteral("artifact.field.noise")) {
                const float phase = static_cast<float>(index) * 1.6180339f;
                accumulated += QVector3D(
                    std::sin(timeSeconds * 1.7f + phase),
                    std::cos(timeSeconds * 1.3f + phase * 0.5f), 0.0f) *
                    force * 40.0f;
            }
        }
        if (!accumulated.isNull()) {
            QMatrix4x4 fieldDelta;
            fieldDelta.translate(accumulated);
            instance.transform = fieldDelta * instance.transform;
        }
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

inline float clonerSequenceWeight(const ArtifactAbstractLayer* layer,
                                  const QJsonObject& settings,
                                  int cloneIndex)
{
    if (!layer || !generatorSettingBool(
                      settings, QStringLiteral("sequenceEnabled"), false)) {
        return 1.0f;
    }

    const float rate = std::max(
        0.01f, generatorSettingFloat(
                   settings, QStringLiteral("sequenceRate"), 8.0f));
    const float softness = std::max(
        0.01f, generatorSettingFloat(
                   settings, QStringLiteral("sequenceSoftness"), 1.0f));
    const float timeSeconds =
        static_cast<float>(layer->currentFrame()) / 30.0f;
    const float head = timeSeconds * rate;
    const float ramp = (head - static_cast<float>(cloneIndex)) / softness;
    return std::clamp(ramp, 0.0f, 1.0f);
}

inline float cloneModifierSequenceWeight(
    const ArtifactAbstractLayer* layer,
    const std::vector<LayerModifierDescriptor>& modifiers,
    const QJsonObject& fallbackSettings,
    int cloneIndex)
{
    for (const auto& modifier : modifiers) {
        if (!modifier.enabled ||
            modifier.typeId != QStringLiteral("artifact.modifier.sequence")) {
            continue;
        }
        const bool enabled =
            modifier.settings.value(QStringLiteral("enabled")).toBool(true);
        if (!enabled || !layer) {
            continue;
        }
        const float rate = std::max(
            0.01f, static_cast<float>(
                       modifier.settings.value(QStringLiteral("rate"))
                           .toDouble(8.0)));
        const float softness = std::max(
            0.01f, static_cast<float>(
                       modifier.settings.value(QStringLiteral("softness"))
                           .toDouble(1.0)));
        const float timeSeconds =
            static_cast<float>(layer->currentFrame()) / 30.0f;
        const float head = timeSeconds * rate;
        const float ramp =
            (head - static_cast<float>(cloneIndex)) / softness;
        return std::clamp(ramp, 0.0f, 1.0f);
    }
    return clonerSequenceWeight(layer, fallbackSettings, cloneIndex);
}

inline float cloneModifierTimeOffset(
    const std::vector<LayerModifierDescriptor>& modifiers,
    const QJsonObject& fallbackSettings,
    int cloneIndex)
{
    for (const auto& modifier : modifiers) {
        if (!modifier.enabled ||
            modifier.typeId != QStringLiteral("artifact.modifier.time-offset")) {
            continue;
        }
        const float step = static_cast<float>(
            modifier.settings.value(QStringLiteral("step")).toDouble(0.0));
        return step * static_cast<float>(cloneIndex);
    }
    const float timeOffsetStep = generatorSettingFloat(
        fallbackSettings, QStringLiteral("timeOffsetStep"), 0.0f);
    return timeOffsetStep * static_cast<float>(cloneIndex);
}

inline bool applyAuthoritativeComponentState(
    const ArtifactAbstractLayer* layer,
    std::vector<CloneRenderInstance>& instances)
{
    if (!layer) {
        return false;
    }
    const auto state = layer->authoritativeComponentEvaluationState();
    if (!state || state->instances.size() != instances.size()) {
        return false;
    }
    for (std::size_t index = 0; index < instances.size(); ++index) {
        instances[index].transform = state->instances[index].transform;
        instances[index].weight = std::clamp(
            state->instances[index].opacity, 0.0f, 1.0f);
    }
    return true;
}

export std::vector<CloneRenderInstance> fragmentCloneRenderInstances(
    const ArtifactAbstractLayer* layer, const QMatrix4x4& baseTransform)
{
    std::vector<CloneRenderInstance> instances;
    if (!layer) {
        return instances;
    }
    const auto& fragments = layer->layerEvaluationState().fragments;
    instances.reserve(fragments.size());
    for (const auto& fragment : fragments) {
        if (!fragment.active || fragment.opacity <= 0.0f) {
            continue;
        }
        CloneRenderInstance instance;
        instance.transform = baseTransform * fragment.transform;
        instance.weight = std::clamp(fragment.opacity, 0.0f, 1.0f);
        instance.timeOffset = fragment.age;
        instances.push_back(std::move(instance));
    }
    populateCloneInstanceGeometry(layer, instances);
    return instances;
}

export std::vector<FragmentRenderInstance> fragmentRenderInstances(
    const ArtifactAbstractLayer* layer, const QMatrix4x4& baseTransform)
{
    std::vector<FragmentRenderInstance> instances;
    if (!layer) {
        return instances;
    }
    const auto& evaluationState = layer->layerEvaluationState();
    instances.reserve(evaluationState.fragments.size());
    for (const auto& fragment : evaluationState.fragments) {
        if (!fragment.active || fragment.opacity <= 0.0f) {
            continue;
        }
        const auto geometry = std::find_if(
            evaluationState.fragmentGeometry.begin(),
            evaluationState.fragmentGeometry.end(),
            [&](const LayerFragmentGeometry& candidate) {
                return candidate.geometryHandle == fragment.geometryHandle;
            });
        if (geometry == evaluationState.fragmentGeometry.end() ||
            geometry->localPolygon.size() < 3U) {
            continue;
        }
        FragmentRenderInstance instance;
        instance.entityId = fragment.entityId;
        instance.geometryHandle = fragment.geometryHandle;
        instance.materialHandle = geometry->materialHandle;
        instance.localPolygon = geometry->localPolygon;
        instance.localUV = geometry->localUV;
        instance.transform = baseTransform * fragment.transform;
        instance.weight = std::clamp(fragment.opacity, 0.0f, 1.0f);
        instances.push_back(std::move(instance));
    }
    return instances;
}

inline std::vector<CloneRenderInstance> cloneRenderInstancesImpl(
    const ArtifactAbstractLayer* layer, const QMatrix4x4& baseTransform,
    const bool useAuthoritativeState, const bool applyPreviewDynamics)
{
    std::vector<CloneRenderInstance> instances;
    if (!layer) {
        return instances;
    }

    // ClonerGenerator used to be an Effect.  Keep this adapter only for
    // projects that already serialized one; all new authoring goes through
    // the layer component generator stack below.
    bool hasLegacyEffectCloner = false;
    std::vector<CloneRenderInstance> legacyEffectInstances;
    for (const auto& effect : layer->getEffects()) {
        const auto cloner = std::dynamic_pointer_cast<ClonerGenerator>(effect);
        if (!cloner || !cloner->isEnabled()) {
            continue;
        }
        hasLegacyEffectCloner = true;

        const auto clones = cloner->generateCloneData();
        legacyEffectInstances.reserve(legacyEffectInstances.size() + clones.size());
        for (const auto& clone : clones) {
            if (!clone.visible || clone.transform.isIdentity()) {
                continue;
            }

            CloneRenderInstance instance;
            instance.transform = baseTransform * clone.transform;
            instance.weight = std::clamp(clone.weight, 0.0f, 1.0f);
            legacyEffectInstances.push_back(instance);
        }
    }

    if (hasLegacyEffectCloner) {
        // Compatibility projects may also contain component generators. Keep
        // the historical append behavior until their Effect is migrated.
        const auto componentInstances =
            clonerComponentInstances(layer, baseTransform);
        if (componentInstances.size() > 1U) {
            legacyEffectInstances.insert(legacyEffectInstances.end(),
                                   componentInstances.begin() + 1,
                                   componentInstances.end());
        }
        instances.reserve(legacyEffectInstances.size() + 1U);
        instances.push_back(CloneRenderInstance{baseTransform, 1.0f});
        instances.insert(instances.end(), legacyEffectInstances.begin(), legacyEffectInstances.end());
        applyLayoutComponent(layer, instances);
        const bool authoritative = useAuthoritativeState &&
            applyAuthoritativeComponentState(layer, instances);
        if (!authoritative && applyPreviewDynamics) {
            applyCrowdComponent(layer, instances);
            applyClonePhysicsTiming(layer, instances);
            applyCloneFields(layer, instances);
            applyInstanceCollisionComponent(layer, instances);
        }
        populateCloneInstanceGeometry(layer, instances);
        return instances;
    }

    instances = clonerComponentInstances(layer, baseTransform);
    if (!instances.empty()) {
        applyLayoutComponent(layer, instances);
        const bool authoritative = useAuthoritativeState &&
            applyAuthoritativeComponentState(layer, instances);
        if (!authoritative && applyPreviewDynamics) {
            applyCrowdComponent(layer, instances);
            applyClonePhysicsTiming(layer, instances);
            applyCloneFields(layer, instances);
            applyInstanceCollisionComponent(layer, instances);
        }
        populateCloneInstanceGeometry(layer, instances);
        return instances;
    }

    instances.push_back(CloneRenderInstance{baseTransform, 1.0f});
    applyLayoutComponent(layer, instances);
    const bool authoritative = useAuthoritativeState &&
        applyAuthoritativeComponentState(layer, instances);
    if (!authoritative && applyPreviewDynamics) {
        applyCrowdComponent(layer, instances);
        applyClonePhysicsTiming(layer, instances);
        applyCloneFields(layer, instances);
        applyInstanceCollisionComponent(layer, instances);
    }
    populateCloneInstanceGeometry(layer, instances);
    return instances;
}

export std::vector<CloneRenderInstance> cloneRenderInstances(
    const ArtifactAbstractLayer* layer, const QMatrix4x4& baseTransform)
{
    return cloneRenderInstancesImpl(layer, baseTransform, true, true);
}

export std::vector<CloneRenderInstance> cloneRenderInstancesForSimulation(
    const ArtifactAbstractLayer* layer, const QMatrix4x4& baseTransform)
{
    return cloneRenderInstancesImpl(layer, baseTransform, false, false);
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
    gpuInstance.timeOffset = instance.timeOffset;
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

bool generatorSettingBool(const QJsonObject& settings,
                          const QString& key,
                          bool fallback)
{
    return settings.value(key).toBool(fallback);
}

int generatorSettingInt(const QJsonObject& settings,
                        const QString& key,
                        int fallback)
{
    return settings.value(key).toInt(fallback);
}

float generatorSettingFloat(const QJsonObject& settings,
                            const QString& key,
                            float fallback)
{
    return static_cast<float>(settings.value(key).toDouble(fallback));
}

int cloneComponentMode(const ArtifactAbstractLayer* layer)
{
    return cloneComponentIntProperty(layer, QStringLiteral("component.cloner.mode"), 0);
}

inline void applyGeneratorTransformStack(const QJsonObject& settings,
                                         QMatrix4x4& cloneTransform)
{
    const auto transformArray =
        settings.value(QStringLiteral("transformStack")).toArray();
    for (const auto& transformValue : transformArray) {
        if (!transformValue.isObject()) {
            continue;
        }
        const auto transformObject = transformValue.toObject();
        if (!transformObject.value(QStringLiteral("enabled")).toBool(true)) {
            continue;
        }
        cloneTransform.translate(
            static_cast<float>(
                transformObject.value(QStringLiteral("positionX")).toDouble()),
            static_cast<float>(
                transformObject.value(QStringLiteral("positionY")).toDouble()),
            static_cast<float>(
                transformObject.value(QStringLiteral("positionZ")).toDouble()));
        const float rotationZ = static_cast<float>(
            transformObject.value(QStringLiteral("rotationZ")).toDouble());
        if (rotationZ != 0.0f) {
            cloneTransform.rotate(rotationZ, 0.0f, 0.0f, 1.0f);
        }
        cloneTransform.scale(
            static_cast<float>(
                transformObject.value(QStringLiteral("scaleX")).toDouble(1.0)),
            static_cast<float>(
                transformObject.value(QStringLiteral("scaleY")).toDouble(1.0)),
            static_cast<float>(
                transformObject.value(QStringLiteral("scaleZ")).toDouble(1.0)));
    }
}

inline void applyCloneEffectorModifiers(
    const std::vector<LayerModifierDescriptor>& modifiers,
    const int cloneIndex,
    QMatrix4x4& cloneTransform)
{
    for (const auto& modifier : modifiers) {
        if (!modifier.enabled) {
            continue;
        }
        const auto& settings = modifier.settings;
        const float strength = std::clamp(static_cast<float>(
            settings.value(QStringLiteral("strength")).toDouble(1.0)), 0.0f, 1.0f);
        if (modifier.typeId == QStringLiteral("artifact.modifier.plain")) {
            const float positionX = static_cast<float>(
                settings.value(QStringLiteral("positionX")).toDouble(0.0)) * strength;
            const float positionY = static_cast<float>(
                settings.value(QStringLiteral("positionY")).toDouble(0.0)) * strength;
            const float positionZ = static_cast<float>(
                settings.value(QStringLiteral("positionZ")).toDouble(0.0)) * strength;
            cloneTransform.translate(positionX, positionY, positionZ);
            const float rotationZ = static_cast<float>(
                settings.value(QStringLiteral("rotationZ")).toDouble(0.0)) * strength;
            if (rotationZ != 0.0f) {
                cloneTransform.rotate(rotationZ, 0.0f, 0.0f, 1.0f);
            }
            const float scaleX = 1.0f + (static_cast<float>(
                settings.value(QStringLiteral("scaleX")).toDouble(1.0)) - 1.0f) * strength;
            const float scaleY = 1.0f + (static_cast<float>(
                settings.value(QStringLiteral("scaleY")).toDouble(1.0)) - 1.0f) * strength;
            const float scaleZ = 1.0f + (static_cast<float>(
                settings.value(QStringLiteral("scaleZ")).toDouble(1.0)) - 1.0f) * strength;
            cloneTransform.scale(scaleX, scaleY, scaleZ);
        } else if (modifier.typeId == QStringLiteral("artifact.modifier.random")) {
            const int seed = settings.value(QStringLiteral("seed")).toInt(1);
            std::mt19937 rng(static_cast<std::uint32_t>(seed) ^
                             (static_cast<std::uint32_t>(cloneIndex) * 0x9E3779B9u));
            std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
            const float positionX = static_cast<float>(
                settings.value(QStringLiteral("positionX")).toDouble(0.0));
            const float positionY = static_cast<float>(
                settings.value(QStringLiteral("positionY")).toDouble(0.0));
            const float positionZ = static_cast<float>(
                settings.value(QStringLiteral("positionZ")).toDouble(0.0));
            cloneTransform.translate(unit(rng) * positionX * strength,
                                     unit(rng) * positionY * strength,
                                     unit(rng) * positionZ * strength);
            const float rotationZ = static_cast<float>(
                settings.value(QStringLiteral("rotationZ")).toDouble(0.0));
            cloneTransform.rotate(unit(rng) * rotationZ * strength,
                                  0.0f, 0.0f, 1.0f);
            const float scaleVariance = std::max(0.0f, static_cast<float>(
                settings.value(QStringLiteral("scaleVariance")).toDouble(0.0)));
            const float scale = 1.0f + unit(rng) * scaleVariance * strength;
            cloneTransform.scale(std::max(0.001f, scale),
                                 std::max(0.001f, scale),
                                 std::max(0.001f, scale));
        }
    }
}

std::vector<CloneRenderInstance> clonerComponentInstances(
      const ArtifactAbstractLayer* layer,
      const QMatrix4x4& baseTransform)
  {
    std::vector<CloneRenderInstance> instances;
    if (!layer) {
        return instances;
    }

    const auto generators = layer->layerGenerators();
    const auto modifiers = layer->layerCloneModifiers();
    if (generators.empty()) {
        return instances;
    }

    instances.push_back(CloneRenderInstance{baseTransform, 1.0f, 0.0f});
    const auto appendCloneInstance = [&](const QJsonObject& settings,
                                         const QMatrix4x4& cloneTransform,
                                         float weight,
                                         int cloneIndex) {
        if (cloneTransform.isIdentity()) {
            return;
        }
        const float sequenceWeight =
            cloneModifierSequenceWeight(layer, modifiers, settings, cloneIndex);
        instances.push_back(CloneRenderInstance{
            baseTransform * cloneTransform,
            std::clamp(weight * sequenceWeight, 0.0f, 1.0f),
            cloneModifierTimeOffset(modifiers, settings, cloneIndex)});
    };

    for (const auto& generator : generators) {
        if (!generator.enabled) {
            continue;
        }

        const auto& settings = generator.settings;
        if (generator.typeId ==
            QStringLiteral("artifact.generator.cloner.grid")) {
            const int cols = std::max(
                1, generatorSettingInt(settings, QStringLiteral("columns"), 3));
            const int rows = std::max(
                1, generatorSettingInt(settings, QStringLiteral("rows"), 3));
            const int depth = std::max(
                1, generatorSettingInt(settings, QStringLiteral("depth"), 1));
            const float spacingX = generatorSettingFloat(
                settings, QStringLiteral("spacingX"), 160.0f);
            const float spacingY = generatorSettingFloat(
                settings, QStringLiteral("spacingY"), 48.0f);
            const float spacingZ = generatorSettingFloat(
                settings, QStringLiteral("spacingZ"), 0.0f);
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
                        applyGeneratorTransformStack(settings, cloneTransform);
                        const int cloneIndex = z * (rows * cols) + y * cols + x;
                        applyCloneEffectorModifiers(modifiers, cloneIndex, cloneTransform);
                        appendCloneInstance(
                            settings, cloneTransform, 1.0f, cloneIndex);
                    }
                }
            }
            continue;
        }

        if (generator.typeId ==
            QStringLiteral("artifact.generator.cloner.radial")) {
            const int count = std::max(
                1, generatorSettingInt(settings, QStringLiteral("radialCount"), 8));
            const float radius = generatorSettingFloat(
                settings, QStringLiteral("radius"), 160.0f);
            const float startAngle = generatorSettingFloat(
                settings, QStringLiteral("startAngle"), 0.0f);
            const float endAngle = generatorSettingFloat(
                settings, QStringLiteral("endAngle"), 360.0f);
            const float rotationStep = generatorSettingFloat(
                settings, QStringLiteral("rotationStep"), 0.0f);
            const float opacityDecay = generatorSettingFloat(
                settings, QStringLiteral("opacityDecay"), 0.0f);
            const float angleStep = count > 1
                                        ? (endAngle - startAngle) /
                                              static_cast<float>(count - 1)
                                        : 0.0f;
            for (int i = 0; i < count; ++i) {
                const float angle = startAngle + angleStep * static_cast<float>(i);
                const float rad = angle * 3.1415926535f / 180.0f;
                QMatrix4x4 cloneTransform;
                cloneTransform.setToIdentity();
                cloneTransform.translate(std::cos(rad) * radius,
                                         std::sin(rad) * radius, 0.0f);
                cloneTransform.rotate(angle + rotationStep * static_cast<float>(i),
                                      0.0f, 0.0f, 1.0f);
                applyGeneratorTransformStack(settings, cloneTransform);
                applyCloneEffectorModifiers(modifiers, i, cloneTransform);
                appendCloneInstance(
                    settings, cloneTransform,
                    1.0f - opacityDecay * static_cast<float>(i), i);
            }
            continue;
        }

        const int legacyMode =
            generatorSettingInt(settings, QStringLiteral("legacyMode"), 0);
        const int count = std::max(
            1, generatorSettingInt(settings, QStringLiteral("count"), 3));
        const float offsetX = generatorSettingFloat(
            settings, QStringLiteral("offsetX"), 160.0f);
        const float offsetY = generatorSettingFloat(
            settings, QStringLiteral("offsetY"), 48.0f);
        const float offsetZ = generatorSettingFloat(
            settings, QStringLiteral("offsetZ"), 0.0f);
        const float rotationStep = generatorSettingFloat(
            settings, QStringLiteral("rotationStep"), 0.0f);
        const float opacityDecay = generatorSettingFloat(
            settings, QStringLiteral("opacityDecay"), 0.0f);

        if (legacyMode == 3) {
            const float jitterX = generatorSettingFloat(
                settings, QStringLiteral("jitterX"), 24.0f);
            const float jitterY = generatorSettingFloat(
                settings, QStringLiteral("jitterY"), 24.0f);
            const float jitterZ = generatorSettingFloat(
                settings, QStringLiteral("jitterZ"), 0.0f);
            const int seed =
                generatorSettingInt(settings, QStringLiteral("seed"), 1);
            std::mt19937 rng(static_cast<uint32_t>(seed));
            std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
            for (int i = 0; i < count; ++i) {
                QMatrix4x4 cloneTransform;
                cloneTransform.setToIdentity();
                const float mix = std::pow(
                    static_cast<float>(i + 1) / static_cast<float>(count + 1),
                    0.65f);
                cloneTransform.translate(
                    offsetX * static_cast<float>(i) + unit(rng) * jitterX * mix,
                    offsetY * static_cast<float>(i) + unit(rng) * jitterY * mix,
                    offsetZ * static_cast<float>(i) + unit(rng) * jitterZ * mix);
                cloneTransform.rotate(
                    rotationStep * static_cast<float>(i) +
                        unit(rng) * 30.0f * mix,
                    0.0f, 0.0f, 1.0f);
                applyGeneratorTransformStack(settings, cloneTransform);
                applyCloneEffectorModifiers(modifiers, i, cloneTransform);
                appendCloneInstance(
                    settings, cloneTransform,
                    1.0f - opacityDecay * static_cast<float>(i), i);
            }
            continue;
        }

        for (int i = 1; i <= count; ++i) {
            const int cloneIndex = i - 1;
            QMatrix4x4 cloneTransform;
            cloneTransform.setToIdentity();
            cloneTransform.translate(offsetX * static_cast<float>(i),
                                     offsetY * static_cast<float>(i),
                                     offsetZ * static_cast<float>(i));
            if (rotationStep != 0.0f) {
                cloneTransform.rotate(rotationStep * static_cast<float>(i), 0.0f,
                                      0.0f, 1.0f);
            }
            applyGeneratorTransformStack(settings, cloneTransform);
            applyCloneEffectorModifiers(modifiers, cloneIndex, cloneTransform);
            appendCloneInstance(
                settings, cloneTransform,
                1.0f - opacityDecay * static_cast<float>(cloneIndex),
                cloneIndex);
        }
    }
    return instances;
  }

} // namespace Artifact


