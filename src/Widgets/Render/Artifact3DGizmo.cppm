module;
#include <array>
#include <utility>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QQuaternion>
#include <wobjectimpl.h>

module Artifact.Widgets.Gizmo3D;

import std;
import Settings.Accessibility;

namespace Artifact {

W_OBJECT_IMPL(Artifact3DGizmo)

namespace {

constexpr float kIntersectionEpsilon = 1e-6f;
constexpr float kPlaneHandleInsetScale = 0.22f;
constexpr float kPlaneHandleSizeScale = 0.14f;
constexpr float kMinPlaneHandleInset = 6.0f;
constexpr float kMinPlaneHandleSize = 4.0f;
constexpr float kAxisHitThresholdScale = 0.11f;
constexpr float kMinAxisHitThreshold = 8.0f;
constexpr float kAxisTipRadiusScale = 0.055f;
constexpr float kMinAxisTipRadius = 5.0f;
constexpr float kMinimumScale = 0.01f;
constexpr float kScaleDivisionEpsilon = 1e-3f;
constexpr float kScaleHandleLength = 0.92f;
constexpr float kRotateAxisRadiusScale = 0.78f;
constexpr float kRotateScreenRadiusScale = 0.96f;
constexpr float kUniformScaleRadiusScale = 0.60f;
// Dedicated Rotate stays comparable in footprint to the Move/Scale handles.
// Blender and Unity use the full object-space ring size; clarity comes from
// isolating the mode and reducing visual weight, not shrinking the target.
constexpr float kDedicatedRotateAxisRadiusScale = 0.78f;
constexpr float kDedicatedRotateScreenRadiusScale = 0.96f;
constexpr float kMoveArrowSizeScale = 0.18f;
constexpr float kMoveCenterHalfSizeScale = 0.065f;
constexpr float kDedicatedScaleCubeHalfScale = 0.052f;
constexpr float kDedicatedScaleCenterHitScale = 0.13f;
constexpr float kTranslationSnap = 1.0f;
constexpr float kRotationSnapDegrees = 15.0f;
constexpr float kScaleSnap = 0.1f;

float snapInteractionValue(float value, float increment, bool enabled) {
    if (!enabled || !(increment > 0.0f) || !std::isfinite(value)) {
        return value;
    }
    return std::round(value / increment) * increment;
}

} // namespace

struct Artifact3DGizmo::Impl {
    QVector3D position;
    QVector3D rotation;
    QVector3D scale = QVector3D(1, 1, 1);
    QVector3D localAxisX{1.0f, 0.0f, 0.0f};
    QVector3D localAxisY{0.0f, -1.0f, 0.0f};
    QVector3D localAxisZ{0.0f, 0.0f, 1.0f};
    QVector3D viewAxisX{1.0f, 0.0f, 0.0f};
    QVector3D viewAxisY{0.0f, -1.0f, 0.0f};
    QVector3D viewAxisZ{0.0f, 0.0f, 1.0f};
    bool hasLocalBasis = false;
    float currentScale = 1.0f;

    Ray dragStartRay;
    QVector3D dragStartPosition;
    QVector3D dragStartInteractionCenter;
    QVector3D dragStartRotation;
    QVector3D dragStartScale;
    QVector3D dragOffset;
    QVector3D dragStartHitPoint;
    QVector3D dragAxisDirection;
    QVector3D dragPlaneNormal;
    QVector3D dragScaleAxes;
    QVector3D dragScaleSigns{1.0f, 1.0f, 1.0f};
    QVector3D dragStartBoundingBoxHandlePoint;
    bool boundingBoxDrag = false;
    float dragReferenceScale = 1.0f;
    float dragStartAngle = 0.0f;
    bool firstDrag = true;
    bool numericInputActive = false;
    float numericInput = 0.0f;
    bool numericPlanarScale = false;
    // Full mode recursively draws the three transform modes. Keep shared
    // bounding geometry to the Scale pass rather than emitting it three times.
    bool drawingFullOverlay = false;
    bool testingFullOverlay = false;
    
    // Intersection helpers
    float rayLineDistance(const QVector3D& rayOrigin, const QVector3D& rayDir, 
                          const QVector3D& p1, const QVector3D& p2, float& t) {
        QVector3D u = rayDir;
        QVector3D v = p2 - p1;
        QVector3D w = rayOrigin - p1;
        float a = QVector3D::dotProduct(u, u);
        float b = QVector3D::dotProduct(u, v);
        float c = QVector3D::dotProduct(v, v);
        float d = QVector3D::dotProduct(u, w);
        float e = QVector3D::dotProduct(v, w);
        float D = a * c - b * b;
        float sc, tc;

        if (D < kIntersectionEpsilon) { // Lines are parallel
            sc = 0.0f;
            tc = (b > c ? d / b : e / c);
        } else {
            sc = (b * e - c * d) / D;
            tc = (a * e - b * d) / D;
        }

        tc = std::clamp(tc, 0.0f, 1.0f);
        t = tc;
        QVector3D closestPointOnRay = rayOrigin + u * sc;
        QVector3D closestPointOnSegment = p1 + v * tc;
        return (closestPointOnRay - closestPointOnSegment).length();
    }

