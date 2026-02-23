module;
#include <QObject>
#include <QImage>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QTransform>
#include <QVariant>
#include <wobjectimpl.h>

module Artifact.Layer.Particle;

import std;
import Artifact.Layer.Abstract;
import Artifact.Generator.Particle;
import Animation.Transform2D;
import Animation.Transform3D;
import Size;
import Utils.Id;
import Utils.String.UniString;

namespace Artifact {

// ==================== ArtifactParticleLayer::Impl ====================

class ArtifactParticleLayer::Impl {
public:
    std::unique_ptr<ParticleSystem> particleSystem;
    QImage cachedFrame;
    int64_t cachedFrameNumber = -1;
    bool playing = true;
    float lastTime = 0.0f;
    int width = 1920;
    int height = 1080;
    
    Impl() {
        particleSystem = std::make_unique<ParticleSystem>();
    }
};

// ==================== ArtifactParticleLayer ====================

ArtifactParticleLayer::ArtifactParticleLayer()
    : ArtifactAbstractLayer()
    , impl_(new Impl())
{
    createParticleSystem();
}

ArtifactParticleLayer::~ArtifactParticleLayer()
{
    delete impl_;
}

void ArtifactParticleLayer::draw()
{
    // Drawing is handled by renderFrame/renderToImage
    // This is called by the composition renderer
}

QJsonObject ArtifactParticleLayer::toJson() const
{
    QJsonObject json;
    json["type"] = "ParticleLayer";
    json["name"] = layerName();
    json["visible"] = isVisible();
    json["blendMode"] = static_cast<int>(layerBlendType());
    
    // Save render settings
    const auto& rs = renderSettings();
    QJsonObject renderJson;
    renderJson["blendMode"] = static_cast<int>(rs.blendMode);
    renderJson["billboardMode"] = static_cast<int>(rs.billboardMode);
    renderJson["sortMode"] = static_cast<int>(rs.sortMode);
    renderJson["depthTest"] = rs.depthTest;
    renderJson["depthWrite"] = rs.depthWrite;
    json["renderSettings"] = renderJson;
    
    // Save emitters
    QJsonArray emittersArray;
    for (const auto& emitter : impl_->particleSystem->emitters()) {
        QJsonObject emitterJson;
        const auto& params = emitter->params();
        
        emitterJson["shape"] = static_cast<int>(params.shape);
        emitterJson["mode"] = static_cast<int>(params.mode);
        emitterJson["rate"] = params.rate;
        emitterJson["burstCount"] = params.burstCount;
        emitterJson["burstInterval"] = params.burstInterval;
        
        emitterJson["lifeMin"] = params.lifeMin;
        emitterJson["lifeMax"] = params.lifeMax;
        emitterJson["speedMin"] = params.speedMin;
        emitterJson["speedMax"] = params.speedMax;
        emitterJson["directionSpread"] = params.directionSpread;
        
        emitterJson["positionX"] = params.position.x();
        emitterJson["positionY"] = params.position.y();
        emitterJson["positionZ"] = params.position.z();
        
        emitterJson["directionX"] = params.direction.x();
        emitterJson["directionY"] = params.direction.y();
        emitterJson["directionZ"] = params.direction.z();
        
        emitterJson["radius"] = params.radius;
        emitterJson["width"] = params.width;
        emitterJson["height"] = params.height;
        emitterJson["depth"] = params.depth;
        
        emitterJson["scaleMin"] = params.scaleMin;
        emitterJson["scaleMax"] = params.scaleMax;
        emitterJson["scaleEndMin"] = params.scaleEndMin;
        emitterJson["scaleEndMax"] = params.scaleEndMax;
        
        emitterJson["colorStart"] = params.colorStart.name(QColor::HexArgb);
        emitterJson["colorEnd"] = params.colorEnd.name(QColor::HexArgb);
        emitterJson["colorVariation"] = params.colorVariation;
        
        emitterJson["opacityMin"] = params.opacityMin;
        emitterJson["opacityMax"] = params.opacityMax;
        emitterJson["opacityEndMin"] = params.opacityEndMin;
        emitterJson["opacityEndMax"] = params.opacityEndMax;
        
        emitterJson["drag"] = params.drag;
        emitterJson["maxParticles"] = params.maxParticles;
        
        emittersArray.append(emitterJson);
    }
    json["emitters"] = emittersArray;
    
    return json;
}

ArtifactAbstractLayerPtr ArtifactParticleLayer::fromJson(const QJsonObject& obj)
{
    auto layer = std::make_shared<ArtifactParticleLayer>();
    layer->applyPropertiesFromJson(obj);
    return layer;
}

void ArtifactParticleLayer::applyPropertiesFromJson(const QJsonObject& obj)
{
    if (obj.contains("name")) {
        setLayerName(obj["name"].toString());
    }
    if (obj.contains("visible")) {
        setVisible(obj["visible"].toBool());
    }
    if (obj.contains("blendMode")) {
        setBlendMode(static_cast<LAYER_BLEND_TYPE>(obj["blendMode"].toInt()));
    }
    
    // Load render settings
    if (obj.contains("renderSettings")) {
        QJsonObject renderJson = obj["renderSettings"].toObject();
        auto& rs = renderSettings();
        if (renderJson.contains("blendMode")) {
            rs.blendMode = static_cast<ParticleBlendMode>(renderJson["blendMode"].toInt());
        }
        if (renderJson.contains("billboardMode")) {
            rs.billboardMode = static_cast<ParticleRenderSettings::BillboardMode>(renderJson["billboardMode"].toInt());
        }
        if (renderJson.contains("sortMode")) {
            rs.sortMode = static_cast<ParticleRenderSettings::SortMode>(renderJson["sortMode"].toInt());
        }
        if (renderJson.contains("depthTest")) {
            rs.depthTest = renderJson["depthTest"].toBool();
        }
        if (renderJson.contains("depthWrite")) {
            rs.depthWrite = renderJson["depthWrite"].toBool();
        }
    }
    
    // Load emitters
    if (obj.contains("emitters")) {
        clearEmitters();
        QJsonArray emittersArray = obj["emitters"].toArray();
        
        for (const auto& emitterVal : emittersArray) {
            QJsonObject emitterJson = emitterVal.toObject();
            EmitterParams params;
            
            if (emitterJson.contains("shape")) {
                params.shape = static_cast<EmitterShape>(emitterJson["shape"].toInt());
            }
            if (emitterJson.contains("mode")) {
                params.mode = static_cast<EmissionMode>(emitterJson["mode"].toInt());
            }
            if (emitterJson.contains("rate")) {
                params.rate = emitterJson["rate"].toDouble();
            }
            if (emitterJson.contains("burstCount")) {
                params.burstCount = emitterJson["burstCount"].toInt();
            }
            if (emitterJson.contains("burstInterval")) {
                params.burstInterval = emitterJson["burstInterval"].toDouble();
            }
            
            if (emitterJson.contains("lifeMin")) {
                params.lifeMin = emitterJson["lifeMin"].toDouble();
            }
            if (emitterJson.contains("lifeMax")) {
                params.lifeMax = emitterJson["lifeMax"].toDouble();
            }
            if (emitterJson.contains("speedMin")) {
                params.speedMin = emitterJson["speedMin"].toDouble();
            }
            if (emitterJson.contains("speedMax")) {
                params.speedMax = emitterJson["speedMax"].toDouble();
            }
            if (emitterJson.contains("directionSpread")) {
                params.directionSpread = emitterJson["directionSpread"].toDouble();
            }
            
            if (emitterJson.contains("positionX")) {
                params.position.setX(emitterJson["positionX"].toDouble());
            }
            if (emitterJson.contains("positionY")) {
                params.position.setY(emitterJson["positionY"].toDouble());
            }
            if (emitterJson.contains("positionZ")) {
                params.position.setZ(emitterJson["positionZ"].toDouble());
            }
            
            if (emitterJson.contains("directionX")) {
                params.direction.setX(emitterJson["directionX"].toDouble());
            }
            if (emitterJson.contains("directionY")) {
                params.direction.setY(emitterJson["directionY"].toDouble());
            }
            if (emitterJson.contains("directionZ")) {
                params.direction.setZ(emitterJson["directionZ"].toDouble());
            }
            
            if (emitterJson.contains("radius")) {
                params.radius = emitterJson["radius"].toDouble();
            }
            if (emitterJson.contains("width")) {
                params.width = emitterJson["width"].toDouble();
            }
            if (emitterJson.contains("height")) {
                params.height = emitterJson["height"].toDouble();
            }
            if (emitterJson.contains("depth")) {
                params.depth = emitterJson["depth"].toDouble();
            }
            
            if (emitterJson.contains("scaleMin")) {
                params.scaleMin = emitterJson["scaleMin"].toDouble();
            }
            if (emitterJson.contains("scaleMax")) {
                params.scaleMax = emitterJson["scaleMax"].toDouble();
            }
            if (emitterJson.contains("scaleEndMin")) {
                params.scaleEndMin = emitterJson["scaleEndMin"].toDouble();
            }
            if (emitterJson.contains("scaleEndMax")) {
                params.scaleEndMax = emitterJson["scaleEndMax"].toDouble();
            }
            
            if (emitterJson.contains("colorStart")) {
                params.colorStart = QColor(emitterJson["colorStart"].toString());
            }
            if (emitterJson.contains("colorEnd")) {
                params.colorEnd = QColor(emitterJson["colorEnd"].toString());
            }
            if (emitterJson.contains("colorVariation")) {
                params.colorVariation = emitterJson["colorVariation"].toDouble();
            }
            
            if (emitterJson.contains("opacityMin")) {
                params.opacityMin = emitterJson["opacityMin"].toDouble();
            }
            if (emitterJson.contains("opacityMax")) {
                params.opacityMax = emitterJson["opacityMax"].toDouble();
            }
            if (emitterJson.contains("opacityEndMin")) {
                params.opacityEndMin = emitterJson["opacityEndMin"].toDouble();
            }
            if (emitterJson.contains("opacityEndMax")) {
                params.opacityEndMax = emitterJson["opacityEndMax"].toDouble();
            }
            
            if (emitterJson.contains("drag")) {
                params.drag = emitterJson["drag"].toDouble();
            }
            if (emitterJson.contains("maxParticles")) {
                params.maxParticles = emitterJson["maxParticles"].toInt();
            }
            
            addEmitter(params);
        }
    }
}

ParticleSystem* ArtifactParticleLayer::particleSystem()
{
    return impl_->particleSystem.get();
}

const ParticleSystem* ArtifactParticleLayer::particleSystem() const
{
    return impl_->particleSystem.get();
}

void ArtifactParticleLayer::createParticleSystem()
{
    impl_->particleSystem = std::make_unique<ParticleSystem>();
    emit particleSystemChanged();
}

void ArtifactParticleLayer::resetParticleSystem()
{
    if (impl_->particleSystem) {
        impl_->particleSystem->clear();
    }
    impl_->cachedFrameNumber = -1;
}

ParticleEmitter* ArtifactParticleLayer::addEmitter()
{
    auto* emitter = impl_->particleSystem->createEmitter();
    emit emitterAdded(impl_->particleSystem->emitterCount() - 1);
    return emitter;
}

ParticleEmitter* ArtifactParticleLayer::addEmitter(const EmitterParams& params)
{
    auto* emitter = addEmitter();
    if (emitter) {
        emitter->setParams(params);
    }
    return emitter;
}

void ArtifactParticleLayer::removeEmitter(int index)
{
    impl_->particleSystem->removeEmitter(index);
    emit emitterRemoved(index);
}

void ArtifactParticleLayer::clearEmitters()
{
    impl_->particleSystem->clearEmitters();
}

int ArtifactParticleLayer::emitterCount() const
{
    return impl_->particleSystem->emitterCount();
}

void ArtifactParticleLayer::addForceEffector(const QVector3D& force)
{
    auto* emitter = impl_->particleSystem->createEmitter();
    if (emitter) {
        auto effector = std::make_unique<ForceEffector>();
        effector->force = force;
        emitter->addEffector(std::move(effector));
    }
}

void ArtifactParticleLayer::addVortexEffector(const QVector3D& position, float radius, float angularVelocity)
{
    auto* emitter = impl_->particleSystem->createEmitter();
    if (emitter) {
        auto effector = std::make_unique<VortexEffector>();
        effector->position = position;
        effector->radius = radius;
        effector->angularVelocity = angularVelocity;
        emitter->addEffector(std::move(effector));
    }
}

void ArtifactParticleLayer::addTurbulenceEffector(float frequency, float amplitude)
{
    auto* emitter = impl_->particleSystem->createEmitter();
    if (emitter) {
        auto effector = std::make_unique<TurbulenceEffector>();
        effector->frequency = frequency;
        effector->amplitude = amplitude;
        emitter->addEffector(std::move(effector));
    }
}

void ArtifactParticleLayer::addAttractorEffector(const QVector3D& position, float radius, float strength)
{
    auto* emitter = impl_->particleSystem->createEmitter();
    if (emitter) {
        auto effector = std::make_unique<AttractorEffector>();
        effector->position = position;
        effector->radius = radius;
        effector->strength = strength;
        emitter->addEffector(std::move(effector));
    }
}

void ArtifactParticleLayer::addWindEffector(const QVector3D& direction, float strength)
{
    auto* emitter = impl_->particleSystem->createEmitter();
    if (emitter) {
        auto effector = std::make_unique<WindEffector>();
        effector->windDirection = direction;
        effector->windStrength = strength;
        emitter->addEffector(std::move(effector));
    }
}

void ArtifactParticleLayer::clearEffectors()
{
    for (auto& emitter : const_cast<std::vector<std::unique_ptr<ParticleEmitter>>&>(impl_->particleSystem->emitters())) {
        emitter->clearEffectors();
    }
}

ParticleRenderSettings& ArtifactParticleLayer::renderSettings()
{
    return impl_->particleSystem->renderSettings();
}

const ParticleRenderSettings& ArtifactParticleLayer::renderSettings() const
{
    return impl_->particleSystem->renderSettings();
}

void ArtifactParticleLayer::setRenderSettings(const ParticleRenderSettings& settings)
{
    impl_->particleSystem->setRenderSettings(settings);
}

void ArtifactParticleLayer::setParticleBlendMode(ParticleBlendMode mode)
{
    impl_->particleSystem->renderSettings().blendMode = mode;
}

ParticleBlendMode ArtifactParticleLayer::particleBlendMode() const
{
    return impl_->particleSystem->renderSettings().blendMode;
}

void ArtifactParticleLayer::play()
{
    impl_->playing = true;
    impl_->particleSystem->setPaused(false);
    emit playbackStateChanged(true);
}

void ArtifactParticleLayer::pause()
{
    impl_->playing = false;
    impl_->particleSystem->setPaused(true);
    emit playbackStateChanged(false);
}

void ArtifactParticleLayer::stop()
{
    impl_->playing = false;
    impl_->particleSystem->setPaused(true);
    reset();
    emit playbackStateChanged(false);
}

void ArtifactParticleLayer::reset()
{
    impl_->particleSystem->clear();
    impl_->cachedFrameNumber = -1;
    impl_->lastTime = 0.0f;
}

bool ArtifactParticleLayer::isPlaying() const
{
    return impl_->playing;
}

void ArtifactParticleLayer::setTimeScale(float scale)
{
    impl_->particleSystem->setTimeScale(scale);
}

float ArtifactParticleLayer::timeScale() const
{
    return impl_->particleSystem->timeScale();
}

void ArtifactParticleLayer::preWarm(float duration)
{
    impl_->particleSystem->preWarm(duration);
}

void ArtifactParticleLayer::goToFrame(int64_t frameNumber)
{
    // Calculate time from frame
    float frameRate = 30.0f;  // TODO: Get from composition
    float time = frameNumber / frameRate;
    
    // Check cache
    if (frameNumber == impl_->cachedFrameNumber) {
        return;
    }
    
    // Update particle system
    if (impl_->playing) {
        float deltaTime = time - impl_->lastTime;
        if (deltaTime < 0) {
            // Time went backwards, reset
            reset();
            impl_->lastTime = 0;
            deltaTime = time;
        }
        impl_->particleSystem->update(deltaTime);
        impl_->lastTime = time;
    }
    
    impl_->cachedFrameNumber = frameNumber;
}

QImage ArtifactParticleLayer::renderFrame(int width, int height, float time)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    renderToImage(image, time);
    return image;
}

