module;
#include <QObject>
#include <QVector2D>
#include <QVector3D>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QRandomGenerator>
#include <QtMath>
#include <cmath>
#include <algorithm>
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

// ==================== ParticleEffector Base ====================

void ParticleEffector::apply(std::vector<Particle>& particles, float deltaTime)
{
    for (auto& p : particles) {
        if (p.alive && enabled) {
            apply(p, deltaTime);
        }
    }
}

// ==================== ForceEffector ====================

void ForceEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled) return;
    
    particle.acceleration += force * strength * deltaTime;
}

// ==================== VortexEffector ====================

void VortexEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled) return;
    
    QVector3D toParticle = particle.position - position;
    float dist = toParticle.length();
    
    if (dist < radius && dist > 0.1f) {
        // Calculate tangential force
        QVector3D tangent = QVector3D(-toParticle.y(), toParticle.x(), 0).normalized();
        float factor = 1.0f - (dist / radius);
        factor = std::pow(factor, tightness);
        
        float angularVelRad = qDegreesToRadians(angularVelocity * strength);
        particle.velocity += tangent * angularVelRad * factor * deltaTime * 50.0f;
    }
}

// ==================== TurbulenceEffector ====================

void TurbulenceEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled) return;
    
    // Simple noise-based turbulence
    float noiseX = std::sin(particle.position.x() * frequency + evolution + seed) *
                   std::cos(particle.position.y() * frequency * 0.7f);
    float noiseY = std::cos(particle.position.x() * frequency * 0.8f + evolution * 1.3f + seed) *
                   std::sin(particle.position.y() * frequency * 0.9f);
    float noiseZ = std::sin(particle.position.z() * frequency * 0.5f + evolution * 0.7f + seed);
    
    QVector3D turbulenceForce(noiseX, noiseY, noiseZ);
    turbulenceForce *= amplitude * strength;
    
    particle.velocity += turbulenceForce * deltaTime;
}

// ==================== AttractorEffector ====================

void AttractorEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled) return;
    
    QVector3D toAttractor = position - particle.position;
    float dist = toAttractor.length();
    
    if (dist > killRadius && dist < radius) {
        float factor = 1.0f - std::pow(dist / radius, falloff);
        toAttractor.normalize();
        particle.velocity += toAttractor * strength * factor * 100.0f * deltaTime;
    } else if (dist <= killRadius && killOnReach) {
        particle.alive = false;
    }
}

// ==================== RepellerEffector ====================

void RepellerEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled) return;
    
    QVector3D fromRepeller = particle.position - position;
    float dist = fromRepeller.length();
    
    if (dist < radius && dist > 0.1f) {
        float factor = 1.0f - std::pow(dist / radius, falloff);
        fromRepeller.normalize();
        particle.velocity += fromRepeller * strength * factor * 100.0f * deltaTime;
    }
}

// ==================== WindEffector ====================

void WindEffector::apply(Particle& particle, float deltaTime)
{
    if (!enabled) return;
    
    // Base wind
    QVector3D windForce = windDirection.normalized() * windStrength * strength;
    
    // Add turbulence
    float noise = std::sin(particle.position.x() * turbulenceFrequency + evolution) *
                  std::cos(particle.position.y() * turbulenceFrequency + evolution * 0.7f);
    windForce += windDirection * noise * turbulence;
    
    particle.velocity += windForce * deltaTime;
}

// ==================== KillZoneEffector ====================

void KillZoneEffector::apply(Particle& particle, float deltaTime)
{
    Q_UNUSED(deltaTime);
    
    if (!enabled) return;
    
    bool inside = false;
    
    switch (zoneType) {
        case ZoneType::Sphere: {
            float dist = (particle.position - position).length();
            inside = dist < size.x();
            break;
        }
        case ZoneType::Box: {
            QVector3D rel = particle.position - position;
            inside = std::abs(rel.x()) < size.x() * 0.5f &&
                    std::abs(rel.y()) < size.y() * 0.5f &&
                    std::abs(rel.z()) < size.z() * 0.5f;
            break;
        }
        case ZoneType::Plane: {
            // Plane defined by position and direction (normal)
            float dist = QVector3D::dotProduct(particle.position - position, direction.normalized());
            inside = dist < 0;
            break;
        }
    }
    
    // Kill if inside and not inverted, or outside and inverted
    if (inside != invert) {
        particle.alive = false;
    }
}

// ==================== ParticleEmitter::Impl ====================