    // Intersects ray with a plane
    bool intersectRayPlane(const Ray& ray, const QVector3D& planePos, const QVector3D& planeNormal, QVector3D& hitPoint, float* tOut = nullptr) {
        float denom = QVector3D::dotProduct(planeNormal, ray.direction);
        if (std::abs(denom) < kIntersectionEpsilon) return false;
        float t = QVector3D::dotProduct(planePos - ray.origin, planeNormal) / denom;
        if (t < 0) return false;
        if (tOut) {
            *tOut = t;
        }
        hitPoint = ray.origin + ray.direction * t;
        return true;
    }
};

namespace {

QVector3D worldAxisDirectionFor(GizmoAxis axis) {
    switch (axis) {
    case GizmoAxis::X:
        return QVector3D(1, 0, 0);
    case GizmoAxis::Y:
        // Viewport coordinates are Y-down, but gizmo axes should read as Y-up
        // so the Move handle matches the conventional editor feel.
        return QVector3D(0, -1, 0);
    case GizmoAxis::Z:
        return QVector3D(0, 0, 1);
    default:
        return QVector3D();
    }
}

struct GizmoBasis {
    QVector3D x{1.0f, 0.0f, 0.0f};
    QVector3D y{0.0f, -1.0f, 0.0f};
    QVector3D z{0.0f, 0.0f, 1.0f};
};

struct BoundingBoxGeometry {
    std::array<QVector3D, 8> corners{};
    std::array<QVector3D, 12> edgeCenters{};
    std::array<QVector3D, 6> faceCenters{};
};

struct BoundingBoxHit {
    GizmoAxis axis = GizmoAxis::None;
    QVector3D scaleAxes;
    QVector3D scaleSigns{1.0f, 1.0f, 1.0f};
};

QVector3D transformBoundingBoxPoint(const QVector3D& localPoint,
                                    const QVector3D& position,
                                    const QVector3D& scale,
                                    const GizmoBasis& basis) {
    return position + basis.x * (localPoint.x() * scale.x()) +
           basis.y * (localPoint.y() * scale.y()) +
           basis.z * (localPoint.z() * scale.z());
}

BoundingBoxGeometry boundingBoxGeometryFor(const QVector3D& minBounds,
                                           const QVector3D& maxBounds,
                                           const QVector3D& position,
                                           const QVector3D& scale,
                                           const GizmoBasis& basis) {
    const QVector3D localCenter = (minBounds + maxBounds) * 0.5f;
    const QVector3D localHalf = (maxBounds - minBounds) * 0.5f;
    const std::array<QVector3D, 8> localCorners = {
        QVector3D(minBounds.x(), minBounds.y(), minBounds.z()),
        QVector3D(maxBounds.x(), minBounds.y(), minBounds.z()),
        QVector3D(maxBounds.x(), maxBounds.y(), minBounds.z()),
        QVector3D(minBounds.x(), maxBounds.y(), minBounds.z()),
        QVector3D(minBounds.x(), minBounds.y(), maxBounds.z()),
        QVector3D(maxBounds.x(), minBounds.y(), maxBounds.z()),
        QVector3D(maxBounds.x(), maxBounds.y(), maxBounds.z()),
        QVector3D(minBounds.x(), maxBounds.y(), maxBounds.z())};

    BoundingBoxGeometry geometry;
    for (std::size_t i = 0; i < localCorners.size(); ++i) {
        geometry.corners[i] = transformBoundingBoxPoint(
            localCorners[i], position, scale, basis);
    }

    static constexpr int edgeIndices[][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
        {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (std::size_t i = 0; i < 12; ++i) {
        const auto& edge = edgeIndices[i];
        geometry.edgeCenters[i] =
            (geometry.corners[edge[0]] + geometry.corners[edge[1]]) * 0.5f;
    }

    const std::array<QVector3D, 6> localFaces = {
        localCenter + QVector3D(localHalf.x(), 0.0f, 0.0f),
        localCenter - QVector3D(localHalf.x(), 0.0f, 0.0f),
        localCenter + QVector3D(0.0f, localHalf.y(), 0.0f),
        localCenter - QVector3D(0.0f, localHalf.y(), 0.0f),
        localCenter + QVector3D(0.0f, 0.0f, localHalf.z()),
        localCenter - QVector3D(0.0f, 0.0f, localHalf.z())};
    for (std::size_t i = 0; i < localFaces.size(); ++i) {
        geometry.faceCenters[i] = transformBoundingBoxPoint(
            localFaces[i], position, scale, basis);
    }
    return geometry;
}

QVector3D boundingBoxHandlePointFor(const QVector3D& minBounds,
                                   const QVector3D& maxBounds,
                                   const QVector3D& position,
                                   const QVector3D& scale,
                                   const GizmoBasis& basis,
                                   const QVector3D& scaleAxes,
                                   const QVector3D& scaleSigns) {
    const QVector3D localCenter = (minBounds + maxBounds) * 0.5f;
    const QVector3D localHalf = (maxBounds - minBounds) * 0.5f;
    QVector3D localPoint = localCenter;
    if (scaleAxes.x() > 0.5f) {
        localPoint.setX(localCenter.x() + localHalf.x() * scaleSigns.x());
    }
    if (scaleAxes.y() > 0.5f) {
        localPoint.setY(localCenter.y() + localHalf.y() * scaleSigns.y());
    }
    if (scaleAxes.z() > 0.5f) {
        localPoint.setZ(localCenter.z() + localHalf.z() * scaleSigns.z());
    }
    return transformBoundingBoxPoint(localPoint, position, scale, basis);
}

QVector3D interactionCenterFor(const QVector3D& minBounds,
                               const QVector3D& maxBounds,
                               bool boundingBoxEnabled,
                               const QVector3D& position,
                               const QVector3D& scale,
                               const GizmoBasis& basis) {
    if (!boundingBoxEnabled) {
        return position;
    }
    return transformBoundingBoxPoint((minBounds + maxBounds) * 0.5f,
                                     position, scale, basis);
}

BoundingBoxHit hitBoundingBoxHandle(const Ray& ray,
                                    const BoundingBoxGeometry& geometry,
                                    float handleThreshold,
                                    bool depthEnabled) {
    BoundingBoxHit result;
    float bestDistance = std::numeric_limits<float>::max();
    float bestDepth = std::numeric_limits<float>::max();

    const auto consider = [&](const QVector3D& point, GizmoAxis axis,
                              const QVector3D& scaleAxes,
                              const QVector3D& scaleSigns) {
        const float rayDepth = QVector3D::dotProduct(point - ray.origin,
                                                     ray.direction);
        if (rayDepth < 0.0f) {
            return;
        }
        const QVector3D closest = ray.origin + ray.direction * rayDepth;
        const float distance = (closest - point).length();
        if (distance > handleThreshold ||
            (distance > bestDistance && rayDepth >= bestDepth)) {
            return;
        }
        bestDistance = distance;
        bestDepth = rayDepth;
        result.axis = axis;
        result.scaleAxes = scaleAxes;
        result.scaleSigns = scaleSigns;
    };

    // Corners drive all three dimensions. Shift can later collapse this to a
    // uniform factor, while the default behavior remains non-uniform.
    const float cornerSigns[][3] = {
        {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}};
    for (std::size_t i = 0; i < geometry.corners.size(); ++i) {
        const auto& signs = cornerSigns[i];
        consider(geometry.corners[i], GizmoAxis::Screen,
                 QVector3D(1.0f, 1.0f, 1.0f),
                 QVector3D(signs[0], signs[1], signs[2]));
    }

    // Edge centers scale two dimensions at once. The edge direction is the
    // dimension that remains unchanged.
    const GizmoAxis edgeAxes[] = {
        GizmoAxis::YZ, GizmoAxis::XZ, GizmoAxis::YZ, GizmoAxis::XZ,
        GizmoAxis::YZ, GizmoAxis::XZ, GizmoAxis::YZ, GizmoAxis::XZ,
        GizmoAxis::XY, GizmoAxis::XY, GizmoAxis::XY, GizmoAxis::XY};
    const QVector3D edgeMasks[] = {
        {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
    const QVector3D edgeSigns[] = {
        {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
        {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f},
        {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}};
    for (int i = 0; i < 12; ++i) {
        consider(geometry.edgeCenters[static_cast<std::size_t>(i)],
                 edgeAxes[i], edgeMasks[i], edgeSigns[i]);
    }

    const GizmoAxis faceAxes[] = {GizmoAxis::X, GizmoAxis::X,
                                  GizmoAxis::Y, GizmoAxis::Y,
                                  GizmoAxis::Z, GizmoAxis::Z};
    const QVector3D faceMasks[] = {{1.0f, 0.0f, 0.0f},
                                   {1.0f, 0.0f, 0.0f},
                                   {0.0f, 1.0f, 0.0f},
                                   {0.0f, 1.0f, 0.0f},
                                   {0.0f, 0.0f, 1.0f},
                                   {0.0f, 0.0f, 1.0f}};
    const float faceSigns[] = {1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f};
    for (int i = 0; i < 6; ++i) {
        if (!depthEnabled && faceAxes[i] == GizmoAxis::Z) {
            continue;
        }
        consider(geometry.faceCenters[static_cast<std::size_t>(i)],
                 faceAxes[i], faceMasks[i],
                 QVector3D(i < 2 ? faceSigns[i] : 1.0f,
                           i >= 2 && i < 4 ? faceSigns[i] : 1.0f,
                           i >= 4 ? faceSigns[i] : 1.0f));
    }
    return result;
}

GizmoBasis gizmoBasisFor(const QVector3D& rotation, GizmoSpace space,
                         const QVector3D& localAxisX,
                         const QVector3D& localAxisY,
                         const QVector3D& localAxisZ,
                         bool hasLocalBasis,
                         const QVector3D& viewAxisX,
                         const QVector3D& viewAxisY,
                         const QVector3D& viewAxisZ) {
    if (space == GizmoSpace::View) {
        return {viewAxisX.normalized(), viewAxisY.normalized(),
                viewAxisZ.normalized()};
    }
    if (space != GizmoSpace::Local) {
        return {};
    }

    if (hasLocalBasis) {
        return {localAxisX, localAxisY, localAxisZ};
    }

    const QQuaternion orientation = QQuaternion::fromEulerAngles(rotation);
    return {orientation.rotatedVector(worldAxisDirectionFor(GizmoAxis::X)).normalized(),
            orientation.rotatedVector(worldAxisDirectionFor(GizmoAxis::Y)).normalized(),
            orientation.rotatedVector(worldAxisDirectionFor(GizmoAxis::Z)).normalized()};
}

QVector3D axisDirectionFor(GizmoAxis axis, const GizmoBasis& basis) {
    switch (axis) {
    case GizmoAxis::X: return basis.x;
    case GizmoAxis::Y: return basis.y;
    case GizmoAxis::Z: return basis.z;
    default: return {};
    }
}

GizmoOperation operationForMode(GizmoMode mode) {
    switch (mode) {
    case GizmoMode::Move: return GizmoOperation::Translate;
    case GizmoMode::Rotate: return GizmoOperation::Rotate;
    case GizmoMode::Scale: return GizmoOperation::Scale;
    case GizmoMode::Full: break;
    }
    return GizmoOperation::None;
}

bool isPlaneHandle(GizmoAxis axis) {
    return axis == GizmoAxis::XY || axis == GizmoAxis::YZ || axis == GizmoAxis::XZ;
}

struct PlaneHandleFrame {
    QVector3D u;
    QVector3D v;
    QVector3D normal;
};

PlaneHandleFrame planeHandleFrameFor(GizmoAxis axis, const GizmoBasis& basis) {
    switch (axis) {
    case GizmoAxis::XY: {
        const QVector3D u = axisDirectionFor(GizmoAxis::X, basis);
        const QVector3D v = axisDirectionFor(GizmoAxis::Y, basis);
        return {u, v, QVector3D::crossProduct(u, v).normalized()};
    }
    case GizmoAxis::XZ: {
        const QVector3D u = axisDirectionFor(GizmoAxis::X, basis);
        const QVector3D v = axisDirectionFor(GizmoAxis::Z, basis);
        return {u, v, QVector3D::crossProduct(u, v).normalized()};
    }
    case GizmoAxis::YZ: {
        const QVector3D u = axisDirectionFor(GizmoAxis::Y, basis);
        const QVector3D v = axisDirectionFor(GizmoAxis::Z, basis);
        return {u, v, QVector3D::crossProduct(u, v).normalized()};
    }
    default:
        return {QVector3D(), QVector3D(), QVector3D()};
    }
}

struct PlaneHandleGeometry {
    PlaneHandleFrame frame;
    QVector3D corner;
    float size = 0.0f;
};

PlaneHandleGeometry planeHandleGeometryFor(GizmoAxis axis, const QVector3D& center,
                                           float scale, const GizmoBasis& basis) {
    const PlaneHandleFrame frame = planeHandleFrameFor(axis, basis);
    const float inset = std::max(scale * kPlaneHandleInsetScale,
                                 kMinPlaneHandleInset);
    const float size = std::max(scale * kPlaneHandleSizeScale,
                                kMinPlaneHandleSize);
    return {frame, center + frame.u * inset + frame.v * inset, size};
}

bool hitPlaneHandle(const Ray& ray,
                    GizmoAxis axis,
                    const QVector3D& center,
                    float scale,
                    const GizmoBasis& basis,
                    QVector3D& hitPoint,
                    float& depthOut) {
    const PlaneHandleGeometry geom = planeHandleGeometryFor(axis, center, scale, basis);
    if (geom.frame.normal.isNull()) {
        return false;
    }

    const float denom = QVector3D::dotProduct(geom.frame.normal, ray.direction);
    if (std::abs(denom) < kIntersectionEpsilon) {
        return false;
    }

    const float t = QVector3D::dotProduct(center - ray.origin, geom.frame.normal) / denom;
    if (t < 0.0f) {
        return false;
    }

    hitPoint = ray.origin + ray.direction * t;
    const QVector3D rel = hitPoint - geom.corner;
    const float u = QVector3D::dotProduct(rel, geom.frame.u);
    const float v = QVector3D::dotProduct(rel, geom.frame.v);
    if (u < 0.0f || v < 0.0f || u > geom.size || v > geom.size) {
        return false;
    }

    depthOut = t;
    return true;
}

FloatColor planeBaseColorFor(GizmoAxis axis) {
    switch (axis) {
    case GizmoAxis::XY:
        return {0.28f, 0.48f, 1.0f, 1.0f};
    case GizmoAxis::XZ:
        return {0.30f, 0.82f, 0.32f, 1.0f};
    case GizmoAxis::YZ:
        return {0.92f, 0.22f, 0.18f, 1.0f};
    default:
        return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

FloatColor axisBaseColorFor(GizmoAxis axis) {
    switch (axis) {
    case GizmoAxis::X:
        return {0.92f, 0.22f, 0.18f, 1.0f};
    case GizmoAxis::Y:
        return {0.30f, 0.82f, 0.32f, 1.0f};
    case GizmoAxis::Z:
        return {0.28f, 0.48f, 1.0f, 1.0f};
    default:
        return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

QVector3D cameraForwardForView(const QMatrix4x4& view) {
    bool invertible = false;
    const QMatrix4x4 inverse = view.inverted(&invertible);
    if (!invertible) {
        return QVector3D(0.0f, 0.0f, 1.0f);
    }
    QVector3D forward(inverse(0, 2), inverse(1, 2), inverse(2, 2));
    if (forward.lengthSquared() < 1.0e-6f) {
        return QVector3D(0.0f, 0.0f, 1.0f);
    }
    return forward.normalized();
}

std::pair<QVector3D, QVector3D> ringBasisForNormal(const QVector3D& normal) {
    QVector3D n = normal.normalized();
    if (n.lengthSquared() < 1.0e-6f) {
        n = QVector3D(0.0f, 0.0f, 1.0f);
    }
    QVector3D reference = std::abs(n.y()) < 0.85f
        ? QVector3D(0.0f, 1.0f, 0.0f)
        : QVector3D(1.0f, 0.0f, 0.0f);
    QVector3D tangent = QVector3D::crossProduct(reference, n);
    if (tangent.lengthSquared() < 1.0e-6f) {
        tangent = QVector3D::crossProduct(QVector3D(0.0f, 0.0f, 1.0f), n);
    }
    tangent.normalize();
    return {tangent, QVector3D::crossProduct(n, tangent).normalized()};
}

float rayPointDistance(const Ray &ray, const QVector3D &point) {
    const float t = QVector3D::dotProduct(point - ray.origin, ray.direction);
    const QVector3D closest = ray.origin + ray.direction * t;
    return (closest - point).length();
}

QVector3D axisHandleEndFor(GizmoAxis axis, const QVector3D& center, float scale,
                          const GizmoBasis& basis) {
    const QVector3D dir = axisDirectionFor(axis, basis);
    const float length = axis == GizmoAxis::Z ? scale * 1.12f : scale * 1.16f;
    return center + dir * length;
}

QVector3D scaleAxisHandleEndFor(GizmoAxis axis, const QVector3D& center, float scale,
                               const GizmoBasis& basis) {
    const QVector3D dir = axisDirectionFor(axis, basis);
    return center + dir * (scale * kScaleHandleLength);
}

float axisHandleHitThreshold(float scale) {
    return std::max(scale * kAxisHitThresholdScale,
                    kMinAxisHitThreshold);
}

float axisHandleTipRadius(float scale) {
    return std::max(scale * kAxisTipRadiusScale, kMinAxisTipRadius);
}

} // namespace

Artifact3DGizmo::Artifact3DGizmo(QObject* parent)
    : QObject(parent), impl_(std::make_unique<Impl>()) {
}

Artifact3DGizmo::~Artifact3DGizmo() = default;

void Artifact3DGizmo::setMode(GizmoMode mode) {
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    activeAxis_ = GizmoAxis::None;
    hoverAxis_ = GizmoAxis::None;
    activeOperation_ = GizmoOperation::None;
    hoverOperation_ = GizmoOperation::None;
    hoverAxisDirectionSign_ = 1.0f;
    hoverScaleAxes_ = QVector3D();
    hoverScaleSigns_ = QVector3D(1.0f, 1.0f, 1.0f);
    fullModeDrag_ = false;
}

void Artifact3DGizmo::setTransform(const QVector3D& position, const QVector3D& rotation) {
    const auto finiteOr = [](float value, float fallback) {
        return std::isfinite(value) ? value : fallback;
    };
    impl_->position = QVector3D(
        finiteOr(position.x(), 0.0f), finiteOr(position.y(), 0.0f),
        finiteOr(position.z(), 0.0f));
    impl_->rotation = QVector3D(
        finiteOr(rotation.x(), 0.0f), finiteOr(rotation.y(), 0.0f),
        finiteOr(rotation.z(), 0.0f));
}

void Artifact3DGizmo::resetState() {
    impl_->position = QVector3D();
    impl_->rotation = QVector3D();
    impl_->scale = QVector3D(1.0f, 1.0f, 1.0f);
    impl_->localAxisX = QVector3D(1.0f, 0.0f, 0.0f);
    impl_->localAxisY = QVector3D(0.0f, -1.0f, 0.0f);
    impl_->localAxisZ = QVector3D(0.0f, 0.0f, 1.0f);
    impl_->viewAxisX = QVector3D(1.0f, 0.0f, 0.0f);
    impl_->viewAxisY = QVector3D(0.0f, -1.0f, 0.0f);
    impl_->viewAxisZ = QVector3D(0.0f, 0.0f, 1.0f);
    impl_->hasLocalBasis = false;
    impl_->currentScale = 1.0f;
    impl_->boundingBoxDrag = false;
    impl_->numericInputActive = false;
    impl_->numericPlanarScale = false;
    impl_->drawingFullOverlay = false;
    impl_->testingFullOverlay = false;
    activeAxis_ = GizmoAxis::None;
    hoverAxis_ = GizmoAxis::None;
    activeOperation_ = GizmoOperation::None;
    hoverOperation_ = GizmoOperation::None;
    hoverAxisDirectionSign_ = 1.0f;
    hoverScaleAxes_ = QVector3D();
    hoverScaleSigns_ = QVector3D(1.0f, 1.0f, 1.0f);
    fullModeDrag_ = false;
    clearBoundingBox();
}

void Artifact3DGizmo::setLocalBasis(const QVector3D& xAxis,
                                    const QVector3D& yAxis,
                                    const QVector3D& zAxis) {
    if (xAxis.lengthSquared() < kIntersectionEpsilon ||
        yAxis.lengthSquared() < kIntersectionEpsilon ||
        zAxis.lengthSquared() < kIntersectionEpsilon) {
        impl_->hasLocalBasis = false;
        return;
    }
    impl_->localAxisX = xAxis.normalized();
    impl_->localAxisY = yAxis.normalized();
    impl_->localAxisZ = zAxis.normalized();
    impl_->hasLocalBasis = true;
}

void Artifact3DGizmo::setViewBasis(const QVector3D& xAxis,
                                   const QVector3D& yAxis,
                                   const QVector3D& zAxis) {
    const auto normalizedOr = [](const QVector3D& axis,
                                 const QVector3D& fallback) {
        return axis.lengthSquared() > kIntersectionEpsilon
            ? axis.normalized()
            : fallback;
    };
    impl_->viewAxisX = normalizedOr(xAxis, QVector3D(1.0f, 0.0f, 0.0f));
    impl_->viewAxisY = normalizedOr(yAxis, QVector3D(0.0f, -1.0f, 0.0f));
    impl_->viewAxisZ = normalizedOr(zAxis, QVector3D(0.0f, 0.0f, 1.0f));
}

QVector3D Artifact3DGizmo::position() const {
    return impl_->position;
}

QVector3D Artifact3DGizmo::rotation() const {
    return impl_->rotation;
}

void Artifact3DGizmo::setScale(const QVector3D &scale) {
    const auto sanitizeScale = [](float value) {
        if (!std::isfinite(value)) return 1.0f;
        if (std::abs(value) >= kMinimumScale) return value;
        return (value < 0.0f ? -1.0f : 1.0f) * kMinimumScale;
    };
    impl_->scale = QVector3D(sanitizeScale(scale.x()), sanitizeScale(scale.y()),
                             sanitizeScale(scale.z()));
}

QVector3D Artifact3DGizmo::scale() const {
    return impl_->scale;
}

void Artifact3DGizmo::setBoundingBox(const QVector3D& minBounds,
                                     const QVector3D& maxBounds) {
    const auto finite = [](float value) { return std::isfinite(value); };
    if (!finite(minBounds.x()) || !finite(minBounds.y()) ||
        !finite(minBounds.z()) || !finite(maxBounds.x()) ||
        !finite(maxBounds.y()) || !finite(maxBounds.z()) ||
        maxBounds.x() <= minBounds.x() || maxBounds.y() <= minBounds.y() ||
        maxBounds.z() <= minBounds.z()) {
        clearBoundingBox();
        return;
    }
    boundingBoxMin_ = minBounds;
    boundingBoxMax_ = maxBounds;
    boundingBoxEnabled_ = true;
}

void Artifact3DGizmo::clearBoundingBox() {
    boundingBoxEnabled_ = false;
    boundingBoxMin_ = QVector3D();
    boundingBoxMax_ = QVector3D();
    hoverAxisDirectionSign_ = 1.0f;
    hoverScaleAxes_ = QVector3D();
    hoverScaleSigns_ = QVector3D(1.0f, 1.0f, 1.0f);
}

GizmoAxis Artifact3DGizmo::hitTest(const Ray& ray, const QMatrix4x4& view, const QMatrix4x4& proj) {
    (void)proj;
    if (mode_ == GizmoMode::Full) {
        // Rings are visually distinct and get priority, followed by scale
        // handles and finally translation handles. The screen-rotation ring
        // is deliberately omitted from Full because the axis rings already
        // provide rotation handles there.
        const GizmoMode savedMode = mode_;
        const bool savedTestingFullOverlay = impl_->testingFullOverlay;
        impl_->testingFullOverlay = true;
        const GizmoMode candidates[] = {GizmoMode::Rotate, GizmoMode::Scale, GizmoMode::Move};
        for (const GizmoMode candidate : candidates) {
            mode_ = candidate;
            const GizmoAxis hit = hitTest(ray, view, proj);
            if (hit != GizmoAxis::None) {
                const GizmoOperation operation = operationForMode(candidate);
                mode_ = savedMode;
                impl_->testingFullOverlay = savedTestingFullOverlay;
                hoverAxis_ = hit;
                hoverOperation_ = operation;
                return hit;
            }
        }
        mode_ = savedMode;
        impl_->testingFullOverlay = savedTestingFullOverlay;
        hoverAxis_ = GizmoAxis::None;
        hoverOperation_ = GizmoOperation::None;
        return GizmoAxis::None;
    }
    float threshold = 0.12f * impl_->currentScale;
    float minDistance = std::numeric_limits<float>::max();
    GizmoAxis result = GizmoAxis::None;
    const GizmoBasis basis = gizmoBasisFor(
        impl_->rotation, space_, impl_->localAxisX, impl_->localAxisY,
        impl_->localAxisZ, impl_->hasLocalBasis, impl_->viewAxisX,
        impl_->viewAxisY, impl_->viewAxisZ);
    const QVector3D interactionCenter = interactionCenterFor(
        boundingBoxMin_, boundingBoxMax_, boundingBoxEnabled_, impl_->position,
        impl_->scale, basis);

    hoverAxisDirectionSign_ = 1.0f;
    hoverScaleAxes_ = QVector3D();
    hoverScaleSigns_ = QVector3D(1.0f, 1.0f, 1.0f);
    if (boundingBoxEnabled_ && mode_ == GizmoMode::Scale &&
        (impl_->testingFullOverlay || fullModeDrag_)) {
        const BoundingBoxGeometry geometry = boundingBoxGeometryFor(
            boundingBoxMin_, boundingBoxMax_, impl_->position, impl_->scale,
            basis);
        const BoundingBoxHit boxHit = hitBoundingBoxHandle(
            ray, geometry,
            std::max(impl_->currentScale * 0.075f, 7.0f), depthEnabled_);
        if (boxHit.axis != GizmoAxis::None) {
            hoverAxis_ = boxHit.axis;
            hoverOperation_ = GizmoOperation::Scale;
            hoverScaleAxes_ = boxHit.scaleAxes;
            hoverScaleSigns_ = boxHit.scaleSigns;
            hoverAxisDirectionSign_ = boxHit.scaleSigns.x();
            return boxHit.axis;
        }
    }

    if (mode_ == GizmoMode::Move) {
        const bool dedicatedMoveHit =
            !impl_->testingFullOverlay && !fullModeDrag_;
        auto checkPlane = [&](GizmoAxis axis) {
            if (result != GizmoAxis::None) {
                return;
            }
            if (!depthEnabled_ && axis != GizmoAxis::XY) {
                return;
            }
            QVector3D hit;
            float depth = std::numeric_limits<float>::max();
            if (hitPlaneHandle(ray, axis, interactionCenter, impl_->currentScale,
                               basis, hit, depth)) {
                result = axis;
                minDistance = depth;
            }
        };

        checkPlane(GizmoAxis::XY);
        checkPlane(GizmoAxis::XZ);
        checkPlane(GizmoAxis::YZ);

        if (result == GizmoAxis::None) {
            if (dedicatedMoveHit) {
                const float centerDist =
                    rayPointDistance(ray, interactionCenter);
                const float centerHitRadius = std::max(
                    axisHandleTipRadius(impl_->currentScale),
                    impl_->currentScale * 0.12f);
                if (centerDist < centerHitRadius) {
                    minDistance = centerDist;
                    result = GizmoAxis::Screen;
                }
            }
            auto checkAxis = [&](const QVector3D& axisDir, GizmoAxis axis) {
                Q_UNUSED(axisDir);
                if (result != GizmoAxis::None) {
                    return;
                }
                if (!depthEnabled_ && axis == GizmoAxis::Z) {
                    return;
                }
                float t;
                const QVector3D end = axisHandleEndFor(
                    axis, interactionCenter, impl_->currentScale, basis);
                const float lineDist = impl_->rayLineDistance(ray.origin, ray.direction,
                                                              interactionCenter, end, t);
                const float tipDist = rayPointDistance(ray, end);
                const float dist = std::min(lineDist, tipDist);
                if (dist < axisHandleHitThreshold(impl_->currentScale) && dist < minDistance) {
                    minDistance = dist;
                    result = axis;
                }
            };

            checkAxis(axisDirectionFor(GizmoAxis::X, basis), GizmoAxis::X);
            checkAxis(axisDirectionFor(GizmoAxis::Y, basis), GizmoAxis::Y);
            checkAxis(axisDirectionFor(GizmoAxis::Z, basis), GizmoAxis::Z);

            if (!dedicatedMoveHit) {
                const float centerDist =
                    rayPointDistance(ray, interactionCenter);
                const float centerHitRadius = std::max(
                    axisHandleTipRadius(impl_->currentScale),
                    impl_->currentScale * 0.12f);
                if (centerDist < centerHitRadius && centerDist < minDistance) {
                    minDistance = centerDist;
                    result = GizmoAxis::Screen;
                }
            }
        }
    } else if (mode_ == GizmoMode::Scale) {
        const bool dedicatedScaleHit =
            !impl_->testingFullOverlay && !fullModeDrag_;
        auto checkPlane = [&](GizmoAxis axis) {
            if (result != GizmoAxis::None) {
                return;
            }
            if (!depthEnabled_ && axis != GizmoAxis::XY) {
                return;
            }
            QVector3D hit;
            float depth = std::numeric_limits<float>::max();
            if (hitPlaneHandle(ray, axis, interactionCenter, impl_->currentScale,
                               basis, hit, depth)) {
                result = axis;
                minDistance = depth;
            }
        };

        checkPlane(GizmoAxis::XY);
        checkPlane(GizmoAxis::XZ);
        checkPlane(GizmoAxis::YZ);

        if (result == GizmoAxis::None && dedicatedScaleHit) {
            const float centerDist = rayPointDistance(ray, interactionCenter);
            if (centerDist < std::max(
                    impl_->currentScale * kDedicatedScaleCenterHitScale,
                    kMinAxisTipRadius)) {
                minDistance = centerDist;
                result = GizmoAxis::Screen;
            }
        }

        auto checkAxis = [&](const QVector3D& axisDir, GizmoAxis axis) {
            Q_UNUSED(axisDir);
            if (result != GizmoAxis::None) {
                return;
            }
            if (!depthEnabled_ && axis == GizmoAxis::Z) {
                return;
            }
            float t;
            const QVector3D end = scaleAxisHandleEndFor(
                axis, interactionCenter, impl_->currentScale, basis);
            const float lineDist = impl_->rayLineDistance(ray.origin, ray.direction,
                                                          interactionCenter, end, t);
            const float tipDist = rayPointDistance(ray, end);
            const float dist = std::min(lineDist, tipDist);
            if (dist < axisHandleHitThreshold(impl_->currentScale) && dist < minDistance) {
                minDistance = dist;
                result = axis;
            }
        };

        checkAxis(axisDirectionFor(GizmoAxis::X, basis), GizmoAxis::X);
        checkAxis(axisDirectionFor(GizmoAxis::Y, basis), GizmoAxis::Y);
        checkAxis(axisDirectionFor(GizmoAxis::Z, basis), GizmoAxis::Z);

        QVector3D scaleRingHit;
        const QVector3D cameraForward = cameraForwardForView(view);
        if (!dedicatedScaleHit && result == GizmoAxis::None &&
            impl_->intersectRayPlane(ray, interactionCenter,
                                     cameraForward, scaleRingHit)) {
            const float ringDistance = (scaleRingHit - interactionCenter).length();
            if (std::abs(ringDistance - impl_->currentScale * kUniformScaleRadiusScale) <
                    axisHandleHitThreshold(impl_->currentScale)) {
                result = GizmoAxis::Screen;
            }
        }
    }
    else if (mode_ == GizmoMode::Rotate) {
        const bool dedicatedRotateHit =
            !impl_->testingFullOverlay && !fullModeDrag_;
        auto checkRing = [&](const QVector3D& planeNormal, GizmoAxis axis) {
            if (!depthEnabled_ && (axis == GizmoAxis::X || axis == GizmoAxis::Y)) {
                return;
            }
            QVector3D hit;
            float rayDepth = std::numeric_limits<float>::max();
            if (impl_->intersectRayPlane(ray, interactionCenter, planeNormal, hit,
                                         &rayDepth)) {
                float distToCenter = (hit - interactionCenter).length();
                const float ringRadius = impl_->currentScale *
                    (axis == GizmoAxis::Screen
                         ? (dedicatedRotateHit
                                ? kDedicatedRotateScreenRadiusScale
                                : kRotateScreenRadiusScale)
                         : (dedicatedRotateHit
                                ? kDedicatedRotateAxisRadiusScale
                                : kRotateAxisRadiusScale));
                if (std::abs(distToCenter - ringRadius) < threshold) {
                    if (rayDepth < minDistance) {
                        minDistance = rayDepth;
                        result = axis;
                    }
                }
            }
        };
        checkRing(axisDirectionFor(GizmoAxis::X, basis), GizmoAxis::X);
        checkRing(axisDirectionFor(GizmoAxis::Y, basis), GizmoAxis::Y);
        checkRing(axisDirectionFor(GizmoAxis::Z, basis), GizmoAxis::Z);
        if (!impl_->testingFullOverlay) {
            checkRing(cameraForwardForView(view), GizmoAxis::Screen);
        }
    }

    hoverAxis_ = result;
    hoverOperation_ = result == GizmoAxis::None ? GizmoOperation::None
                                                : operationForMode(mode_);
    return result;
}

void Artifact3DGizmo::beginDrag(GizmoAxis axis, const Ray& ray,
                                 float axisDirectionSign) {
    impl_->numericInputActive = false;
    impl_->numericPlanarScale = false;
    impl_->boundingBoxDrag = false;
    impl_->dragScaleAxes = QVector3D();
    impl_->dragScaleSigns = QVector3D(1.0f, 1.0f, 1.0f);
    if (mode_ == GizmoMode::Full) {
        fullModeDrag_ = true;
        switch (hoverOperation_) {
        case GizmoOperation::Rotate: mode_ = GizmoMode::Rotate; break;
        case GizmoOperation::Scale: mode_ = GizmoMode::Scale; break;
        case GizmoOperation::Translate: mode_ = GizmoMode::Move; break;
        case GizmoOperation::None:
        default:
            mode_ = GizmoMode::Move;
            break;
        }
    } else {
        fullModeDrag_ = false;
    }
    activeAxis_ = axis;
    activeOperation_ = axis == GizmoAxis::None ? GizmoOperation::None
                                               : operationForMode(mode_);
    impl_->dragStartRay = ray;
    impl_->dragStartPosition = impl_->position;
    impl_->dragStartRotation = impl_->rotation;
    impl_->dragStartScale = impl_->scale;
    impl_->dragReferenceScale = std::max(
        impl_->currentScale, kScaleDivisionEpsilon);
    impl_->firstDrag = true;

    const GizmoBasis basis = gizmoBasisFor(
        impl_->dragStartRotation, space_, impl_->localAxisX,
        impl_->localAxisY, impl_->localAxisZ, impl_->hasLocalBasis,
        impl_->viewAxisX, impl_->viewAxisY, impl_->viewAxisZ);
    impl_->dragStartInteractionCenter = interactionCenterFor(
        boundingBoxMin_, boundingBoxMax_, boundingBoxEnabled_,
        impl_->dragStartPosition, impl_->dragStartScale, basis);

    QVector3D viewDir = (ray.origin - impl_->dragStartInteractionCenter).normalized();
    if (viewDir.lengthSquared() < 0.01f) {
        viewDir = QVector3D(0.0f, 0.0f, 1.0f);
    }
    impl_->dragPlaneNormal = viewDir;
    QVector3D axisDir = axis == GizmoAxis::Screen
        ? viewDir
        : axisDirectionFor(axis, basis);
    if ((mode_ == GizmoMode::Move || mode_ == GizmoMode::Scale) &&
        axisDirectionSign < 0.0f) {
        axisDir = -axisDir;
    }
    impl_->dragAxisDirection = axisDir;

    if (isPlaneHandle(axis)) {
        const auto frame = planeHandleFrameFor(axis, basis);
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->dragStartInteractionCenter,
                                     frame.normal, hit)) {
            impl_->dragStartHitPoint = hit;
        } else {
            impl_->dragStartHitPoint = impl_->dragStartInteractionCenter;
        }
        return;
    }

    if (mode_ == GizmoMode::Move && axis == GizmoAxis::Screen) {
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->dragStartInteractionCenter,
                                     impl_->dragPlaneNormal, hit)) {
            impl_->dragStartHitPoint = hit;
        } else {
            impl_->dragStartHitPoint = impl_->dragStartInteractionCenter;
        }
        return;
    }

    if (mode_ == GizmoMode::Rotate) {
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->dragStartInteractionCenter,
                                     axisDir, hit)) {
            QVector3D dir = (hit - impl_->dragStartInteractionCenter).normalized();
            const auto [tangent, bitangent] = ringBasisForNormal(axisDir);
            
            float x = QVector3D::dotProduct(dir, tangent);
            float y = QVector3D::dotProduct(dir, bitangent);
            impl_->dragStartAngle = std::atan2(y, x) * 180.0f / M_PI;
        }
    } else {
        QVector3D planeNormal;
        if (mode_ == GizmoMode::Scale && axis == GizmoAxis::Screen) {
            planeNormal = impl_->dragPlaneNormal;
        } else {
            planeNormal = QVector3D::crossProduct(
                QVector3D::crossProduct(impl_->dragPlaneNormal, axisDir),
                axisDir).normalized();
            if (planeNormal.lengthSquared() < 0.01f) {
                QVector3D other = (std::abs(axisDir.y()) > 0.9f) ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
                planeNormal = QVector3D::crossProduct(axisDir, other).normalized();
            }
        }
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->dragStartInteractionCenter,
                                     planeNormal, hit)) {
            impl_->dragStartHitPoint = hit;
            if (mode_ == GizmoMode::Scale && axis != GizmoAxis::Screen) {
                const QVector3D referenceOrigin = fullModeDrag_
                    ? impl_->dragStartPosition
                    : impl_->dragStartInteractionCenter;
                impl_->dragReferenceScale = std::max(
                    std::abs(QVector3D::dotProduct(
                        hit - referenceOrigin, axisDir)),
                    kScaleDivisionEpsilon);
            }
        } else {
            impl_->dragStartHitPoint = impl_->dragStartInteractionCenter;
        }
    }
}

void Artifact3DGizmo::beginDrag(GizmoAxis axis, const Ray& ray,
                                const QVector3D& scaleSigns) {
    beginDrag(axis, ray, 1.0f);
    // The controller uses this overload for every 3D hit. Enter the legacy
    // bounding-box resize path only when hitTest() actually selected one of
    // its handles; dedicated Scale axes keep hoverScaleAxes_ empty and must
    // use the regular axis/plane drag math initialized above.
    if (!boundingBoxEnabled_ || mode_ != GizmoMode::Scale ||
        hoverScaleAxes_.lengthSquared() < kIntersectionEpsilon) {
        return;
    }

    QVector3D scaleAxes = hoverScaleAxes_;
    switch (axis) {
    case GizmoAxis::X:
        scaleAxes = QVector3D(1.0f, 0.0f, 0.0f);
        break;
    case GizmoAxis::Y:
        scaleAxes = QVector3D(0.0f, 1.0f, 0.0f);
        break;
    case GizmoAxis::Z:
        scaleAxes = QVector3D(0.0f, 0.0f, 1.0f);
        break;
    case GizmoAxis::XY:
        scaleAxes = QVector3D(1.0f, 1.0f, 0.0f);
        break;
    case GizmoAxis::YZ:
        scaleAxes = QVector3D(0.0f, 1.0f, 1.0f);
        break;
    case GizmoAxis::XZ:
        scaleAxes = QVector3D(1.0f, 0.0f, 1.0f);
        break;
    case GizmoAxis::Screen:
        if (hoverAxis_ != GizmoAxis::Screen ||
            hoverOperation_ != GizmoOperation::Scale ||
            scaleAxes.lengthSquared() < kIntersectionEpsilon) {
            scaleAxes = QVector3D(1.0f, 1.0f, 1.0f);
        }
        break;
    case GizmoAxis::None:
        return;
    }
    if (scaleAxes.lengthSquared() < kIntersectionEpsilon) {
        return;
    }

    const GizmoBasis basis = gizmoBasisFor(
        impl_->dragStartRotation, space_, impl_->localAxisX,
        impl_->localAxisY, impl_->localAxisZ, impl_->hasLocalBasis,
        impl_->viewAxisX, impl_->viewAxisY, impl_->viewAxisZ);
    impl_->boundingBoxDrag = true;
    impl_->dragScaleAxes = scaleAxes;
    impl_->dragScaleSigns = scaleSigns;
    impl_->dragStartBoundingBoxHandlePoint = boundingBoxHandlePointFor(
        boundingBoxMin_, boundingBoxMax_, impl_->dragStartPosition,
        impl_->dragStartScale, basis, scaleAxes, scaleSigns);
    impl_->dragPlaneNormal =
        (ray.origin - impl_->dragStartBoundingBoxHandlePoint).normalized();
    if (impl_->dragPlaneNormal.lengthSquared() < 0.01f) {
        impl_->dragPlaneNormal = QVector3D(0.0f, 0.0f, 1.0f);
    }
    QVector3D hit;
    if (impl_->intersectRayPlane(ray, impl_->dragStartBoundingBoxHandlePoint,
                                 impl_->dragPlaneNormal, hit)) {
        impl_->dragStartHitPoint = hit;
    } else {
        impl_->dragStartHitPoint = impl_->dragStartBoundingBoxHandlePoint;
    }
}

void Artifact3DGizmo::updateDrag(const Ray& ray) {
    if (activeAxis_ == GizmoAxis::None) return;
    
    const QVector3D axisDir = impl_->dragAxisDirection;

    const auto applyBoundingBoxScale = [&](const QVector3D& delta,
                                           bool numericInput,
                                           float numericFactor) {
        const GizmoBasis basis = gizmoBasisFor(
            impl_->dragStartRotation, space_, impl_->localAxisX,
            impl_->localAxisY, impl_->localAxisZ, impl_->hasLocalBasis,
            impl_->viewAxisX, impl_->viewAxisY, impl_->viewAxisZ);
        const QVector3D basisAxes[] = {basis.x, basis.y, basis.z};
        const float startDimensions[] = {
            std::abs((boundingBoxMax_.x() - boundingBoxMin_.x()) *
                     impl_->dragStartScale.x()),
            std::abs((boundingBoxMax_.y() - boundingBoxMin_.y()) *
                     impl_->dragStartScale.y()),
            std::abs((boundingBoxMax_.z() - boundingBoxMin_.z()) *
                     impl_->dragStartScale.z())};
        QVector3D factors(1.0f, 1.0f, 1.0f);
        for (int i = 0; i < 3; ++i) {
            if (impl_->dragScaleAxes[i] <= 0.5f) {
                continue;
            }
            const QVector3D axis = basisAxes[i].normalized();
            const float halfDimension = std::max(
                startDimensions[i] * 0.5f, kScaleDivisionEpsilon);
            float factor = numericInput
                ? numericFactor
                : 1.0f + QVector3D::dotProduct(
                    delta, axis * impl_->dragScaleSigns[i]) / halfDimension;
            if (!numericInput && fineAdjustment_) {
                factor = 1.0f + (factor - 1.0f) * 0.1f;
            }
            factors[i] = std::max(kMinimumScale, factor);
        }

        // Shift on a corner/edge keeps the affected dimensions proportional.
        if (!numericInput && fineAdjustment_ &&
            impl_->dragScaleAxes.x() + impl_->dragScaleAxes.y() +
                    impl_->dragScaleAxes.z() > 1.5f) {
            float uniformFactor = 1.0f;
            float largestDelta = -1.0f;
            for (int i = 0; i < 3; ++i) {
                if (impl_->dragScaleAxes[i] <= 0.5f) continue;
                const float distance = std::abs(factors[i] - 1.0f);
                if (distance > largestDelta) {
                    largestDelta = distance;
                    uniformFactor = factors[i];
                }
            }
            for (int i = 0; i < 3; ++i) {
                if (impl_->dragScaleAxes[i] > 0.5f) factors[i] = uniformFactor;
            }
        }

        const QVector3D localCenter = (boundingBoxMin_ + boundingBoxMax_) * 0.5f;
        const QVector3D localHalf = (boundingBoxMax_ - boundingBoxMin_) * 0.5f;
        QVector3D newScale = impl_->dragStartScale;
        QVector3D centerShift;
        for (int i = 0; i < 3; ++i) {
            if (impl_->dragScaleAxes[i] <= 0.5f) continue;
            const float start = impl_->dragStartScale[i];
            float resolved = start * factors[i];
            if (snapEnabled_) {
                resolved = snapInteractionValue(resolved, kScaleSnap, true);
            }
            const float sign = std::abs(resolved) > kMinimumScale
                ? (resolved < 0.0f ? -1.0f : 1.0f)
                : (start < 0.0f ? -1.0f : 1.0f);
            newScale[i] = sign * std::max(kMinimumScale, std::abs(resolved));
            // Keep the opposite local face fixed. This also handles meshes
            // whose bounds are not centered around the layer transform origin.
            const float fixedLocal = localCenter[i] -
                localHalf[i] * impl_->dragScaleSigns[i];
            centerShift += basisAxes[i].normalized() *
                (fixedLocal * (start - newScale[i]));
        }
        impl_->position = impl_->dragStartPosition + centerShift;
        impl_->scale = newScale;
    };

    if (impl_->numericInputActive) {
        const float value = impl_->numericInput;
        if (mode_ == GizmoMode::Move) {
            QVector3D direction = axisDir;
            if (activeAxis_ == GizmoAxis::Screen) {
                direction = impl_->position - impl_->dragStartPosition;
                if (direction.lengthSquared() <= kIntersectionEpsilon) {
                    const GizmoBasis basis = gizmoBasisFor(
                        impl_->dragStartRotation, space_, impl_->localAxisX,
                        impl_->localAxisY, impl_->localAxisZ,
                        impl_->hasLocalBasis, impl_->viewAxisX,
                        impl_->viewAxisY, impl_->viewAxisZ);
                    direction = basis.x;
                }
                direction.normalize();
            }
            impl_->position = impl_->dragStartPosition + direction * value;
        } else if (mode_ == GizmoMode::Rotate) {
            impl_->rotation = impl_->dragStartRotation;
            if (activeAxis_ == GizmoAxis::X) {
                impl_->rotation.setX(impl_->dragStartRotation.x() + value);
            } else if (activeAxis_ == GizmoAxis::Y) {
                impl_->rotation.setY(impl_->dragStartRotation.y() + value);
            } else if (activeAxis_ == GizmoAxis::Z) {
                impl_->rotation.setZ(impl_->dragStartRotation.z() + value);
            } else {
                impl_->rotation += axisDir * value;
            }
            if (boundingBoxEnabled_ && axisDir.lengthSquared() >
                    kIntersectionEpsilon) {
                const float pivotDelta = snapEnabled_
                    ? snapInteractionValue(value, kRotationSnapDegrees, true)
                    : value;
                const QQuaternion pivotRotation =
                    QQuaternion::fromAxisAndAngle(axisDir.normalized(), pivotDelta);
                impl_->position = impl_->dragStartInteractionCenter +
                    pivotRotation.rotatedVector(
                        impl_->dragStartPosition - impl_->dragStartInteractionCenter);
            }
        } else if (mode_ == GizmoMode::Scale) {
            if (impl_->boundingBoxDrag) {
                applyBoundingBoxScale(QVector3D(), true, value);
                return;
            }
            const auto scaled = [value](float start) {
                const float result = start * value;
                if (std::abs(result) >= kMinimumScale) return result;
                return (result < 0.0f ? -1.0f : 1.0f) * kMinimumScale;
            };
            impl_->scale = impl_->dragStartScale;
            if (activeAxis_ == GizmoAxis::X) {
                impl_->scale.setX(scaled(impl_->dragStartScale.x()));
            } else if (activeAxis_ == GizmoAxis::Y) {
                impl_->scale.setY(scaled(impl_->dragStartScale.y()));
            } else if (activeAxis_ == GizmoAxis::Z) {
                impl_->scale.setZ(scaled(impl_->dragStartScale.z()));
            } else {
                impl_->scale.setX(scaled(impl_->dragStartScale.x()));
                impl_->scale.setY(scaled(impl_->dragStartScale.y()));
                if (depthEnabled_ && !impl_->numericPlanarScale) {
                    impl_->scale.setZ(scaled(impl_->dragStartScale.z()));
                }
            }
        }
        return;
    }

    if (impl_->boundingBoxDrag && mode_ == GizmoMode::Scale) {
        QVector3D hit;
        if (impl_->intersectRayPlane(
                ray, impl_->dragStartBoundingBoxHandlePoint,
                impl_->dragPlaneNormal, hit)) {
            applyBoundingBoxScale(hit - impl_->dragStartHitPoint, false, 1.0f);
        }
        return;
    }

    if (isPlaneHandle(activeAxis_)) {
        const GizmoBasis basis = gizmoBasisFor(
            impl_->dragStartRotation, space_, impl_->localAxisX,
            impl_->localAxisY, impl_->localAxisZ, impl_->hasLocalBasis,
            impl_->viewAxisX, impl_->viewAxisY, impl_->viewAxisZ);
        const auto frame = planeHandleFrameFor(activeAxis_, basis);
        QVector3D hit;
        if (!impl_->intersectRayPlane(ray, impl_->dragStartInteractionCenter,
                                      frame.normal, hit)) {
            return;
        }

        const QVector3D delta = hit - impl_->dragStartHitPoint;
        const QVector3D adjustedDelta = fineAdjustment_ ? delta * 0.1f : delta;
        if (mode_ == GizmoMode::Scale) {
            const float factorU = std::max(
                kMinimumScale,
                1.0f + QVector3D::dotProduct(adjustedDelta, frame.u) /
                           std::max(impl_->currentScale, kScaleDivisionEpsilon));
            const float factorV = std::max(
                kMinimumScale,
                1.0f + QVector3D::dotProduct(adjustedDelta, frame.v) /
                           std::max(impl_->currentScale, kScaleDivisionEpsilon));
            QVector3D newScale = impl_->dragStartScale;
            if (activeAxis_ == GizmoAxis::XY) {
                newScale.setX(std::max(kMinimumScale, impl_->dragStartScale.x() * factorU));
                newScale.setY(std::max(kMinimumScale, impl_->dragStartScale.y() * factorV));
            } else if (activeAxis_ == GizmoAxis::XZ) {
                newScale.setX(std::max(kMinimumScale, impl_->dragStartScale.x() * factorU));
                newScale.setZ(std::max(kMinimumScale, impl_->dragStartScale.z() * factorV));
            } else if (activeAxis_ == GizmoAxis::YZ) {
                newScale.setY(std::max(kMinimumScale, impl_->dragStartScale.y() * factorU));
                newScale.setZ(std::max(kMinimumScale, impl_->dragStartScale.z() * factorV));
            }
            if (snapEnabled_) {
                newScale.setX(std::max(kMinimumScale, snapInteractionValue(newScale.x(), kScaleSnap, true)));
                newScale.setY(std::max(kMinimumScale, snapInteractionValue(newScale.y(), kScaleSnap, true)));
                newScale.setZ(std::max(kMinimumScale, snapInteractionValue(newScale.z(), kScaleSnap, true)));
            }
            impl_->scale = newScale;
        } else {
            impl_->position = impl_->dragStartPosition
                            + frame.u * QVector3D::dotProduct(adjustedDelta, frame.u)
                            + frame.v * QVector3D::dotProduct(adjustedDelta, frame.v);
            if (snapEnabled_) {
                impl_->position.setX(snapInteractionValue(impl_->position.x(), kTranslationSnap, true));
                impl_->position.setY(snapInteractionValue(impl_->position.y(), kTranslationSnap, true));
                impl_->position.setZ(snapInteractionValue(impl_->position.z(), kTranslationSnap, true));
            }
        }
        return;
    }

    if (mode_ == GizmoMode::Move && activeAxis_ == GizmoAxis::Screen) {
        QVector3D hit;
        if (!impl_->intersectRayPlane(ray, impl_->dragStartInteractionCenter,
                                      impl_->dragPlaneNormal, hit)) {
            return;
        }
        const QVector3D delta = fineAdjustment_ ? (hit - impl_->dragStartHitPoint) * 0.1f
                                                : (hit - impl_->dragStartHitPoint);
        impl_->position = impl_->dragStartPosition + delta;
        if (snapEnabled_) {
            impl_->position.setX(snapInteractionValue(impl_->position.x(), kTranslationSnap, true));
            impl_->position.setY(snapInteractionValue(impl_->position.y(), kTranslationSnap, true));
            impl_->position.setZ(snapInteractionValue(impl_->position.z(), kTranslationSnap, true));
        }
        return;
    }

    if (mode_ == GizmoMode::Move) {
        if (axisDir.isNull()) {
            return;
        }
        if (!depthEnabled_ && activeAxis_ == GizmoAxis::Z) {
            return;
        }
        const QVector3D viewDir = impl_->dragPlaneNormal;
        QVector3D planeNormal = QVector3D::crossProduct(
            QVector3D::crossProduct(viewDir, axisDir), axisDir).normalized();
        if (planeNormal.lengthSquared() < 0.01f) {
            QVector3D other = (std::abs(axisDir.y()) > 0.9f) ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
            planeNormal = QVector3D::crossProduct(axisDir, other).normalized();
        }
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->dragStartInteractionCenter,
                                     planeNormal, hit)) {
            float projectT = QVector3D::dotProduct(hit - impl_->dragStartHitPoint, axisDir);
            if (fineAdjustment_) projectT *= 0.1f;
            impl_->position = impl_->dragStartPosition + axisDir * projectT;
            if (snapEnabled_) {
                impl_->position.setX(snapInteractionValue(impl_->position.x(), kTranslationSnap, true));
                impl_->position.setY(snapInteractionValue(impl_->position.y(), kTranslationSnap, true));
                impl_->position.setZ(snapInteractionValue(impl_->position.z(), kTranslationSnap, true));
            }
        }
    }
    else if (mode_ == GizmoMode::Rotate) {
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->dragStartInteractionCenter,
                                     axisDir, hit)) {
            QVector3D dir = (hit - impl_->dragStartInteractionCenter).normalized();
            const auto [tangent, bitangent] = ringBasisForNormal(axisDir);
            
            float x = QVector3D::dotProduct(dir, tangent);
            float y = QVector3D::dotProduct(dir, bitangent);
            float currentAngle = std::atan2(y, x) * 180.0f / M_PI;
            float delta = currentAngle - impl_->dragStartAngle;
            if (fineAdjustment_) delta *= 0.1f;
            
            QVector3D rot = impl_->dragStartRotation;
            if (activeAxis_ == GizmoAxis::X) rot.setX(rot.x() + delta);
            else if (activeAxis_ == GizmoAxis::Y) rot.setY(rot.y() + delta);
            else if (activeAxis_ == GizmoAxis::Z) rot.setZ(rot.z() + delta);
            else if (activeAxis_ == GizmoAxis::Screen) rot += axisDir * delta;
            if (snapEnabled_) {
                rot.setX(snapInteractionValue(rot.x(), kRotationSnapDegrees, true));
                rot.setY(snapInteractionValue(rot.y(), kRotationSnapDegrees, true));
                rot.setZ(snapInteractionValue(rot.z(), kRotationSnapDegrees, true));
            }
            impl_->rotation = rot;
            if (boundingBoxEnabled_ && axisDir.lengthSquared() >
                    kIntersectionEpsilon) {
                const float pivotDelta = snapEnabled_
                    ? snapInteractionValue(delta, kRotationSnapDegrees, true)
                    : delta;
                const QQuaternion pivotRotation =
                    QQuaternion::fromAxisAndAngle(axisDir.normalized(), pivotDelta);
                impl_->position = impl_->dragStartInteractionCenter +
                    pivotRotation.rotatedVector(
                        impl_->dragStartPosition - impl_->dragStartInteractionCenter);
            }
        }
    } else if (mode_ == GizmoMode::Scale) {
        if (!depthEnabled_ && activeAxis_ == GizmoAxis::Z) {
            return;
        }
        const QVector3D viewDir = impl_->dragPlaneNormal;
        QVector3D planeNormal = (activeAxis_ == GizmoAxis::Screen)
            ? viewDir
            : QVector3D::crossProduct(
                  QVector3D::crossProduct(viewDir, axisDir), axisDir).normalized();
        if (planeNormal.lengthSquared() < 0.01f) {
            QVector3D fallback = (std::abs(viewDir.y()) > 0.9f) ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
            planeNormal = (activeAxis_ == GizmoAxis::Screen)
                ? viewDir
                : QVector3D::crossProduct(axisDir, fallback).normalized();
        }

        QVector3D hit;
        if (!impl_->intersectRayPlane(ray, impl_->dragStartInteractionCenter,
                                      planeNormal, hit)) {
            return;
        }

        QVector3D newScale = impl_->dragStartScale;
        const auto clampSignedScale = [](float value, float fallbackSign) {
            if (std::abs(value) >= kMinimumScale) {
                return value;
            }
            const float sign = std::abs(value) > kScaleDivisionEpsilon
                ? (value < 0.0f ? -1.0f : 1.0f)
                : (fallbackSign < 0.0f ? -1.0f : 1.0f);
            return sign * kMinimumScale;
        };
        if (activeAxis_ == GizmoAxis::X) {
            const float delta = QVector3D::dotProduct(hit - impl_->dragStartHitPoint, axisDir);
            const float factor = 1.0f +
                (fineAdjustment_ ? delta * 0.1f : delta) /
                    impl_->dragReferenceScale;
            newScale.setX(clampSignedScale(
                impl_->dragStartScale.x() * factor,
                impl_->dragStartScale.x()));
            if (snapEnabled_) newScale.setX(clampSignedScale(
                snapInteractionValue(newScale.x(), kScaleSnap, true),
                impl_->dragStartScale.x()));
        } else if (activeAxis_ == GizmoAxis::Y) {
            const float delta = QVector3D::dotProduct(hit - impl_->dragStartHitPoint, axisDir);
            const float factor = 1.0f +
                (fineAdjustment_ ? delta * 0.1f : delta) /
                    impl_->dragReferenceScale;
            newScale.setY(clampSignedScale(
                impl_->dragStartScale.y() * factor,
                impl_->dragStartScale.y()));
            if (snapEnabled_) newScale.setY(clampSignedScale(
                snapInteractionValue(newScale.y(), kScaleSnap, true),
                impl_->dragStartScale.y()));
        } else if (activeAxis_ == GizmoAxis::Z) {
            const float delta = QVector3D::dotProduct(hit - impl_->dragStartHitPoint, axisDir);
            const float factor = 1.0f +
                (fineAdjustment_ ? delta * 0.1f : delta) /
                    impl_->dragReferenceScale;
            newScale.setZ(clampSignedScale(
                impl_->dragStartScale.z() * factor,
                impl_->dragStartScale.z()));
            if (snapEnabled_) newScale.setZ(clampSignedScale(
                snapInteractionValue(newScale.z(), kScaleSnap, true),
                impl_->dragStartScale.z()));
        } else if (activeAxis_ == GizmoAxis::Screen) {
            float rawFactor = 1.0f;
            if (!fullModeDrag_) {
                const auto [screenRight, screenUp] =
                    ringBasisForNormal(viewDir);
                const QVector3D dragDirection =
                    (screenRight + screenUp).normalized();
                const float dragDistance = QVector3D::dotProduct(
                    hit - impl_->dragStartHitPoint, dragDirection);
                rawFactor = 1.0f + dragDistance /
                    std::max(impl_->currentScale, kScaleDivisionEpsilon);
            } else {
                const QVector3D startVector =
                    impl_->dragStartHitPoint - impl_->dragStartPosition;
                const QVector3D currentVector =
                    hit - impl_->dragStartPosition;
                const float startLengthSquared = std::max(
                    startVector.lengthSquared(),
                    kScaleDivisionEpsilon * kScaleDivisionEpsilon);
                rawFactor =
                    QVector3D::dotProduct(currentVector, startVector) /
                    startLengthSquared;
            }
            const float factor = fineAdjustment_
                ? 1.0f + (rawFactor - 1.0f) * 0.1f
                : rawFactor;
            newScale.setX(clampSignedScale(
                impl_->dragStartScale.x() * factor,
                impl_->dragStartScale.x()));
            newScale.setY(clampSignedScale(
                impl_->dragStartScale.y() * factor,
                impl_->dragStartScale.y()));
            if (depthEnabled_) {
                newScale.setZ(clampSignedScale(
                    impl_->dragStartScale.z() * factor,
                    impl_->dragStartScale.z()));
            }
            if (snapEnabled_) {
                newScale.setX(clampSignedScale(
                    snapInteractionValue(newScale.x(), kScaleSnap, true),
                    impl_->dragStartScale.x()));
                newScale.setY(clampSignedScale(
                    snapInteractionValue(newScale.y(), kScaleSnap, true),
                    impl_->dragStartScale.y()));
                if (depthEnabled_) {
                    newScale.setZ(clampSignedScale(
                        snapInteractionValue(newScale.z(), kScaleSnap, true),
                        impl_->dragStartScale.z()));
                }
            }
        } else {
            return;
        }
        impl_->scale = newScale;
    }
}