void ArtifactParticleLayer::renderToImage(QImage& target, float time)
{
    // Update particle system to this time
    float deltaTime = time - impl_->lastTime;
    if (deltaTime > 0 && impl_->playing) {
        impl_->particleSystem->update(deltaTime);
        impl_->lastTime = time;
    }
    
    // Clear target
    target.fill(Qt::transparent);
    
    // Get transform from layer
    QTransform transform;
    const auto& t2d = transform2D();
    // TODO: Apply transform from AnimatableTransform2D
    
    // Render particles
    QPainter painter(&target);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    impl_->particleSystem->render(painter, transform);
    
    emit frameRendered(static_cast<int64_t>(time * 30.0f));  // Assuming 30fps
}

bool ArtifactParticleLayer::getCachedFrame(int64_t frame, QImage& out)
{
    if (frame == impl_->cachedFrameNumber && !impl_->cachedFrame.isNull()) {
        out = impl_->cachedFrame;
        return true;
    }
    return false;
}

void ArtifactParticleLayer::clearFrameCache()
{
    impl_->cachedFrame = QImage();
    impl_->cachedFrameNumber = -1;
}

void ArtifactParticleLayer::loadPreset(const QString& presetName)
{
    clearEmitters();
    
    EmitterParams params;
    
    if (presetName == "fire") {
        params = ParticlePresets::fire();
    } else if (presetName == "campfire") {
        params = ParticlePresets::campfire();
    } else if (presetName == "torch") {
        params = ParticlePresets::torch();
    } else if (presetName == "smoke") {
        params = ParticlePresets::smoke();
    } else if (presetName == "steam") {
        params = ParticlePresets::steam();
    } else if (presetName == "dust") {
        params = ParticlePresets::dust();
    } else if (presetName == "rain") {
        params = ParticlePresets::rain();
    } else if (presetName == "splash") {
        params = ParticlePresets::splash();
    } else if (presetName == "fountain") {
        params = ParticlePresets::fountain();
    } else if (presetName == "explosion") {
        params = ParticlePresets::explosion();
    } else if (presetName == "debris") {
        params = ParticlePresets::debris();
    } else if (presetName == "sparks") {
        params = ParticlePresets::sparks();
    } else if (presetName == "leaves") {
        params = ParticlePresets::leaves();
    } else if (presetName == "snow") {
        params = ParticlePresets::snow();
    } else if (presetName == "pollen") {
        params = ParticlePresets::pollen();
    } else if (presetName == "magic") {
        params = ParticlePresets::magic();
    } else if (presetName == "sparkles") {
        params = ParticlePresets::sparkles();
    } else if (presetName == "energyField") {
        params = ParticlePresets::energyField();
    } else if (presetName == "confetti") {
        params = ParticlePresets::confetti();
    } else if (presetName == "bubbles") {
        params = ParticlePresets::bubbles();
    } else {
        // Default fallback
        params = ParticlePresets::fire();
    }
    
    addEmitter(params);
    emit particleSystemChanged();
}

QStringList ArtifactParticleLayer::availablePresets() const
{
    return {
        "fire",
        "campfire",
        "torch",
        "smoke",
        "steam",
        "dust",
        "rain",
        "splash",
        "fountain",
        "explosion",
        "debris",
        "sparks",
        "leaves",
        "snow",
        "pollen",
        "magic",
        "sparkles",
        "energyField",
        "confetti",
        "bubbles"
    };
}

// ==================== Factory Functions ====================

std::shared_ptr<ArtifactParticleLayer> createParticleLayer()
{
    return std::make_shared<ArtifactParticleLayer>();
}

std::shared_ptr<ArtifactParticleLayer> createParticleLayer(const QString& preset)
{
    auto layer = std::make_shared<ArtifactParticleLayer>();
    layer->loadPreset(preset);
    return layer;
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactParticleLayer)