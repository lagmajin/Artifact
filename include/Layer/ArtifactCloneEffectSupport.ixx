module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <vector>
#include <Graphics/InstanceData.h>
#include <QMatrix4x4>
#include <QJsonArray>
#include <QJsonObject>
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
import Container.NamedVector;
import Property.Abstract;
import Memory.SharedPtr;

export namespace Artifact {

class ArtifactAbstractLayer;

struct CloneRenderInstance {
    QMatrix4x4 transform;
    float weight = 1.0f;
    float timeOffset = 0.0f;
    std::array<QPointF, 4> canvasCorners{};
    QRectF canvasBounds;
    int cloneIndex = -1;
};

using CloneGridPlacementObserver = void (*)(
    void*, const QString&, const QJsonObject&, float, float, float, float,
    float, float);

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

    if (shape == 3) {
        // Clone instances use the outline's bounding box as a conservative
        // proxy; vertex-exact collision stays with the solvers.
        const auto outline = layer->collisionOutlineLocalPoints();
        if (outline.size() >= 3) {
            const QPointF& firstPoint = *outline.first();
            qreal minX = firstPoint.x();
            qreal minY = firstPoint.y();
            qreal maxX = minX;
            qreal maxY = minY;
            for (const QPointF& point : outline) {
                minX = std::min(minX, point.x());
                minY = std::min(minY, point.y());
                maxX = std::max(maxX, point.x());
                maxY = std::max(maxY, point.y());
            }
            return QRectF(QPointF(minX, minY), QPointF(maxX, maxY))
                .translated(static_cast<qreal>(offsetX),
                            static_cast<qreal>(offsetY));
        }
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
        static_cast<float>(static_cast<double>(layer->currentFrame()) /
                           std::max(1.0, layer->compositionFrameRate()));
    const float maxFrameStep =
        maxSpeed / static_cast<float>(
                       std::max(1.0, layer->compositionFrameRate()));

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
    const float linearDamping = std::max(0.0f, cloneComponentFloatProperty(
        layer, QStringLiteral("physics.linearDamping"), 0.0f));
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
            velocity *= restitution * std::exp(-linearDamping * timeToFloor);
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
        static_cast<float>(static_cast<double>(layer->currentFrame()) /
                           std::max(1.0, layer->compositionFrameRate()));
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
      const QMatrix4x4& baseTransform,
      CloneGridPlacementObserver placementObserver = nullptr,
      void* placementObserverContext = nullptr);
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
        static_cast<float>(static_cast<double>(layer->currentFrame()) /
                           std::max(1.0, layer->compositionFrameRate()));
    const float head = timeSeconds * rate;
    const float ramp = (head - static_cast<float>(cloneIndex)) / softness;
    return std::clamp(ramp, 0.0f, 1.0f);
}

inline float cloneModifierSequenceWeight(
    const ArtifactAbstractLayer* layer,
    const ArtifactCore::NamedVector<LayerModifierDescriptor>& modifiers,
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
            static_cast<float>(static_cast<double>(layer->currentFrame()) /
                               std::max(1.0, layer->compositionFrameRate()));
        const float head = timeSeconds * rate;
        const float ramp =
            (head - static_cast<float>(cloneIndex)) / softness;
        return std::clamp(ramp, 0.0f, 1.0f);
    }
    return clonerSequenceWeight(layer, fallbackSettings, cloneIndex);
}

inline float cloneModifierTimeOffset(
    const ArtifactCore::NamedVector<LayerModifierDescriptor>& modifiers,
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
        // CloneRenderInstance remains a render-boundary DTO. Keep the
        // conversion here instead of leaking std::vector into layer state.
        instance.localPolygon = geometry->localPolygon.toStdVector();
        instance.localUV = geometry->localUV.toStdVector();
        instance.transform = baseTransform * fragment.transform;
        instance.weight = std::clamp(fragment.opacity, 0.0f, 1.0f);
        instances.push_back(std::move(instance));
    }
    return instances;
}

