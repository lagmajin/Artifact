module;
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>
#include <wobjectimpl.h>

module Artifact.Widgets.Gizmo3D;

import std;

namespace Artifact {

W_OBJECT_IMPL(Artifact3DGizmo)

struct Artifact3DGizmo::Impl {
    QVector3D position;
    QVector3D rotation;
    QVector3D scale = QVector3D(1, 1, 1);
    float currentScale = 1.0f;

    Ray dragStartRay;
    QVector3D dragStartPosition;
    QVector3D dragStartRotation;
    QVector3D dragStartScale;
    QVector3D dragOffset;
    QVector3D dragStartHitPoint;
    float dragStartAngle = 0.0f;
    bool firstDrag = true;
    
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

        if (D < 1e-6f) { // Lines are parallel
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
    bool intersectRayPlane(const Ray& ray, const QVector3D& planePos, const QVector3D& planeNormal, QVector3D& hitPoint) {
        float denom = QVector3D::dotProduct(planeNormal, ray.direction);
        if (std::abs(denom) < 1e-6f) return false;
        float t = QVector3D::dotProduct(planePos - ray.origin, planeNormal) / denom;
        if (t < 0) return false;
        hitPoint = ray.origin + ray.direction * t;
        return true;
    }
};

namespace {

QVector3D axisDirectionFor(GizmoAxis axis) {
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

float rayPointDistance(const Ray &ray, const QVector3D &point) {
    const float t = QVector3D::dotProduct(point - ray.origin, ray.direction);
    const QVector3D closest = ray.origin + ray.direction * t;
    return (closest - point).length();
}

} // namespace

Artifact3DGizmo::Artifact3DGizmo(QObject* parent)
    : QObject(parent), impl_(std::make_unique<Impl>()) {
}

Artifact3DGizmo::~Artifact3DGizmo() = default;

void Artifact3DGizmo::setTransform(const QVector3D& position, const QVector3D& rotation) {
    impl_->position = position;
    impl_->rotation = rotation;
}

QVector3D Artifact3DGizmo::position() const {
    return impl_->position;
}

QVector3D Artifact3DGizmo::rotation() const {
    return impl_->rotation;
}

void Artifact3DGizmo::setScale(const QVector3D &scale) {
    impl_->scale = scale;
}

QVector3D Artifact3DGizmo::scale() const {
    return impl_->scale;
}

GizmoAxis Artifact3DGizmo::hitTest(const Ray& ray, const QMatrix4x4& view, const QMatrix4x4& proj) {
    (void)proj;
    float threshold = 0.12f * impl_->currentScale;
    float minDistance = std::numeric_limits<float>::max();
    GizmoAxis result = GizmoAxis::None;

    if (mode_ == GizmoMode::Move || mode_ == GizmoMode::Scale) {
        auto checkAxis = [&](const QVector3D& axisDir, GizmoAxis axis) {
            if (!depthEnabled_ && axis == GizmoAxis::Z) {
                return;
            }
            float t;
            float dist = impl_->rayLineDistance(ray.origin, ray.direction, 
                                                impl_->position, impl_->position + axisDir * (impl_->currentScale * 1.0f), t);
            if (dist < threshold && dist < minDistance) {
                minDistance = dist;
                result = axis;
            }
        };

        checkAxis(QVector3D(1, 0, 0), GizmoAxis::X);
        checkAxis(QVector3D(0, 1, 0), GizmoAxis::Y);
        checkAxis(QVector3D(0, 0, 1), GizmoAxis::Z);

        if (mode_ == GizmoMode::Scale) {
            const float centerDist = rayPointDistance(ray, impl_->position);
            if (centerDist < threshold * 0.85f && centerDist < minDistance) {
                result = GizmoAxis::Screen;
            }
        }
    } 
    else if (mode_ == GizmoMode::Rotate) {
        auto checkRing = [&](const QVector3D& planeNormal, GizmoAxis axis) {
            QVector3D hit;
            if (impl_->intersectRayPlane(ray, impl_->position, planeNormal, hit)) {
                float distToCenter = (hit - impl_->position).length();
                float ringRadius = impl_->currentScale * 1.0f;
                if (std::abs(distToCenter - ringRadius) < threshold) {
                    float depth = (view * QVector4D(hit, 1.0f)).z();
                    if (depth < minDistance) {
                        result = axis;
                    }
                }
            }
        };
        checkRing(QVector3D(1, 0, 0), GizmoAxis::X);
        checkRing(QVector3D(0, 1, 0), GizmoAxis::Y);
        checkRing(QVector3D(0, 0, 1), GizmoAxis::Z);
    }

    hoverAxis_ = result;
    return result;
}

void Artifact3DGizmo::beginDrag(GizmoAxis axis, const Ray& ray) {
    activeAxis_ = axis;
    impl_->dragStartRay = ray;
    impl_->dragStartPosition = impl_->position;
    impl_->dragStartRotation = impl_->rotation;
    impl_->dragStartScale = impl_->scale;
    impl_->firstDrag = true;
    
    const QVector3D axisDir = axisDirectionFor(axis);
    const QVector3D viewDir = (ray.origin - impl_->dragStartPosition).normalized();

    if (mode_ == GizmoMode::Rotate) {
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->position, axisDir, hit)) {
            QVector3D dir = (hit - impl_->position).normalized();
            QVector3D tangent, bitangent;
            if (std::abs(axisDir.x()) > 0.9f) { tangent = QVector3D(0, 1, 0); bitangent = QVector3D(0, 0, 1); }
            else if (std::abs(axisDir.y()) > 0.9f) { tangent = QVector3D(1, 0, 0); bitangent = QVector3D(0, 0, 1); }
            else { tangent = QVector3D(1, 0, 0); bitangent = QVector3D(0, 1, 0); }
            
            float x = QVector3D::dotProduct(dir, tangent);
            float y = QVector3D::dotProduct(dir, bitangent);
            impl_->dragStartAngle = std::atan2(y, x) * 180.0f / M_PI;
        }
    } else {
        QVector3D planeNormal;
        if (mode_ == GizmoMode::Scale && axis == GizmoAxis::Screen) {
            planeNormal = viewDir;
        } else {
            planeNormal = QVector3D::crossProduct(QVector3D::crossProduct(viewDir, axisDir), axisDir).normalized();
            if (planeNormal.lengthSquared() < 0.01f) {
                QVector3D other = (std::abs(axisDir.y()) > 0.9f) ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
                planeNormal = QVector3D::crossProduct(axisDir, other).normalized();
            }
        }
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->dragStartPosition, planeNormal, hit)) {
            impl_->dragStartHitPoint = hit;
        } else {
            impl_->dragStartHitPoint = impl_->dragStartPosition;
        }
    }
}