void Artifact3DGizmo::constrainDrag(GizmoAxis axis, const Ray& currentRay) {
    if (activeAxis_ == GizmoAxis::None || axis == GizmoAxis::None ||
        axis == activeAxis_) {
        return;
    }
    const Ray originalStartRay = impl_->dragStartRay;
    const QVector3D originalStartPosition = impl_->dragStartPosition;
    const QVector3D originalStartRotation = impl_->dragStartRotation;
    const QVector3D originalStartScale = impl_->dragStartScale;
    const QVector3D originalScaleSigns = hoverScaleSigns_;
    const bool preserveBoundingBoxScale = impl_->boundingBoxDrag;
    impl_->position = originalStartPosition;
    impl_->rotation = originalStartRotation;
    impl_->scale = originalStartScale;
    activeAxis_ = GizmoAxis::None;
    activeOperation_ = GizmoOperation::None;
    if (preserveBoundingBoxScale) {
        beginDrag(axis, originalStartRay, originalScaleSigns);
    } else {
        beginDrag(axis, originalStartRay);
    }
    updateDrag(currentRay);
}

void Artifact3DGizmo::setNumericInput(float value) {
    if (!std::isfinite(value)) return;
    impl_->numericInput = value;
    impl_->numericInputActive = true;
    impl_->numericPlanarScale = false;
}