inline std::vector<CloneRenderInstance> cloneRenderInstancesImpl(
    const ArtifactAbstractLayer* layer, const QMatrix4x4& baseTransform,
    const bool useAuthoritativeState, const bool applyPreviewDynamics,
    CloneGridPlacementObserver placementObserver = nullptr,
    void* placementObserverContext = nullptr)
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
        const auto cloner = ArtifactCore::dynamicPointerCast<ClonerGenerator>(effect);
        if (!cloner || !cloner->isEnabled()) {
            continue;
        }
        hasLegacyEffectCloner = true;

        const auto clones = cloner->generateCloneData();
        legacyEffectInstances.reserve(legacyEffectInstances.size() + clones.size());
        for (const auto& clone : clones) {
            // An identity transform is a legitimate placement (e.g. the first
            // clone of an unoffset linear array); only drop invisible clones.
            if (!clone.visible) {
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

    instances = clonerComponentInstances(
        layer, baseTransform, placementObserver, placementObserverContext);
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
    const ArtifactAbstractLayer* layer, const QMatrix4x4& baseTransform,
    CloneGridPlacementObserver placementObserver = nullptr,
    void* placementObserverContext = nullptr)
{
    return cloneRenderInstancesImpl(layer, baseTransform, true, true,
                                    placementObserver,
                                    placementObserverContext);
}

export std::vector<CloneRenderInstance> cloneRenderInstancesForSimulation(
    const ArtifactAbstractLayer* layer, const QMatrix4x4& baseTransform)
{
    return cloneRenderInstancesImpl(layer, baseTransform, false, false);
}

export void drawWithClonerEffect(const ArtifactAbstractLayer* layer,
                                 const QMatrix4x4& baseTransform,
                                 const std::function<void(const QMatrix4x4&, float)>& drawFn,
                                 CloneGridPlacementObserver placementObserver = nullptr,
                                 void* placementObserverContext = nullptr)
{
    const auto instances = cloneRenderInstances(
        layer, baseTransform, placementObserver, placementObserverContext);
    for (const auto& instance : instances) {
        drawFn(instance.transform, instance.weight);
    }
}

export void drawWithClonerEffectIndexed(
    const ArtifactAbstractLayer* layer, const QMatrix4x4& baseTransform,
    const std::function<void(const QMatrix4x4&, float, int)>& drawFn,
    CloneGridPlacementObserver placementObserver = nullptr,
    void* placementObserverContext = nullptr)
{
    const auto instances = cloneRenderInstances(
        layer, baseTransform, placementObserver, placementObserverContext);
    for (const auto& instance : instances) {
        drawFn(instance.transform, instance.weight, instance.cloneIndex);
    }
}

// Stream plain Grid/Matrix, Linear, and Radial placements for the GPU placement
// renderer. Opacity decay is carried as a per-draw weight; finite Plain
// translation, Z rotation, and XY scale are folded into placement while their
// composition remains representable as rotation plus diagonal scale.
inline bool gpuCompatibleTransformStack(
    const QJsonObject& settings, float& offsetX, float& offsetY, float& offsetZ,
    float& rotationDegrees, float& scaleX, float& scaleY);
inline void applyGeneratorTransformStack(const QJsonObject& settings,
                                         QMatrix4x4& cloneTransform);
// Accumulate an enabled Plain modifier chain onto the transform already in the
// output references. Callers seed identity for a Plain-only transform or the
// compatible generator stack when validating a combined chain.
inline bool gpuCompatiblePlainModifierTransform(
    const ArtifactCore::NamedVector<LayerModifierDescriptor>& modifiers,
    float& translationX, float& translationY, float& translationZ,
    float& rotationDegrees, float& scaleX, float& scaleY);

export bool drawWithGpuEligibleClonerGridIndexed(
    const ArtifactAbstractLayer* layer, const QMatrix4x4& baseTransform,
    const std::function<void(const QMatrix4x4&, float, int)>& drawFn,
    CloneGridPlacementObserver placementObserver,
    void* placementObserverContext)
{
    if (!layer || !drawFn || !placementObserver) {
        return false;
    }

    for (const auto& effect : layer->getEffects()) {
        const auto cloner =
            ArtifactCore::dynamicPointerCast<ClonerGenerator>(effect);
        if (cloner && cloner->isEnabled()) {
            return false;
        }
    }

    const auto generators = layer->layerGenerators();
    if (generators.size() != 1U) {
        return false;
    }
    const auto& generator = generators[0];
    const bool isGrid =
        generator.typeId == QStringLiteral("artifact.generator.cloner.grid") ||
        generator.typeId == QStringLiteral("artifact.generator.cloner.matrix");
    const bool isLinear = generator.typeId ==
        QStringLiteral("artifact.generator.cloner.linear");
    const bool isRadial = generator.typeId ==
        QStringLiteral("artifact.generator.cloner.radial");
    float stackOffsetX = 0.0f;
    float stackOffsetY = 0.0f;
    float stackOffsetZ = 0.0f;
    float stackRotationDegrees = 0.0f;
    float stackScaleX = 1.0f;
    float stackScaleY = 1.0f;
    const bool hasCompatibleTransformStack =
        gpuCompatibleTransformStack(
            generator.settings, stackOffsetX, stackOffsetY, stackOffsetZ,
            stackRotationDegrees, stackScaleX, stackScaleY);
    if (!generator.enabled || (!isGrid && !isLinear && !isRadial) ||
        !hasCompatibleTransformStack ||
        generator.settings.value(QStringLiteral("sequenceEnabled"))
            .toBool(false)) {
        return false;
    }
    if (isGrid) {
        const float opacityDecay = generatorSettingFloat(
            generator.settings, QStringLiteral("opacityDecay"), 0.0f);
        if (!std::isfinite(opacityDecay) || opacityDecay < 0.0f ||
            opacityDecay > 1.0f) {
            return false;
        }
    }
    if ((isLinear &&
         (generatorSettingInt(generator.settings,
                              QStringLiteral("legacyMode"), 0) == 3 ||
          !std::isfinite(generatorSettingFloat(
              generator.settings, QStringLiteral("rotationStep"), 0.0f)) ||
          !std::isfinite(generatorSettingFloat(
              generator.settings, QStringLiteral("opacityDecay"), 0.0f)) ||
          generatorSettingFloat(generator.settings,
                                QStringLiteral("opacityDecay"), 0.0f) < 0.0f ||
          generatorSettingFloat(generator.settings,
                                QStringLiteral("opacityDecay"), 0.0f) > 1.0f)) ||
        (isRadial &&
         (!std::isfinite(generatorSettingFloat(generator.settings,
                                QStringLiteral("rotationStep"), 0.0f)) ||
          !std::isfinite(generatorSettingFloat(generator.settings,
                                QStringLiteral("opacityDecay"), 0.0f)) ||
          generatorSettingFloat(generator.settings,
                                QStringLiteral("opacityDecay"), 0.0f) < 0.0f ||
          generatorSettingFloat(generator.settings,
                                QStringLiteral("opacityDecay"), 0.0f) > 1.0f))) {
        return false;
    }

    const auto modifiers = layer->layerCloneModifiers();
    float modifierTranslationX = 0.0f;
    float modifierTranslationY = 0.0f;
    float modifierTranslationZ = 0.0f;
    float modifierRotationDegrees = stackRotationDegrees;
    float modifierScaleX = stackScaleX;
    float modifierScaleY = stackScaleY;
    modifierTranslationX = stackOffsetX;
    modifierTranslationY = stackOffsetY;
    modifierTranslationZ = stackOffsetZ;
    if (!gpuCompatiblePlainModifierTransform(
            modifiers, modifierTranslationX, modifierTranslationY,
            modifierTranslationZ, modifierRotationDegrees, modifierScaleX,
            modifierScaleY)) {
        return false;
    }
    float plainTranslationX = 0.0f;
    float plainTranslationY = 0.0f;
    float plainTranslationZ = 0.0f;
    float plainRotationDegrees = 0.0f;
    float plainScaleX = 1.0f;
    float plainScaleY = 1.0f;
    if (!gpuCompatiblePlainModifierTransform(
            modifiers, plainTranslationX, plainTranslationY,
            plainTranslationZ, plainRotationDegrees, plainScaleX,
            plainScaleY)) {
        return false;
    }

    if (layer->authoritativeComponentEvaluationState() ||
        cloneComponentBoolProperty(
            layer, QStringLiteral("component.layout.enabled")) ||
        cloneComponentBoolProperty(
            layer, QStringLiteral("component.crowd.enabled")) ||
        cloneComponentBoolProperty(layer, QStringLiteral("physics.enabled")) ||
        cloneComponentBoolProperty(
            layer, QStringLiteral("component.collision.enabled"))) {
        return false;
    }
    for (const auto& field : layer->layerFields()) {
        if (field.enabled && field.strength != 0.0f) {
            return false;
        }
    }

    const auto& settings = generator.settings;
    const int columns = std::max(
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
    const float gridOpacityDecay = generatorSettingFloat(
        settings, QStringLiteral("opacityDecay"), 0.0f);
    const QRectF localBounds = layer->localBounds();
    const bool applyCompositionFieldChannels =
        localBounds.isValid() && localBounds.width() > 0.0 &&
        localBounds.height() > 0.0;
    const std::array<QPointF, 4> localCorners{
        localBounds.topLeft(), localBounds.topRight(),
        localBounds.bottomRight(), localBounds.bottomLeft()};

    const auto drawLinearCandidate = [&](const QMatrix4x4& cloneTransform,
                                         int cloneIndex,
                                         float generatorWeight = 1.0f) {
        QMatrix4x4 placedCloneTransform = cloneTransform;
        applyGeneratorTransformStack(settings, placedCloneTransform);
        placedCloneTransform.translate(modifierTranslationX,
                                       modifierTranslationY,
                                       modifierTranslationZ);
        if (modifierRotationDegrees != 0.0f) {
            placedCloneTransform.rotate(modifierRotationDegrees,
                                        0.0f, 0.0f, 1.0f);
        }
        placedCloneTransform.scale(modifierScaleX, modifierScaleY, 1.0f);
        if (placedCloneTransform.isIdentity()) {
            return;
        }
        QMatrix4x4 instanceTransform =
            baseTransform * placedCloneTransform;
        float instanceWeight = std::clamp(generatorWeight, 0.0f, 1.0f);
        if (applyCompositionFieldChannels) {
            qreal minX = 0.0;
            qreal maxX = 0.0;
            qreal minY = 0.0;
            qreal maxY = 0.0;
            for (std::size_t cornerIndex = 0;
                 cornerIndex < localCorners.size(); ++cornerIndex) {
                const QPointF& point = localCorners[cornerIndex];
                const QVector3D mapped = instanceTransform.map(
                    QVector3D(static_cast<float>(point.x()),
                              static_cast<float>(point.y()), 0.0f));
                const qreal mappedX = static_cast<qreal>(mapped.x());
                const qreal mappedY = static_cast<qreal>(mapped.y());
                if (cornerIndex == 0) {
                    minX = maxX = mappedX;
                    minY = maxY = mappedY;
                } else {
                    minX = std::min(minX, mappedX);
                    maxX = std::max(maxX, mappedX);
                    minY = std::min(minY, mappedY);
                    maxY = std::max(maxY, mappedY);
                }
            }
            const QRectF canvasBounds(minX, minY, maxX - minX,
                                      maxY - minY);
            const auto channels = layer->compositionFieldChannelsAtCanvasPoint(
                canvasBounds.center());
            if (channels.affected) {
                instanceWeight = std::clamp(
                    instanceWeight * channels.weight, 0.0f, 1.0f);
                if (std::abs(channels.scaleMultiplier - 1.0f) > 0.0001f) {
                    const QPointF localCenter = localBounds.center();
                    instanceTransform.translate(
                        static_cast<float>(localCenter.x()),
                        static_cast<float>(localCenter.y()));
                    instanceTransform.scale(channels.scaleMultiplier);
                    instanceTransform.translate(
                        static_cast<float>(-localCenter.x()),
                        static_cast<float>(-localCenter.y()));
                }
            }
        }
        drawFn(instanceTransform, instanceWeight, cloneIndex);
    };

    placementObserver(placementObserverContext, generator.typeId, settings,
                      plainTranslationX, plainTranslationY,
                      plainTranslationZ, plainRotationDegrees,
                      plainScaleX, plainScaleY);
    drawFn(baseTransform, 1.0f, -1);
    if (isRadial) {
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
            ? (endAngle - startAngle) / static_cast<float>(count - 1)
            : 0.0f;
        for (int i = 0; i < count; ++i) {
            const float angle = startAngle + angleStep * static_cast<float>(i);
            const float radians = angle * 3.1415926535f / 180.0f;
            QMatrix4x4 cloneTransform;
            cloneTransform.setToIdentity();
            cloneTransform.translate(std::cos(radians) * radius,
                                     std::sin(radians) * radius, 0.0f);
            cloneTransform.rotate(angle + rotationStep * static_cast<float>(i),
                                  0.0f, 0.0f, 1.0f);
            drawLinearCandidate(
                cloneTransform, i,
                1.0f - opacityDecay * static_cast<float>(i));
        }
        return true;
    }
    if (isLinear) {
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
        for (int i = 1; i <= count; ++i) {
            QMatrix4x4 cloneTransform;
            cloneTransform.setToIdentity();
            cloneTransform.translate(offsetX * static_cast<float>(i),
                                     offsetY * static_cast<float>(i),
                                     offsetZ * static_cast<float>(i));
            if (rotationStep != 0.0f) {
                cloneTransform.rotate(rotationStep * static_cast<float>(i),
                                      0.0f, 0.0f, 1.0f);
            }
            drawLinearCandidate(
                cloneTransform, i - 1,
                1.0f - opacityDecay * static_cast<float>(i - 1));
        }
        return true;
    }
    const QVector3D startPos(
        -((columns - 1) * spacingX) * 0.5f,
        -((rows - 1) * spacingY) * 0.5f,
        -((depth - 1) * spacingZ) * 0.5f);
    std::uint64_t candidateIndex = 0;
    const auto maxCloneIndex = static_cast<std::uint64_t>(
        std::numeric_limits<int>::max());
    for (int z = 0; z < depth; ++z) {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < columns; ++x) {
                const int cloneIndex = candidateIndex <= maxCloneIndex
                    ? static_cast<int>(candidateIndex)
                    : -2;
                if (candidateIndex <= maxCloneIndex) {
                    ++candidateIndex;
                }
                QMatrix4x4 cloneTransform;
                cloneTransform.setToIdentity();
                cloneTransform.translate(startPos.x() + spacingX * x,
                                         startPos.y() + spacingY * y,
                                         startPos.z() + spacingZ * z);
                applyGeneratorTransformStack(settings, cloneTransform);
                cloneTransform.translate(modifierTranslationX,
                                         modifierTranslationY,
                                         modifierTranslationZ);
                if (modifierRotationDegrees != 0.0f) {
                    cloneTransform.rotate(modifierRotationDegrees,
                                          0.0f, 0.0f, 1.0f);
                }
                cloneTransform.scale(modifierScaleX, modifierScaleY, 1.0f);
                if (!cloneTransform.isIdentity()) {
                    // Grid groups larger than the renderer's bounded GPU
                    // arena use CPU drawing. Keep their callback index out of
                    // signed-overflow territory and make GPU matching fail.
                    QMatrix4x4 instanceTransform =
                        baseTransform * cloneTransform;
                    float instanceWeight = std::clamp(
                        1.0f - gridOpacityDecay * static_cast<float>(cloneIndex),
                        0.0f, 1.0f);
                    if (applyCompositionFieldChannels) {
                        qreal minX = 0.0;
                        qreal maxX = 0.0;
                        qreal minY = 0.0;
                        qreal maxY = 0.0;
                        for (std::size_t cornerIndex = 0;
                             cornerIndex < localCorners.size();
                             ++cornerIndex) {
                            const QPointF& point = localCorners[cornerIndex];
                            const QVector3D mapped = instanceTransform.map(
                                QVector3D(static_cast<float>(point.x()),
                                          static_cast<float>(point.y()), 0.0f));
                            const qreal mappedX = static_cast<qreal>(mapped.x());
                            const qreal mappedY = static_cast<qreal>(mapped.y());
                            if (cornerIndex == 0) {
                                minX = maxX = mappedX;
                                minY = maxY = mappedY;
                            } else {
                                minX = std::min(minX, mappedX);
                                maxX = std::max(maxX, mappedX);
                                minY = std::min(minY, mappedY);
                                maxY = std::max(maxY, mappedY);
                            }
                        }
                        const QRectF canvasBounds(minX, minY,
                                                  maxX - minX, maxY - minY);
                        const auto channels =
                            layer->compositionFieldChannelsAtCanvasPoint(
                                canvasBounds.center());
                        if (channels.affected) {
                            instanceWeight = std::clamp(
                                instanceWeight * channels.weight, 0.0f, 1.0f);
                            if (std::abs(channels.scaleMultiplier - 1.0f) >
                                0.0001f) {
                                const QPointF localCenter = localBounds.center();
                                instanceTransform.translate(
                                    static_cast<float>(localCenter.x()),
                                    static_cast<float>(localCenter.y()));
                                instanceTransform.scale(
                                    channels.scaleMultiplier);
                                instanceTransform.translate(
                                    static_cast<float>(-localCenter.x()),
                                    static_cast<float>(-localCenter.y()));
                            }
                        }
                    }
                    drawFn(instanceTransform, instanceWeight, cloneIndex);
                }
            }
        }
    }
    return true;
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

inline bool gpuCompatibleTransformStack(
    const QJsonObject& settings, float& offsetX, float& offsetY, float& offsetZ,
    float& rotationDegrees, float& outputScaleX, float& outputScaleY)
{
    float accumulatedX = 0.0f;
    float accumulatedY = 0.0f;
    float accumulatedZ = 0.0f;
    float accumulatedRotation = 0.0f;
    float accumulatedScaleX = 1.0f;
    float accumulatedScaleY = 1.0f;
    const auto transformStack =
        settings.value(QStringLiteral("transformStack")).toArray();
    for (const auto& value : transformStack) {
        if (!value.isObject()) {
            continue;
        }
        const auto transform = value.toObject();
        if (!transform.value(QStringLiteral("enabled")).toBool(true)) {
            continue;
        }
        const double x = transform.value(QStringLiteral("positionX")).toDouble();
        const double y = transform.value(QStringLiteral("positionY")).toDouble();
        const double z = transform.value(QStringLiteral("positionZ")).toDouble();
        const double rotationZ =
            transform.value(QStringLiteral("rotationZ")).toDouble();
        const double scaleX =
            transform.value(QStringLiteral("scaleX")).toDouble(1.0);
        const double scaleY =
            transform.value(QStringLiteral("scaleY")).toDouble(1.0);
        const double scaleZ =
            transform.value(QStringLiteral("scaleZ")).toDouble(1.0);
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
            !std::isfinite(rotationZ) || !std::isfinite(scaleX) ||
            !std::isfinite(scaleY) || !std::isfinite(scaleZ) ||
            std::abs(rotationZ) > 360000.0 || scaleZ != 1.0) {
            return false;
        }
        const float transformScaleX = static_cast<float>(scaleX);
        const float transformScaleY = static_cast<float>(scaleY);
        const float transformRotation = static_cast<float>(rotationZ);
        if (!std::isfinite(transformScaleX) ||
            !std::isfinite(transformScaleY) ||
            !std::isfinite(transformRotation) ||
            (accumulatedScaleX != accumulatedScaleY &&
             transformRotation != 0.0f)) {
            return false;
        }
        const float translationX = static_cast<float>(x);
        const float translationY = static_cast<float>(y);
        const float translationZ = static_cast<float>(z);
        const float angleRadians = accumulatedRotation *
            0.017453292519943295f;
        const float cosineAngle = std::cos(angleRadians);
        const float sineAngle = std::sin(angleRadians);
        accumulatedX += translationX * accumulatedScaleX * cosineAngle -
                        translationY * accumulatedScaleY * sineAngle;
        accumulatedY += translationX * accumulatedScaleX * sineAngle +
                        translationY * accumulatedScaleY * cosineAngle;
        accumulatedZ += translationZ;
        accumulatedRotation += transformRotation;
        accumulatedScaleX *= transformScaleX;
        accumulatedScaleY *= transformScaleY;
        if (!std::isfinite(translationX) || !std::isfinite(translationY) ||
            !std::isfinite(translationZ) || !std::isfinite(accumulatedX) ||
            !std::isfinite(accumulatedY) || !std::isfinite(accumulatedZ) ||
            !std::isfinite(accumulatedRotation) ||
            !std::isfinite(accumulatedScaleX) ||
            !std::isfinite(accumulatedScaleY) ||
            std::abs(accumulatedRotation) > 360000.0f) {
            return false;
        }
    }
    offsetX = accumulatedX;
    offsetY = accumulatedY;
    offsetZ = accumulatedZ;
    rotationDegrees = accumulatedRotation;
    outputScaleX = accumulatedScaleX;
    outputScaleY = accumulatedScaleY;
    return true;
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

inline bool gpuCompatiblePlainModifierTransform(
    const ArtifactCore::NamedVector<LayerModifierDescriptor>& modifiers,
    float& translationX, float& translationY, float& translationZ,
    float& rotationDegrees, float& outputScaleX, float& outputScaleY)
{
    for (const auto& modifier : modifiers) {
        if (!modifier.enabled) {
            continue;
        }
        const auto& settings = modifier.settings;
        if (modifier.typeId == QStringLiteral("artifact.modifier.time-offset")) {
            if (settings.value(QStringLiteral("step")).toDouble(0.0) != 0.0) {
                return false;
            }
            continue;
        }
        if (modifier.typeId == QStringLiteral("artifact.modifier.sequence")) {
            if (settings.value(QStringLiteral("enabled")).toBool(true)) {
                return false;
            }
            continue;
        }
        if (modifier.typeId != QStringLiteral("artifact.modifier.plain")) {
            return false;
        }
        const float strength = static_cast<float>(
            settings.value(QStringLiteral("strength")).toDouble(1.0));
        const float positionX = static_cast<float>(
            settings.value(QStringLiteral("positionX")).toDouble(0.0));
        const float positionY = static_cast<float>(
            settings.value(QStringLiteral("positionY")).toDouble(0.0));
        const float positionZ = static_cast<float>(
            settings.value(QStringLiteral("positionZ")).toDouble(0.0));
        const float rotationX = static_cast<float>(
            settings.value(QStringLiteral("rotationX")).toDouble(0.0));
        const float rotationY = static_cast<float>(
            settings.value(QStringLiteral("rotationY")).toDouble(0.0));
        const float modifierRotationZ = static_cast<float>(
            settings.value(QStringLiteral("rotationZ")).toDouble(0.0));
        const float scaleX = static_cast<float>(
            settings.value(QStringLiteral("scaleX")).toDouble(1.0));
        const float scaleY = static_cast<float>(
            settings.value(QStringLiteral("scaleY")).toDouble(1.0));
        const float scaleZ = static_cast<float>(
            settings.value(QStringLiteral("scaleZ")).toDouble(1.0));
        if (!std::isfinite(strength) || !std::isfinite(positionX) ||
            !std::isfinite(positionY) || !std::isfinite(positionZ) ||
            !std::isfinite(rotationX) || !std::isfinite(rotationY) ||
            !std::isfinite(modifierRotationZ) || !std::isfinite(scaleX) ||
            !std::isfinite(scaleY) || !std::isfinite(scaleZ) ||
            rotationX != 0.0f || rotationY != 0.0f || scaleZ != 1.0f) {
            return false;
        }
        const float clampedStrength = std::clamp(strength, 0.0f, 1.0f);
        const float effectiveScaleX =
            1.0f + (scaleX - 1.0f) * clampedStrength;
        const float effectiveScaleY =
            1.0f + (scaleY - 1.0f) * clampedStrength;
        const float effectiveRotation =
            modifierRotationZ * clampedStrength;
        // The placement record stores rotation plus diagonal XY scale. Once
        // the accumulated scale is non-uniform, a later Z rotation would add
        // shear that this compact representation cannot preserve.
        if (outputScaleX != outputScaleY && effectiveRotation != 0.0f) {
            return false;
        }
        const float radians = rotationDegrees * 0.017453292519943295f;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        const float scaledPositionX = positionX * clampedStrength *
                                      outputScaleX;
        const float scaledPositionY = positionY * clampedStrength *
                                      outputScaleY;
        translationX += scaledPositionX * cosine - scaledPositionY * sine;
        translationY += scaledPositionX * sine + scaledPositionY * cosine;
        translationZ += positionZ * clampedStrength;
        rotationDegrees += effectiveRotation;
        outputScaleX *= effectiveScaleX;
        outputScaleY *= effectiveScaleY;
        if (!std::isfinite(translationX) || !std::isfinite(translationY) ||
            !std::isfinite(translationZ) ||
            !std::isfinite(rotationDegrees) || !std::isfinite(outputScaleX) ||
            !std::isfinite(outputScaleY)) {
            return false;
        }
    }
    return true;
}

inline void applyCloneEffectorModifiers(
    const ArtifactAbstractLayer* layer,
    const ArtifactCore::NamedVector<LayerModifierDescriptor>& modifiers,
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
            const float rotationX = static_cast<float>(
                settings.value(QStringLiteral("rotationX")).toDouble(0.0)) * strength;
            const float rotationY = static_cast<float>(
                settings.value(QStringLiteral("rotationY")).toDouble(0.0)) * strength;
            if (rotationX != 0.0f) {
                cloneTransform.rotate(rotationX, 1.0f, 0.0f, 0.0f);
            }
            if (rotationY != 0.0f) {
                cloneTransform.rotate(rotationY, 0.0f, 1.0f, 0.0f);
            }
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
            cloneTransform.rotate(unit(rng) * static_cast<float>(
                                      settings.value(QStringLiteral("rotationX")).toDouble(0.0)) * strength,
                                  1.0f, 0.0f, 0.0f);
            cloneTransform.rotate(unit(rng) * static_cast<float>(
                                      settings.value(QStringLiteral("rotationY")).toDouble(0.0)) * strength,
                                  0.0f, 1.0f, 0.0f);
            cloneTransform.rotate(unit(rng) * rotationZ * strength,
                                  0.0f, 0.0f, 1.0f);
            const float scaleVariance = std::max(0.0f, static_cast<float>(
                settings.value(QStringLiteral("scaleVariance")).toDouble(0.0)));
            const float scale = 1.0f + unit(rng) * scaleVariance * strength;
            cloneTransform.scale(std::max(0.001f, scale),
                                 std::max(0.001f, scale),
                                 std::max(0.001f, scale));
        } else if (modifier.typeId == QStringLiteral("artifact.modifier.formula")) {
            const double frameRate = std::max(1.0, layer ? layer->compositionFrameRate() : 30.0);
            const float timeSeconds = layer
                ? static_cast<float>(static_cast<double>(layer->currentFrame()) / frameRate)
                : 0.0f;
            const float frequency = static_cast<float>(
                settings.value(QStringLiteral("frequency")).toDouble(1.0));
            const float phase = static_cast<float>(
                settings.value(QStringLiteral("phase")).toDouble(0.0));
            const float indexPhase = static_cast<float>(
                settings.value(QStringLiteral("indexPhase")).toDouble(0.0));
            const float wave = std::sin(
                timeSeconds * frequency * 6.28318530718f + phase +
                static_cast<float>(cloneIndex) * indexPhase) * strength;
            cloneTransform.translate(
                static_cast<float>(settings.value(QStringLiteral("amplitudeX")).toDouble(0.0)) * wave,
                static_cast<float>(settings.value(QStringLiteral("amplitudeY")).toDouble(0.0)) * wave,
                static_cast<float>(settings.value(QStringLiteral("amplitudeZ")).toDouble(0.0)) * wave);
        } else if (modifier.typeId == QStringLiteral("artifact.modifier.spline")) {
            const float indexScale = std::max(0.0001f, static_cast<float>(
                settings.value(QStringLiteral("indexScale")).toDouble(0.1)));
            const float t = std::clamp(static_cast<float>(cloneIndex) * indexScale,
                                       0.0f, 1.0f);
            const float oneMinusT = 1.0f - t;
            const auto point = [&](const QString& prefix) {
                return QVector3D(
                    static_cast<float>(settings.value(prefix + QStringLiteral("X")).toDouble(0.0)),
                    static_cast<float>(settings.value(prefix + QStringLiteral("Y")).toDouble(0.0)),
                    static_cast<float>(settings.value(prefix + QStringLiteral("Z")).toDouble(0.0)));
            };
            const QVector3D start = point(QStringLiteral("start"));
            const QVector3D control = point(QStringLiteral("control"));
            const QVector3D end = point(QStringLiteral("end"));
            const QVector3D position = start * (oneMinusT * oneMinusT) +
                control * (2.0f * oneMinusT * t) + end * (t * t);
            cloneTransform.translate(position * strength);
        } else if (modifier.typeId == QStringLiteral("artifact.modifier.step")) {
            const float step = static_cast<float>(cloneIndex) * strength;
            cloneTransform.translate(
                static_cast<float>(settings.value(QStringLiteral("positionX")).toDouble(0.0)) * step,
                static_cast<float>(settings.value(QStringLiteral("positionY")).toDouble(0.0)) * step,
                static_cast<float>(settings.value(QStringLiteral("positionZ")).toDouble(0.0)) * step);
            const float rotationZ = static_cast<float>(
                settings.value(QStringLiteral("rotationZ")).toDouble(0.0)) * step;
            const float rotationX = static_cast<float>(
                settings.value(QStringLiteral("rotationX")).toDouble(0.0)) * step;
            const float rotationY = static_cast<float>(
                settings.value(QStringLiteral("rotationY")).toDouble(0.0)) * step;
            if (rotationX != 0.0f) {
                cloneTransform.rotate(rotationX, 1.0f, 0.0f, 0.0f);
            }
            if (rotationY != 0.0f) {
                cloneTransform.rotate(rotationY, 0.0f, 1.0f, 0.0f);
            }
            if (rotationZ != 0.0f) {
                cloneTransform.rotate(rotationZ, 0.0f, 0.0f, 1.0f);
            }
            const float scaleX = 1.0f + static_cast<float>(
                settings.value(QStringLiteral("scaleX")).toDouble(1.0) - 1.0) * step;
            const float scaleY = 1.0f + static_cast<float>(
                settings.value(QStringLiteral("scaleY")).toDouble(1.0) - 1.0) * step;
            const float scaleZ = 1.0f + static_cast<float>(
                settings.value(QStringLiteral("scaleZ")).toDouble(1.0) - 1.0) * step;
            cloneTransform.scale(std::max(0.001f, scaleX),
                                 std::max(0.001f, scaleY),
                                 std::max(0.001f, scaleZ));
        }
    }
}

