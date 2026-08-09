module;
#include <QObject>
#include <QVector2D>
#include <QVector3D>
#include <QColor>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QRandomGenerator>
#include <QRect>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <limits>
#include <map>
#include <tuple>
#ifdef emit
#pragma push_macro("emit")
#undef emit
#define ARTIFACT_RESTORE_QT_EMIT_MACRO
#endif
#ifdef ARTIFACT_RESTORE_QT_EMIT_MACRO
#pragma pop_macro("emit")
#undef ARTIFACT_RESTORE_QT_EMIT_MACRO
#endif
#include <wobjectimpl.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Generator.Particle;





namespace Artifact {

namespace {

float evaluateTriStageLinear(float startValue,
                             float midValue,
                             float endValue,
                             float midPosition,
                             float t)
{
    const float clampedMid = std::isfinite(midPosition)
        ? std::clamp(midPosition, 0.001f, 0.999f) : 0.5f;
    const float clampedT = std::isfinite(t) ? std::clamp(t, 0.0f, 1.0f) : 0.0f;
    if (clampedT <= clampedMid) {
        const float localT = clampedT / clampedMid;
        return startValue + (midValue - startValue) * localT;
    }

    const float localT = (clampedT - clampedMid) / (1.0f - clampedMid);
    return midValue + (endValue - midValue) * localT;
}

QColor scaledAlphaColor(const QColor& color, float alphaScale)
{
    QColor out = color;
    out.setAlphaF(std::clamp(color.alphaF() * alphaScale, 0.0f, 1.0f));
    return out;
}

QColor evaluateTriStageColor(const QColor& startColor,
                             const QColor& midColor,
                             const QColor& endColor,
                             float midPosition,
                             float t)
{
    const auto lerpChannel = [&](int startValue, int midValue, int endValue) {
        return static_cast<int>(std::round(evaluateTriStageLinear(
            static_cast<float>(startValue),
            static_cast<float>(midValue),
            static_cast<float>(endValue),
            midPosition,
            t)));
    };

    return QColor(
        std::clamp(lerpChannel(startColor.red(), midColor.red(), endColor.red()), 0, 255),
        std::clamp(lerpChannel(startColor.green(), midColor.green(), endColor.green()), 0, 255),
        std::clamp(lerpChannel(startColor.blue(), midColor.blue(), endColor.blue()), 0, 255),
        std::clamp(lerpChannel(startColor.alpha(), midColor.alpha(), endColor.alpha()), 0, 255));
}

float particleStretchFactor(const QVector3D& velocity)
{
    const float speed = velocity.length();
    return std::isfinite(speed)
        ? std::clamp(1.0f + speed * 0.004f, 1.0f, 6.0f) : 1.0f;
}

int clampFlipbookFrame(int frame, int startFrame, int frameCount, int rows, int cols)
{
    const int safeRows = std::clamp(rows, 1, 1024);
    const int safeCols = std::clamp(cols, 1, 1024);
    const int atlasFrames = safeRows * safeCols;
    const int safeStart = std::clamp(startFrame, 0, atlasFrames - 1);
    const int available = std::max(1, std::min(frameCount, atlasFrames - safeStart));
    return safeStart + std::clamp(frame, 0, available - 1);
}

int flipbookFrameCount(int startFrame, int frameCount, int rows, int cols)
{
    const int safeRows = std::clamp(rows, 1, 1024);
    const int safeCols = std::clamp(cols, 1, 1024);
    const int atlasFrames = safeRows * safeCols;
    const int safeStart = std::clamp(startFrame, 0, atlasFrames - 1);
    return std::max(1, std::min(frameCount, atlasFrames - safeStart));
}

QImage loadFlipbookAtlasImage(const QString& path)
{
    if (path.isEmpty()) {
        return {};
    }

    QImage image(path);
    if (image.isNull()) {
        return {};
    }
    return image.convertToFormat(QImage::Format_ARGB32);
}

} // namespace

// ==================== ParticleEffector Base ====================

void ParticleEffector::apply(std::vector<Particle>& particles, float deltaTime)
{
    for (auto& p : particles) {
        if (p.alive && enabled) {
            apply(p, deltaTime);
        }
    }
}

void FlockingEffector::apply(Particle& particle, float deltaTime)
{
    Q_UNUSED(particle);
    Q_UNUSED(deltaTime);
}

void FlockingEffector::apply(std::vector<Particle>& particles, float deltaTime)
{
    if (particles.size() < 2 || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
    const auto isFiniteVector = [](const QVector3D& value) {
        return std::isfinite(value.x()) && std::isfinite(value.y()) && std::isfinite(value.z());
    };
    const float radius = std::isfinite(neighborhoodRadius)
        ? std::clamp(neighborhoodRadius, 0.001f, 1000000.0f)
        : 0.0f;
    if (radius <= 0.0f) return;
    const float radius2 = radius * radius;
    const auto safeWeight = [](float value) {
        return std::isfinite(value)
            ? std::clamp(value, -1000000.0f, 1000000.0f)
            : 0.0f;
    };
    const float safeSeparationWeight = safeWeight(separationWeight);
    const float safeAlignmentWeight = safeWeight(alignmentWeight);
    const float safeCohesionWeight = safeWeight(cohesionWeight);
    const float limit = std::isfinite(maxAcceleration)
        ? std::clamp(maxAcceleration, 0.0f, 1000000.0f)
        : 0.0f;
    std::vector<QVector3D> accelerations(particles.size());
    for (std::size_t i = 0; i < particles.size(); ++i) {
        const Particle& source = particles[i];
        if (!source.alive || !isFiniteVector(source.position) || !isFiniteVector(source.velocity)) continue;
        QVector3D separation, averageVelocity, center;
        int neighbors = 0;
        for (std::size_t j = 0; j < particles.size(); ++j) {
            if (i == j || !particles[j].alive) continue;
            if (!isFiniteVector(particles[j].position) || !isFiniteVector(particles[j].velocity)) continue;
            const QVector3D offset = particles[j].position - source.position;
            const float distance2 = offset.lengthSquared();
            if (distance2 <= 1.0e-6f || distance2 > radius2) continue;
            const float distance = std::sqrt(distance2);
            separation -= offset / std::max(distance, 1.0e-3f);
            averageVelocity += particles[j].velocity;
            center += particles[j].position;
            ++neighbors;
        }
        if (neighbors == 0) continue;
        const float inverseCount = 1.0f / static_cast<float>(neighbors);
        QVector3D acceleration =
            separation * safeSeparationWeight +
            (averageVelocity * inverseCount - source.velocity) * safeAlignmentWeight +
            (center * inverseCount - source.position) * safeCohesionWeight;
        if (!isFiniteVector(acceleration)) {
            acceleration = QVector3D();
        } else if (limit > 0.0f && acceleration.lengthSquared() > limit * limit) {
            acceleration = acceleration.normalized() * limit;
        }
        accelerations[i] = acceleration;
    }
    for (std::size_t i = 0; i < particles.size(); ++i) {
        if (particles[i].alive) particles[i].acceleration += accelerations[i];
    }
}

// ==================== ForceEffector ====================

void ForceEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
    const auto safeComponent = [](float value) {
        return std::isfinite(value)
            ? std::clamp(value, -1000000.0f, 1000000.0f)
            : 0.0f;
    };
    const QVector3D safeForce(
        safeComponent(force.x()), safeComponent(force.y()), safeComponent(force.z()));
    const float safeStrength = std::isfinite(strength)
        ? std::clamp(strength, -1000000.0f, 1000000.0f)
        : 0.0f;
    particle.acceleration += safeForce * safeStrength * deltaTime;
}

// ==================== VortexEffector ====================

void VortexEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
    
    QVector3D toParticle = particle.position - position;
    const float dist = toParticle.length();
    const float safeRadius = std::isfinite(radius)
        ? std::clamp(radius, 0.001f, 1000000.0f)
        : 0.0f;
    const float safeTightness = std::isfinite(tightness)
        ? std::clamp(tightness, 0.0f, 1000000.0f)
        : 0.0f;
    const float safeAngularVelocity = std::isfinite(angularVelocity)
        ? std::clamp(angularVelocity, -1000000.0f, 1000000.0f)
        : 0.0f;
    const float safeStrength = std::isfinite(strength)
        ? std::clamp(strength, -1000000.0f, 1000000.0f)
        : 0.0f;
    
    if (std::isfinite(dist) && dist < safeRadius && dist > 0.1f) {
        // Calculate tangential force
        QVector3D tangent = QVector3D(-toParticle.y(), toParticle.x(), 0).normalized();
        float factor = 1.0f - (dist / safeRadius);
        factor = std::pow(factor, safeTightness);
        
        const float angularVelRad = qDegreesToRadians(safeAngularVelocity * safeStrength);
        particle.velocity += tangent * angularVelRad * factor * deltaTime * 50.0f;
    }
}

// ==================== TurbulenceEffector ====================

void TurbulenceEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
    const float safeFrequency = std::isfinite(frequency)
        ? std::clamp(frequency, 0.0f, 10000.0f)
        : 0.0f;
    const float safeAmplitude = std::isfinite(amplitude)
        ? std::clamp(amplitude, 0.0f, 100000.0f)
        : 0.0f;
    const float safeEvolution = std::isfinite(evolution)
        ? std::clamp(evolution, -1000000.0f, 1000000.0f)
        : 0.0f;
    const float safeStrength = std::isfinite(strength)
        ? std::clamp(strength, -1000000.0f, 1000000.0f)
        : 0.0f;
    
    // Simple noise-based turbulence
    float noiseX = std::sin(particle.position.x() * safeFrequency + safeEvolution + seed) *
                   std::cos(particle.position.y() * safeFrequency * 0.7f);
    float noiseY = std::cos(particle.position.x() * safeFrequency * 0.8f + safeEvolution * 1.3f + seed) *
                   std::sin(particle.position.y() * safeFrequency * 0.9f);
    float noiseZ = std::sin(particle.position.z() * safeFrequency * 0.5f + safeEvolution * 0.7f + seed);
    
    QVector3D turbulenceForce(noiseX, noiseY, noiseZ);
    turbulenceForce *= safeAmplitude * safeStrength;
    
    particle.velocity += turbulenceForce * deltaTime;
}

// ==================== AttractorEffector ====================

void AttractorEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
    
    QVector3D toAttractor = position - particle.position;
    const float dist = toAttractor.length();
    const float safeRadius = std::isfinite(radius)
        ? std::clamp(radius, 0.001f, 1000000.0f)
        : 0.0f;
    const float safeFalloff = std::isfinite(falloff)
        ? std::clamp(falloff, 0.0f, 1000000.0f)
        : 0.0f;
    const float safeKillRadius = std::isfinite(killRadius)
        ? std::clamp(killRadius, 0.0f, 1000000.0f)
        : 0.0f;
    const float safeStrength = std::isfinite(strength)
        ? std::clamp(strength, -1000000.0f, 1000000.0f)
        : 0.0f;
    
    if (std::isfinite(dist) && dist > safeKillRadius && dist < safeRadius) {
        const float factor = 1.0f - std::pow(dist / safeRadius, safeFalloff);
        toAttractor.normalize();
        particle.velocity += toAttractor * safeStrength * factor * 100.0f * deltaTime;
    } else if (std::isfinite(dist) && dist <= safeKillRadius && killOnReach) {
        particle.alive = false;
    }
}

// ==================== RepellerEffector ====================

void RepellerEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
    
    QVector3D fromRepeller = particle.position - position;
    const float dist = fromRepeller.length();
    const float safeRadius = std::isfinite(radius)
        ? std::clamp(radius, 0.001f, 1000000.0f)
        : 0.0f;
    const float safeFalloff = std::isfinite(falloff)
        ? std::clamp(falloff, 0.0f, 1000000.0f)
        : 0.0f;
    const float safeStrength = std::isfinite(strength)
        ? std::clamp(strength, -1000000.0f, 1000000.0f)
        : 0.0f;
    
    if (std::isfinite(dist) && dist < safeRadius && dist > 0.1f) {
        const float factor = 1.0f - std::pow(dist / safeRadius, safeFalloff);
        fromRepeller.normalize();
        particle.velocity += fromRepeller * safeStrength * factor * 100.0f * deltaTime;
    }
}

// ==================== WindEffector ====================

void WindEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
    const auto safeComponent = [](float value) {
        return std::isfinite(value)
            ? std::clamp(value, -1000000.0f, 1000000.0f)
            : 0.0f;
    };
    const QVector3D safeDirection(
        safeComponent(windDirection.x()),
        safeComponent(windDirection.y()),
        safeComponent(windDirection.z()));
    const float safeStrength = std::isfinite(strength)
        ? std::clamp(strength, -1000000.0f, 1000000.0f)
        : 0.0f;
    const float safeWindStrength = std::isfinite(windStrength)
        ? std::clamp(windStrength, -1000000.0f, 1000000.0f)
        : 0.0f;
    const float safeTurbulence = std::isfinite(turbulence)
        ? std::clamp(turbulence, -1000000.0f, 1000000.0f)
        : 0.0f;
    const float safeFrequency = std::isfinite(turbulenceFrequency)
        ? std::clamp(turbulenceFrequency, 0.0f, 10000.0f)
        : 0.0f;
    const float safeEvolution = std::isfinite(evolution)
        ? std::clamp(evolution, -1000000.0f, 1000000.0f)
        : 0.0f;
    
    // Base wind
    QVector3D windForce = safeDirection.normalized() * safeWindStrength * safeStrength;
    
    // Add turbulence
    float noise = std::sin(particle.position.x() * safeFrequency + safeEvolution) *
                  std::cos(particle.position.y() * safeFrequency + safeEvolution * 0.7f);
    windForce += safeDirection * noise * safeTurbulence;
    
    particle.velocity += windForce * deltaTime;
}

// ==================== KillZoneEffector ====================

void KillZoneEffector::apply(Particle& particle, float deltaTime)
{
    Q_UNUSED(deltaTime);
    
    if (!enabled) return;
    const auto finiteComponent = [](float value) {
        return std::isfinite(value)
            ? std::clamp(value, 0.0f, 1000000.0f)
            : 0.0f;
    };
    const auto finiteVector = [](const QVector3D& value) {
        return std::isfinite(value.x()) &&
               std::isfinite(value.y()) &&
               std::isfinite(value.z());
    };
    if (!finiteVector(particle.position) || !finiteVector(position)) return;
    const QVector3D safeSize(
        finiteComponent(size.x()), finiteComponent(size.y()), finiteComponent(size.z()));
    bool inside = false;
    
    switch (zoneType) {
        case ZoneType::Sphere: {
            const float dist = (particle.position - position).length();
            inside = std::isfinite(dist) && dist < safeSize.x();
            break;
        }
        case ZoneType::Box: {
            QVector3D rel = particle.position - position;
            inside = std::abs(rel.x()) < safeSize.x() * 0.5f &&
                    std::abs(rel.y()) < safeSize.y() * 0.5f &&
                    std::abs(rel.z()) < safeSize.z() * 0.5f;
            break;
        }
        case ZoneType::Plane: {
            // Plane defined by position and direction (normal)
            if (!finiteVector(direction) || direction.lengthSquared() <= 1.0e-6f) return;
            const float dist = QVector3D::dotProduct(
                particle.position - position, direction.normalized());
            inside = std::isfinite(dist) && dist < 0.0f;
            break;
        }
        default:
            return;
    }
    
    // Kill if inside and not inverted, or outside and inverted
    if (inside != invert) {
        particle.alive = false;
    }
}

// ==================== ParticleEmitter::Impl ====================

class ParticleEmitter::Impl {
public:
    mutable QRandomGenerator rng;
    float fixedStepAccumulator = 0.0f;
    bool seeded = false;
    std::uint32_t currentSeed = 0;
    bool hasEmitterTransform = false;
    QVector3D lastEmitterPosition{0.0f, 0.0f, 0.0f};
    QVector3D lastEmitterRotation{0.0f, 0.0f, 0.0f};
    QVector3D inheritedVelocity{0.0f, 0.0f, 0.0f};
};

// ==================== ParticleEmitter ====================

ParticleEmitter::ParticleEmitter(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{
    impl_->rng.seed(params_.randomSeed);
    impl_->seeded = true;
    impl_->currentSeed = params_.randomSeed;
}

ParticleEmitter::~ParticleEmitter()
{
}

void ParticleEmitter::addEffector(std::unique_ptr<ParticleEffector> effector)
{
    if (!effector) return;
    effectors_.push_back(std::move(effector));
}

void ParticleEmitter::removeEffector(int index)
{
    if (index >= 0 && index < static_cast<int>(effectors_.size())) {
        effectors_.erase(effectors_.begin() + index);
    }
}

void ParticleEmitter::clearEffectors()
{
    effectors_.clear();
}

namespace {

QMatrix4x4 buildEulerRotationMatrix(const QVector3D& eulerDegrees)
{
    QMatrix4x4 matrix;
    matrix.setToIdentity();
    matrix.rotate(eulerDegrees.x(), 1.0f, 0.0f, 0.0f);
    matrix.rotate(eulerDegrees.y(), 0.0f, 1.0f, 0.0f);
    matrix.rotate(eulerDegrees.z(), 0.0f, 0.0f, 1.0f);
    return matrix;
}

QVector3D rotateByEulerDegrees(const QVector3D& value, const QVector3D& eulerDegrees)
{
    return buildEulerRotationMatrix(eulerDegrees).map(value);
}

} // namespace

QVector3D ParticleEmitter::getEmissionPosition() const
{
    const auto safeComponent = [](float value) {
        return std::isfinite(value)
            ? std::clamp(value, -1000000.0f, 1000000.0f)
            : 0.0f;
    };
    const auto safeVector = [&](const QVector3D& value) {
        return QVector3D(
            safeComponent(value.x()),
            safeComponent(value.y()),
            safeComponent(value.z()));
    };
    const auto safeNonNegative = [&](float value) {
        return std::isfinite(value)
            ? std::clamp(value, 0.0f, 1000000.0f)
            : 0.0f;
    };
    const QVector3D safePosition = safeVector(params_.position);
    const QVector3D safeRotation = safeVector(params_.rotation);
    QVector3D localOffset;
    
    switch (params_.shape) {
        case EmitterShape::Point:
            // No offset
            break;
            
        case EmitterShape::Sphere: {
            float theta = impl_->rng.bounded(360.0f) * M_PI / 180.0f;
            const float cosPhi = impl_->rng.bounded(2.0f) - 1.0f;
            const float sinPhi = std::sqrt(std::max(0.0f, 1.0f - cosPhi * cosPhi));
            const float radius = safeNonNegative(params_.radius);
            float r = radius * std::cbrt(impl_->rng.bounded(1.0f));  // Uniform distribution
            
            localOffset = QVector3D(
                r * sinPhi * std::cos(theta),
                r * sinPhi * std::sin(theta),
                r * cosPhi
            );
            break;
        }
            
        case EmitterShape::Box: {
            const float width = safeNonNegative(params_.width);
            const float height = safeNonNegative(params_.height);
            const float depth = safeNonNegative(params_.depth);
            localOffset = QVector3D(
                (impl_->rng.bounded(1.0f) - 0.5f) * width,
                (impl_->rng.bounded(1.0f) - 0.5f) * height,
                (impl_->rng.bounded(1.0f) - 0.5f) * depth
            );
            break;
        }
            
        case EmitterShape::Circle: {
            float angle = impl_->rng.bounded(360.0f) * M_PI / 180.0f;
            const float radius = safeNonNegative(params_.radius);
            float r = radius * std::sqrt(impl_->rng.bounded(1.0f));
            
            localOffset = QVector3D(
                r * std::cos(angle),
                r * std::sin(angle),
                0
            );
            break;
        }
            
        case EmitterShape::Rectangle: {
            const float width = safeNonNegative(params_.width);
            const float height = safeNonNegative(params_.height);
            localOffset = QVector3D(
                (impl_->rng.bounded(1.0f) - 0.5f) * width,
                (impl_->rng.bounded(1.0f) - 0.5f) * height,
                0
            );
            break;
        }
            
        case EmitterShape::Line: {
            const float lineLength = safeNonNegative(params_.lineLength);
            localOffset = QVector3D(
                (impl_->rng.bounded(1.0f) - 0.5f) * lineLength,
                0,
                0
            );
            break;
        }
            
        default:
            break;
    }
    
    return safePosition + rotateByEulerDegrees(localOffset, safeRotation);
}

QVector3D ParticleEmitter::getEmissionDirection() const
{
    const auto safeComponent = [](float value) {
        return std::isfinite(value)
            ? std::clamp(value, -1000000.0f, 1000000.0f)
            : 0.0f;
    };
    const auto safeVector = [&](const QVector3D& value) {
        return QVector3D(
            safeComponent(value.x()),
            safeComponent(value.y()),
            safeComponent(value.z()));
    };
    QVector3D dir = rotateByEulerDegrees(
        safeVector(params_.direction), safeVector(params_.rotation)).normalized();
    if (dir.lengthSquared() < 1.0e-6f) {
        dir = QVector3D(0.0f, 1.0f, 0.0f);
    }
    
    const float spreadDegrees = std::isfinite(params_.directionSpread)
                                    ? std::clamp(params_.directionSpread, 0.0f, 180.0f)
                                    : 0.0f;
    if (spreadDegrees > 0.0f) {
        float spreadRad = spreadDegrees * M_PI / 180.0f;
        float theta = (impl_->rng.bounded(1.0f) - 0.5f) * spreadRad;
        float phi = (impl_->rng.bounded(1.0f) - 0.5f) * spreadRad;
        
        // Apply spread (simplified cone spread)
        float cosT = std::cos(theta);
        float sinT = std::sin(theta);
        float cosP = std::cos(phi);
        float sinP = std::sin(phi);
        
        // Rotate direction
        QVector3D right = QVector3D::crossProduct(dir, QVector3D(0, 0, 1));
        if (right.length() < 0.01f) {
            right = QVector3D(1, 0, 0);
        }
        right.normalize();
        QVector3D up = QVector3D::crossProduct(right, dir);
        
        dir = dir * cosT * cosP + right * sinT + up * sinP;
    }
    
    return dir.lengthSquared() > 1.0e-6f ? dir.normalized()
                                         : QVector3D(0.0f, 1.0f, 0.0f);
}

void ParticleEmitter::applyEmitterLocalSpaceDelta()
{
    if (!impl_->hasEmitterTransform) {
        impl_->lastEmitterPosition = params_.position;
        impl_->lastEmitterRotation = params_.rotation;
        impl_->hasEmitterTransform = true;
        return;
    }

    const QVector3D oldPosition = impl_->lastEmitterPosition;
    const QVector3D oldRotation = impl_->lastEmitterRotation;
    const QVector3D newPosition = params_.position;
    const QVector3D newRotation = params_.rotation;

    if (oldPosition == newPosition && oldRotation == newRotation) {
        impl_->inheritedVelocity = QVector3D(0.0f, 0.0f, 0.0f);
        return;
    }

    QMatrix4x4 oldRotationMatrix = buildEulerRotationMatrix(oldRotation);
    bool invertible = false;
    QMatrix4x4 oldInverse = oldRotationMatrix.inverted(&invertible);
    QMatrix4x4 newRotationMatrix = buildEulerRotationMatrix(newRotation);

    for (auto& particle : particles_) {
        if (!particle.alive) {
            continue;
        }

        const QVector3D localPosition = invertible
            ? oldInverse.map(particle.position - oldPosition)
            : (particle.position - oldPosition);
        const QVector3D localPrevPosition = invertible
            ? oldInverse.map(particle.prevPosition - oldPosition)
            : (particle.prevPosition - oldPosition);

        particle.position = newPosition + newRotationMatrix.map(localPosition);
        particle.prevPosition = newPosition + newRotationMatrix.map(localPrevPosition);
        particle.velocity = newRotationMatrix.mapVector(
            invertible ? oldInverse.mapVector(particle.velocity) : particle.velocity);
    }

    impl_->lastEmitterPosition = newPosition;
    impl_->lastEmitterRotation = newRotation;
}

void ParticleEmitter::initializeParticle(Particle& p)
{
    if (!impl_->seeded || impl_->currentSeed != params_.randomSeed) {
        impl_->rng.seed(params_.randomSeed);
        impl_->seeded = true;
        impl_->currentSeed = params_.randomSeed;
    }

    const auto randomInRange = [&](float minimum, float maximum,
                                   float fallback = 0.0f) {
        if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
            return std::isfinite(fallback) ? fallback : 0.0f;
        }
        const float safeMinimum = std::clamp(minimum, -1000000.0f, 1000000.0f);
        const float safeMaximum = std::clamp(maximum, safeMinimum, 1000000.0f);
        if (safeMaximum <= safeMinimum) return safeMinimum;
        return safeMinimum + impl_->rng.bounded(safeMaximum - safeMinimum);
    };

    // Position
    p.position = getEmissionPosition();
    p.prevPosition = p.position;
    
    // Velocity
    const float speed = randomInRange(params_.speedMin, params_.speedMax);
    QVector3D dir = getEmissionDirection();
    p.velocity = dir * speed;
    p.velocity += QVector3D(
        (impl_->rng.bounded(1.0f) - 0.5f) * 2.0f * params_.velocityRandom.x(),
        (impl_->rng.bounded(1.0f) - 0.5f) * 2.0f * params_.velocityRandom.y(),
        (impl_->rng.bounded(1.0f) - 0.5f) * 2.0f * params_.velocityRandom.z());
    if (params_.inheritVelocity) {
        p.velocity += impl_->inheritedVelocity;
    }
    p.acceleration = QVector3D(0, 0, 0);
    
    // Rotation
    p.rotation = randomInRange(params_.rotationMin, params_.rotationMax);
    p.rotationSpeed = randomInRange(
        params_.rotationSpeedMin, params_.rotationSpeedMax);
    
    // Scale
    p.scaleStart = randomInRange(params_.scaleMin, params_.scaleMax, 1.0f);
    p.customData[0] = randomInRange(
        params_.scaleMidMin, params_.scaleMidMax, 1.0f);
    p.scaleEnd = randomInRange(params_.scaleEndMin, params_.scaleEndMax, 1.0f);
    p.scale = p.scaleStart;
    
    // Color
    p.colorStart = params_.colorStart;
    p.colorMid = params_.colorMid;
    p.colorEnd = params_.colorEnd;
    
    // Apply color variation
    const float safeColorVariation = std::isfinite(params_.colorVariation)
        ? std::clamp(params_.colorVariation, 0.0f, 1.0f)
        : 0.0f;
    if (safeColorVariation > 0.0f) {
        const float var = safeColorVariation;
        auto variation = [&]() -> float {
            return static_cast<float>(impl_->rng.generateDouble() * (2.0 * var) - var);
        };
        int r = std::clamp(p.colorStart.red() + static_cast<int>(variation() * 255.0f), 0, 255);
        int g = std::clamp(p.colorStart.green() + static_cast<int>(variation() * 255.0f), 0, 255);
        int b = std::clamp(p.colorStart.blue() + static_cast<int>(variation() * 255.0f), 0, 255);
        p.colorStart = QColor(r, g, b, p.colorStart.alpha());
        p.colorMid = QColor(
            std::clamp(p.colorMid.red() + static_cast<int>(variation() * 255.0f), 0, 255),
            std::clamp(p.colorMid.green() + static_cast<int>(variation() * 255.0f), 0, 255),
            std::clamp(p.colorMid.blue() + static_cast<int>(variation() * 255.0f), 0, 255),
            p.colorMid.alpha());
    }
    
    p.color = p.colorStart;
    
    // Opacity
    p.opacityStart = randomInRange(params_.opacityMin, params_.opacityMax, 1.0f);
    p.customData[1] = randomInRange(
        params_.opacityMidMin, params_.opacityMidMax, 1.0f);
    p.opacityEnd = randomInRange(
        params_.opacityEndMin, params_.opacityEndMax);
    p.opacity = p.opacityStart;
    
    // Lifetime
    p.maxLife = randomInRange(params_.lifeMin, params_.lifeMax, 1.0f);
    p.life = 1.0f;
    p.age = 0.0f;

    // Flipbook
    p.spriteRows = std::max(1, params_.textureRows);
    p.spriteCols = std::max(1, params_.textureCols);
    const int availableFrames = flipbookFrameCount(
        params_.startFrame,
        params_.frameCount,
        p.spriteRows,
        p.spriteCols);
    const int initialFrame = params_.randomFrame
        ? impl_->rng.bounded(availableFrames)
        : 0;
    p.spriteFrame = clampFlipbookFrame(
        initialFrame,
        params_.startFrame,
        params_.frameCount,
        p.spriteRows,
        p.spriteCols);
    
    // ID
    p.id = nextParticleId_++;
    p.alive = true;
    p.active = true;
    p.customData[2] = std::max(0.001f, params_.auxInterval);
    p.customData[3] = 0.0f;
}