void Artifact3DGizmo::updateDrag(const Ray& ray) {
    if (activeAxis_ == GizmoAxis::None) return;
    
    const QVector3D axisDir = axisDirectionFor(activeAxis_);

    if (mode_ == GizmoMode::Move) {
        if (axisDir.isNull()) {
            return;
        }
        if (!depthEnabled_ && activeAxis_ == GizmoAxis::Z) {
            return;
        }
        QVector3D viewDir = (ray.origin - impl_->dragStartPosition).normalized();
        QVector3D planeNormal = QVector3D::crossProduct(QVector3D::crossProduct(viewDir, axisDir), axisDir).normalized();
        if (planeNormal.lengthSquared() < 0.01f) {
            QVector3D other = (std::abs(axisDir.y()) > 0.9f) ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
            planeNormal = QVector3D::crossProduct(axisDir, other).normalized();
        }
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->dragStartPosition, planeNormal, hit)) {
            float projectT = QVector3D::dotProduct(hit - impl_->dragStartHitPoint, axisDir);
            impl_->position = impl_->dragStartPosition + axisDir * projectT;
        }
    } 
    else if (mode_ == GizmoMode::Rotate) {
        if (!depthEnabled_ && activeAxis_ == GizmoAxis::Z) {
            return;
        }
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->position, axisDir, hit)) {
            QVector3D dir = (hit - impl_->position).normalized();
            QVector3D tangent, bitangent;
            if (std::abs(axisDir.x()) > 0.9f) { tangent = QVector3D(0, 1, 0); bitangent = QVector3D(0, 0, 1); }
            else if (std::abs(axisDir.y()) > 0.9f) { tangent = QVector3D(1, 0, 0); bitangent = QVector3D(0, 0, 1); }
            else { tangent = QVector3D(1, 0, 0); bitangent = QVector3D(0, 1, 0); }
            
            float x = QVector3D::dotProduct(dir, tangent);
            float y = QVector3D::dotProduct(dir, bitangent);
            float currentAngle = std::atan2(y, x) * 180.0f / M_PI;
            float delta = currentAngle - impl_->dragStartAngle;
            
            QVector3D rot = impl_->dragStartRotation;
            if (activeAxis_ == GizmoAxis::X) rot.setX(rot.x() + delta);
            else if (activeAxis_ == GizmoAxis::Y) rot.setY(rot.y() + delta);
            else if (activeAxis_ == GizmoAxis::Z) rot.setZ(rot.z() + delta);
            impl_->rotation = rot;
        }
    } else if (mode_ == GizmoMode::Scale) {
        if (!depthEnabled_ && activeAxis_ == GizmoAxis::Z) {
            return;
        }
        QVector3D viewDir = (ray.origin - impl_->dragStartPosition).normalized();
        QVector3D planeNormal = (activeAxis_ == GizmoAxis::Screen)
            ? viewDir
            : QVector3D::crossProduct(QVector3D::crossProduct(viewDir, axisDir), axisDir).normalized();
        if (planeNormal.lengthSquared() < 0.01f) {
            QVector3D fallback = (std::abs(viewDir.y()) > 0.9f) ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
            planeNormal = (activeAxis_ == GizmoAxis::Screen)
                ? viewDir
                : QVector3D::crossProduct(axisDir, fallback).normalized();
        }

        QVector3D hit;
        if (!impl_->intersectRayPlane(ray, impl_->dragStartPosition, planeNormal, hit)) {
            return;
        }

        QVector3D newScale = impl_->dragStartScale;
        if (activeAxis_ == GizmoAxis::X) {
            const float delta = QVector3D::dotProduct(hit - impl_->dragStartHitPoint, axisDir);
            const float factor = std::max(0.01f, 1.0f + delta / std::max(impl_->currentScale, 1e-3f));
            newScale.setX(std::max(0.01f, impl_->dragStartScale.x() * factor));
        } else if (activeAxis_ == GizmoAxis::Y) {
            const float delta = QVector3D::dotProduct(hit - impl_->dragStartHitPoint, axisDir);
            const float factor = std::max(0.01f, 1.0f + delta / std::max(impl_->currentScale, 1e-3f));
            newScale.setY(std::max(0.01f, impl_->dragStartScale.y() * factor));
        } else if (activeAxis_ == GizmoAxis::Z || activeAxis_ == GizmoAxis::Screen) {
            const float startDist = std::max((impl_->dragStartHitPoint - impl_->dragStartPosition).length(),
                                              std::max(impl_->currentScale * 0.5f, 1e-3f));
            const float currentDist = (hit - impl_->dragStartPosition).length();
            const float factor = std::max(0.01f, currentDist / startDist);
            newScale.setX(std::max(0.01f, impl_->dragStartScale.x() * factor));
            newScale.setY(std::max(0.01f, impl_->dragStartScale.y() * factor));
        } else {
            return;
        }
        impl_->scale = newScale;
    }
}