void Artifact3DGizmo::setNumericPlanarScaleInput(float factor) {
    if (!std::isfinite(factor)) return;
    impl_->numericInput = factor;
    impl_->numericInputActive = true;
    impl_->numericPlanarScale = true;
}

void Artifact3DGizmo::clearNumericInput() {
    impl_->numericInputActive = false;
    impl_->numericPlanarScale = false;
}

bool Artifact3DGizmo::isBoundingBoxDragging() const {
    return impl_->boundingBoxDrag;
}

void Artifact3DGizmo::endDrag() {
    impl_->numericInputActive = false;
    impl_->numericPlanarScale = false;
    impl_->boundingBoxDrag = false;
    impl_->dragScaleAxes = QVector3D();
    impl_->dragScaleSigns = QVector3D(1.0f, 1.0f, 1.0f);
    activeAxis_ = GizmoAxis::None;
    activeOperation_ = GizmoOperation::None;
    if (fullModeDrag_) {
        mode_ = GizmoMode::Full;
        fullModeDrag_ = false;
    }
}

void Artifact3DGizmo::draw(ArtifactIRenderer* renderer, const QMatrix4x4& view, const QMatrix4x4& proj) {
    if (!renderer) return;

    if (mode_ == GizmoMode::Full) {
        const GizmoMode savedMode = mode_;
        const bool savedDrawingFullOverlay = impl_->drawingFullOverlay;
        impl_->drawingFullOverlay = true;
        const GizmoMode modes[] = {GizmoMode::Move, GizmoMode::Rotate, GizmoMode::Scale};
        for (const GizmoMode mode : modes) {
            mode_ = mode;
            draw(renderer, view, proj);
        }
        mode_ = savedMode;
        impl_->drawingFullOverlay = savedDrawingFullOverlay;
        return;
    }

    const QVector4D viewPos = view * QVector4D(impl_->position, 1.0f);
    const float distance = std::abs(viewPos.z());
    const bool orthographic = std::abs(proj(3, 3) - 1.0f) < 0.001f;
    const float zoom = std::max(renderer->getZoom(), 0.001f);
    const float contrastScale = Accessibility::contrastScale();
    // Keep the manipulator compact while preserving its existing world-space
    // hit-test contract. Orthographic view compensates for viewport zoom;
    // perspective view already derives its screen-space size from distance.
    impl_->currentScale = orthographic
        ? 72.0f / zoom
        : std::max(distance * 0.14f, 0.1f);

    const float s = impl_->currentScale;

    renderer->setUseExternalMatrices(true);
    renderer->setViewMatrix(view);
    renderer->setProjectionMatrix(proj);
    renderer->setGizmoCameraMatrices(view, proj);

    auto clamp01 = [](float v) {
        return std::clamp(v, 0.0f, 1.0f);
    };
    auto tintColor = [&](const FloatColor& baseColor, float boost, float alpha) {
        return FloatColor{
            clamp01(baseColor.r() * boost + 0.04f),
            clamp01(baseColor.g() * boost + 0.04f),
            clamp01(baseColor.b() * boost + 0.04f),
            clamp01(alpha)
        };
    };
    auto getAxisCoreColor = [&](GizmoAxis axis, const FloatColor& baseColor) {
        if (activeAxis_ == axis || hoverAxis_ == axis) {
            return FloatColor{1.0f, 0.72f, 0.12f, 1.0f};
        }
        if (activeAxis_ != GizmoAxis::None) {
            return tintColor(baseColor, 0.58f, 0.42f);
        }
        return tintColor(baseColor, 0.96f, 0.96f);
    };
    auto getAxisShadowColor = [&](GizmoAxis axis, const FloatColor& baseColor) {
        if (activeAxis_ == axis) {
            return FloatColor{0.0f, 0.0f, 0.0f, 0.56f};
        }
        if (hoverAxis_ == axis) {
            return FloatColor{0.0f, 0.0f, 0.0f, 0.42f};
        }
        if (activeAxis_ != GizmoAxis::None) {
            return FloatColor{0.0f, 0.0f, 0.0f, 0.24f};
        }
        (void)baseColor;
        return FloatColor{0.0f, 0.0f, 0.0f, 0.34f};
    };

    const GizmoBasis basis = gizmoBasisFor(
        impl_->rotation, space_, impl_->localAxisX, impl_->localAxisY,
        impl_->localAxisZ, impl_->hasLocalBasis, impl_->viewAxisX,
        impl_->viewAxisY, impl_->viewAxisZ);
    const QVector3D interactionCenter = interactionCenterFor(
        boundingBoxMin_, boundingBoxMax_, boundingBoxEnabled_, impl_->position,
        impl_->scale, basis);
    const Detail::float3 center = {interactionCenter.x(), interactionCenter.y(),
                                   interactionCenter.z()};
    const QVector3D axisX = axisDirectionFor(GizmoAxis::X, basis);
    const QVector3D axisY = axisDirectionFor(GizmoAxis::Y, basis);
    const QVector3D axisZ = axisDirectionFor(GizmoAxis::Z, basis);
    const QVector3D cameraForward = cameraForwardForView(view);
    const auto [cameraRight, cameraUp] = ringBasisForNormal(cameraForward);
    const bool dedicatedMovePass =
        mode_ == GizmoMode::Move && !impl_->drawingFullOverlay &&
        !fullModeDrag_;
    const bool dedicatedRotatePass =
        mode_ == GizmoMode::Rotate && !impl_->drawingFullOverlay &&
        !fullModeDrag_;
    const bool dedicatedScalePass =
        mode_ == GizmoMode::Scale && !impl_->drawingFullOverlay &&
        !fullModeDrag_;

    auto toFloat3 = [](const QVector3D& v) -> Detail::float3 {
        return {v.x(), v.y(), v.z()};
    };

    auto drawPlaneHandle = [&](GizmoAxis axis) {
        if (!depthEnabled_ && axis != GizmoAxis::XY) {
            return;
        }
        const PlaneHandleGeometry geom =
            planeHandleGeometryFor(axis, interactionCenter, s, basis);
        if (geom.frame.normal.isNull()) {
            return;
        }

        const FloatColor baseColor = planeBaseColorFor(axis);
        const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
        const FloatColor coreColor = getAxisCoreColor(axis, baseColor);
        const FloatColor fillColor{
            clamp01(baseColor.r() * 0.72f + 0.10f),
            clamp01(baseColor.g() * 0.72f + 0.10f),
            clamp01(baseColor.b() * 0.72f + 0.10f),
            activeAxis_ == axis
                ? 0.42f
                : (hoverAxis_ == axis ? 0.34f
                                      : ((dedicatedMovePass || dedicatedScalePass)
                                             ? 0.17f
                                             : 0.24f))
        };

        const QVector3D shadowCorner = geom.corner - geom.frame.u * std::max(geom.size * 0.11f, 1.0f) - geom.frame.v * std::max(geom.size * 0.11f, 1.0f);
        const float shadowSize = geom.size * 1.08f;
        const QVector3D shadowP1 = shadowCorner + geom.frame.u * shadowSize;
        const QVector3D shadowP2 = shadowP1 + geom.frame.v * shadowSize;
        const QVector3D shadowP3 = shadowCorner + geom.frame.v * shadowSize;

        const QVector3D coreP0 = geom.corner;
        const QVector3D coreP1 = coreP0 + geom.frame.u * geom.size;
        const QVector3D coreP2 = coreP1 + geom.frame.v * geom.size;
        const QVector3D coreP3 = coreP0 + geom.frame.v * geom.size;

        renderer->draw3DQuad(toFloat3(shadowCorner), toFloat3(shadowP1), toFloat3(shadowP2), toFloat3(shadowP3), shadowColor);
        renderer->draw3DQuad(toFloat3(coreP0), toFloat3(coreP1), toFloat3(coreP2), toFloat3(coreP3), fillColor);
        const float outlineThickness =
            ((dedicatedMovePass || dedicatedScalePass) ? 0.82f : 1.0f) *
            contrastScale;
        renderer->drawGizmoLine(toFloat3(coreP0), toFloat3(coreP1), coreColor, outlineThickness);
        renderer->drawGizmoLine(toFloat3(coreP1), toFloat3(coreP2), coreColor, outlineThickness);
        renderer->drawGizmoLine(toFloat3(coreP2), toFloat3(coreP3), coreColor, outlineThickness);
        renderer->drawGizmoLine(toFloat3(coreP3), toFloat3(coreP0), coreColor, outlineThickness);
    };

    auto drawAxisArrow = [&](GizmoAxis axis,
                             const Detail::float3& start,
                             const Detail::float3& end,
                             float size) {
        const FloatColor baseColor = axisBaseColorFor(axis);
        const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
        const FloatColor coreColor = getAxisCoreColor(axis, baseColor);
        renderer->drawGizmoArrow(start, end, shadowColor, size * 1.18f * contrastScale);
        renderer->drawGizmoArrow(start, end, coreColor, size * contrastScale);
    };
    auto drawMoveCenterHandle = [&]() {
        const float halfSize = s * kMoveCenterHalfSizeScale;
        const float shadowHalfSize = halfSize * 1.18f;
        const auto corner = [&](float right, float up) {
            return interactionCenter + cameraRight * right + cameraUp * up;
        };
        const QVector3D shadow0 = corner(-shadowHalfSize, -shadowHalfSize);
        const QVector3D shadow1 = corner(shadowHalfSize, -shadowHalfSize);
        const QVector3D shadow2 = corner(shadowHalfSize, shadowHalfSize);
        const QVector3D shadow3 = corner(-shadowHalfSize, shadowHalfSize);
        const QVector3D core0 = corner(-halfSize, -halfSize);
        const QVector3D core1 = corner(halfSize, -halfSize);
        const QVector3D core2 = corner(halfSize, halfSize);
        const QVector3D core3 = corner(-halfSize, halfSize);
        const FloatColor baseColor{0.94f, 0.94f, 0.96f, 0.96f};
        const FloatColor coreColor =
            getAxisCoreColor(GizmoAxis::Screen, baseColor);
        const FloatColor fillColor{
            coreColor.r(), coreColor.g(), coreColor.b(),
            activeAxis_ == GizmoAxis::Screen
                ? 0.92f
                : (hoverAxis_ == GizmoAxis::Screen ? 0.82f : 0.68f)};
        renderer->draw3DQuad(toFloat3(shadow0), toFloat3(shadow1),
                             toFloat3(shadow2), toFloat3(shadow3),
                             FloatColor{0.0f, 0.0f, 0.0f, 0.48f});
        renderer->draw3DQuad(toFloat3(core0), toFloat3(core1),
                             toFloat3(core2), toFloat3(core3), fillColor);
        renderer->drawGizmoLine(toFloat3(core0), toFloat3(core1), coreColor,
                                0.9f * contrastScale);
        renderer->drawGizmoLine(toFloat3(core1), toFloat3(core2), coreColor,
                                0.9f * contrastScale);
        renderer->drawGizmoLine(toFloat3(core2), toFloat3(core3), coreColor,
                                0.9f * contrastScale);
        renderer->drawGizmoLine(toFloat3(core3), toFloat3(core0), coreColor,
                                0.9f * contrastScale);
    };
    auto drawAxisRing = [&](GizmoAxis axis,
                            const Detail::float3& centerPos,
                            const Detail::float3& normal,
                            float radius,
                            const FloatColor& baseColor,
                            float thickness = 1.0f) {
        const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
        const FloatColor coreColor = getAxisCoreColor(axis, baseColor);
        renderer->drawGizmoRing(centerPos, normal, radius * 1.02f, shadowColor, thickness * 1.30f * contrastScale);
        renderer->drawGizmoRing(centerPos, normal, radius, coreColor, thickness * contrastScale);
    };

    auto drawRotateRing = [&](GizmoAxis axis,
                              const Detail::float3& centerPos,
                              const Detail::float3& normal,
                              float radius,
                              const FloatColor& baseColor) {
        const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
        const FloatColor coreColor = getAxisCoreColor(axis, baseColor);
        const float tubeRadius =
            s * (dedicatedRotatePass ? 0.014f : 0.018f);
        const float shadowTube = tubeRadius * 1.42f;
        renderer->drawGizmoTorus(centerPos, normal, radius * 1.018f, shadowTube, shadowColor);
        renderer->drawGizmoTorus(centerPos, normal, radius, tubeRadius, coreColor);
    };

    if (boundingBoxEnabled_ && mode_ == GizmoMode::Scale) {
        const BoundingBoxGeometry geometry = boundingBoxGeometryFor(
            boundingBoxMin_, boundingBoxMax_, impl_->position, impl_->scale,
            basis);
        static constexpr int edges[][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
            {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        const FloatColor edgeColor{1.0f, 0.56f, 0.18f, 0.92f};
        const FloatColor edgeShadow{0.02f, 0.03f, 0.04f, 0.74f};
        for (const auto& edge : edges) {
            const QVector3D& start = geometry.corners[edge[0]];
            const QVector3D& end = geometry.corners[edge[1]];
            renderer->drawGizmoLine(toFloat3(start), toFloat3(end),
                                    edgeShadow, 3.0f);
            renderer->drawGizmoLine(toFloat3(start), toFloat3(end),
                                    edgeColor, 1.45f);
        }

        if (mode_ == GizmoMode::Scale || mode_ == GizmoMode::Full) {
            const float handleHalf = std::max(s * 0.045f, 2.5f);
            const auto drawBoxHandle = [&](const QVector3D& point,
                                           GizmoAxis axis,
                                           const QVector3D& signs) {
                const FloatColor baseColor = axis == GizmoAxis::Screen
                    ? FloatColor{0.96f, 0.96f, 0.98f, 0.96f}
                    : axis == GizmoAxis::XY
                          ? FloatColor{0.92f, 0.78f, 0.18f, 0.96f}
                          : axis == GizmoAxis::YZ
                                ? FloatColor{0.22f, 0.84f, 0.86f, 0.96f}
                                : axis == GizmoAxis::XZ
                                      ? FloatColor{0.82f, 0.30f, 0.88f, 0.96f}
                                      : axisBaseColorFor(axis);
                const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
                const bool signMatches =
                    (signs - hoverScaleSigns_).lengthSquared() < 0.01f;
                const bool activeSignMatches =
                    impl_->boundingBoxDrag &&
                    (signs - impl_->dragScaleSigns).lengthSquared() < 0.01f;
                const FloatColor coreColor =
                    (activeSignMatches || (hoverAxis_ == axis && signMatches))
                        ? FloatColor{1.0f, 0.72f, 0.12f, 1.0f}
                        : (activeAxis_ != GizmoAxis::None
                               ? tintColor(baseColor, 0.58f, 0.42f)
                               : tintColor(baseColor, 0.96f, 0.96f));
                // A world-aligned cube becomes a ragged diamond at oblique
                // camera angles.  A camera-facing quad remains a crisp square
                // and reduces each two-pass handle from 72 to 12 vertices.
                const auto drawBillboardSquare = [&](float halfExtent,
                                                      const FloatColor& color) {
                    const QVector3D p0 = point - cameraRight * halfExtent -
                                         cameraUp * halfExtent;
                    const QVector3D p1 = point + cameraRight * halfExtent -
                                         cameraUp * halfExtent;
                    const QVector3D p2 = point + cameraRight * halfExtent +
                                         cameraUp * halfExtent;
                    const QVector3D p3 = point - cameraRight * halfExtent +
                                         cameraUp * halfExtent;
                    renderer->draw3DQuad(toFloat3(p0), toFloat3(p1),
                                         toFloat3(p2), toFloat3(p3), color);
                };
                drawBillboardSquare(handleHalf * 1.18f, shadowColor);
                drawBillboardSquare(handleHalf, coreColor);
            };

            const float cornerSigns[][3] = {
                {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
                {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
                {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f},
                {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}};
            for (std::size_t i = 0; i < geometry.corners.size(); ++i) {
                const auto& signs = cornerSigns[i];
                drawBoxHandle(geometry.corners[i], GizmoAxis::Screen,
                              QVector3D(signs[0], signs[1], signs[2]));
            }

            const GizmoAxis edgeAxes[] = {
                GizmoAxis::YZ, GizmoAxis::XZ, GizmoAxis::YZ, GizmoAxis::XZ,
                GizmoAxis::YZ, GizmoAxis::XZ, GizmoAxis::YZ, GizmoAxis::XZ,
                GizmoAxis::XY, GizmoAxis::XY, GizmoAxis::XY, GizmoAxis::XY};
            const QVector3D edgeSigns[] = {
                {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f},
                {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
                {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
                {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f},
                {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
                {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}};
            for (int i = 0; i < 12; ++i) {
                drawBoxHandle(geometry.edgeCenters[static_cast<std::size_t>(i)],
                              edgeAxes[i], edgeSigns[i]);
            }

            const GizmoAxis faceAxes[] = {GizmoAxis::X, GizmoAxis::X,
                                          GizmoAxis::Y, GizmoAxis::Y,
                                          GizmoAxis::Z, GizmoAxis::Z};
            const float faceSigns[] = {1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f};
            for (int i = 0; i < 6; ++i) {
                if (!depthEnabled_ && faceAxes[i] == GizmoAxis::Z) {
                    continue;
                }
                drawBoxHandle(geometry.faceCenters[static_cast<std::size_t>(i)],
                              faceAxes[i],
                              QVector3D(i < 2 ? faceSigns[i] : 1.0f,
                                        i >= 2 && i < 4 ? faceSigns[i] : 1.0f,
                                        i >= 4 ? faceSigns[i] : 1.0f));
            }
        }
    }

    if (mode_ == GizmoMode::Move) {
        const QVector3D endX =
            axisHandleEndFor(GizmoAxis::X, interactionCenter, s, basis);
        const QVector3D endY =
            axisHandleEndFor(GizmoAxis::Y, interactionCenter, s, basis);
        const QVector3D endZ =
            axisHandleEndFor(GizmoAxis::Z, interactionCenter, s, basis);
        drawPlaneHandle(GizmoAxis::XY);
        drawPlaneHandle(GizmoAxis::XZ);
        drawPlaneHandle(GizmoAxis::YZ);

        drawAxisArrow(GizmoAxis::X,
                      center,
                      {endX.x(), endX.y(), endX.z()},
                      s * (dedicatedMovePass ? kMoveArrowSizeScale : 0.28f));
        drawAxisArrow(GizmoAxis::Y,
                      center,
                      {endY.x(), endY.y(), endY.z()},
                      s * (dedicatedMovePass ? kMoveArrowSizeScale : 0.28f));
        if (depthEnabled_) {
            drawAxisArrow(GizmoAxis::Z,
                          center,
                          {endZ.x(), endZ.y(), endZ.z()},
                          s * (dedicatedMovePass ? kMoveArrowSizeScale : 0.28f));
        }
        if (dedicatedMovePass) {
            drawMoveCenterHandle();
        } else {
            drawAxisRing(GizmoAxis::Screen, center, toFloat3(cameraForward),
                         s * 0.085f, FloatColor{1.0f, 1.0f, 1.0f, 0.96f}, 1.4f);
        }
    } 
    else if (mode_ == GizmoMode::Rotate) {
        const float axisRadiusScale = dedicatedRotatePass
            ? kDedicatedRotateAxisRadiusScale
            : kRotateAxisRadiusScale;
        if (depthEnabled_) {
            drawRotateRing(GizmoAxis::X, center, toFloat3(axisX),
                           s * axisRadiusScale, axisBaseColorFor(GizmoAxis::X));
            drawRotateRing(GizmoAxis::Y, center, toFloat3(axisY),
                           s * axisRadiusScale, axisBaseColorFor(GizmoAxis::Y));
        }
        drawRotateRing(GizmoAxis::Z, center, toFloat3(axisZ),
                       s * axisRadiusScale, axisBaseColorFor(GizmoAxis::Z));
        if (!impl_->drawingFullOverlay) {
            drawAxisRing(GizmoAxis::Screen, center, toFloat3(cameraForward),
                         s * (dedicatedRotatePass
                                  ? kDedicatedRotateScreenRadiusScale
                                  : kRotateScreenRadiusScale),
                         dedicatedRotatePass
                             ? FloatColor{0.92f, 0.92f, 0.92f, 0.72f}
                             : FloatColor{0.92f, 0.92f, 0.92f, 0.86f},
                         dedicatedRotatePass ? 1.15f : 1.4f);
        }
    } else if (mode_ == GizmoMode::Scale) {
      if (!boundingBoxEnabled_ || dedicatedScalePass) {
        drawPlaneHandle(GizmoAxis::XY);
        drawPlaneHandle(GizmoAxis::XZ);
        drawPlaneHandle(GizmoAxis::YZ);

        auto drawScaleAxis = [&](GizmoAxis axis,
                                 const Detail::float3& start,
                                 const Detail::float3& tipPos,
                                 float cubeHalf) {
            const FloatColor baseColor = axisBaseColorFor(axis);
            const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
            const FloatColor coreColor = getAxisCoreColor(axis, baseColor);
            renderer->drawGizmoLine(start, tipPos, shadowColor, 2.2f);
            renderer->drawGizmoLine(start, tipPos, coreColor, 1.35f);
            Detail::float3 tip = tipPos;
            renderer->drawGizmoCube(tip, cubeHalf * 1.12f, shadowColor);
            renderer->drawGizmoCube(tip, cubeHalf, coreColor);
        };

        const float cubeHalf = s * (dedicatedScalePass
            ? kDedicatedScaleCubeHalfScale
            : 0.065f);
        const QVector3D scaleEndX =
            scaleAxisHandleEndFor(GizmoAxis::X, interactionCenter, s, basis);
        const QVector3D scaleEndY =
            scaleAxisHandleEndFor(GizmoAxis::Y, interactionCenter, s, basis);
        const QVector3D scaleEndZ =
            scaleAxisHandleEndFor(GizmoAxis::Z, interactionCenter, s, basis);
        drawScaleAxis(GizmoAxis::X, center,
                      {scaleEndX.x(), scaleEndX.y(), scaleEndX.z()},
                      cubeHalf);
        drawScaleAxis(GizmoAxis::Y, center,
                      {scaleEndY.x(), scaleEndY.y(), scaleEndY.z()},
                      cubeHalf);
        if (depthEnabled_) {
            drawScaleAxis(GizmoAxis::Z, center,
                          {scaleEndZ.x(), scaleEndZ.y(), scaleEndZ.z()},
                          cubeHalf);
        }
        if (dedicatedScalePass) {
            const FloatColor baseColor{0.94f, 0.94f, 0.96f, 0.96f};
            const FloatColor shadowColor =
                getAxisShadowColor(GizmoAxis::Screen, baseColor);
            const FloatColor coreColor =
                getAxisCoreColor(GizmoAxis::Screen, baseColor);
            Detail::float3 centerHandle = center;
            renderer->drawGizmoCube(centerHandle, cubeHalf * 1.24f,
                                    shadowColor);
            renderer->drawGizmoCube(centerHandle, cubeHalf * 1.08f,
                                    coreColor);
        } else {
            drawAxisRing(GizmoAxis::Screen, center, toFloat3(cameraForward),
                         s * 0.50f,
                         FloatColor{1.0f, 1.0f, 1.0f, 0.62f}, 1.0f);
            drawAxisRing(GizmoAxis::Screen, center, toFloat3(cameraForward),
                         s * 0.68f,
                         FloatColor{1.0f, 1.0f, 1.0f, 0.86f}, 1.3f);
        }
      }
    }

    // PrimitiveRenderer3D submits its queued vertices with the camera matrices
    // that are active at flush time. Flush before restoring them; otherwise
    // the whole gizmo is projected with identity matrices and disappears.
    renderer->flushGizmo3D();
    renderer->setUseExternalMatrices(false);
    renderer->resetGizmoCameraMatrices();
}

} // namespace Artifact