void ParticleEmitter::emitParticles(int count)
{
    if (count <= 0 || params_.maxParticles <= 0) {
        return;
    }
    int available = std::max(
        0, params_.maxParticles - static_cast<int>(particles_.size()));
    int toEmit = std::min(count, available);
    
    for (int i = 0; i < toEmit; i++) {
        Particle p;
        initializeParticle(p);
        particles_.push_back(p);
        if (params_.auxEnabled && params_.auxTrigger == AuxTriggerMode::Birth) {
            const Particle source = particles_.back();
            emitAuxParticlesFromParticle(source, std::max(0, params_.auxCount));
        }
    }
    
    if (toEmit > 0) {
        Q_EMIT particleEmitted(toEmit);
    }
}

void ParticleEmitter::emitAuxParticlesFromParticle(const Particle& source, int count)
{
    if (!params_.auxEnabled || count <= 0) {
        return;
    }

    int available = params_.maxParticles - static_cast<int>(particles_.size());
    int toEmit = std::min(count, available);
    for (int i = 0; i < toEmit; ++i) {
        Particle p;
        initializeParticle(p);
        p.position = source.position;
        p.prevPosition = source.prevPosition;
        p.velocity = source.velocity * std::max(0.0f, params_.auxVelocityScale);
        p.acceleration = QVector3D(0, 0, 0);
        const float auxSizeScale = std::max(0.0f, params_.auxSizeScale);
        const float auxOpacityScale = std::max(0.0f, params_.auxOpacityScale);
        const float auxLifeScale = std::max(0.01f, params_.auxLifeScale);
        p.scaleStart = std::max(0.01f, source.scale * auxSizeScale);
        p.scale = p.scaleStart;
        p.customData[0] = std::max(0.0f, p.scaleStart * 0.8f);
        p.scaleEnd = std::max(0.0f, source.scale * 0.1f);
        p.opacityStart = std::clamp(source.opacity * auxOpacityScale, 0.0f, 1.0f);
        p.opacity = p.opacityStart;
        p.customData[1] = std::clamp(p.opacityStart * 0.45f, 0.0f, 1.0f);
        p.opacityEnd = 0.0f;
        p.colorStart = scaledAlphaColor(source.color, auxOpacityScale);
        p.colorMid = scaledAlphaColor(source.colorMid, auxOpacityScale * 0.8f);
        p.color = p.colorStart;
        p.colorEnd = scaledAlphaColor(source.colorEnd, 0.0f);
        p.maxLife = std::max(0.03f, source.maxLife * auxLifeScale);
        p.age = 0.0f;
        p.life = 1.0f;
        p.customData[2] = std::max(0.001f, params_.auxInterval);
        p.customData[3] = 1.0f;
        particles_.push_back(p);
    }

    if (toEmit > 0) {
        Q_EMIT particleEmitted(toEmit);
    }
}

void ParticleEmitter::emitBurst()
{
    emitParticles(params_.burstCount);
}

void ParticleEmitter::updateParticle(Particle& p, float deltaTime)
{
    const auto discardInvalidParticle = [&]() {
        p.alive = false;
        Q_EMIT particleDied(p.id);
    };
    const auto finiteVector = [](const QVector3D& value) {
        return std::isfinite(value.x()) &&
               std::isfinite(value.y()) &&
               std::isfinite(value.z());
    };
    // Store previous position
    p.prevPosition = p.position;
    
    // Update age and life
    p.age += deltaTime;
    if (!std::isfinite(p.age)) {
        discardInvalidParticle();
        return;
    }
    const float safeMaxLife = std::isfinite(p.maxLife)
                                  ? std::max(0.001f, p.maxLife)
                                  : 0.001f;
    p.maxLife = safeMaxLife;
    p.life = 1.0f - (p.age / safeMaxLife);
    
    if (p.life <= 0.0f) {
        const Particle source = p;
        p.alive = false;
        if (params_.auxEnabled &&
            params_.auxTrigger == AuxTriggerMode::Death &&
            p.customData[3] < 0.5f) {
            emitAuxParticlesFromParticle(source, std::max(0, params_.auxCount));
        }
        Q_EMIT particleDied(source.id);
        return;
    }
    
    // Apply emitter-level physics with simple mass scaling.
    const float inverseMass = 1.0f / std::max(0.01f, params_.mass);

    // Apply physics
    p.velocity += params_.gravity * deltaTime * inverseMass;
    if (params_.windStrength != 0.0f && !params_.windDirection.isNull()) {
        QVector3D wind = params_.windDirection.normalized() * params_.windStrength * deltaTime * inverseMass;
        p.velocity += wind;
    }
    if (params_.turbulenceAmplitude != 0.0f && params_.turbulenceFrequency != 0.0f) {
        const float frequency = params_.turbulenceFrequency;
        const float evolution = params_.turbulenceEvolution;
        const float noiseX = std::sin(p.position.x() * frequency + evolution) *
                             std::cos(p.position.y() * frequency * 0.7f);
        const float noiseY = std::cos(p.position.x() * frequency * 0.8f + evolution * 1.3f) *
                             std::sin(p.position.y() * frequency * 0.9f);
        const float noiseZ = std::sin(p.position.z() * frequency * 0.5f + evolution * 0.7f);
        p.velocity += QVector3D(noiseX, noiseY, noiseZ) * params_.turbulenceAmplitude * deltaTime * inverseMass;
    }
    p.velocity += p.acceleration * deltaTime;
    
    // Apply drag
    if (params_.drag > 0.0f) {
        p.velocity *= (1.0f - params_.drag * deltaTime);
    }
    
    p.prevPosition = p.position;
    p.position += p.velocity * deltaTime;
    p.acceleration = QVector3D(0, 0, 0);
    
    // Update rotation
    p.rotation += p.rotationSpeed * deltaTime;
    if (!finiteVector(p.position) || !finiteVector(p.velocity) ||
        !std::isfinite(p.rotation)) {
        discardInvalidParticle();
        return;
    }
    
    // Interpolate properties based on life
    float t = 1.0f - p.life;  // 0 = just born, 1 = about to die
    
    // Scale interpolation
    p.scale = evaluateTriStageLinear(
        p.scaleStart,
        p.customData[0],
        p.scaleEnd,
        params_.scaleMidPosition,
        t);
    
    // Color interpolation
    p.color = evaluateTriStageColor(
        p.colorStart,
        p.colorMid,
        p.colorEnd,
        params_.colorMidPosition,
        t);
    
    // Opacity interpolation
    p.opacity = evaluateTriStageLinear(
        p.opacityStart,
        p.customData[1],
        p.opacityEnd,
        params_.opacityMidPosition,
        t);
    p.scale = std::isfinite(p.scale)
        ? std::clamp(p.scale, 0.0f, 1000000.0f)
        : 0.0f;
    p.opacity = std::isfinite(p.opacity)
        ? std::clamp(p.opacity, 0.0f, 1.0f)
        : 0.0f;

    const float safeFrameRate = std::isfinite(params_.frameRate)
        ? std::clamp(params_.frameRate, 0.0f, 1000.0f)
        : 0.0f;
    if (!params_.randomFrame && safeFrameRate > 0.0f) {
        const int availableFrames = flipbookFrameCount(
            params_.startFrame,
            params_.frameCount,
            p.spriteRows,
            p.spriteCols);
        const double frameProgress = std::floor(
            static_cast<double>(p.age) * static_cast<double>(safeFrameRate));
        const double wrappedFrame = std::fmod(
            frameProgress, static_cast<double>(availableFrames));
        const int frameOffset = static_cast<int>(std::clamp(
            wrappedFrame, 0.0, static_cast<double>(availableFrames - 1)));
        p.spriteFrame = clampFlipbookFrame(
            frameOffset,
            params_.startFrame,
            params_.frameCount,
            p.spriteRows,
            p.spriteCols);
    }

    if (params_.auxEnabled &&
        params_.auxTrigger == AuxTriggerMode::Trails &&
        p.customData[3] < 0.5f) {
        const float interval = std::max(0.001f, params_.auxInterval);
        if (p.age >= p.customData[2]) {
            const Particle source = p;
            p.customData[2] += interval;
            emitAuxParticlesFromParticle(source, std::max(0, params_.auxCount));
        }
    }
}