void Artifact3DGizmo::endDrag() {
    activeAxis_ = GizmoAxis::None;
}

void Artifact3DGizmo::draw(ArtifactIRenderer* renderer, const QMatrix4x4& view, const QMatrix4x4& proj) {
    if (!renderer) return;

    QVector4D viewPos = view * QVector4D(impl_->position, 1.0f);
    const float distance = std::abs(viewPos.z());
    impl_->currentScale = std::max(distance * 0.63f, 126.0f);

    Detail::float3 center = { impl_->position.x(), impl_->position.y(), impl_->position.z() };
    float s = impl_->currentScale;

    renderer->setUseExternalMatrices(true);
    renderer->setViewMatrix(view);
    renderer->setProjectionMatrix(proj);
    renderer->setGizmoCameraMatrices(view, proj);

    auto clamp01 = [](float v) {
        return std::clamp(v, 0.0f, 1.0f);
    };
    auto tintColor = [&](const FloatColor& baseColor, float boost, float alpha) {
        return FloatColor{
            clamp01(baseColor.r() * boost + 0.08f),
            clamp01(baseColor.g() * boost + 0.08f),
            clamp01(baseColor.b() * boost + 0.08f),
            clamp01(alpha)
        };
    };
    auto getAxisCoreColor = [&](GizmoAxis axis, const FloatColor& baseColor) {
        if (activeAxis_ == axis) {
            return tintColor(baseColor, 1.18f, 1.0f);
        }
        if (hoverAxis_ == axis) {
            return tintColor(baseColor, 1.12f, 1.0f);
        }
        if (activeAxis_ != GizmoAxis::None) {
            return tintColor(baseColor, 0.68f, 0.48f);
        }
        return tintColor(baseColor, 0.95f, 0.90f);
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

    const float anchorRadius = std::max(s * 0.22f, 8.0f);
    const float anchorArm = std::max(s * 0.15f, 6.0f);
    const FloatColor anchorShadow{0.0f, 0.0f, 0.0f, 0.88f};
    const FloatColor anchorCore{1.0f, 1.0f, 1.0f, 1.0f};

    // Central anchor marker: a small halo + compact cross so the pivot is easy to read.
    renderer->drawGizmoRing(center, {0, 0, 1}, anchorRadius * 1.12f, anchorShadow);
    renderer->drawGizmoRing(center, {0, 0, 1}, anchorRadius, anchorCore);
    renderer->drawGizmoLine({center.x - anchorArm * 1.15f, center.y, center.z},
                            {center.x + anchorArm * 1.15f, center.y, center.z},
                            anchorShadow);
    renderer->drawGizmoLine({center.x, center.y - anchorArm * 1.15f, center.z},
                            {center.x, center.y + anchorArm * 1.15f, center.z},
                            anchorShadow);
    renderer->drawGizmoLine({center.x, center.y, center.z - anchorArm * 1.15f},
                            {center.x, center.y, center.z + anchorArm * 1.15f},
                            anchorShadow);
    renderer->drawGizmoLine({center.x - anchorArm, center.y, center.z},
                            {center.x + anchorArm, center.y, center.z},
                            anchorCore);
    renderer->drawGizmoLine({center.x, center.y - anchorArm, center.z},
                            {center.x, center.y + anchorArm, center.z},
                            anchorCore);
    renderer->drawGizmoLine({center.x, center.y, center.z - anchorArm},
                            {center.x, center.y, center.z + anchorArm},
                            anchorCore);

    auto drawAxisArrow = [&](GizmoAxis axis,
                             const Detail::float3& start,
                             const Detail::float3& end,
                             const FloatColor& baseColor,
                             float size) {
        const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
        const FloatColor coreColor = getAxisCoreColor(axis, baseColor);
        renderer->drawGizmoArrow(start, end, shadowColor, size * 1.14f);
        renderer->drawGizmoArrow(start, end, coreColor, size);
    };
    auto drawAxisRing = [&](GizmoAxis axis,
                            const Detail::float3& centerPos,
                            const Detail::float3& normal,
                            float radius,
                            const FloatColor& baseColor,
                            float thickness = 1.0f) {
        const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
        const FloatColor coreColor = getAxisCoreColor(axis, baseColor);
        renderer->drawGizmoRing(centerPos, normal, radius * 1.02f, shadowColor, thickness * 1.30f);
        renderer->drawGizmoRing(centerPos, normal, radius, coreColor, thickness);
    };

    if (mode_ == GizmoMode::Move) {
        drawAxisArrow(GizmoAxis::X,
                      center,
                      {impl_->position.x() + s * 1.16f, impl_->position.y(), impl_->position.z()},
                      {1.0f, 0.22f, 0.18f, 1.0f},
                      s * 0.42f);
        drawAxisArrow(GizmoAxis::Y,
                      center,
                      {impl_->position.x(), impl_->position.y() - s * 1.16f, impl_->position.z()},
                      {0.20f, 1.0f, 0.28f, 1.0f},
                      s * 0.42f);
        drawAxisArrow(GizmoAxis::Z,
                      center,
                      {impl_->position.x(), impl_->position.y(), impl_->position.z() + s * 1.12f},
                      depthEnabled_ ? FloatColor{0.28f, 0.58f, 1.0f, 1.0f} : FloatColor{0.45f, 0.45f, 0.45f, 0.7f},
                      s * 0.42f);
    } 
    else if (mode_ == GizmoMode::Rotate) {
        // Torus rings for each axis
        auto drawAxisTorus = [&](GizmoAxis axis,
                                 const Detail::float3& centerPos,
                                 const Detail::float3& normal,
                                 float radius,
                                 const FloatColor& baseColor) {
            const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
            const FloatColor coreColor = getAxisCoreColor(axis, baseColor);
            const float tubeRadius = s * 0.032f;
            renderer->drawGizmoTorus(centerPos, normal, radius * 1.02f, tubeRadius * 1.3f, shadowColor);
            renderer->drawGizmoTorus(centerPos, normal, radius, tubeRadius, coreColor);
        };
        drawAxisTorus(GizmoAxis::X, center, {1, 0, 0}, s * 1.04f, {1.0f, 0.22f, 0.18f, 1.0f});
        drawAxisTorus(GizmoAxis::Y, center, {0, 1, 0}, s * 1.04f, {0.20f, 1.0f, 0.28f, 1.0f});
        drawAxisTorus(GizmoAxis::Z, center, {0, 0, 1}, s * 1.04f,
                     depthEnabled_ ? FloatColor{0.28f, 0.58f, 1.0f, 1.0f} : FloatColor{0.45f, 0.45f, 0.45f, 0.7f});
        // Outer free-rotation gray ring (thinner torus)
        renderer->drawGizmoTorus(center, {0,0,1}, s * 1.18f, s * 0.018f, FloatColor{0.55f, 0.55f, 0.55f, 0.50f});
    } else if (mode_ == GizmoMode::Scale) {
        // Scale gizmo: shaft lines with cube tips (distinguishes from move gizmo's cone/pyramid)
        auto drawScaleAxis = [&](GizmoAxis axis,
                                 const Detail::float3& start,
                                 const Detail::float3& tipPos,
                                 const FloatColor& baseColor,
                                 float cubeHalf) {
            const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
            const FloatColor coreColor = getAxisCoreColor(axis, baseColor);
            renderer->drawGizmoLine(start, tipPos, shadowColor, 1.2f);
            renderer->drawGizmoLine(start, tipPos, coreColor);
            Detail::float3 tip = tipPos;
            renderer->drawGizmoCube(tip, cubeHalf * 1.12f, shadowColor);
            renderer->drawGizmoCube(tip, cubeHalf, coreColor);
        };

        const float cubeHalf = s * 0.065f;
        drawScaleAxis(GizmoAxis::X, center,
                      {impl_->position.x() + s * 0.92f, impl_->position.y(), impl_->position.z()},
                      {1.0f, 0.38f, 0.18f, 1.0f}, cubeHalf);
        drawScaleAxis(GizmoAxis::Y, center,
                      {impl_->position.x(), impl_->position.y() - s * 0.92f, impl_->position.z()},
                      {0.22f, 1.0f, 0.55f, 1.0f}, cubeHalf);
        drawScaleAxis(GizmoAxis::Z, center,
                      {impl_->position.x(), impl_->position.y(), impl_->position.z() + s},
                      depthEnabled_ ? FloatColor{0.72f, 0.28f, 1.0f, 1.0f} : FloatColor{0.45f, 0.45f, 0.45f, 0.7f},
                      cubeHalf);
        // Center uniform scale handle
        drawAxisRing(GizmoAxis::Screen, center, {0,0,1}, s * 0.60f, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
        renderer->drawGizmoRing(center, {0,0,1}, s * 0.18f, FloatColor{1.0f, 1.0f, 1.0f, 0.30f}, 1.0f);
    }

    renderer->setUseExternalMatrices(false);
    renderer->resetGizmoCameraMatrices();
}

} // namespace Artifact