std::vector<CloneRenderInstance> clonerComponentInstances(
      const ArtifactAbstractLayer* layer,
      const QMatrix4x4& baseTransform,
      CloneGridPlacementObserver placementObserver,
      void* placementObserverContext)
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

    // The GPU placement observer accepts one plain Grid/Matrix, Linear, or
    // Radial generator with a finite per-clone opacity-decay weight.
    // It runs before clone modifiers and later component stages, so its output
    // is the generator placement data rather than a claim about final drawing.
    if (placementObserver && generators.size() == 1U) {
        const auto& generator = generators[0];
        const bool isGrid =
            generator.typeId ==
                QStringLiteral("artifact.generator.cloner.grid") ||
            generator.typeId ==
                QStringLiteral("artifact.generator.cloner.matrix");
        const bool isLinear = generator.typeId ==
            QStringLiteral("artifact.generator.cloner.linear");
        const bool isRadial = generator.typeId ==
            QStringLiteral("artifact.generator.cloner.radial");
        const float gridOpacityDecay = generatorSettingFloat(
            generator.settings, QStringLiteral("opacityDecay"), 0.0f);
        const bool gridOpacityValid = std::isfinite(gridOpacityDecay) &&
            gridOpacityDecay >= 0.0f && gridOpacityDecay <= 1.0f;
        const float linearRotationStep = generatorSettingFloat(
            generator.settings, QStringLiteral("rotationStep"), 0.0f);
        const float linearOpacityDecay = generatorSettingFloat(
            generator.settings, QStringLiteral("opacityDecay"), 0.0f);
        const bool linearOpacityValid =
            std::isfinite(linearOpacityDecay) && linearOpacityDecay >= 0.0f &&
            linearOpacityDecay <= 1.0f;
        const bool simplePlacement =
            (!isLinear && !isRadial && (!isGrid || gridOpacityValid)) ||
            ((isLinear || isRadial) &&
             generatorSettingInt(generator.settings,
                                 QStringLiteral("legacyMode"), 0) != 3 &&
             (!isLinear ||
              (std::isfinite(linearRotationStep) && linearOpacityValid)) &&
             (!isRadial ||
              (std::isfinite(generatorSettingFloat(generator.settings,
                                     QStringLiteral("rotationStep"), 0.0f)) &&
               std::isfinite(generatorSettingFloat(generator.settings,
                                     QStringLiteral("opacityDecay"), 0.0f)) &&
               generatorSettingFloat(generator.settings,
                                     QStringLiteral("opacityDecay"), 0.0f) >= 0.0f &&
               generatorSettingFloat(generator.settings,
                                     QStringLiteral("opacityDecay"), 0.0f) <= 1.0f)));
        if (generator.enabled && (isGrid || isLinear || isRadial) && simplePlacement &&
            generator.settings
                          .value(QStringLiteral("transformStack"))
                          .toArray()
                          .isEmpty()) {
            float modifierTranslationX = 0.0f;
            float modifierTranslationY = 0.0f;
            float modifierTranslationZ = 0.0f;
            float modifierRotationDegrees = 0.0f;
            float modifierScaleX = 1.0f;
            float modifierScaleY = 1.0f;
            const bool placementOnlyModifiers =
                !generator.settings.value(QStringLiteral("sequenceEnabled"))
                     .toBool(false) &&
                gpuCompatiblePlainModifierTransform(
                    modifiers, modifierTranslationX, modifierTranslationY,
                    modifierTranslationZ, modifierRotationDegrees,
                    modifierScaleX, modifierScaleY);
            if (placementOnlyModifiers) {
                placementObserver(placementObserverContext, generator.typeId,
                                  generator.settings, modifierTranslationX,
                                  modifierTranslationY, modifierTranslationZ,
                                  modifierRotationDegrees, modifierScaleX,
                                  modifierScaleY);
            }
        }
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
            cloneModifierTimeOffset(modifiers, settings, cloneIndex),
            {}, {}, cloneIndex});
    };

    for (const auto& generator : generators) {
        if (!generator.enabled) {
            continue;
        }

        const auto& settings = generator.settings;
        if (generator.typeId ==
                QStringLiteral("artifact.generator.cloner.grid") ||
            generator.typeId ==
                QStringLiteral("artifact.generator.cloner.matrix")) {
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
            const float opacityDecay = generatorSettingFloat(
                settings, QStringLiteral("opacityDecay"), 0.0f);
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
                        applyCloneEffectorModifiers(layer, modifiers, cloneIndex, cloneTransform);
                        appendCloneInstance(
                            settings, cloneTransform,
                            1.0f - opacityDecay *
                                static_cast<float>(cloneIndex),
                            cloneIndex);
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
                applyCloneEffectorModifiers(layer, modifiers, i, cloneTransform);
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
                applyCloneEffectorModifiers(layer, modifiers, i, cloneTransform);
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
            applyCloneEffectorModifiers(layer, modifiers, cloneIndex, cloneTransform);
            appendCloneInstance(
                settings, cloneTransform,
                1.0f - opacityDecay * static_cast<float>(cloneIndex),
                cloneIndex);
        }
    }
    return instances;
  }

} // namespace Artifact