void ParticleEmitter::applyEffectors(Particle& p, float deltaTime)
{
    for (const auto& effector : effectors_) {
        if (effector && effector->enabled) {
            effector->apply(p, deltaTime);
        }
    }
}

void ParticleEmitter::removeDeadParticles()
{
    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
            [](const Particle& p) { return !p.alive; }),
        particles_.end()
    );
}

void ParticleEmitter::applySelfCollisionBroadPhase(float deltaTime)
{
    if (!params_.enableSelfCollision || particles_.size() < 2) {
        return;
    }

    const float radius = std::isfinite(params_.selfCollisionRadius)
        ? std::clamp(params_.selfCollisionRadius, 0.001f, 1000000.0f)
        : 0.001f;
    const float radius2 = radius * radius;
    const float cellSize = radius * 2.0f;
    const float response = std::isfinite(params_.selfCollisionResponse)
        ? std::clamp(params_.selfCollisionResponse, 0.0f, 1.0f)
        : 0.0f;

    struct CellKey {
        int x = 0;
        int y = 0;
        int z = 0;
        bool operator<(const CellKey& rhs) const {
            if (x != rhs.x) return x < rhs.x;
            if (y != rhs.y) return y < rhs.y;
            return z < rhs.z;
        }
    };

    std::map<CellKey, std::vector<int>> grid;
    const auto safeCellCoordinate = [cellSize](float value) {
        if (!std::isfinite(value)) return 0;
        const double coordinate = std::floor(
            static_cast<double>(value) / static_cast<double>(cellSize));
        const double minimum = static_cast<double>(std::numeric_limits<int>::min() + 1);
        const double maximum = static_cast<double>(std::numeric_limits<int>::max() - 1);
        return static_cast<int>(std::clamp(coordinate, minimum, maximum));
    };
    for (int i = 0; i < static_cast<int>(particles_.size()); ++i) {
        if (!particles_[i].alive) continue;
        const QVector3D& pos = particles_[i].position;
        if (!std::isfinite(pos.x()) || !std::isfinite(pos.y()) || !std::isfinite(pos.z())) {
            continue;
        }
        CellKey key{
            safeCellCoordinate(pos.x()),
            safeCellCoordinate(pos.y()),
            safeCellCoordinate(pos.z())
        };
        grid[key].push_back(i);
    }

    for (auto& [cell, indices] : grid) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    CellKey nearCell{cell.x + dx, cell.y + dy, cell.z + dz};
                    auto found = grid.find(nearCell);
                    if (found == grid.end()) {
                        continue;
                    }

                    const bool sameCell = (dx == 0 && dy == 0 && dz == 0);
                    auto& nearIndices = found->second;

                    for (int iIdx = 0; iIdx < static_cast<int>(indices.size()); ++iIdx) {
                        int i = indices[iIdx];
                        int jStart = 0;
                        if (sameCell) {
                            jStart = iIdx + 1;
                        }
                        for (int jIdx = jStart; jIdx < static_cast<int>(nearIndices.size()); ++jIdx) {
                            int j = nearIndices[jIdx];
                            if (i >= j && !sameCell) {
                                continue;
                            }
                            if (!particles_[i].alive || !particles_[j].alive) {
                                continue;
                            }

                            QVector3D delta = particles_[i].position - particles_[j].position;
                            float dist2 = delta.lengthSquared();
                            if (!std::isfinite(dist2) || dist2 <= 0.0f || dist2 >= radius2) {
                                continue;
                            }

                            float dist = std::sqrt(dist2);
                            float penetration = radius - dist;
                            if (penetration <= 0.0f) {
                                continue;
                            }

                            QVector3D normal = delta / dist;
                            QVector3D correction = normal * (penetration * 0.5f);
                            particles_[i].position += correction;
                            particles_[j].position -= correction;

                            QVector3D impulse = normal * (penetration * response / std::max(deltaTime, 1e-6f));
                            particles_[i].velocity += impulse;
                            particles_[j].velocity -= impulse;
                        }
                    }
                }
            }
        }
    }
}

void ParticleEmitter::simulateStep(float deltaTime)
{
    // Emit new particles
    switch (params_.mode) {
        case EmissionMode::Continuous: {
            const float emissionRate = std::isfinite(params_.rate)
                                           ? std::max(0.0f, params_.rate)
                                           : 0.0f;
            emitAccumulator_ += deltaTime * emissionRate;
            if (!std::isfinite(emitAccumulator_)) {
                emitAccumulator_ = 0.0f;
            }
            int toEmit = 0;
            constexpr float kSafeIntCastLimit = 2147483000.0f;
            if (emitAccumulator_ >= kSafeIntCastLimit) {
                toEmit = std::max(0, params_.maxParticles);
                emitAccumulator_ = 0.0f;
            } else {
                toEmit = static_cast<int>(emitAccumulator_);
            }
            if (toEmit > 0) {
                emitParticles(toEmit);
                if (emitAccumulator_ > 0.0f) {
                    emitAccumulator_ -= toEmit;
                }
            }
            break;
        }
        case EmissionMode::Burst: {
            const float burstInterval = std::isfinite(params_.burstInterval)
                                             ? std::max(0.0f, params_.burstInterval)
                                             : 0.0f;
            burstTimer_ += deltaTime;
            if (burstTimer_ >= burstInterval) {
                emitBurst();
                burstTimer_ = 0.0f;
            }
            break;
        }
        case EmissionMode::Triggered:
            // Manual emission only
            break;
    }

    // Phase 1a: vector effectors such as Boids/Flocking.
    for (const auto& effector : effectors_) {
        if (effector && effector->enabled) effector->apply(particles_, deltaTime);
    }

    // Phase 1b: effectors + integration (particle-local)
    // updateParticle() may append auxiliary particles. Process only the
    // particles that existed at the start of this phase so vector reallocation
    // cannot invalidate the range-for reference, and new aux particles start
    // on the next simulation step.
    const std::size_t particleCountAtPhaseStart = particles_.size();
    for (std::size_t particleIndex = 0;
         particleIndex < particleCountAtPhaseStart &&
         particleIndex < particles_.size();
         ++particleIndex) {
        auto& p = particles_[particleIndex];
        if (!p.alive) continue;
        applyEffectors(p, deltaTime);
        updateParticle(p, deltaTime);
    }

    // Phase 2: broad-phase collision (deterministic ordered grid)
    applySelfCollisionBroadPhase(deltaTime);

    // Phase 3: compact dead particles
    removeDeadParticles();
}

void ParticleEmitter::update(float deltaTime)
{
    if (!active_) return;
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

    const auto finiteVector = [](const QVector3D& value) {
        return std::isfinite(value.x()) &&
               std::isfinite(value.y()) &&
               std::isfinite(value.z());
    };
    const bool validPosition = finiteVector(params_.position);
    const bool validRotation = finiteVector(params_.rotation);
    const QVector3D currentPosition = validPosition
        ? params_.position
        : impl_->lastEmitterPosition;
    const QVector3D currentRotation = validRotation
        ? params_.rotation
        : impl_->lastEmitterRotation;

    if (!impl_->hasEmitterTransform) {
        impl_->lastEmitterPosition = currentPosition;
        impl_->lastEmitterRotation = currentRotation;
        impl_->hasEmitterTransform = true;
    }

    if (validPosition) {
        const QVector3D deltaPosition = currentPosition - impl_->lastEmitterPosition;
        const auto safeVelocityComponent = [deltaTime](float value) {
            const float raw = value / deltaTime;
            return std::isfinite(raw)
                ? std::clamp(raw, -1000000.0f, 1000000.0f)
                : 0.0f;
        };
        impl_->inheritedVelocity = QVector3D(
            safeVelocityComponent(deltaPosition.x()),
            safeVelocityComponent(deltaPosition.y()),
            safeVelocityComponent(deltaPosition.z()));
    } else {
        impl_->inheritedVelocity = QVector3D(0.0f, 0.0f, 0.0f);
    }

    if (!validPosition || !validRotation) {
        impl_->lastEmitterPosition = currentPosition;
        impl_->lastEmitterRotation = currentRotation;
    } else if (!params_.worldSpace) {
        applyEmitterLocalSpaceDelta();
    } else if (impl_->lastEmitterPosition != currentPosition ||
               impl_->lastEmitterRotation != currentRotation) {
        impl_->lastEmitterPosition = currentPosition;
        impl_->lastEmitterRotation = currentRotation;
    }

    if (!std::isfinite(time_) || time_ < 0.0f) time_ = 0.0f;
    time_ = std::min(
        time_ + deltaTime,
        std::numeric_limits<float>::max());
    if (!std::isfinite(time_)) time_ = std::numeric_limits<float>::max();

    const float fixedDt = std::isfinite(params_.fixedTimeStep)
        ? std::clamp(params_.fixedTimeStep, 0.000001f, 1.0f)
        : (1.0f / 120.0f);
    const int maxSubSteps = std::clamp(params_.maxSubSteps, 1, 256);
    if (params_.deterministic && fixedDt > 0.0f) {
        impl_->fixedStepAccumulator += deltaTime;
        if (!std::isfinite(impl_->fixedStepAccumulator) ||
            impl_->fixedStepAccumulator < 0.0f) {
            impl_->fixedStepAccumulator = 0.0f;
        }

        const double availableSteps = std::floor(
            static_cast<double>(impl_->fixedStepAccumulator) /
            static_cast<double>(fixedDt));
        const int steps = static_cast<int>(std::clamp(
            availableSteps, 0.0, static_cast<double>(maxSubSteps)));

        for (int i = 0; i < steps; ++i) {
            simulateStep(fixedDt);
        }

        impl_->fixedStepAccumulator -= steps * fixedDt;
        const float maxCarry = fixedDt * maxSubSteps;
        if (impl_->fixedStepAccumulator > maxCarry) {
            impl_->fixedStepAccumulator = maxCarry;
        }
        return;
    }

    simulateStep(deltaTime);
}

