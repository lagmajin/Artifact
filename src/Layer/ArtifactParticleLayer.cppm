module;
#include <QObject>
#include <QImage>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QTransform>
#include <QVariant>
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
module Artifact.Layer.Particle;




import Artifact.Layer.Abstract;
import Artifact.Generator.Particle;
import Animation.Transform2D;
import Animation.Transform3D;
import Size;
import Utils.Id;
import Utils.String.UniString;
import Property.Abstract;
import Property.Group;

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

void ArtifactParticleLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer || !impl_->particleSystem) {
        return;
    }

    const int64_t frameNumber = currentFrame();
    
    // 1. 決定論的なシミュレーション状態の更新
    // ※ goToFrame は内部で reset() と forward simulation を行う
    float fps = 30.0f;
    if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
        fps = comp->frameRate().framerate();
    }
    // フレーム0でも最低1フレーム分のシミュレーションを走らせて初期パーティクルを生成する
    impl_->particleSystem->goToFrame(std::max(int64_t{1}, frameNumber), fps);

    // 2. GPU レンダリングパス
    // Diligent 経路が使える場合は billboard 描画を優先し、ここではソフト描画へ落とさない
    if (renderer->isInitialized()) {
        const auto sourceData = impl_->particleSystem->captureRenderData();
        if (!sourceData.particles.empty()) {
            // Core 側の ParticleRenderData に変換 (型不整合を避けるため明示的に)
            ArtifactCore::ParticleRenderData renderData;
            renderData.frameNumber = sourceData.frameNumber;
            renderData.particles.reserve(sourceData.particles.size());
            
            for (const auto& src : sourceData.particles) {
                ArtifactCore::ParticleVertex v;
                v.px = src.px; v.py = src.py; v.pz = src.pz;
                v.vx = src.vx; v.vy = src.vy; v.vz = src.vz;
                v.r = src.r; v.g = src.g; v.b = src.b; v.a = src.a;
                v.size = src.size;
                v.rotation = src.rotation;
                v.age = src.age;
                v.lifetime = src.lifetime;
                renderData.particles.push_back(v);
            }
            
            renderer->drawParticles(renderData);
        }
        return;
    }

    // 3. ソフトウェアフォールバックパス
    // renderer が未初期化のときだけ従来の QPainter 描画を使う
    if (frameNumber != impl_->cachedFrameNumber || impl_->cachedFrame.isNull()) {
        float fallbackFps = 30.0f;
        if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
            fallbackFps = comp->frameRate().framerate();
        }
        const float time = static_cast<float>(frameNumber) / fallbackFps;
        impl_->cachedFrame = renderFrame(std::max(1, impl_->width),
                                         std::max(1, impl_->height),
                                         time);
        impl_->cachedFrameNumber = frameNumber;
    }

    if (impl_->cachedFrame.isNull()) {
        return;
    }

    renderer->drawSprite(
        0.0f,
        0.0f,
        static_cast<float>(impl_->cachedFrame.width()),
        static_cast<float>(impl_->cachedFrame.height()),
        impl_->cachedFrame,
        opacity());
}

QJsonObject ArtifactParticleLayer::toJson() const
{
    QJsonObject json = ArtifactAbstractLayer::toJson();
    json["type"] = static_cast<int>(LayerType::Particle);
    
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
    layer->ArtifactAbstractLayer::fromJsonProperties(obj);
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
    // デフォルトエミッターをキャンバス中心に配置して即座に描画確認できるようにする
    if (auto* emitter = impl_->particleSystem->createEmitter()) {
        EmitterParams params;
        params.position = QVector3D(
            static_cast<float>(impl_->width) / 2.0f,
            static_cast<float>(impl_->height) / 2.0f,
            0.0f);
        params.rate = 30.0f;
        emitter->setParams(params);
    }
    clearFrameCache();
    emit particleSystemChanged();
}

void ArtifactParticleLayer::resetParticleSystem()
{
    if (impl_->particleSystem) {
        impl_->particleSystem->clear();
    }
    clearFrameCache();
    impl_->cachedFrameNumber = -1;
}

ParticleEmitter* ArtifactParticleLayer::addEmitter()
{
    auto* emitter = impl_->particleSystem->createEmitter();
    clearFrameCache();
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
    clearFrameCache();
    emit emitterRemoved(index);
}

void ArtifactParticleLayer::clearEmitters()
{
    impl_->particleSystem->clearEmitters();
    clearFrameCache();
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
    clearFrameCache();
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
    clearFrameCache();
}

void ArtifactParticleLayer::setParticleBlendMode(ParticleBlendMode mode)
{
    impl_->particleSystem->renderSettings().blendMode = mode;
    clearFrameCache();
}

ParticleBlendMode ArtifactParticleLayer::particleBlendMode() const
{
    return impl_->particleSystem->renderSettings().blendMode;
}

void ArtifactParticleLayer::play()
{
    impl_->playing = true;
    impl_->particleSystem->setPaused(false);
    emit playbackStateChanged(PlaybackState::Playing);
}

void ArtifactParticleLayer::pause()
{
    impl_->playing = false;
    impl_->particleSystem->setPaused(true);
    emit playbackStateChanged(PlaybackState::Paused);
}

void ArtifactParticleLayer::stop()
{
    impl_->playing = false;
    impl_->particleSystem->setPaused(true);
    reset();
    emit playbackStateChanged(PlaybackState::Stopped);
}

