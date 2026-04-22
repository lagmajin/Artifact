module;
#include <utility>
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
    bool intersectRayPlane(const Ray& ray, const QVector3D& planePos, const QVector3D& planeNormal, QVector3D& hitPoint, float* tOut = nullptr) {
        float denom = QVector3D::dotProduct(planeNormal, ray.direction);
        if (std::abs(denom) < 1e-6f) return false;
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

bool isPlaneHandle(GizmoAxis axis) {
    return axis == GizmoAxis::XY || axis == GizmoAxis::YZ || axis == GizmoAxis::XZ;
}

struct PlaneHandleFrame {
    QVector3D u;
    QVector3D v;
    QVector3D normal;
};

PlaneHandleFrame planeHandleFrameFor(GizmoAxis axis) {
    switch (axis) {
    case GizmoAxis::XY: {
        const QVector3D u = axisDirectionFor(GizmoAxis::X);
        const QVector3D v = axisDirectionFor(GizmoAxis::Y);
        return {u, v, QVector3D::crossProduct(u, v).normalized()};
    }
    case GizmoAxis::XZ: {
        const QVector3D u = axisDirectionFor(GizmoAxis::X);
        const QVector3D v = axisDirectionFor(GizmoAxis::Z);
        return {u, v, QVector3D::crossProduct(u, v).normalized()};
    }
    case GizmoAxis::YZ: {
        const QVector3D u = axisDirectionFor(GizmoAxis::Y);
        const QVector3D v = axisDirectionFor(GizmoAxis::Z);
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

PlaneHandleGeometry planeHandleGeometryFor(GizmoAxis axis, const QVector3D& center, float scale) {
    const PlaneHandleFrame frame = planeHandleFrameFor(axis);
    const float inset = std::max(scale * 0.22f, 6.0f);
    const float size = std::max(scale * 0.14f, 4.0f);
    return {frame, center + frame.u * inset + frame.v * inset, size};
}

bool hitPlaneHandle(const Ray& ray,
                    GizmoAxis axis,
                    const QVector3D& center,
                    float scale,
                    QVector3D& hitPoint,
                    float& depthOut) {
    const PlaneHandleGeometry geom = planeHandleGeometryFor(axis, center, scale);
    if (geom.frame.normal.isNull()) {
        return false;
    }

    const float denom = QVector3D::dotProduct(geom.frame.normal, ray.direction);
    if (std::abs(denom) < 1e-6f) {
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
        return {1.0f, 0.90f, 0.20f, 1.0f};
    case GizmoAxis::XZ:
        return {1.0f, 0.40f, 0.90f, 1.0f};
    case GizmoAxis::YZ:
        return {0.22f, 0.88f, 1.0f, 1.0f};
    default:
        return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

float rayPointDistance(const Ray &ray, const QVector3D &point) {
    const float t = QVector3D::dotProduct(point - ray.origin, ray.direction);
    const QVector3D closest = ray.origin + ray.direction * t;
    return (closest - point).length();
}

QVector3D axisHandleEndFor(GizmoAxis axis, const QVector3D& center, float scale) {
    const QVector3D dir = axisDirectionFor(axis);
    const float length = axis == GizmoAxis::Z ? scale * 1.12f : scale * 1.16f;
    return center + dir * length;
}

float axisHandleHitThreshold(float scale) {
    return std::max(scale * 0.11f, 8.0f);
}

float axisHandleTipRadius(float scale) {
    return std::max(scale * 0.055f, 5.0f);
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

    if (mode_ == GizmoMode::Move) {
        auto checkPlane = [&](GizmoAxis axis) {
            if (result != GizmoAxis::None) {
                return;
            }
            QVector3D hit;
            float depth = std::numeric_limits<float>::max();
            if (hitPlaneHandle(ray, axis, impl_->position, impl_->currentScale, hit, depth)) {
                result = axis;
                minDistance = depth;
            }
        };

        checkPlane(GizmoAxis::XY);
        checkPlane(GizmoAxis::XZ);
        checkPlane(GizmoAxis::YZ);

        if (result == GizmoAxis::None) {
            auto checkAxis = [&](const QVector3D& axisDir, GizmoAxis axis) {
                Q_UNUSED(axisDir);
                if (!depthEnabled_ && axis == GizmoAxis::Z) {
                    return;
                }
                float t;
                const QVector3D end = axisHandleEndFor(axis, impl_->position, impl_->currentScale);
                const float lineDist = impl_->rayLineDistance(ray.origin, ray.direction,
                                                              impl_->position, end, t);
                const float tipDist = rayPointDistance(ray, end);
                const float dist = std::min(lineDist, tipDist);
                if (dist < axisHandleHitThreshold(impl_->currentScale) && dist < minDistance) {
                    minDistance = dist;
                    result = axis;
                }
            };

            checkAxis(axisDirectionFor(GizmoAxis::X), GizmoAxis::X);
            checkAxis(axisDirectionFor(GizmoAxis::Y), GizmoAxis::Y);
            checkAxis(axisDirectionFor(GizmoAxis::Z), GizmoAxis::Z);

            const float centerDist = rayPointDistance(ray, impl_->position);
            if (centerDist < axisHandleTipRadius(impl_->currentScale) * 0.85f && centerDist < minDistance) {
                minDistance = centerDist;
                result = GizmoAxis::Screen;
            }
        }
    } else if (mode_ == GizmoMode::Scale) {
        auto checkAxis = [&](const QVector3D& axisDir, GizmoAxis axis) {
            Q_UNUSED(axisDir);
            if (!depthEnabled_ && axis == GizmoAxis::Z) {
                return;
            }
            float t;
            const QVector3D end = axisHandleEndFor(axis, impl_->position, impl_->currentScale);
            const float lineDist = impl_->rayLineDistance(ray.origin, ray.direction,
                                                          impl_->position, end, t);
            const float tipDist = rayPointDistance(ray, end);
            const float dist = std::min(lineDist, tipDist);
            if (dist < axisHandleHitThreshold(impl_->currentScale) && dist < minDistance) {
                minDistance = dist;
                result = axis;
            }
        };

        checkAxis(axisDirectionFor(GizmoAxis::X), GizmoAxis::X);
        checkAxis(axisDirectionFor(GizmoAxis::Y), GizmoAxis::Y);
        checkAxis(axisDirectionFor(GizmoAxis::Z), GizmoAxis::Z);

        const float centerDist = rayPointDistance(ray, impl_->position);
        if (centerDist < axisHandleTipRadius(impl_->currentScale) * 1.25f && centerDist < minDistance) {
            result = GizmoAxis::Screen;
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
        checkRing(axisDirectionFor(GizmoAxis::X), GizmoAxis::X);
        checkRing(axisDirectionFor(GizmoAxis::Y), GizmoAxis::Y);
        checkRing(axisDirectionFor(GizmoAxis::Z), GizmoAxis::Z);
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

    if (isPlaneHandle(axis)) {
        const auto frame = planeHandleFrameFor(axis);
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->position, frame.normal, hit)) {
            impl_->dragStartHitPoint = hit;
        } else {
            impl_->dragStartHitPoint = impl_->dragStartPosition;
        }
        return;
    }

    if (mode_ == GizmoMode::Move && axis == GizmoAxis::Screen) {
        QVector3D planeNormal = viewDir;
        if (planeNormal.lengthSquared() < 0.01f) {
            planeNormal = QVector3D(0.0f, 0.0f, 1.0f);
        }
        QVector3D hit;
        if (impl_->intersectRayPlane(ray, impl_->dragStartPosition, planeNormal.normalized(), hit)) {
            impl_->dragStartHitPoint = hit;
        } else {
            impl_->dragStartHitPoint = impl_->dragStartPosition;
        }
        return;
    }

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

    if (isPlaneHandle(activeAxis_)) {
        const auto frame = planeHandleFrameFor(activeAxis_);
        QVector3D hit;
        if (!impl_->intersectRayPlane(ray, impl_->dragStartPosition, frame.normal, hit)) {
            return;
        }

        const QVector3D delta = hit - impl_->dragStartHitPoint;
        impl_->position = impl_->dragStartPosition
                        + frame.u * QVector3D::dotProduct(delta, frame.u)
                        + frame.v * QVector3D::dotProduct(delta, frame.v);
        return;
    }

    if (mode_ == GizmoMode::Move && activeAxis_ == GizmoAxis::Screen) {
        QVector3D viewDir = (ray.origin - impl_->dragStartPosition).normalized();
        if (viewDir.lengthSquared() < 0.01f) {
            viewDir = QVector3D(0.0f, 0.0f, 1.0f);
        }
        viewDir.normalize();
        QVector3D hit;
        if (!impl_->intersectRayPlane(ray, impl_->dragStartPosition, viewDir.normalized(), hit)) {
            return;
        }
        impl_->position = impl_->dragStartPosition + (hit - impl_->dragStartHitPoint);
        return;
    }

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
    const float baseScale = std::max(distance * 0.63f, 126.0f);
    const float modeScaleFactor = mode_ == GizmoMode::Scale ? 2.0f
                               : mode_ == GizmoMode::Rotate ? 0.5f
                               : 1.0f;
    impl_->currentScale = baseScale * modeScaleFactor;

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
    const QVector3D axisX = axisDirectionFor(GizmoAxis::X);
    const QVector3D axisY = axisDirectionFor(GizmoAxis::Y);
    const QVector3D axisZ = axisDirectionFor(GizmoAxis::Z);

    auto toFloat3 = [](const QVector3D& v) -> Detail::float3 {
        return {v.x(), v.y(), v.z()};
    };

    auto drawPlaneHandle = [&](GizmoAxis axis) {
        const PlaneHandleGeometry geom = planeHandleGeometryFor(axis, impl_->position, s);
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
            activeAxis_ == axis ? 0.38f : (hoverAxis_ == axis ? 0.30f : 0.22f)
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
        renderer->drawGizmoLine(toFloat3(coreP0), toFloat3(coreP1), coreColor, 1.0f);
        renderer->drawGizmoLine(toFloat3(coreP1), toFloat3(coreP2), coreColor, 1.0f);
        renderer->drawGizmoLine(toFloat3(coreP2), toFloat3(coreP3), coreColor, 1.0f);
        renderer->drawGizmoLine(toFloat3(coreP3), toFloat3(coreP0), coreColor, 1.0f);
    };

    // Central anchor marker: a small halo + compact cross so the pivot is easy to read.
    renderer->drawGizmoRing(center, toFloat3(axisZ), anchorRadius * 1.12f, anchorShadow);
    renderer->drawGizmoRing(center, toFloat3(axisZ), anchorRadius, anchorCore);
    renderer->drawGizmoLine({center.x - anchorArm * 1.15f * axisX.x(), center.y - anchorArm * 1.15f * axisX.y(), center.z - anchorArm * 1.15f * axisX.z()},
                            {center.x + anchorArm * 1.15f * axisX.x(), center.y + anchorArm * 1.15f * axisX.y(), center.z + anchorArm * 1.15f * axisX.z()},
                            anchorShadow);
    renderer->drawGizmoLine({center.x - anchorArm * 1.15f * axisY.x(), center.y - anchorArm * 1.15f * axisY.y(), center.z - anchorArm * 1.15f * axisY.z()},
                            {center.x + anchorArm * 1.15f * axisY.x(), center.y + anchorArm * 1.15f * axisY.y(), center.z + anchorArm * 1.15f * axisY.z()},
                            anchorShadow);
    renderer->drawGizmoLine({center.x - anchorArm * 1.15f * axisZ.x(), center.y - anchorArm * 1.15f * axisZ.y(), center.z - anchorArm * 1.15f * axisZ.z()},
                            {center.x + anchorArm * 1.15f * axisZ.x(), center.y + anchorArm * 1.15f * axisZ.y(), center.z + anchorArm * 1.15f * axisZ.z()},
                            anchorShadow);
    renderer->drawGizmoLine({center.x - anchorArm * axisX.x(), center.y - anchorArm * axisX.y(), center.z - anchorArm * axisX.z()},
                            {center.x + anchorArm * axisX.x(), center.y + anchorArm * axisX.y(), center.z + anchorArm * axisX.z()},
                            anchorCore);
    renderer->drawGizmoLine({center.x - anchorArm * axisY.x(), center.y - anchorArm * axisY.y(), center.z - anchorArm * axisY.z()},
                            {center.x + anchorArm * axisY.x(), center.y + anchorArm * axisY.y(), center.z + anchorArm * axisY.z()},
                            anchorCore);
    renderer->drawGizmoLine({center.x - anchorArm * axisZ.x(), center.y - anchorArm * axisZ.y(), center.z - anchorArm * axisZ.z()},
                            {center.x + anchorArm * axisZ.x(), center.y + anchorArm * axisZ.y(), center.z + anchorArm * axisZ.z()},
                            anchorCore);

    auto drawAxisArrow = [&](GizmoAxis axis,
                             const Detail::float3& start,
                             const Detail::float3& end,
                             const FloatColor& baseColor,
                             float size) {
        const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
        const FloatColor coreColor = getAxisCoreColor(axis, baseColor);
        renderer->drawGizmoLine(start, end, shadowColor, std::max(1.4f, size * 0.24f));
        renderer->drawGizmoLine(start, end, coreColor, std::max(1.0f, size * 0.18f));
        renderer->drawGizmoCube(end, std::max(size * 0.18f, 3.2f), shadowColor);
        renderer->drawGizmoCube(end, std::max(size * 0.14f, 2.6f), coreColor);
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

    auto ringBasisFor = [](const QVector3D& normal) {
        QVector3D n = normal.normalized();
        if (n.lengthSquared() < 1e-6f) {
            n = QVector3D(0.0f, 0.0f, 1.0f);
        }
        QVector3D reference = std::abs(n.y()) < 0.85f ? QVector3D(0.0f, 1.0f, 0.0f)
                                                       : QVector3D(1.0f, 0.0f, 0.0f);
        QVector3D tangent = QVector3D::crossProduct(reference, n);
        if (tangent.lengthSquared() < 1e-6f) {
            reference = QVector3D(0.0f, 0.0f, 1.0f);
            tangent = QVector3D::crossProduct(reference, n);
        }
        tangent.normalize();
        QVector3D bitangent = QVector3D::crossProduct(n, tangent).normalized();
        return std::pair{tangent, bitangent};
    };

    auto drawRotateRing = [&](GizmoAxis axis,
                              const Detail::float3& centerPos,
                              const Detail::float3& normal,
                              float radius,
                              const FloatColor& baseColor,
                              bool freeRotation = false) {
        const FloatColor shadowColor = getAxisShadowColor(axis, baseColor);
        const FloatColor coreColor = getAxisCoreColor(axis, baseColor);
        const float tubeRadius = freeRotation ? s * 0.020f : s * 0.034f;
        const float shadowTube = freeRotation ? tubeRadius * 1.18f : tubeRadius * 1.32f;
        renderer->drawGizmoTorus(centerPos, normal, radius * 1.018f, shadowTube, shadowColor);
        renderer->drawGizmoTorus(centerPos, normal, radius, tubeRadius, coreColor);

        const QVector3D normalVec(normal.x, normal.y, normal.z);
        const auto [tangent, bitangent] = ringBasisFor(normalVec);
        const QVector3D centerVec(centerPos.x, centerPos.y, centerPos.z);
        const QVector3D markerPos = centerVec + tangent * radius;
        const QVector3D tickStart = markerPos - bitangent * std::max(radius * 0.075f, s * 0.03f);
        const QVector3D tickEnd = markerPos + bitangent * std::max(radius * 0.075f, s * 0.03f);
        const FloatColor tickShadow = getAxisShadowColor(axis, baseColor);
        const FloatColor tickCore = getAxisCoreColor(axis, baseColor);
        renderer->drawGizmoLine({tickStart.x(), tickStart.y(), tickStart.z()},
                                {tickEnd.x(), tickEnd.y(), tickEnd.z()},
                                tickShadow,
                                std::max(1.4f, s * 0.006f));
        renderer->drawGizmoLine({tickStart.x(), tickStart.y(), tickStart.z()},
                                {tickEnd.x(), tickEnd.y(), tickEnd.z()},
                                tickCore,
                                std::max(1.0f, s * 0.0045f));

        Detail::float3 marker = {markerPos.x(), markerPos.y(), markerPos.z()};
        renderer->drawGizmoCube(marker, std::max(1.6f, s * 0.016f), tickShadow);
        renderer->drawGizmoCube(marker, std::max(1.2f, s * 0.012f), tickCore);
    };

    if (mode_ == GizmoMode::Move) {
        const QVector3D endX = axisHandleEndFor(GizmoAxis::X, impl_->position, s);
        const QVector3D endY = axisHandleEndFor(GizmoAxis::Y, impl_->position, s);
        const QVector3D endZ = axisHandleEndFor(GizmoAxis::Z, impl_->position, s);
        drawPlaneHandle(GizmoAxis::XY);
        drawPlaneHandle(GizmoAxis::XZ);
        drawPlaneHandle(GizmoAxis::YZ);

        drawAxisArrow(GizmoAxis::X,
                      center,
                      {endX.x(), endX.y(), endX.z()},
                      {1.0f, 0.22f, 0.18f, 1.0f},
                      s * 0.42f);
        drawAxisArrow(GizmoAxis::Y,
                      center,
                      {endY.x(), endY.y(), endY.z()},
                      {0.20f, 1.0f, 0.28f, 1.0f},
                      s * 0.42f);
        drawAxisArrow(GizmoAxis::Z,
                      center,
                      {endZ.x(), endZ.y(), endZ.z()},
                      depthEnabled_ ? FloatColor{0.28f, 0.58f, 1.0f, 1.0f} : FloatColor{0.45f, 0.45f, 0.45f, 0.7f},
                      s * 0.42f);
        const float moveCenterSize = std::max(s * 0.048f, 4.2f);
        renderer->drawGizmoCube(center, moveCenterSize * 1.18f, FloatColor{0.0f, 0.0f, 0.0f, 0.42f});
        renderer->drawGizmoCube(center, moveCenterSize, FloatColor{1.0f, 1.0f, 1.0f, 0.88f});
    } 
    else if (mode_ == GizmoMode::Rotate) {
        // imGuIZMO-style rotate rings: thin torus + visible grab marker.
        drawRotateRing(GizmoAxis::X, center, toFloat3(axisX), s * 1.04f, {1.0f, 0.22f, 0.18f, 1.0f});
        drawRotateRing(GizmoAxis::Y, center, toFloat3(axisY), s * 1.04f, {0.20f, 1.0f, 0.28f, 1.0f});
        drawRotateRing(GizmoAxis::Z, center, toFloat3(axisZ), s * 1.04f,
                       depthEnabled_ ? FloatColor{0.28f, 0.58f, 1.0f, 1.0f}
                                     : FloatColor{0.45f, 0.45f, 0.45f, 0.7f});
        // Outer free-rotation gray ring (slightly thinner and more subdued)
        drawRotateRing(GizmoAxis::Screen, center, toFloat3(axisZ), s * 1.18f,
                       {0.55f, 0.55f, 0.55f, 0.50f}, true);
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
                      {impl_->position.x() + axisX.x() * s * 0.92f, impl_->position.y() + axisX.y() * s * 0.92f, impl_->position.z() + axisX.z() * s * 0.92f},
                      {1.0f, 0.38f, 0.18f, 1.0f}, cubeHalf);
        drawScaleAxis(GizmoAxis::Y, center,
                      {impl_->position.x() + axisY.x() * s * 0.92f, impl_->position.y() + axisY.y() * s * 0.92f, impl_->position.z() + axisY.z() * s * 0.92f},
                      {0.22f, 1.0f, 0.55f, 1.0f}, cubeHalf);
        drawScaleAxis(GizmoAxis::Z, center,
                      {impl_->position.x() + axisZ.x() * s, impl_->position.y() + axisZ.y() * s, impl_->position.z() + axisZ.z() * s},
                      depthEnabled_ ? FloatColor{0.72f, 0.28f, 1.0f, 1.0f} : FloatColor{0.45f, 0.45f, 0.45f, 0.7f},
                      cubeHalf);
        // Center uniform scale handle
        drawAxisRing(GizmoAxis::Screen, center, toFloat3(axisZ), s * 0.60f, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
        renderer->drawGizmoRing(center, toFloat3(axisZ), s * 0.18f, FloatColor{1.0f, 1.0f, 1.0f, 0.30f}, 1.0f);
    }

    renderer->setUseExternalMatrices(false);
    renderer->resetGizmoCameraMatrices();
}

} // namespace Artifact