void ParticleEmitter::clear()
{
    particles_.clear();
    nextParticleId_ = 0;
    time_ = 0.0f;
    emitAccumulator_ = 0.0f;
    burstTimer_ = 0.0f;
    impl_->fixedStepAccumulator = 0.0f;
    impl_->rng.seed(params_.randomSeed);
    impl_->seeded = true;
    impl_->currentSeed = params_.randomSeed;
    impl_->hasEmitterTransform = false;
    impl_->lastEmitterPosition = params_.position;
    impl_->lastEmitterRotation = params_.rotation;
    impl_->inheritedVelocity = QVector3D(0.0f, 0.0f, 0.0f);
}

void ParticleEmitter::preWarm(float duration, float stepSize)
{
    if (!std::isfinite(duration) || duration <= 0.0f ||
        !std::isfinite(stepSize) || stepSize <= 0.0f) {
        return;
    }
    const float safeDuration = std::min(duration, 1000000.0f);
    const float safeStepSize = std::clamp(stepSize, 0.000001f, 1.0f);
    clear();
    
    double time = 0.0;
    while (time < static_cast<double>(safeDuration)) {
        const float dt = static_cast<float>(std::min(
            static_cast<double>(safeStepSize),
            static_cast<double>(safeDuration) - time));
        if (!std::isfinite(dt) || dt <= 0.0f) break;
        update(dt);
        time += dt;
    }
}

// ==================== ParticleSystem::Impl ====================

class ParticleSystem::Impl {
public:
    QVector3D cameraPosition;
    std::map<QString, QImage> flipbookAtlases;
};

// ==================== ParticleSystem ====================

ParticleSystem::ParticleSystem(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{
}

ParticleSystem::~ParticleSystem()
{
}

ParticleEmitter* ParticleSystem::createEmitter()
{
    auto emitter = std::make_unique<ParticleEmitter>(this);
    ParticleEmitter* ptr = emitter.get();
    emitters_.push_back(std::move(emitter));
    Q_EMIT emitterAdded(ptr);
    return ptr;
}

void ParticleSystem::removeEmitter(ParticleEmitter* emitter)
{
    if (!emitter) return;
    const auto found = std::find_if(
        emitters_.begin(), emitters_.end(),
        [emitter](const std::unique_ptr<ParticleEmitter>& candidate) {
            return candidate && candidate.get() == emitter;
        });
    if (found == emitters_.end()) return;
    auto removed = std::move(*found);
    emitters_.erase(found);
    Q_EMIT emitterRemoved(removed.get());
}

void ParticleSystem::removeEmitter(int index)
{
    if (index >= 0 && index < static_cast<int>(emitters_.size())) {
        auto removed = std::move(emitters_[index]);
        if (!removed) return;
        ParticleEmitter* ptr = removed.get();
        emitters_.erase(emitters_.begin() + index);
        Q_EMIT emitterRemoved(ptr);
    }
}

void ParticleSystem::clearEmitters()
{
    emitters_.clear();
}

int ParticleSystem::totalParticleCount() const
{
    int count = 0;
    for (const auto& emitter : emitters_) {
        if (!emitter) continue;
        const int emitterCount = std::max(0, emitter->particleCount());
        if (emitterCount > std::numeric_limits<int>::max() - count) {
            return std::numeric_limits<int>::max();
        }
        count += emitterCount;
    }
    return count;
}

void ParticleSystem::update(float deltaTime)
{
    if (paused_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
    
    const float scaledDelta = deltaTime * timeScale_;
    if (!std::isfinite(scaledDelta) || scaledDelta <= 0.0f) return;
    if (!std::isfinite(time_) || time_ < 0.0f) time_ = 0.0f;
    time_ = std::min(
        time_ + scaledDelta,
        std::numeric_limits<float>::max());
    if (!std::isfinite(time_)) time_ = std::numeric_limits<float>::max();
    
    for (auto& emitter : emitters_) {
        if (emitter) emitter->update(scaledDelta);
    }
    
    Q_EMIT updated(deltaTime);
}

void ParticleSystem::reset()
{
    time_ = 0.0f;
    for (auto& emitter : emitters_) {
        if (emitter) emitter->clear();
    }
}

void ParticleSystem::goToFrame(int64_t frame, double fps)
{
    reset();
    if (frame < 0 || !std::isfinite(fps) || fps <= 0.0) return;

    // 冒頭フレーム（frame <= 1）で preWarm を要求するエミッタがある場合だけ、
    // 本来のシミュレーションに先立って短時間のプリウォームを行う。
    // これにより rate の低いエミッタでも冒頭フレームから十分な粒子が描画される。
    // frame > 1 ではスキップするので、タイムライン途中の見た目は変わらない。
    if (frame <= 1) {
        bool anyPreWarm = false;
        for (const auto& emitter : emitters_) {
            if (emitter && emitter->params().preWarm) {
                anyPreWarm = true;
                break;
            }
        }
        if (anyPreWarm) {
            // preWarm() は内部で clear()+update() を呼ぶが、直前の reset() と
            // 重複しても副作用はない。プリウォーム後の粒子状態を保持したまま
            // 以下の通常ループへ進むため、ここでは各エミッタの preWarm ではなく
            // update() を直接回して状態を温める。
            constexpr float kPreWarmDuration = 0.5f; // 秒
            const float stepSize = 1.0f / 120.0f;    // 決定論的な基本刻み
            float warmTime = 0.0f;
            while (warmTime < kPreWarmDuration) {
                const float dt = std::min(stepSize, kPreWarmDuration - warmTime);
                for (auto& emitter : emitters_) {
                    if (emitter && emitter->params().preWarm) {
                        emitter->update(dt);
                    }
                }
                warmTime += dt;
            }
            // プリウォームで進めた時刻を time_ に反映させない（frame 時刻は下で上書き）
        }
    }

    // 固定ステップでシミュレートしてターゲット時間に到達させる
    const double rawTargetTime = static_cast<double>(frame) / fps;
    if (!std::isfinite(rawTargetTime) || rawTargetTime <= 0.0) {
        return;
    }
    constexpr double kMaxSimulationTime = 1000000.0;
    const double targetTime = std::min(rawTargetTime, kMaxSimulationTime);
    const float stepSize = 1.0f / 120.0f; // 決定論的な基本刻み

    double currentTime = 0.0;
    while (currentTime < targetTime) {
        const float dt = static_cast<float>(std::min(
            static_cast<double>(stepSize), targetTime - currentTime));
        if (!std::isfinite(dt) || dt <= 0.0f) {
            break;
        }
        // Emitter 内の update ロジックをそのまま呼ぶ
        for (auto& emitter : emitters_) {
            if (emitter) emitter->update(dt);
        }
        currentTime += dt;
    }
    time_ = std::isfinite(currentTime)
        ? static_cast<float>(std::min(
            currentTime,
            static_cast<double>(std::numeric_limits<float>::max())))
        : 0.0f;
}

ParticleRenderData ParticleSystem::captureRenderData() const
{
    ParticleRenderData data;
    constexpr int kCaptureReserveLimit = 1000000;
    data.particles.reserve(static_cast<std::size_t>(std::min(
        totalParticleCount(), kCaptureReserveLimit)));
    
    for (const auto& emitter : emitters_) {
        if (!emitter) continue;
        for (const auto& p : emitter->particles()) {
            if (p.alive) {
                const auto finiteVector = [](const QVector3D& value) {
                    return std::isfinite(value.x()) &&
                           std::isfinite(value.y()) &&
                           std::isfinite(value.z());
                };
                if (!finiteVector(p.position) || !finiteVector(p.velocity)) continue;
                const auto safeNonNegative = [](float value, float fallback = 0.0f) {
                    return std::isfinite(value)
                        ? std::clamp(value, 0.0f, 1000000.0f)
                        : fallback;
                };
                ParticleRenderData::Vertex v;
                v.px = p.position.x();
                v.py = p.position.y();
                v.pz = p.position.z();
                v.vx = p.velocity.x();
                v.vy = p.velocity.y();
                v.vz = p.velocity.z();
                v.r  = p.color.redF();
                v.g  = p.color.greenF();
                v.b  = p.color.blueF();
                v.a  = std::clamp(p.color.alphaF() * safeNonNegative(p.opacity), 0.0f, 1.0f);
                v.size = safeNonNegative(p.scale);
                const float velocityStretch = particleStretchFactor(p.velocity);
                v.stretch = renderSettings_.stretchEnabled
                    ? std::max(1.0f, velocityStretch * safeNonNegative(renderSettings_.stretchFactor))
                    : 1.0f;
                v.rotation = std::isfinite(p.rotation) ? p.rotation : 0.0f;
                v.age = safeNonNegative(p.age);
                v.lifetime = std::max(0.001f, safeNonNegative(p.maxLife, 0.001f));
                v.spriteRows = std::clamp(p.spriteRows, 1, 1024);
                v.spriteCols = std::clamp(p.spriteCols, 1, 1024);
                v.spriteFrame = std::clamp(
                    p.spriteFrame, 0, v.spriteRows * v.spriteCols - 1);
                data.particles.push_back(v);
            }
        }
    }
    
    return data;
}

QImage ParticleSystem::updateAndRenderSoftwareFrame(float deltaTime, int width, int height, const QColor& clearColor)
{
    if (width <= 0 || height <= 0) {
        return QImage();
    }
    width = std::clamp(width, 1, 16384);
    height = std::clamp(height, 1, 16384);

    update(deltaTime);

    QImage target(width, height, QImage::Format_ARGB32_Premultiplied);
    target.fill(qPremultiply(clearColor.rgba()));

    struct SoftwareParticle {
        const Particle* particle = nullptr;
        const EmitterParams* emitterParams = nullptr;
    };

    std::vector<SoftwareParticle> allParticles;
    for (const auto& emitter : emitters_) {
        if (!emitter) continue;
        for (const auto& p : emitter->particles()) {
            const auto finiteVector = [](const QVector3D& value) {
                return std::isfinite(value.x()) &&
                       std::isfinite(value.y()) &&
                       std::isfinite(value.z());
            };
            if (p.alive && finiteVector(p.position) &&
                finiteVector(p.velocity) && finiteVector(p.prevPosition) &&
                std::isfinite(p.scale) && std::isfinite(p.opacity)) {
                allParticles.push_back({&p, &emitter->params()});
            }
        }
    }

    std::sort(allParticles.begin(), allParticles.end(),
        [this](const SoftwareParticle& a, const SoftwareParticle& b) {
            const float distA = (a.particle->position - impl_->cameraPosition).lengthSquared();
            const float distB = (b.particle->position - impl_->cameraPosition).lengthSquared();
            return distA > distB; // far to near
        });

    const float fovDeg = 60.0f;
    const float nearPlane = 1.0f;
    const float halfH = static_cast<float>(height) * 0.5f;
    const float halfW = static_cast<float>(width) * 0.5f;
    const float focal = halfH / std::tan(qDegreesToRadians(fovDeg * 0.5f));

    const auto blendPixel = [this](QRgb& dst, int srcR, int srcG, int srcB, int srcA) {
        if (srcA <= 0) return;

        int dA = qAlpha(dst);
        int dR = qRed(dst);
        int dG = qGreen(dst);
        int dB = qBlue(dst);

        switch (renderSettings_.blendMode) {
            case ParticleBlendMode::Additive:
            case ParticleBlendMode::Screen: {
                dR = std::min(255, dR + srcR);
                dG = std::min(255, dG + srcG);
                dB = std::min(255, dB + srcB);
                dA = std::min(255, dA + srcA);
                dst = qRgba(dR, dG, dB, dA);
                break;
            }
            case ParticleBlendMode::Normal:
            case ParticleBlendMode::Multiply:
            case ParticleBlendMode::Subtractive:
            default: {
                const int invA = 255 - srcA;
                const int outA = srcA + (dA * invA + 127) / 255;
                const int outR = srcR + (dR * invA + 127) / 255;
                const int outG = srcG + (dG * invA + 127) / 255;
                const int outB = srcB + (dB * invA + 127) / 255;
                dst = qRgba(outR, outG, outB, outA);
                break;
            }
        }
    };

    auto* scan = reinterpret_cast<QRgb*>(target.bits());
    const int stride = target.bytesPerLine() / static_cast<int>(sizeof(QRgb));

    for (const SoftwareParticle& item : allParticles) {
        const Particle* p = item.particle;
        const EmitterParams* emitterParams = item.emitterParams;
        const QColor particleColor = p->color;
        const QVector3D view = p->position - impl_->cameraPosition;
        const float depth = view.z();
        if (depth <= nearPlane) {
            continue;
        }

        const float invDepth = 1.0f / depth;
        const float sx = halfW + view.x() * focal * invDepth;
        const float sy = halfH - view.y() * focal * invDepth;
        if (!std::isfinite(sx) || !std::isfinite(sy)) {
            continue;
        }
        constexpr double kPixelCastMargin = 1000001.0;
        const double minPixel =
            static_cast<double>(std::numeric_limits<int>::min()) + kPixelCastMargin;
        const double maxPixel =
            static_cast<double>(std::numeric_limits<int>::max()) - kPixelCastMargin;
        const int px = static_cast<int>(std::clamp(
            std::round(static_cast<double>(sx)), minPixel, maxPixel));
        const int py = static_cast<int>(std::clamp(
            std::round(static_cast<double>(sy)), minPixel, maxPixel));

        const float safeScale = std::clamp(p->scale, 0.0f, 1000000.0f);
        const float projectedRadius = std::clamp(
            safeScale * 12.0f * focal * invDepth, 1.0f, 1000000.0f);
        const float safeStretchFactor = std::isfinite(renderSettings_.stretchFactor)
            ? std::clamp(renderSettings_.stretchFactor, 0.0f, 1000000.0f)
            : 0.0f;
        const float stretch = renderSettings_.stretchEnabled
            ? std::clamp(
                std::max(1.0f, particleStretchFactor(p->velocity) * safeStretchFactor),
                1.0f, 1000000.0f)
            : 1.0f;
        const float radiusX = std::clamp(projectedRadius * 0.20f, 1.0f, 1000000.0f);
        const float radiusY = std::clamp(
            std::max(radiusX, projectedRadius * stretch), radiusX, 1000000.0f);
        const int radius = static_cast<int>(std::ceil(std::max(radiusX, radiusY)));
        if (radius <= 0) continue;

        const int minX = std::max(0, px - radius);
        const int maxX = std::min(width - 1, px + radius);
        const int minY = std::max(0, py - radius);
        const int maxY = std::min(height - 1, py + radius);
        if (minX > maxX || minY > maxY) {
            continue;
        }

        const float safeOpacity = std::clamp(p->opacity, 0.0f, 1.0f);
        const float alpha = std::clamp(particleColor.alphaF() * safeOpacity, 0.0f, 1.0f);
        const int baseA = static_cast<int>(alpha * 255.0f);
        if (baseA <= 0) continue;

        const int baseR = particleColor.red();
        const int baseG = particleColor.green();
        const int baseB = particleColor.blue();
        const float radiusX2 = radiusX * radiusX;
        const float radiusY2 = radiusY * radiusY;

        const QImage* atlas = nullptr;
        QRect atlasFrame;
        if (emitterParams && !emitterParams->texturePath.isEmpty()) {
            auto [it, inserted] = impl_->flipbookAtlases.try_emplace(emitterParams->texturePath);
            if (inserted) {
                it->second = loadFlipbookAtlasImage(emitterParams->texturePath);
            }

            if (!it->second.isNull()) {
                const int rows = std::clamp(p->spriteRows, 1, 1024);
                const int cols = std::clamp(p->spriteCols, 1, 1024);
                const int frameWidth = it->second.width() / cols;
                const int frameHeight = it->second.height() / rows;
                if (frameWidth > 0 && frameHeight > 0) {
                    const int frame = std::clamp(p->spriteFrame, 0, rows * cols - 1);
                    atlasFrame = QRect(
                        (frame % cols) * frameWidth,
                        (frame / cols) * frameHeight,
                        frameWidth,
                        frameHeight);
                    atlas = &it->second;
                }
            }
        }

        for (int y = minY; y <= maxY; ++y) {
            const int dy = y - py;
            auto* row = scan + y * stride;
            for (int x = minX; x <= maxX; ++x) {
                const int dx = x - px;
                const float dist2 =
                    ((static_cast<float>(dx) * static_cast<float>(dx)) / radiusX2) +
                    ((static_cast<float>(dy) * static_cast<float>(dy)) / radiusY2);
                if (dist2 > 1.0f) {
                    continue;
                }

                int sourceR = 255;
                int sourceG = 255;
                int sourceB = 255;
                int sourceA = 255;
                float falloff = 1.0f - dist2;
                if (renderSettings_.softParticles) {
                    const float safeSoftParticleDistance = std::isfinite(renderSettings_.softParticleDistance)
                        ? std::clamp(renderSettings_.softParticleDistance, 0.0f, 1000000.0f)
                        : 0.0f;
                    const float softness = std::clamp(
                        safeSoftParticleDistance / std::max(1, radius),
                        0.01f,
                        0.95f);
                    const float edge = 1.0f - softness;
                    const float t = std::clamp((dist2 - edge) / std::max(0.0001f, 1.0f - edge), 0.0f, 1.0f);
                    falloff = 1.0f - (t * t * (3.0f - 2.0f * t));
                }
                if (atlas) {
                    const float u = std::clamp(
                        (static_cast<float>(dx) / radiusX + 1.0f) * 0.5f,
                        0.0f,
                        1.0f);
                    const float v = std::clamp(
                        (static_cast<float>(dy) / radiusY + 1.0f) * 0.5f,
                        0.0f,
                        1.0f);
                    const int sourceX = atlasFrame.left() + std::min(
                        atlasFrame.width() - 1,
                        static_cast<int>(u * static_cast<float>(atlasFrame.width())));
                    const int sourceY = atlasFrame.top() + std::min(
                        atlasFrame.height() - 1,
                        static_cast<int>(v * static_cast<float>(atlasFrame.height())));
                    const QRgb source = atlas->pixel(sourceX, sourceY);
                    sourceR = qRed(source);
                    sourceG = qGreen(source);
                    sourceB = qBlue(source);
                    sourceA = qAlpha(source);
                    falloff = 1.0f;
                }

                const int a = static_cast<int>(baseA * falloff * (sourceA / 255.0f));
                if (a <= 0) continue;

                const int tintedR = (baseR * sourceR + 127) / 255;
                const int tintedG = (baseG * sourceG + 127) / 255;
                const int tintedB = (baseB * sourceB + 127) / 255;
                const int srcR = (tintedR * a + 127) / 255;
                const int srcG = (tintedG * a + 127) / 255;
                const int srcB = (tintedB * a + 127) / 255;
                blendPixel(row[x], srcR, srcG, srcB, a);
            }
        }

        if (renderSettings_.trailEnabled) {
            const QVector3D prevView = p->prevPosition - impl_->cameraPosition;
            const float prevDepth = prevView.z();
            if (prevDepth > nearPlane) {
                const float prevInvDepth = 1.0f / prevDepth;
                const QPoint prevPoint(
                    static_cast<int>(std::round(halfW + prevView.x() * focal * prevInvDepth)),
                    static_cast<int>(std::round(halfH - prevView.y() * focal * prevInvDepth)));
                const QPoint currPoint(px, py);
                const float safeTrailFade = std::isfinite(renderSettings_.trailFade)
                    ? std::clamp(renderSettings_.trailFade, 0.0f, 1.0f)
                    : 0.0f;
                const float trailAlpha = safeTrailFade * alpha;
                if (trailAlpha > 0.0f) {
                    QPainter trailPainter(&target);
                    QPen pen(particleColor);
                    const float safeTrailWidth = std::isfinite(renderSettings_.trailWidth)
                        ? std::clamp(renderSettings_.trailWidth, 0.5f, 1000000.0f)
                        : 0.5f;
                    pen.setWidthF(safeTrailWidth);
                    pen.setCapStyle(Qt::RoundCap);
                    pen.setJoinStyle(Qt::RoundJoin);
                    QColor trailColor = particleColor;
                    trailColor.setAlphaF(trailAlpha);
                    pen.setColor(trailColor);
                    trailPainter.setPen(pen);
                    trailPainter.drawLine(prevPoint, currPoint);
                }
            }
        }
    }

    return target;
}

void ParticleSystem::setCameraPosition(const QVector3D& position)
{
    const auto safeComponent = [](float value) {
        return std::isfinite(value)
            ? std::clamp(value, -1000000.0f, 1000000.0f)
            : 0.0f;
    };
    impl_->cameraPosition = QVector3D(
        safeComponent(position.x()),
        safeComponent(position.y()),
        safeComponent(position.z()));
}

QVector3D ParticleSystem::cameraPosition() const
{
    return impl_->cameraPosition;
}

void ParticleSystem::clear()
{
    for (auto& emitter : emitters_) {
        if (emitter) emitter->clear();
    }
    time_ = 0.0f;
}

void ParticleSystem::preWarm(float duration)
{
    if (!std::isfinite(duration) || duration <= 0.0f) return;
    for (auto& emitter : emitters_) {
        if (emitter) emitter->preWarm(duration);
    }
}

void ParticleSystem::render(QPainter& painter, const QTransform& transform)
{
    painter.save();
    const auto finite = [](double value) { return std::isfinite(value); };
    const bool transformFinite =
        finite(transform.m11()) && finite(transform.m12()) &&
        finite(transform.m13()) && finite(transform.m21()) &&
        finite(transform.m22()) && finite(transform.m23()) &&
        finite(transform.m31()) && finite(transform.m32()) &&
        finite(transform.m33()) && finite(transform.dx()) && finite(transform.dy());
    painter.setTransform(transformFinite ? transform : QTransform(), true);
    
    // Collect all particles from all emitters
    std::vector<const Particle*> allParticles;
    const auto finiteVector = [](const QVector3D& value) {
        return std::isfinite(value.x()) &&
               std::isfinite(value.y()) &&
               std::isfinite(value.z());
    };
    const auto finiteParticle = [&](const Particle& particle) {
        return particle.alive &&
               finiteVector(particle.position) &&
               finiteVector(particle.prevPosition) &&
               finiteVector(particle.velocity) &&
               std::isfinite(particle.scale) &&
               std::isfinite(particle.opacity) &&
               std::isfinite(particle.rotation) &&
               std::isfinite(particle.age);
    };
    for (const auto& emitter : emitters_) {
        if (!emitter) continue;
        for (const auto& p : emitter->particles()) {
            if (finiteParticle(p)) {
                allParticles.push_back(&p);
            }
        }
    }
    
    // Sort particles if needed
    if (renderSettings_.sortMode == ParticleRenderSettings::SortMode::Distance) {
        // Sort by distance from camera (back to front for proper blending)
        std::sort(allParticles.begin(), allParticles.end(),
            [this](const Particle* a, const Particle* b) {
                float distA = (a->position - impl_->cameraPosition).lengthSquared();
                float distB = (b->position - impl_->cameraPosition).lengthSquared();
                return distA > distB;  // Far to near
            });
    } else if (renderSettings_.sortMode == ParticleRenderSettings::SortMode::OldestFirst) {
        std::sort(allParticles.begin(), allParticles.end(),
            [](const Particle* a, const Particle* b) {
                return a->age > b->age;
            });
    } else if (renderSettings_.sortMode == ParticleRenderSettings::SortMode::YoungestFirst) {
        std::sort(allParticles.begin(), allParticles.end(),
            [](const Particle* a, const Particle* b) {
                return a->age < b->age;
            });
    }
    
    // Set blend mode
    switch (renderSettings_.blendMode) {
        case ParticleBlendMode::Additive:
            painter.setCompositionMode(QPainter::CompositionMode_Plus);
            break;
        case ParticleBlendMode::Screen:
            painter.setCompositionMode(QPainter::CompositionMode_Screen);
            break;
        case ParticleBlendMode::Multiply:
            painter.setCompositionMode(QPainter::CompositionMode_Multiply);
            break;
        default:
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            break;
    }
    
    // Render particles
    for (const Particle* p : allParticles) {
        QColor color = p->color;
        const float safeOpacity = std::clamp(p->opacity, 0.0f, 1.0f);
        color.setAlphaF(color.alphaF() * safeOpacity);

        if (renderSettings_.trailEnabled) {
            const float trailFade = std::isfinite(renderSettings_.trailFade)
                ? std::clamp(renderSettings_.trailFade, 0.0f, 1.0f)
                : 0.0f;
            const float trailWidth = std::isfinite(renderSettings_.trailWidth)
                ? std::clamp(renderSettings_.trailWidth, 0.1f, 1000000.0f)
                : 0.1f;
            const QPointF prevPos(p->prevPosition.x(), p->prevPosition.y());
            const QPointF currPos(p->position.x(), p->position.y());
            QPen trailPen(color);
            trailPen.setCapStyle(Qt::RoundCap);
            trailPen.setJoinStyle(Qt::RoundJoin);
            trailPen.setWidthF(trailWidth);
            QColor trailColor = color;
            trailColor.setAlphaF(std::clamp(color.alphaF() * trailFade, 0.0f, 1.0f));
            trailPen.setColor(trailColor);
            painter.setPen(trailPen);
            painter.drawLine(prevPos, currPos);
        }
        
        QPointF pos(p->position.x(), p->position.y());
        const float size = std::clamp(p->scale, 0.0f, 100000.0f) * 10.0f;  // Base size
        const float safeStretchFactor = std::isfinite(renderSettings_.stretchFactor)
            ? std::clamp(renderSettings_.stretchFactor, 0.0f, 1000000.0f)
            : 0.0f;
        const float stretch = renderSettings_.stretchEnabled
            ? std::clamp(
                std::max(1.0f, particleStretchFactor(p->velocity) * safeStretchFactor),
                1.0f, 1000000.0f)
            : 1.0f;
        
        painter.save();
        painter.translate(pos);
        painter.rotate(p->rotation);

        if (stretch > 1.05f) {
            const float width = std::max(0.75f, size * 0.18f);
            const float height = std::max(width, size * stretch);

            QLinearGradient gradient(QPointF(0, -height * 0.5f), QPointF(0, height * 0.5f));
            gradient.setColorAt(0.0, QColor(color.red(), color.green(), color.blue(), 0));
            gradient.setColorAt(0.15, color);
            gradient.setColorAt(0.85, color);
            gradient.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 0));

            painter.setBrush(gradient);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(QRectF(-width * 0.5f, -height * 0.5f, width, height),
                                    width * 0.5f,
                                    width * 0.5f);
        } else {
            QRadialGradient gradient(QPointF(0, 0), size);
            gradient.setColorAt(0, color);
            gradient.setColorAt(1, QColor(color.red(), color.green(), color.blue(), 0));

            painter.setBrush(gradient);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QPointF(0, 0), size, size);
        }
        
        painter.restore();
    }
    
    painter.restore();
}