class ParticleEmitter::Impl {
public:
    QRandomGenerator rng;
};

// ==================== ParticleEmitter ====================

ParticleEmitter::ParticleEmitter(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{
}

ParticleEmitter::~ParticleEmitter()
{
}

void ParticleEmitter::addEffector(std::unique_ptr<ParticleEffector> effector)
{
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

QVector3D ParticleEmitter::getEmissionPosition() const
{
    QVector3D pos = params_.position;
    
    switch (params_.shape) {
        case EmitterShape::Point:
            // No offset
            break;
            
        case EmitterShape::Sphere: {
            float theta = impl_->rng.bounded(360.0f) * M_PI / 180.0f;
            float phi = impl_->rng.bounded(180.0f) * M_PI / 180.0f;
            float r = params_.radius * std::cbrt(impl_->rng.bounded(1.0f));  // Uniform distribution
            
            pos += QVector3D(
                r * std::sin(phi) * std::cos(theta),
                r * std::sin(phi) * std::sin(theta),
                r * std::cos(phi)
            );
            break;
        }
            
        case EmitterShape::Box: {
            pos += QVector3D(
                (impl_->rng.bounded(1.0f) - 0.5f) * params_.width,
                (impl_->rng.bounded(1.0f) - 0.5f) * params_.height,
                (impl_->rng.bounded(1.0f) - 0.5f) * params_.depth
            );
            break;
        }
            
        case EmitterShape::Circle: {
            float angle = impl_->rng.bounded(360.0f) * M_PI / 180.0f;
            float r = params_.radius * std::sqrt(impl_->rng.bounded(1.0f));
            
            pos += QVector3D(
                r * std::cos(angle),
                r * std::sin(angle),
                0
            );
            break;
        }
            
        case EmitterShape::Rectangle: {
            pos += QVector3D(
                (impl_->rng.bounded(1.0f) - 0.5f) * params_.width,
                (impl_->rng.bounded(1.0f) - 0.5f) * params_.height,
                0
            );
            break;
        }
            
        case EmitterShape::Line: {
            pos += QVector3D(
                (impl_->rng.bounded(1.0f) - 0.5f) * params_.lineLength,
                0,
                0
            );
            break;
        }
            
        default:
            break;
    }
    
    return pos;
}

QVector3D ParticleEmitter::getEmissionDirection() const
{
    QVector3D dir = params_.direction.normalized();
    
    if (params_.directionSpread > 0.0f) {
        float spreadRad = params_.directionSpread * M_PI / 180.0f;
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
    
    return dir;
}

void ParticleEmitter::initializeParticle(Particle& p)
{
    // Position
    p.position = getEmissionPosition();
    p.prevPosition = p.position;
    
    // Velocity
    float speed = params_.speedMin + impl_->rng.bounded(params_.speedMax - params_.speedMin);
    QVector3D dir = getEmissionDirection();
    p.velocity = dir * speed;
    p.acceleration = QVector3D(0, 0, 0);
    
    // Rotation
    p.rotation = params_.rotationMin + impl_->rng.bounded(params_.rotationMax - params_.rotationMin);
    p.rotationSpeed = params_.rotationSpeedMin + 
                      impl_->rng.bounded(params_.rotationSpeedMax - params_.rotationSpeedMin);
    
    // Scale
    p.scaleStart = params_.scaleMin + impl_->rng.bounded(params_.scaleMax - params_.scaleMin);
    p.scaleEnd = params_.scaleEndMin + impl_->rng.bounded(params_.scaleEndMax - params_.scaleEndMin);
    p.scale = p.scaleStart;
    
    // Color
    p.colorStart = params_.colorStart;
    p.colorEnd = params_.colorEnd;
    
    // Apply color variation
    if (params_.colorVariation > 0.0f) {
        float var = params_.colorVariation;
        auto variation = [&]() -> float {
            return static_cast<float>(impl_->rng.generateDouble() * (2.0 * var) - var);
        };
        int r = std::clamp(p.colorStart.red() + static_cast<int>(variation() * 255.0f), 0, 255);
        int g = std::clamp(p.colorStart.green() + static_cast<int>(variation() * 255.0f), 0, 255);
        int b = std::clamp(p.colorStart.blue() + static_cast<int>(variation() * 255.0f), 0, 255);
        p.colorStart = QColor(r, g, b, p.colorStart.alpha());
    }
    
    p.color = p.colorStart;
    
    // Opacity
    p.opacityStart = params_.opacityMin + impl_->rng.bounded(params_.opacityMax - params_.opacityMin);
    p.opacityEnd = params_.opacityEndMin + impl_->rng.bounded(params_.opacityEndMax - params_.opacityEndMin);
    p.opacity = p.opacityStart;
    
    // Lifetime
    p.maxLife = params_.lifeMin + impl_->rng.bounded(params_.lifeMax - params_.lifeMin);
    p.life = 1.0f;
    p.age = 0.0f;
    
    // ID
    p.id = nextParticleId_++;
    p.alive = true;
    p.active = true;
}

void ParticleEmitter::emitParticles(int count)
{
    int available = params_.maxParticles - static_cast<int>(particles_.size());
    int toEmit = std::min(count, available);
    
    for (int i = 0; i < toEmit; i++) {
        Particle p;
        initializeParticle(p);
        particles_.push_back(p);
    }
    
    if (toEmit > 0) {
        emit particleEmitted(toEmit);
    }
}

void ParticleEmitter::emitBurst()
{
    emitParticles(params_.burstCount);
}

void ParticleEmitter::updateParticle(Particle& p, float deltaTime)
{
    // Store previous position
    p.prevPosition = p.position;
    
    // Update age and life
    p.age += deltaTime;
    p.life = 1.0f - (p.age / p.maxLife);
    
    if (p.life <= 0.0f) {
        p.alive = false;
        return;
    }
    
    // Apply physics
    p.velocity += p.acceleration * deltaTime;
    
    // Apply drag
    if (params_.drag > 0.0f) {
        p.velocity *= (1.0f - params_.drag * deltaTime);
    }
    
    p.position += p.velocity * deltaTime;
    p.acceleration = QVector3D(0, 0, 0);
    
    // Update rotation
    p.rotation += p.rotationSpeed * deltaTime;
    
    // Interpolate properties based on life
    float t = 1.0f - p.life;  // 0 = just born, 1 = about to die
    
    // Scale interpolation
    p.scale = p.scaleStart + (p.scaleEnd - p.scaleStart) * t;
    
    // Color interpolation
    p.color = QColor(
        p.colorStart.red() + static_cast<int>((p.colorEnd.red() - p.colorStart.red()) * t),
        p.colorStart.green() + static_cast<int>((p.colorEnd.green() - p.colorStart.green()) * t),
        p.colorStart.blue() + static_cast<int>((p.colorEnd.blue() - p.colorStart.blue()) * t),
        p.colorStart.alpha() + static_cast<int>((p.colorEnd.alpha() - p.colorStart.alpha()) * t)
    );
    
    // Opacity interpolation
    p.opacity = p.opacityStart + (p.opacityEnd - p.opacityStart) * t;
}

void ParticleEmitter::applyEffectors(Particle& p, float deltaTime)
{
    for (const auto& effector : effectors_) {
        if (effector->enabled) {
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

void ParticleEmitter::update(float deltaTime)
{
    if (!active_) return;
    
    time_ += deltaTime;
    
    // Emit new particles
    switch (params_.mode) {
        case EmissionMode::Continuous: {
            emitAccumulator_ += deltaTime * params_.rate;
            int toEmit = static_cast<int>(emitAccumulator_);
            if (toEmit > 0) {
                emitParticles(toEmit);
                emitAccumulator_ -= toEmit;
            }
            break;
        }
        case EmissionMode::Burst: {
            burstTimer_ += deltaTime;
            if (burstTimer_ >= params_.burstInterval) {
                emitBurst();
                burstTimer_ = 0.0f;
            }
            break;
        }
        case EmissionMode::Triggered:
            // Manual emission only
            break;
    }
    
    // Update existing particles
    for (auto& p : particles_) {
        if (p.alive) {
            applyEffectors(p, deltaTime);
            updateParticle(p, deltaTime);
        }
    }
    
    // Remove dead particles
    removeDeadParticles();
}

void ParticleEmitter::clear()
{
    particles_.clear();
    time_ = 0.0f;
    emitAccumulator_ = 0.0f;
    burstTimer_ = 0.0f;
}

void ParticleEmitter::preWarm(float duration, float stepSize)
{
    clear();
    
    float time = 0.0f;
    while (time < duration) {
        update(stepSize);
        time += stepSize;
    }
}

// ==================== ParticleSystem::Impl ====================

class ParticleSystem::Impl {
public:
    QVector3D cameraPosition;
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
    emit emitterAdded(ptr);
    return ptr;
}

void ParticleSystem::removeEmitter(ParticleEmitter* emitter)
{
    emitters_.erase(
        std::remove_if(emitters_.begin(), emitters_.end(),
            [emitter](const std::unique_ptr<ParticleEmitter>& e) { return e.get() == emitter; }),
        emitters_.end()
    );
    emit emitterRemoved(emitter);
}

void ParticleSystem::removeEmitter(int index)
{
    if (index >= 0 && index < static_cast<int>(emitters_.size())) {
        ParticleEmitter* ptr = emitters_[index].get();
        emitters_.erase(emitters_.begin() + index);
        emit emitterRemoved(ptr);
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
        count += emitter->particleCount();
    }
    return count;
}

void ParticleSystem::update(float deltaTime)
{
    if (paused_) return;
    
    float scaledDelta = deltaTime * timeScale_;
    time_ += scaledDelta;
    
    for (auto& emitter : emitters_) {
        emitter->update(scaledDelta);
    }
    
    emit updated(deltaTime);
}

void ParticleSystem::clear()
{
    for (auto& emitter : emitters_) {
        emitter->clear();
    }
    time_ = 0.0f;
}

void ParticleSystem::preWarm(float duration)
{
    for (auto& emitter : emitters_) {
        emitter->preWarm(duration);
    }
}

void ParticleSystem::render(QPainter& painter, const QTransform& transform)
{
    painter.save();
    painter.setTransform(transform, true);
    
    // Collect all particles from all emitters
    std::vector<const Particle*> allParticles;
    for (const auto& emitter : emitters_) {
        for (const auto& p : emitter->particles()) {
            if (p.alive) {
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
        color.setAlphaF(color.alphaF() * p->opacity);
        
        QPointF pos(p->position.x(), p->position.y());
        float size = p->scale * 10.0f;  // Base size
        
        painter.save();
        painter.translate(pos);
        painter.rotate(p->rotation);
        
        QRadialGradient gradient(QPointF(0, 0), size);
        gradient.setColorAt(0, color);
        gradient.setColorAt(1, QColor(color.red(), color.green(), color.blue(), 0));
        
        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(0, 0), size, size);
        
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
    
    for (const auto& emitter : emitters_) {
        for (const auto& p : emitter->particles()) {
            if (!p.alive || vertexCount >= maxVertices) continue;
            
            // Each particle needs 4 vertices (quad)
            int idx = vertexCount * 8;  // 8 floats per vertex (pos + uv + color)
            
            float halfSize = p.scale * 5.0f;
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
                float x = corners[i][0] * cosR - corners[i][1] * sinR + p.position.x();
                float y = corners[i][0] * sinR + corners[i][1] * cosR + p.position.y();
                
                vertexBuffer[idx + i * 8 + 0] = x;
                vertexBuffer[idx + i * 8 + 1] = y;
                vertexBuffer[idx + i * 8 + 2] = (i == 0 || i == 3) ? 0.0f : 1.0f;  // UV
                vertexBuffer[idx + i * 8 + 3] = (i == 0 || i == 1) ? 0.0f : 1.0f;
                vertexBuffer[idx + i * 8 + 4] = p.color.redF();
                vertexBuffer[idx + i * 8 + 5] = p.color.greenF();
                vertexBuffer[idx + i * 8 + 6] = p.color.blueF();
                vertexBuffer[idx + i * 8 + 7] = p.color.alphaF() * p.opacity;
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
    params.width = 1000.0f;
    params.height = 10.0f;
    params.mode = EmissionMode::Continuous;
    params.rate = 200.0f;
    params.lifeMin = 1.0f;
    params.lifeMax = 2.0f;
    params.speedMin = 400.0f;
    params.speedMax = 600.0f;
    params.direction = QVector3D(0, 1, 0);  // Down
    params.directionSpread = 5.0f;
    params.scaleMin = 2.0f;
    params.scaleMax = 3.0f;
    params.scaleEndMin = 2.0f;
    params.scaleEndMax = 3.0f;
    params.colorStart = QColor(150, 180, 255, 100);
    params.colorEnd = QColor(150, 180, 255, 50);
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
    params.mode = EmissionMode::Continuous;
    params.rate = 30.0f;
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
    emit systemCreated(name);
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
    impl_->systems.erase(name);
    emit systemRemoved(name);
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