void ArtifactParticleLayer::reset()
{
    impl_->particleSystem->clear();
    clearFrameCache();
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
    clearFrameCache();
}

float ArtifactParticleLayer::timeScale() const
{
    return impl_->particleSystem->timeScale();
}

void ArtifactParticleLayer::preWarm(float duration)
{
    impl_->particleSystem->preWarm(duration);
    clearFrameCache();
}

void ArtifactParticleLayer::goToFrame(int64_t frameNumber)
{
    // Calculate time from frame
    float fps = 30.0f;
    if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
        fps = comp->frameRate().framerate();
    }
    float time = frameNumber / fps;
    
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
    impl_->cachedFrame = image;

    float fps = 30.0f;
    if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
        fps = comp->frameRate().framerate();
    }
    impl_->cachedFrameNumber = static_cast<int64_t>(time * fps);
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
    const QTransform transform = getLocalTransform();

    // Render particles
    QPainter painter(&target);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    impl_->particleSystem->render(painter, transform);
    
    float fps = 30.0f;
    if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
        fps = comp->frameRate().framerate();
    }
    emit frameRendered(static_cast<int64_t>(time * fps));
}

void ArtifactParticleLayer::renderToImage(QImage& target, int64_t frameNumber)
{
    float fps = 30.0f;
    if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
        fps = comp->frameRate().framerate();
    }
    float time = static_cast<float>(frameNumber) / fps;
    renderToImage(target, time);
}

void ArtifactParticleLayer::renderToImage(QImage& target, int64_t frameNumber)
{
    float fps = 30.0f;
    if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
        fps = comp->frameRate().framerate();
    }
    float time = static_cast<float>(frameNumber) / fps;
    renderToImage(target, time);
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

std::vector<ArtifactCore::PropertyGroup> ArtifactParticleLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup particleGroup(QStringLiteral("Particle"));

    auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type, const QVariant& value, int priority = 0) {
        return persistentLayerProperty(name, type, value, priority);
    };

    particleGroup.addProperty(makeProp(QStringLiteral("particle.playing"), ArtifactCore::PropertyType::Boolean, isPlaying(), -140));
    particleGroup.addProperty(makeProp(QStringLiteral("particle.timeScale"), ArtifactCore::PropertyType::Float, timeScale(), -130));
    particleGroup.addProperty(makeProp(QStringLiteral("particle.emitterCount"), ArtifactCore::PropertyType::Integer, emitterCount(), -120));
    particleGroup.addProperty(makeProp(QStringLiteral("particle.previewWidth"), ArtifactCore::PropertyType::Integer, impl_->width, -110));
    particleGroup.addProperty(makeProp(QStringLiteral("particle.previewHeight"), ArtifactCore::PropertyType::Integer, impl_->height, -100));

    const auto& rs = renderSettings();
    auto blendModeProp = makeProp(QStringLiteral("particle.render.blendMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(rs.blendMode), -90);
    blendModeProp->setTooltip(QStringLiteral("0=Additive, 1=Subtractive, 2=Normal, 3=Screen, 4=Multiply"));
    particleGroup.addProperty(blendModeProp);
    auto billboardModeProp = makeProp(QStringLiteral("particle.render.billboardMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(rs.billboardMode), -80);
    billboardModeProp->setTooltip(QStringLiteral("0=None, 1=ScreenAligned, 2=ViewPlane, 3=VelocityAligned"));
    particleGroup.addProperty(billboardModeProp);
    auto sortModeProp = makeProp(QStringLiteral("particle.render.sortMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(rs.sortMode), -70);
    sortModeProp->setTooltip(QStringLiteral("0=None, 1=Distance, 2=OldestFirst, 3=YoungestFirst"));
    particleGroup.addProperty(sortModeProp);
    particleGroup.addProperty(makeProp(QStringLiteral("particle.render.depthTest"), ArtifactCore::PropertyType::Boolean, rs.depthTest, -60));
    particleGroup.addProperty(makeProp(QStringLiteral("particle.render.depthWrite"), ArtifactCore::PropertyType::Boolean, rs.depthWrite, -50));

    groups.push_back(particleGroup);
    return groups;
}

bool ArtifactParticleLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == QStringLiteral("particle.playing")) {
        value.toBool() ? play() : pause();
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.timeScale")) {
        setTimeScale(static_cast<float>(value.toDouble()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.previewWidth")) {
        impl_->width = std::max(1, value.toInt());
        clearFrameCache();
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.previewHeight")) {
        impl_->height = std::max(1, value.toInt());
        clearFrameCache();
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.render.blendMode")) {
        auto rs = renderSettings();
        rs.blendMode = static_cast<ParticleBlendMode>(value.toInt());
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.render.billboardMode")) {
        auto rs = renderSettings();
        rs.billboardMode = static_cast<ParticleRenderSettings::BillboardMode>(value.toInt());
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.render.sortMode")) {
        auto rs = renderSettings();
        rs.sortMode = static_cast<ParticleRenderSettings::SortMode>(value.toInt());
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.render.depthTest")) {
        auto rs = renderSettings();
        rs.depthTest = value.toBool();
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.render.depthWrite")) {
        auto rs = renderSettings();
        rs.depthWrite = value.toBool();
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
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