void ParticleSystem::render(QImage& target, const QTransform& transform)
{
    QPainter painter(&target);
    render(painter, transform);
}

void ParticleSystem::renderGPU(float* vertexBuffer, int maxVertices, int& vertexCount)
{
    vertexCount = 0;
    if (!vertexBuffer || maxVertices <= 0) return;
    const int writableVertexCapacity = std::min(
        maxVertices, std::numeric_limits<int>::max() / 8);
    const auto finiteVector = [](const QVector3D& value) {
        return std::isfinite(value.x()) &&
               std::isfinite(value.y()) &&
               std::isfinite(value.z());
    };
    const auto safePositionComponent = [](float value) {
        return std::isfinite(value)
            ? std::clamp(value, -10000000.0f, 10000000.0f)
            : 0.0f;
    };
    const float safeStretchFactor = std::isfinite(renderSettings_.stretchFactor)
        ? std::clamp(renderSettings_.stretchFactor, 0.0f, 1000000.0f)
        : 0.0f;
    
    for (const auto& emitter : emitters_) {
        if (!emitter) continue;
        for (const auto& p : emitter->particles()) {
            if (!p.alive || vertexCount > writableVertexCapacity - 4 ||
                !finiteVector(p.position) || !finiteVector(p.velocity) ||
                !std::isfinite(p.scale) || !std::isfinite(p.rotation) ||
                !std::isfinite(p.opacity)) continue;
            
            // Each particle needs 4 vertices (quad)
            int idx = vertexCount * 8;  // 8 floats per vertex (pos + uv + color)
            
        const float halfSize = std::clamp(p.scale, 0.0f, 100000.0f) * 5.0f;
        const float safePositionX = safePositionComponent(p.position.x());
        const float safePositionY = safePositionComponent(p.position.y());
        const float stretch = renderSettings_.stretchEnabled
            ? std::clamp(
                std::max(1.0f, particleStretchFactor(p.velocity) * safeStretchFactor),
                1.0f, 1000000.0f)
            : 1.0f;
        float cosR = std::cos(p.rotation * M_PI / 180.0f);
        float sinR = std::sin(p.rotation * M_PI / 180.0f);
            
            // Four corners
            float corners[4][2] = {
                {-halfSize, -halfSize},
                {halfSize, -halfSize},
                {halfSize, halfSize},
                {-halfSize, halfSize}
            };
            
            for (int i = 0; i < 4; i++) {
                float x = corners[i][0] * cosR - (corners[i][1] * stretch) * sinR + safePositionX;
                float y = corners[i][0] * sinR + (corners[i][1] * stretch) * cosR + safePositionY;
                
                vertexBuffer[idx + i * 8 + 0] = x;
                vertexBuffer[idx + i * 8 + 1] = y;
                vertexBuffer[idx + i * 8 + 2] = (i == 0 || i == 3) ? 0.0f : 1.0f;  // UV
                vertexBuffer[idx + i * 8 + 3] = (i == 0 || i == 1) ? 0.0f : 1.0f;
                vertexBuffer[idx + i * 8 + 4] = p.color.redF();
                vertexBuffer[idx + i * 8 + 5] = p.color.greenF();
                vertexBuffer[idx + i * 8 + 6] = p.color.blueF();
                vertexBuffer[idx + i * 8 + 7] =
                    std::clamp(p.color.alphaF() * p.opacity, 0.0f, 1.0f);
            }
            
            vertexCount += 4;
        }
    }
}

// ==================== ParticlePresets ====================

EmitterParams ParticlePresets::fire()
{
    EmitterParams params;
    params.shape = EmitterShape::Point;
    params.mode = EmissionMode::Continuous;
    params.rate = 50.0f;
    params.lifeMin = 0.5f;
    params.lifeMax = 1.5f;
    params.speedMin = 50.0f;
    params.speedMax = 150.0f;
    params.direction = QVector3D(0, -1, 0);  // Up
    params.directionSpread = 30.0f;
    params.scaleMin = 10.0f;
    params.scaleMax = 20.0f;
    params.scaleEndMin = 2.0f;
    params.scaleEndMax = 5.0f;
    params.colorStart = QColor(255, 200, 50, 255);
    params.colorEnd = QColor(255, 50, 0, 0);
    params.opacityEndMin = 0.0f;
    params.opacityEndMax = 0.0f;
    return params;
}

EmitterParams ParticlePresets::campfire()
{
    EmitterParams params = fire();
    params.rate = 100.0f;
    params.shape = EmitterShape::Circle;
    params.radius = 30.0f;
    params.colorStart = QColor(255, 150, 30, 255);
    params.colorEnd = QColor(100, 20, 0, 0);
    return params;
}

EmitterParams ParticlePresets::torch()
{
    EmitterParams params = fire();
    params.rate = 30.0f;
    params.scaleMin = 5.0f;
    params.scaleMax = 15.0f;
    params.colorStart = QColor(255, 220, 100, 200);
    return params;
}

EmitterParams ParticlePresets::smoke()
{
    EmitterParams params;
    params.shape = EmitterShape::Point;
    params.mode = EmissionMode::Continuous;
    params.rate = 20.0f;
    params.lifeMin = 2.0f;
    params.lifeMax = 5.0f;
    params.speedMin = 20.0f;
    params.speedMax = 50.0f;
    params.direction = QVector3D(0, -1, 0);
    params.directionSpread = 60.0f;
    params.scaleMin = 20.0f;
    params.scaleMax = 30.0f;
    params.scaleEndMin = 50.0f;
    params.scaleEndMax = 80.0f;
    params.colorStart = QColor(100, 100, 100, 150);
    params.colorEnd = QColor(50, 50, 50, 0);
    params.drag = 0.5f;
    return params;
}

EmitterParams ParticlePresets::steam()
{
    EmitterParams params = smoke();
    params.colorStart = QColor(200, 200, 200, 100);
    params.colorEnd = QColor(255, 255, 255, 0);
    params.scaleMin = 10.0f;
    params.scaleMax = 20.0f;
    return params;
}

EmitterParams ParticlePresets::dust()
{
    EmitterParams params;
    params.shape = EmitterShape::Rectangle;
    params.width = 500.0f;
    params.height = 500.0f;
    params.mode = EmissionMode::Continuous;
    params.rate = 5.0f;
    params.lifeMin = 5.0f;
    params.lifeMax = 10.0f;
    params.speedMin = 5.0f;
    params.speedMax = 15.0f;
    params.directionSpread = 360.0f;
    params.scaleMin = 2.0f;
    params.scaleMax = 5.0f;
    params.scaleEndMin = 1.0f;
    params.scaleEndMax = 2.0f;
    params.colorStart = QColor(200, 180, 150, 80);
    params.colorEnd = QColor(200, 180, 150, 0);
    return params;
}

EmitterParams ParticlePresets::rain()
{
    EmitterParams params;
    params.shape = EmitterShape::Rectangle;
    params.width = 1400.0f;
    params.height = 24.0f;
    params.mode = EmissionMode::Continuous;
    params.rate = 320.0f;
    params.lifeMin = 0.35f;
    params.lifeMax = 0.9f;
    params.speedMin = 650.0f;
    params.speedMax = 950.0f;
    params.direction = QVector3D(0, 1, 0);  // Down
    params.directionSpread = 2.5f;
    params.rotationMin = 0.0f;
    params.rotationMax = 0.0f;
    params.scaleMin = 0.9f;
    params.scaleMax = 2.0f;
    params.scaleEndMin = 0.9f;
    params.scaleEndMax = 2.0f;
    params.colorStart = QColor(180, 205, 255, 90);
    params.colorEnd = QColor(180, 205, 255, 0);
    params.opacityMin = 0.45f;
    params.opacityMax = 0.75f;
    params.opacityEndMin = 0.0f;
    params.opacityEndMax = 0.0f;
    return params;
}

EmitterParams ParticlePresets::splash()
{
    EmitterParams params;
    params.shape = EmitterShape::Circle;
    params.radius = 20.0f;
    params.mode = EmissionMode::Burst;
    params.burstCount = 50;
    params.lifeMin = 0.3f;
    params.lifeMax = 0.8f;
    params.speedMin = 100.0f;
    params.speedMax = 300.0f;
    params.direction = QVector3D(0, -1, 0);
    params.directionSpread = 90.0f;
    params.scaleMin = 3.0f;
    params.scaleMax = 8.0f;
    params.scaleEndMin = 0.0f;
    params.scaleEndMax = 2.0f;
    params.colorStart = QColor(150, 200, 255, 200);
    params.colorEnd = QColor(200, 230, 255, 0);
    return params;
}

EmitterParams ParticlePresets::fountain()
{
    EmitterParams params;
    params.shape = EmitterShape::Point;
    params.mode = EmissionMode::Continuous;
    params.rate = 100.0f;
    params.lifeMin = 1.0f;
    params.lifeMax = 2.0f;
    params.speedMin = 300.0f;
    params.speedMax = 400.0f;
    params.direction = QVector3D(0, -1, 0);
    params.directionSpread = 15.0f;
    params.scaleMin = 5.0f;
    params.scaleMax = 10.0f;
    params.scaleEndMin = 2.0f;
    params.scaleEndMax = 5.0f;
    params.colorStart = QColor(100, 180, 255, 200);
    params.colorEnd = QColor(150, 200, 255, 0);
    return params;
}

EmitterParams ParticlePresets::explosion()
{
    EmitterParams params;
    params.shape = EmitterShape::Point;
    params.mode = EmissionMode::Burst;
    params.burstCount = 200;
    params.lifeMin = 0.5f;
    params.lifeMax = 1.5f;
    params.speedMin = 200.0f;
    params.speedMax = 500.0f;
    params.directionSpread = 360.0f;
    params.scaleMin = 10.0f;
    params.scaleMax = 30.0f;
    params.scaleEndMin = 0.0f;
    params.scaleEndMax = 10.0f;
    params.colorStart = QColor(255, 200, 50, 255);
    params.colorEnd = QColor(255, 50, 0, 0);
    params.drag = 2.0f;
    return params;
}

EmitterParams ParticlePresets::debris()
{
    EmitterParams params;
    params.shape = EmitterShape::Point;
    params.mode = EmissionMode::Burst;
    params.burstCount = 50;
    params.lifeMin = 1.0f;
    params.lifeMax = 3.0f;
    params.speedMin = 100.0f;
    params.speedMax = 300.0f;
    params.directionSpread = 360.0f;
    params.scaleMin = 5.0f;
    params.scaleMax = 15.0f;
    params.scaleEndMin = 5.0f;
    params.scaleEndMax = 15.0f;
    params.colorStart = QColor(100, 80, 60, 255);
    params.colorEnd = QColor(50, 40, 30, 0);
    params.drag = 0.5f;
    params.rotationSpeedMin = -180.0f;
    params.rotationSpeedMax = 180.0f;
    return params;
}

EmitterParams ParticlePresets::sparks()
{
    EmitterParams params;
    params.shape = EmitterShape::Point;
    params.mode = EmissionMode::Burst;
    params.burstCount = 100;
    params.lifeMin = 0.2f;
    params.lifeMax = 0.6f;
    params.speedMin = 200.0f;
    params.speedMax = 600.0f;
    params.directionSpread = 360.0f;
    params.scaleMin = 2.0f;
    params.scaleMax = 4.0f;
    params.scaleEndMin = 0.0f;
    params.scaleEndMax = 1.0f;
    params.colorStart = QColor(255, 255, 150, 255);
    params.colorEnd = QColor(255, 100, 0, 0);
    params.drag = 3.0f;
    return params;
}

EmitterParams ParticlePresets::leaves()
{
    EmitterParams params;
    params.shape = EmitterShape::Rectangle;
    params.width = 500.0f;
    params.height = 10.0f;
    params.mode = EmissionMode::Continuous;
    params.rate = 10.0f;
    params.lifeMin = 3.0f;
    params.lifeMax = 8.0f;
    params.speedMin = 30.0f;
    params.speedMax = 60.0f;
    params.direction = QVector3D(0, 1, 0);
    params.directionSpread = 30.0f;
    params.scaleMin = 8.0f;
    params.scaleMax = 15.0f;
    params.colorStart = QColor(100, 180, 50, 255);
    params.colorEnd = QColor(180, 120, 30, 0);
    params.rotationSpeedMin = -90.0f;
    params.rotationSpeedMax = 90.0f;
    return params;
}

EmitterParams ParticlePresets::snow()
{
    EmitterParams params;
    params.shape = EmitterShape::Rectangle;
    params.width = 800.0f;
    params.height = 10.0f;
    params.mode = EmissionMode::Continuous;
    params.rate = 50.0f;
    params.lifeMin = 5.0f;
    params.lifeMax = 10.0f;
    params.speedMin = 30.0f;
    params.speedMax = 60.0f;
    params.direction = QVector3D(0, 1, 0);
    params.directionSpread = 15.0f;
    params.scaleMin = 3.0f;
    params.scaleMax = 8.0f;
    params.colorStart = QColor(255, 255, 255, 200);
    params.colorEnd = QColor(255, 255, 255, 100);
    params.drag = 0.2f;
    return params;
}

EmitterParams ParticlePresets::pollen()
{
    EmitterParams params;
    params.shape = EmitterShape::Sphere;
    params.radius = 300.0f;
    params.mode = EmissionMode::Continuous;
    params.rate = 5.0f;
    params.lifeMin = 5.0f;
    params.lifeMax = 15.0f;
    params.speedMin = 5.0f;
    params.speedMax = 15.0f;
    params.directionSpread = 360.0f;
    params.scaleMin = 2.0f;
    params.scaleMax = 4.0f;
    params.colorStart = QColor(255, 255, 200, 150);
    params.colorEnd = QColor(255, 255, 150, 0);
    return params;
}

EmitterParams ParticlePresets::magic()
{
    EmitterParams params;
    params.shape = EmitterShape::Sphere;
    params.radius = 50.0f;
    params.mode = EmissionMode::Continuous;
    params.rate = 50.0f;
    params.lifeMin = 0.5f;
    params.lifeMax = 1.5f;
    params.speedMin = 20.0f;
    params.speedMax = 50.0f;
    params.directionSpread = 360.0f;
    params.scaleMin = 5.0f;
    params.scaleMax = 10.0f;
    params.scaleEndMin = 0.0f;
    params.scaleEndMax = 2.0f;
    params.colorStart = QColor(100, 150, 255, 255);
    params.colorEnd = QColor(200, 100, 255, 0);
    return params;
}

EmitterParams ParticlePresets::sparkles()
{
    EmitterParams params;
    params.shape = EmitterShape::Point;
    params.mode = EmissionMode::Burst;
    params.burstCount = 96;
    params.burstInterval = 0.0f;
    params.lifeMin = 0.3f;
    params.lifeMax = 0.8f;
    params.speedMin = 0.0f;
    params.speedMax = 10.0f;
    params.directionSpread = 360.0f;
    params.scaleMin = 3.0f;
    params.scaleMax = 8.0f;
    params.scaleEndMin = 0.0f;
    params.scaleEndMax = 0.0f;
    params.colorStart = QColor(255, 255, 255, 255);
    params.colorEnd = QColor(255, 200, 100, 0);
    return params;
}

EmitterParams ParticlePresets::energyField()
{
    EmitterParams params;
    params.shape = EmitterShape::Circle;
    params.radius = 100.0f;
    params.mode = EmissionMode::Continuous;
    params.rate = 100.0f;
    params.lifeMin = 0.5f;
    params.lifeMax = 1.0f;
    params.speedMin = 50.0f;
    params.speedMax = 100.0f;
    params.direction = QVector3D(0, 0, 1);  // Outward from circle
    params.directionSpread = 10.0f;
    params.scaleMin = 3.0f;
    params.scaleMax = 6.0f;
    params.colorStart = QColor(0, 200, 255, 200);
    params.colorEnd = QColor(100, 50, 255, 0);
    return params;
}

EmitterParams ParticlePresets::confetti()
{
    EmitterParams params;
    params.shape = EmitterShape::Point;
    params.mode = EmissionMode::Burst;
    params.burstCount = 200;
    params.lifeMin = 2.0f;
    params.lifeMax = 5.0f;
    params.speedMin = 100.0f;
    params.speedMax = 300.0f;
    params.direction = QVector3D(0, -1, 0);
    params.directionSpread = 90.0f;
    params.scaleMin = 5.0f;
    params.scaleMax = 10.0f;
    params.colorStart = QColor(255, 100, 100, 255);
    params.colorEnd = QColor(255, 100, 100, 0);
    params.rotationSpeedMin = -360.0f;
    params.rotationSpeedMax = 360.0f;
    params.drag = 0.5f;
    return params;
}

EmitterParams ParticlePresets::bubbles()
{
    EmitterParams params;
    params.shape = EmitterShape::Line;
    params.lineLength = 200.0f;
    params.mode = EmissionMode::Continuous;
    params.rate = 20.0f;
    params.lifeMin = 2.0f;
    params.lifeMax = 4.0f;
    params.speedMin = 30.0f;
    params.speedMax = 60.0f;
    params.direction = QVector3D(0, -1, 0);
    params.directionSpread = 20.0f;
    params.scaleMin = 5.0f;
    params.scaleMax = 15.0f;
    params.colorStart = QColor(200, 230, 255, 150);
    params.colorEnd = QColor(200, 230, 255, 50);
    return params;
}

// ==================== ParticleManager::Impl ====================

class ParticleManager::Impl {
public:
    std::map<QString, std::unique_ptr<ParticleSystem>> systems;
};

// ==================== ParticleManager ====================

ParticleManager::ParticleManager(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{
}

ParticleManager::~ParticleManager()
{
}

ParticleSystem* ParticleManager::createSystem(const QString& name)
{
    auto found = impl_->systems.find(name);
    if (found != impl_->systems.end()) {
        return found->second.get();
    }
    
    auto system = std::make_unique<ParticleSystem>(this);
    ParticleSystem* ptr = system.get();
    impl_->systems.emplace(name, std::move(system));
    Q_EMIT systemCreated(name);
    return ptr;
}

ParticleSystem* ParticleManager::system(const QString& name) const
{
    auto found = impl_->systems.find(name);
    if (found != impl_->systems.end()) {
        return found->second.get();
    }
    return nullptr;
}

void ParticleManager::removeSystem(const QString& name)
{
    const auto found = impl_->systems.find(name);
    if (found == impl_->systems.end()) return;
    impl_->systems.erase(found);
    Q_EMIT systemRemoved(name);
}

void ParticleManager::clearSystems()
{
    QStringList names;
    names.reserve(static_cast<qsizetype>(impl_->systems.size()));
    for (const auto& [name, system] : impl_->systems) {
        Q_UNUSED(system);
        names.append(name);
    }
    for (const QString& name : names) {
        removeSystem(name);
    }
}

void ParticleManager::update(float deltaTime)
{
    for (auto& [name, system] : impl_->systems) {
        Q_UNUSED(name);
        system->update(deltaTime);
    }
}

void ParticleManager::preWarm(float duration)
{
    for (auto& [name, system] : impl_->systems) {
        Q_UNUSED(name);
        system->preWarm(duration);
    }
}

QStringList ParticleManager::systemNames() const
{
    QStringList names;
    names.reserve(static_cast<qsizetype>(impl_->systems.size()));
    for (const auto& [name, system] : impl_->systems) {
        Q_UNUSED(system);
        names.append(name);
    }
    return names;
}

void ParticleManager::setAllPaused(bool paused)
{
    for (auto& [name, system] : impl_->systems) {
        Q_UNUSED(name);
        system->setPaused(paused);
    }
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ParticleEmitter)
W_OBJECT_IMPL(Artifact::ParticleSystem)
W_OBJECT_IMPL(Artifact::ParticleManager)
