module;
#include <QObject>
#include <QImage>
#include <QDebug>
#include <QString>
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
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Generator.Particle;
import Graphics.ParticleData;
import Animation.Transform2D;
import Animation.Transform3D;
import Size;
import Utils.Id;
import Utils.String.UniString;
import Property.Abstract;
import Property.Group;

namespace Artifact {

namespace {

ParticleEmitter* firstEmitterOrCreate(ParticleSystem* system)
{
    if (!system) {
        return nullptr;
    }
    const auto& emitters = system->emitters();
    if (!emitters.empty()) {
        return emitters.front().get();
    }
    return system->createEmitter();
}

ArtifactCore::ParticleRenderData transformParticleRenderData(
    const ParticleRenderData& source,
    const QTransform& transform,
    float opacity)
{
    ArtifactCore::ParticleRenderData transformed;
    transformed.frameNumber = source.frameNumber;
    transformed.particles.reserve(source.particles.size());

    const float scaleX = std::hypot(transform.m11(), transform.m21());
    const float scaleY = std::hypot(transform.m12(), transform.m22());
    const float scale = std::max(0.001f, std::max(scaleX, scaleY));

    qInfo() << "[ParticleLayer] transform"
            << "m11=" << transform.m11()
            << "m12=" << transform.m12()
            << "m21=" << transform.m21()
            << "m22=" << transform.m22()
            << "dx=" << transform.dx()
            << "dy=" << transform.dy()
            << "scaleX=" << scaleX
            << "scaleY=" << scaleY
            << "scale=" << scale
            << "opacity=" << opacity
            << "sourceCount=" << source.particles.size();

    bool loggedFirstParticle = false;
    for (const auto& src : source.particles) {
        ArtifactCore::ParticleVertex v;
        v.px = src.px;
        v.py = src.py;
        v.pz = src.pz;
        v.vx = src.vx;
        v.vy = src.vy;
        v.vz = src.vz;
        v.r = src.r;
        v.g = src.g;
        v.b = src.b;
        v.a = src.a;
        v.size = src.size;
        v.stretch = src.stretch;
        v.rotation = src.rotation;
        v.age = src.age;
        v.lifetime = src.lifetime;
        v.spriteFrame = src.spriteFrame;
        v.spriteRows = src.spriteRows;
        v.spriteCols = src.spriteCols;
        const QPointF mapped = transform.map(QPointF(src.px, src.py));
        v.px = static_cast<float>(mapped.x());
        v.py = static_cast<float>(mapped.y());
        v.a = std::clamp(v.a * opacity, 0.0f, 1.0f);
        v.size = std::max(4.0f, src.size * scale);
        if (v.stretch <= 0.0f) {
            const float speed = std::hypot(src.vx, src.vy);
            v.stretch = std::clamp(1.0f + speed * 0.004f, 1.0f, 6.0f);
        }
        if (!loggedFirstParticle) {
            qInfo() << "[ParticleLayer] particle0"
                    << "src=(" << src.px << "," << src.py << ")"
                    << "mapped=(" << mapped.x() << "," << mapped.y() << ")"
                    << "size=" << src.size << "->" << v.size
                    << "alpha=" << src.a << "->" << v.a
                    << "stretch=" << src.stretch << "->" << v.stretch;
            loggedFirstParticle = true;
        }
        transformed.particles.push_back(v);
    }

    return transformed;
}

void boostDebugParticleRenderData(ArtifactCore::ParticleRenderData& data)
{
    for (auto& particle : data.particles) {
        particle.size = std::max(18.0f, particle.size * 4.0f);
        particle.a = 1.0f;
        particle.r = std::clamp(particle.r * 1.15f + 0.20f, 0.0f, 1.0f);
        particle.g = std::clamp(particle.g * 1.15f + 0.20f, 0.0f, 1.0f);
        particle.b = std::clamp(particle.b * 1.15f + 0.20f, 0.0f, 1.0f);
    }
}

QVector3D defaultEmitterPositionForPreset(const QString& presetName,
                                          int width,
                                          int height)
{
    const float w = static_cast<float>(std::max(1, width));
    const float h = static_cast<float>(std::max(1, height));
    if (presetName == QStringLiteral("rain") ||
        presetName == QStringLiteral("snow") ||
        presetName == QStringLiteral("leaves") ||
        presetName == QStringLiteral("pollen") ||
        presetName == QStringLiteral("confetti")) {
        return QVector3D(w * 0.5f, h * 0.18f, 0.0f);
    }
    if (presetName == QStringLiteral("splash") ||
        presetName == QStringLiteral("fountain")) {
        return QVector3D(w * 0.5f, h * 0.78f, 0.0f);
    }
    return QVector3D(w * 0.5f, h * 0.5f, 0.0f);
}

} // namespace

// ==================== ArtifactParticleLayer::Impl ====================

class ArtifactParticleLayer::Impl {
public:
    std::unique_ptr<ParticleSystem> particleSystem;
    std::vector<EmitterParams> savedEmitterParams;
    QImage cachedFrame;
    int64_t cachedFrameNumber = -1;
    bool playing = true;
    float lastTime = 0.0f;
    int width = 1920;
    int height = 1080;
    
    Impl() {
        particleSystem = std::make_unique<ParticleSystem>();
    }

    void rebuildSavedEmitterParamsFromSystem()
    {
        savedEmitterParams.clear();
        if (!particleSystem) {
            return;
        }
        savedEmitterParams.reserve(particleSystem->emitters().size());
        for (const auto& emitter : particleSystem->emitters()) {
            if (!emitter) {
                continue;
            }
            savedEmitterParams.push_back(emitter->params());
        }
    }

    void scaleEmitterPositions(float scaleX, float scaleY)
    {
        auto& emitters = const_cast<std::vector<std::unique_ptr<ParticleEmitter>>&>(
            particleSystem->emitters());
        for (auto& emitter : emitters) {
            if (!emitter) {
                continue;
            }
            auto params = emitter->params();
            params.position = QVector3D(params.position.x() * scaleX,
                                        params.position.y() * scaleY,
                                        params.position.z());
            emitter->setParams(params);
        }
        rebuildSavedEmitterParamsFromSystem();
    }

    ParticleEmitter* primaryEmitter()
    {
        return firstEmitterOrCreate(particleSystem.get());
    }

    std::optional<EmitterParams> primaryEmitterParams() const
    {
        if (!savedEmitterParams.empty()) {
            return savedEmitterParams.front();
        }
        if (!particleSystem) {
            return std::nullopt;
        }
        const auto& emitters = particleSystem->emitters();
        if (emitters.empty() || !emitters.front()) {
            return std::nullopt;
        }
        return emitters.front()->params();
    }

    bool applyPrimaryEmitterParams(const std::function<void(EmitterParams&)>& mutator)
    {
        auto* emitter = primaryEmitter();
        if (!emitter) {
            return false;
        }
        auto params = emitter->params();
        mutator(params);
        emitter->setParams(params);
        rebuildSavedEmitterParamsFromSystem();
        return true;
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
        qWarning() << "[ParticleLayer] draw() early exit: renderer=" << (renderer ? "ok" : "null")
                   << "particleSystem=" << (impl_->particleSystem ? "ok" : "null");
        return;
    }

    const int64_t frameNumber = currentFrame();
    const bool rendererReady = renderer->isInitialized();
    const int emitterCount = impl_->particleSystem->emitterCount();
    
    qInfo() << "[ParticleLayer] draw() frame=" << frameNumber
            << "rendererInitialized=" << rendererReady
            << "emitters=" << emitterCount;

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
    if (rendererReady) {
        const auto sourceData = impl_->particleSystem->captureRenderData();
        qInfo() << "[ParticleLayer] GPU path: particleCount=" << sourceData.particles.size();
        if (!sourceData.particles.empty()) {
            const QTransform globalTransform = getGlobalTransform();
            const ArtifactCore::ParticleRenderData renderData =
                transformParticleRenderData(sourceData, globalTransform, opacity());
            renderer->drawParticles(renderData);
        } else {
            qWarning() << "[ParticleLayer] GPU path: NO PARTICLES - emitter may not generate";
        }
        return;
    }

    // 3. ソフトウェアフォールバックパス
    // renderer が未初期化のときだけ従来の QPainter 描画を使う
    qInfo() << "[ParticleLayer] Fallback path: cachedFrame=" << impl_->cachedFrameNumber
            << "currentFrame=" << frameNumber
            << "cachedNull=" << impl_->cachedFrame.isNull();
    
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
        qInfo() << "[ParticleLayer] Fallback rendered: size=" << impl_->cachedFrame.size()
                << "null=" << impl_->cachedFrame.isNull();
    }

    if (impl_->cachedFrame.isNull()) {
        qWarning() << "[ParticleLayer] Fallback draw skipped: cachedFrame is null";
        return;
    }

    qInfo() << "[ParticleLayer] drawSprite: w=" << impl_->cachedFrame.width()
            << "h=" << impl_->cachedFrame.height() << "opacity=" << opacity();
    renderer->drawSprite(
        0.0f,
        0.0f,
        static_cast<float>(impl_->cachedFrame.width()),
        static_cast<float>(impl_->cachedFrame.height()),
        impl_->cachedFrame,
        opacity());
}

QRectF ArtifactParticleLayer::localBounds() const
{
    if (!impl_) {
        return QRectF();
    }
    const int width = std::max(0, impl_->width);
    const int height = std::max(0, impl_->height);
    if (width <= 0 || height <= 0) {
        return QRectF();
    }
    return QRectF(0.0, 0.0, static_cast<qreal>(width), static_cast<qreal>(height));
}

QString ArtifactParticleLayer::debugState() const
{
    if (!impl_ || !impl_->particleSystem) {
        return QStringLiteral("<no particle system>");
    }

    const auto& rs = impl_->particleSystem->renderSettings();
    const auto sourceData = impl_->particleSystem->captureRenderData();
    const int emitterCount = impl_->particleSystem->emitterCount();
    return QStringLiteral("playing=%1 emitters=%2 alive=%3 blend=%4 billboard=%5 sort=%6 depthTest=%7 depthWrite=%8 cachedFrame=%9 timeScale=%10 bounds={%11}")
        .arg(impl_->playing ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(emitterCount)
        .arg(sourceData.particles.size())
        .arg(static_cast<int>(rs.blendMode))
        .arg(static_cast<int>(rs.billboardMode))
        .arg(static_cast<int>(rs.sortMode))
        .arg(rs.depthTest ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(rs.depthWrite ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(impl_->cachedFrameNumber)
        .arg(QString::number(impl_->particleSystem->timeScale(), 'f', 3))
        .arg(contentBoundsSummary());
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
    renderJson["softParticles"] = rs.softParticles;
    renderJson["softParticleDistance"] = rs.softParticleDistance;
    renderJson["stretchEnabled"] = rs.stretchEnabled;
    renderJson["stretchFactor"] = rs.stretchFactor;
    json["renderSettings"] = renderJson;
    
    // Save emitters
    QJsonArray emittersArray;
    for (size_t emitterIndex = 0; emitterIndex < impl_->savedEmitterParams.size(); ++emitterIndex) {
        const auto& params = impl_->savedEmitterParams[emitterIndex];
        QJsonObject emitterJson;
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
        emitterJson["velocityRandomX"] = params.velocityRandom.x();
        emitterJson["velocityRandomY"] = params.velocityRandom.y();
        emitterJson["velocityRandomZ"] = params.velocityRandom.z();
        
        emitterJson["positionX"] = params.position.x();
        emitterJson["positionY"] = params.position.y();
        emitterJson["positionZ"] = params.position.z();
        emitterJson["rotationX"] = params.rotation.x();
        emitterJson["rotationY"] = params.rotation.y();
        emitterJson["rotationZ"] = params.rotation.z();
        emitterJson["rotationSpeedMin"] = params.rotationSpeedMin;
        emitterJson["rotationSpeedMax"] = params.rotationSpeedMax;
        
        emitterJson["directionX"] = params.direction.x();
        emitterJson["directionY"] = params.direction.y();
        emitterJson["directionZ"] = params.direction.z();
        
        emitterJson["radius"] = params.radius;
        emitterJson["width"] = params.width;
        emitterJson["height"] = params.height;
        emitterJson["depth"] = params.depth;
        emitterJson["lineLength"] = params.lineLength;
        
        emitterJson["scaleMin"] = params.scaleMin;
        emitterJson["scaleMax"] = params.scaleMax;
        emitterJson["scaleMidMin"] = params.scaleMidMin;
        emitterJson["scaleMidMax"] = params.scaleMidMax;
        emitterJson["scaleMidPosition"] = params.scaleMidPosition;
        emitterJson["scaleEndMin"] = params.scaleEndMin;
        emitterJson["scaleEndMax"] = params.scaleEndMax;
        
        emitterJson["colorStart"] = params.colorStart.name(QColor::HexArgb);
        emitterJson["colorMid"] = params.colorMid.name(QColor::HexArgb);
        emitterJson["colorEnd"] = params.colorEnd.name(QColor::HexArgb);
        emitterJson["colorMidPosition"] = params.colorMidPosition;
        emitterJson["colorVariation"] = params.colorVariation;
        
        emitterJson["opacityMin"] = params.opacityMin;
        emitterJson["opacityMax"] = params.opacityMax;
        emitterJson["opacityMidMin"] = params.opacityMidMin;
        emitterJson["opacityMidMax"] = params.opacityMidMax;
        emitterJson["opacityMidPosition"] = params.opacityMidPosition;
        emitterJson["opacityEndMin"] = params.opacityEndMin;
        emitterJson["opacityEndMax"] = params.opacityEndMax;
        
        emitterJson["drag"] = params.drag;
        emitterJson["gravityX"] = params.gravity.x();
        emitterJson["gravityY"] = params.gravity.y();
        emitterJson["gravityZ"] = params.gravity.z();
        emitterJson["windDirectionX"] = params.windDirection.x();
        emitterJson["windDirectionY"] = params.windDirection.y();
        emitterJson["windDirectionZ"] = params.windDirection.z();
        emitterJson["windStrength"] = params.windStrength;
        emitterJson["turbulenceFrequency"] = params.turbulenceFrequency;
        emitterJson["turbulenceAmplitude"] = params.turbulenceAmplitude;
        emitterJson["turbulenceEvolution"] = params.turbulenceEvolution;
        emitterJson["texturePath"] = params.texturePath;
        emitterJson["textureRows"] = params.textureRows;
        emitterJson["textureCols"] = params.textureCols;
        emitterJson["randomFrame"] = params.randomFrame;
        emitterJson["startFrame"] = params.startFrame;
        emitterJson["frameCount"] = params.frameCount;
        emitterJson["frameRate"] = params.frameRate;
        emitterJson["mass"] = params.mass;
        emitterJson["inheritVelocity"] = params.inheritVelocity;
        emitterJson["worldSpace"] = params.worldSpace;
        emitterJson["preWarm"] = params.preWarm;
        emitterJson["maxParticles"] = params.maxParticles;
        emitterJson["auxEnabled"] = params.auxEnabled;
        emitterJson["auxTrigger"] = static_cast<int>(params.auxTrigger);
        emitterJson["auxCount"] = params.auxCount;
        emitterJson["auxInterval"] = params.auxInterval;
        emitterJson["auxLifeScale"] = params.auxLifeScale;
        emitterJson["auxSizeScale"] = params.auxSizeScale;
        emitterJson["auxOpacityScale"] = params.auxOpacityScale;
        emitterJson["auxVelocityScale"] = params.auxVelocityScale;
        QJsonArray effectorsArray;
        if (emitterIndex < impl_->particleSystem->emitters().size()) {
            const auto& emitter = impl_->particleSystem->emitters()[emitterIndex];
            if (emitter) {
                for (const auto& effector : emitter->effectors()) {
                    if (!effector) {
                        continue;
                    }
                    QJsonObject effectorJson;
                    effectorJson["type"] = static_cast<int>(effector->type);
                    effectorJson["enabled"] = effector->enabled;
                    effectorJson["strength"] = effector->strength;
                    effectorJson["positionX"] = effector->position.x();
                    effectorJson["positionY"] = effector->position.y();
                    effectorJson["positionZ"] = effector->position.z();
                    effectorJson["directionX"] = effector->direction.x();
                    effectorJson["directionY"] = effector->direction.y();
                    effectorJson["directionZ"] = effector->direction.z();
                    if (const auto* typed = dynamic_cast<const ForceEffector*>(effector.get())) {
                        effectorJson["forceX"] = typed->force.x();
                        effectorJson["forceY"] = typed->force.y();
                        effectorJson["forceZ"] = typed->force.z();
                    } else if (const auto* typed = dynamic_cast<const VortexEffector*>(effector.get())) {
                        effectorJson["radius"] = typed->radius;
                        effectorJson["angularVelocity"] = typed->angularVelocity;
                        effectorJson["tightness"] = typed->tightness;
                    } else if (const auto* typed = dynamic_cast<const TurbulenceEffector*>(effector.get())) {
                        effectorJson["frequency"] = typed->frequency;
                        effectorJson["amplitude"] = typed->amplitude;
                        effectorJson["octaves"] = typed->octaves;
                        effectorJson["evolution"] = typed->evolution;
                        effectorJson["seed"] = typed->seed;
                    } else if (const auto* typed = dynamic_cast<const AttractorEffector*>(effector.get())) {
                        effectorJson["radius"] = typed->radius;
                        effectorJson["falloff"] = typed->falloff;
                        effectorJson["killOnReach"] = typed->killOnReach;
                        effectorJson["killRadius"] = typed->killRadius;
                    } else if (const auto* typed = dynamic_cast<const RepellerEffector*>(effector.get())) {
                        effectorJson["radius"] = typed->radius;
                        effectorJson["falloff"] = typed->falloff;
                    } else if (const auto* typed = dynamic_cast<const WindEffector*>(effector.get())) {
                        effectorJson["windDirectionX"] = typed->windDirection.x();
                        effectorJson["windDirectionY"] = typed->windDirection.y();
                        effectorJson["windDirectionZ"] = typed->windDirection.z();
                        effectorJson["windStrength"] = typed->windStrength;
                        effectorJson["turbulence"] = typed->turbulence;
                        effectorJson["turbulenceFrequency"] = typed->turbulenceFrequency;
                        effectorJson["evolution"] = typed->evolution;
                    } else if (const auto* typed = dynamic_cast<const KillZoneEffector*>(effector.get())) {
                        effectorJson["zoneType"] = static_cast<int>(typed->zoneType);
                        effectorJson["sizeX"] = typed->size.x();
                        effectorJson["sizeY"] = typed->size.y();
                        effectorJson["sizeZ"] = typed->size.z();
                        effectorJson["invert"] = typed->invert;
                    }
                    effectorsArray.append(effectorJson);
                }
            }
        }
        emitterJson["effectors"] = effectorsArray;
        
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
        if (renderJson.contains("softParticles")) {
            rs.softParticles = renderJson["softParticles"].toBool();
        }
        if (renderJson.contains("softParticleDistance")) {
            rs.softParticleDistance = renderJson["softParticleDistance"].toDouble();
        }
        if (renderJson.contains("stretchEnabled")) {
            rs.stretchEnabled = renderJson["stretchEnabled"].toBool();
        }
        if (renderJson.contains("stretchFactor")) {
            rs.stretchFactor = renderJson["stretchFactor"].toDouble();
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
            if (emitterJson.contains("velocityRandomX")) {
                params.velocityRandom.setX(emitterJson["velocityRandomX"].toDouble());
            }
            if (emitterJson.contains("velocityRandomY")) {
                params.velocityRandom.setY(emitterJson["velocityRandomY"].toDouble());
            }
            if (emitterJson.contains("velocityRandomZ")) {
                params.velocityRandom.setZ(emitterJson["velocityRandomZ"].toDouble());
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
            if (emitterJson.contains("rotationX")) {
                params.rotation.setX(emitterJson["rotationX"].toDouble());
            }
            if (emitterJson.contains("rotationY")) {
                params.rotation.setY(emitterJson["rotationY"].toDouble());
            }
            if (emitterJson.contains("rotationZ")) {
                params.rotation.setZ(emitterJson["rotationZ"].toDouble());
            }
            if (emitterJson.contains("rotationSpeedMin")) {
                params.rotationSpeedMin = emitterJson["rotationSpeedMin"].toDouble();
            }
            if (emitterJson.contains("rotationSpeedMax")) {
                params.rotationSpeedMax = emitterJson["rotationSpeedMax"].toDouble();
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
            if (emitterJson.contains("lineLength")) {
                params.lineLength = emitterJson["lineLength"].toDouble();
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
            if (emitterJson.contains("scaleMidMin")) {
                params.scaleMidMin = emitterJson["scaleMidMin"].toDouble();
            }
            if (emitterJson.contains("scaleMidMax")) {
                params.scaleMidMax = emitterJson["scaleMidMax"].toDouble();
            }
            if (emitterJson.contains("scaleMidPosition")) {
                params.scaleMidPosition = emitterJson["scaleMidPosition"].toDouble();
            }
            
            if (emitterJson.contains("colorStart")) {
                params.colorStart = QColor(emitterJson["colorStart"].toString());
            }
            if (emitterJson.contains("colorMid")) {
                params.colorMid = QColor(emitterJson["colorMid"].toString());
            }
            if (emitterJson.contains("colorEnd")) {
                params.colorEnd = QColor(emitterJson["colorEnd"].toString());
            }
            if (emitterJson.contains("colorMidPosition")) {
                params.colorMidPosition = emitterJson["colorMidPosition"].toDouble();
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
            if (emitterJson.contains("opacityMidMin")) {
                params.opacityMidMin = emitterJson["opacityMidMin"].toDouble();
            }
            if (emitterJson.contains("opacityMidMax")) {
                params.opacityMidMax = emitterJson["opacityMidMax"].toDouble();
            }
            if (emitterJson.contains("opacityMidPosition")) {
                params.opacityMidPosition = emitterJson["opacityMidPosition"].toDouble();
            }
            
            if (emitterJson.contains("drag")) {
                params.drag = emitterJson["drag"].toDouble();
            }
            if (emitterJson.contains("gravityX")) {
                params.gravity.setX(emitterJson["gravityX"].toDouble());
            }
            if (emitterJson.contains("gravityY")) {
                params.gravity.setY(emitterJson["gravityY"].toDouble());
            }
            if (emitterJson.contains("gravityZ")) {
                params.gravity.setZ(emitterJson["gravityZ"].toDouble());
            }
            if (emitterJson.contains("windDirectionX")) {
                params.windDirection.setX(emitterJson["windDirectionX"].toDouble());
            }
            if (emitterJson.contains("windDirectionY")) {
                params.windDirection.setY(emitterJson["windDirectionY"].toDouble());
            }
            if (emitterJson.contains("windDirectionZ")) {
                params.windDirection.setZ(emitterJson["windDirectionZ"].toDouble());
            }
            if (emitterJson.contains("windStrength")) {
                params.windStrength = emitterJson["windStrength"].toDouble();
            }
            if (emitterJson.contains("turbulenceFrequency")) {
                params.turbulenceFrequency = emitterJson["turbulenceFrequency"].toDouble();
            }
            if (emitterJson.contains("turbulenceAmplitude")) {
                params.turbulenceAmplitude = emitterJson["turbulenceAmplitude"].toDouble();
            }
            if (emitterJson.contains("turbulenceEvolution")) {
                params.turbulenceEvolution = emitterJson["turbulenceEvolution"].toDouble();
            }
            if (emitterJson.contains("texturePath")) {
                params.texturePath = emitterJson["texturePath"].toString();
            }
            if (emitterJson.contains("textureRows")) {
                params.textureRows = emitterJson["textureRows"].toInt();
            }
            if (emitterJson.contains("textureCols")) {
                params.textureCols = emitterJson["textureCols"].toInt();
            }
            if (emitterJson.contains("randomFrame")) {
                params.randomFrame = emitterJson["randomFrame"].toBool();
            }
            if (emitterJson.contains("startFrame")) {
                params.startFrame = emitterJson["startFrame"].toInt();
            }
            if (emitterJson.contains("frameCount")) {
                params.frameCount = emitterJson["frameCount"].toInt();
            }
            if (emitterJson.contains("frameRate")) {
                params.frameRate = emitterJson["frameRate"].toDouble();
            }
            if (emitterJson.contains("mass")) {
                params.mass = emitterJson["mass"].toDouble();
            }
            if (emitterJson.contains("inheritVelocity")) {
                params.inheritVelocity = emitterJson["inheritVelocity"].toBool();
            }
            if (emitterJson.contains("worldSpace")) {
                params.worldSpace = emitterJson["worldSpace"].toBool();
            }
            if (emitterJson.contains("preWarm")) {
                params.preWarm = emitterJson["preWarm"].toBool();
            }
            if (emitterJson.contains("maxParticles")) {
                params.maxParticles = emitterJson["maxParticles"].toInt();
            }
            if (emitterJson.contains("auxEnabled")) {
                params.auxEnabled = emitterJson["auxEnabled"].toBool();
            }
            if (emitterJson.contains("auxTrigger")) {
                params.auxTrigger = static_cast<AuxTriggerMode>(emitterJson["auxTrigger"].toInt());
            }
            if (emitterJson.contains("auxCount")) {
                params.auxCount = emitterJson["auxCount"].toInt();
            }
            if (emitterJson.contains("auxInterval")) {
                params.auxInterval = emitterJson["auxInterval"].toDouble();
            }
            if (emitterJson.contains("auxLifeScale")) {
                params.auxLifeScale = emitterJson["auxLifeScale"].toDouble();
            }
            if (emitterJson.contains("auxSizeScale")) {
                params.auxSizeScale = emitterJson["auxSizeScale"].toDouble();
            }
            if (emitterJson.contains("auxOpacityScale")) {
                params.auxOpacityScale = emitterJson["auxOpacityScale"].toDouble();
            }
            if (emitterJson.contains("auxVelocityScale")) {
                params.auxVelocityScale = emitterJson["auxVelocityScale"].toDouble();
            }
            
            ParticleEmitter* emitter = addEmitter(params);
            if (emitter && emitterJson.contains("effectors")) {
                const QJsonArray effectorsArray = emitterJson["effectors"].toArray();
                for (const auto& effectorVal : effectorsArray) {
                    const QJsonObject effectorJson = effectorVal.toObject();
                    const auto type = static_cast<EffectorType>(effectorJson["type"].toInt());
                    std::unique_ptr<ParticleEffector> effector;
                    switch (type) {
                        case EffectorType::Force: effector = std::make_unique<ForceEffector>(); break;
                        case EffectorType::Vortex: effector = std::make_unique<VortexEffector>(); break;
                        case EffectorType::Turbulence: effector = std::make_unique<TurbulenceEffector>(); break;
                        case EffectorType::Attractor: effector = std::make_unique<AttractorEffector>(); break;
                        case EffectorType::Repeller: effector = std::make_unique<RepellerEffector>(); break;
                        case EffectorType::Wind: effector = std::make_unique<WindEffector>(); break;
                        case EffectorType::Kill: effector = std::make_unique<KillZoneEffector>(); break;
                    }
                    if (!effector) {
                        continue;
                    }

                    effector->enabled = effectorJson["enabled"].toBool(true);
                    effector->strength = static_cast<float>(effectorJson["strength"].toDouble(1.0));
                    effector->position.setX(static_cast<float>(effectorJson["positionX"].toDouble()));
                    effector->position.setY(static_cast<float>(effectorJson["positionY"].toDouble()));
                    effector->position.setZ(static_cast<float>(effectorJson["positionZ"].toDouble()));
                    effector->direction.setX(static_cast<float>(effectorJson["directionX"].toDouble()));
                    effector->direction.setY(static_cast<float>(effectorJson["directionY"].toDouble()));
                    effector->direction.setZ(static_cast<float>(effectorJson["directionZ"].toDouble()));

                    switch (type) {
                        case EffectorType::Force: {
                            auto* typed = static_cast<ForceEffector*>(effector.get());
                            typed->force.setX(static_cast<float>(effectorJson["forceX"].toDouble()));
                            typed->force.setY(static_cast<float>(effectorJson["forceY"].toDouble()));
                            typed->force.setZ(static_cast<float>(effectorJson["forceZ"].toDouble()));
                            break;
                        }
                        case EffectorType::Vortex: {
                            auto* typed = static_cast<VortexEffector*>(effector.get());
                            typed->radius = static_cast<float>(effectorJson["radius"].toDouble());
                            typed->angularVelocity = static_cast<float>(effectorJson["angularVelocity"].toDouble());
                            typed->tightness = static_cast<float>(effectorJson["tightness"].toDouble(1.0));
                            break;
                        }
                        case EffectorType::Turbulence: {
                            auto* typed = static_cast<TurbulenceEffector*>(effector.get());
                            typed->frequency = static_cast<float>(effectorJson["frequency"].toDouble());
                            typed->amplitude = static_cast<float>(effectorJson["amplitude"].toDouble());
                            typed->octaves = effectorJson["octaves"].toInt(3);
                            typed->evolution = static_cast<float>(effectorJson["evolution"].toDouble());
                            typed->seed = effectorJson["seed"].toInt(0);
                            break;
                        }
                        case EffectorType::Attractor: {
                            auto* typed = static_cast<AttractorEffector*>(effector.get());
                            typed->radius = static_cast<float>(effectorJson["radius"].toDouble());
                            typed->falloff = static_cast<float>(effectorJson["falloff"].toDouble(1.0));
                            typed->killOnReach = effectorJson["killOnReach"].toBool(false);
                            typed->killRadius = static_cast<float>(effectorJson["killRadius"].toDouble(10.0));
                            break;
                        }
                        case EffectorType::Repeller: {
                            auto* typed = static_cast<RepellerEffector*>(effector.get());
                            typed->radius = static_cast<float>(effectorJson["radius"].toDouble());
                            typed->falloff = static_cast<float>(effectorJson["falloff"].toDouble(1.0));
                            break;
                        }
                        case EffectorType::Wind: {
                            auto* typed = static_cast<WindEffector*>(effector.get());
                            typed->windDirection.setX(static_cast<float>(effectorJson["windDirectionX"].toDouble()));
                            typed->windDirection.setY(static_cast<float>(effectorJson["windDirectionY"].toDouble()));
                            typed->windDirection.setZ(static_cast<float>(effectorJson["windDirectionZ"].toDouble()));
                            typed->windStrength = static_cast<float>(effectorJson["windStrength"].toDouble());
                            typed->turbulence = static_cast<float>(effectorJson["turbulence"].toDouble());
                            typed->turbulenceFrequency = static_cast<float>(effectorJson["turbulenceFrequency"].toDouble());
                            typed->evolution = static_cast<float>(effectorJson["evolution"].toDouble());
                            break;
                        }
                        case EffectorType::Kill: {
                            auto* typed = static_cast<KillZoneEffector*>(effector.get());
                            typed->zoneType = static_cast<KillZoneEffector::ZoneType>(effectorJson["zoneType"].toInt(0));
                            typed->size.setX(static_cast<float>(effectorJson["sizeX"].toDouble()));
                            typed->size.setY(static_cast<float>(effectorJson["sizeY"].toDouble()));
                            typed->size.setZ(static_cast<float>(effectorJson["sizeZ"].toDouble()));
                            typed->invert = effectorJson["invert"].toBool(false);
                            break;
                        }
                    }

                    emitter->addEffector(std::move(effector));
                }
            }
        }
        impl_->rebuildSavedEmitterParamsFromSystem();
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
        params.rate = 100.0f;
        params.scaleMin = 10.0f;
        params.scaleMax = 20.0f;
        params.scaleEndMin = 2.0f;
        params.scaleEndMax = 5.0f;
        params.colorStart = QColor(255, 200, 50, 255);
        params.colorEnd = QColor(255, 50, 0, 0);
        // 冒頭フレームでも粒子が蓄積した状態で描画されるよう、プリウォームを有効化する。
        // goToFrame() 側が frame <= 1 のときだけ preWarm() を呼ぶため、
        // タイムライン途中のシミュレーション見た目には影響しない。
        params.preWarm = true;
        emitter->setParams(params);
    }
    impl_->rebuildSavedEmitterParamsFromSystem();
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
    impl_->rebuildSavedEmitterParamsFromSystem();
    clearFrameCache();
    emit emitterAdded(impl_->particleSystem->emitterCount() - 1);
    return emitter;
}

ParticleEmitter* ArtifactParticleLayer::addEmitter(const EmitterParams& params)
{
    auto* emitter = addEmitter();
    if (emitter) {
        emitter->setParams(params);
        impl_->rebuildSavedEmitterParamsFromSystem();
    }
    return emitter;
}

void ArtifactParticleLayer::removeEmitter(int index)
{
    impl_->particleSystem->removeEmitter(index);
    impl_->rebuildSavedEmitterParamsFromSystem();
    clearFrameCache();
    emit emitterRemoved(index);
}

void ArtifactParticleLayer::clearEmitters()
{
    impl_->particleSystem->clearEmitters();
    impl_->savedEmitterParams.clear();
    clearFrameCache();
}

int ArtifactParticleLayer::emitterCount() const
{
    return impl_->particleSystem->emitterCount();
}

void ArtifactParticleLayer::addForceEffector(const QVector3D& force)
{
    auto* emitter = firstEmitterOrCreate(impl_->particleSystem.get());
    if (emitter) {
        auto effector = std::make_unique<ForceEffector>();
        effector->force = force;
        emitter->addEffector(std::move(effector));
    }
}

void ArtifactParticleLayer::addVortexEffector(const QVector3D& position, float radius, float angularVelocity)
{
    auto* emitter = firstEmitterOrCreate(impl_->particleSystem.get());
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
    auto* emitter = firstEmitterOrCreate(impl_->particleSystem.get());
    if (emitter) {
        auto effector = std::make_unique<TurbulenceEffector>();
        effector->frequency = frequency;
        effector->amplitude = amplitude;
        emitter->addEffector(std::move(effector));
    }
}

void ArtifactParticleLayer::addAttractorEffector(const QVector3D& position, float radius, float strength)
{
    auto* emitter = firstEmitterOrCreate(impl_->particleSystem.get());
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
    auto* emitter = firstEmitterOrCreate(impl_->particleSystem.get());
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

    // The simulation state moved to a new frame, so any previously rasterized
    // image is stale even if the frame number cache is about to be updated.
    impl_->cachedFrame = QImage();
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
    
    // Render in layer-local space. The composition renderer applies the
    // layer's transform again when it composites the returned image.
    QPainter painter(&target);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    impl_->particleSystem->render(painter, QTransform());
    
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

    params.position =
        defaultEmitterPositionForPreset(presetName, impl_->width, impl_->height);
    addEmitter(params);
    impl_->rebuildSavedEmitterParamsFromSystem();
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
    ArtifactCore::PropertyGroup particleGroup(QStringLiteral("Particle System"));

    auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type, const QVariant& value, int priority = 0) {
        return persistentLayerProperty(name, type, value, priority);
    };

    particleGroup.addProperty(makeProp(QStringLiteral("particle.playing"), ArtifactCore::PropertyType::Boolean, isPlaying(), -140));
    particleGroup.addProperty(makeProp(QStringLiteral("particle.timeScale"), ArtifactCore::PropertyType::Float, timeScale(), -130));
    // Keep both the editor and the property cache non-negative.
    auto emitterCountProp = makeProp(QStringLiteral("particle.emitterCount"), ArtifactCore::PropertyType::Integer, emitterCount(), -120);
    emitterCountProp->setMinValue(0);
    emitterCountProp->setHardRange(0, QVariant());
    particleGroup.addProperty(emitterCountProp);
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
    auto softParticlesProp = makeProp(QStringLiteral("particle.render.softParticles"), ArtifactCore::PropertyType::Boolean, rs.softParticles, -40);
    softParticlesProp->setDisplayLabel(QStringLiteral("Soft Particles"));
    particleGroup.addProperty(softParticlesProp);

    auto softParticleDistanceProp = makeProp(QStringLiteral("particle.render.softParticleDistance"), ArtifactCore::PropertyType::Float, rs.softParticleDistance, -39);
    softParticleDistanceProp->setDisplayLabel(QStringLiteral("Soft Particle Distance"));
    softParticleDistanceProp->setUnit(QStringLiteral("px"));
    softParticleDistanceProp->setSoftRange(0.0, 200.0);
    softParticleDistanceProp->setStep(0.1);
    particleGroup.addProperty(softParticleDistanceProp);

    auto stretchEnabledProp = makeProp(QStringLiteral("particle.render.stretchEnabled"), ArtifactCore::PropertyType::Boolean, rs.stretchEnabled, -38);
    stretchEnabledProp->setDisplayLabel(QStringLiteral("Velocity Stretch"));
    particleGroup.addProperty(stretchEnabledProp);

    auto stretchFactorProp = makeProp(QStringLiteral("particle.render.stretchFactor"), ArtifactCore::PropertyType::Float, rs.stretchFactor, -37);
    stretchFactorProp->setDisplayLabel(QStringLiteral("Stretch Factor"));
    stretchFactorProp->setSoftRange(0.0, 20.0);
    stretchFactorProp->setStep(0.1);
    particleGroup.addProperty(stretchFactorProp);

    groups.push_back(particleGroup);

    ArtifactCore::PropertyGroup emitterGroup(QStringLiteral("Emitter"));
    const EmitterParams emitter = impl_->primaryEmitterParams().value_or(EmitterParams{});

    auto emitterShapeProp = makeProp(QStringLiteral("particle.emitter.shape"), ArtifactCore::PropertyType::Integer, static_cast<int>(emitter.shape), -240);
    emitterShapeProp->setDisplayLabel(QStringLiteral("Shape"));
    emitterShapeProp->setTooltip(QStringLiteral("0=Point, 1=Sphere, 2=Box, 3=Circle, 4=Rectangle, 5=Line, 6=Mesh, 7=Surface"));
    emitterGroup.addProperty(emitterShapeProp);

    auto emitterModeProp = makeProp(QStringLiteral("particle.emitter.mode"), ArtifactCore::PropertyType::Integer, static_cast<int>(emitter.mode), -239);
    emitterModeProp->setDisplayLabel(QStringLiteral("Emission Mode"));
    emitterModeProp->setTooltip(QStringLiteral("0=Continuous, 1=Burst, 2=Triggered"));
    emitterGroup.addProperty(emitterModeProp);

    auto positionXProp = makeProp(QStringLiteral("particle.emitter.positionX"), ArtifactCore::PropertyType::Float, emitter.position.x(), -238);
    positionXProp->setDisplayLabel(QStringLiteral("Position X"));
    positionXProp->setUnit(QStringLiteral("px"));
    positionXProp->setSoftRange(-20000.0, 20000.0);
    emitterGroup.addProperty(positionXProp);

    auto positionYProp = makeProp(QStringLiteral("particle.emitter.positionY"), ArtifactCore::PropertyType::Float, emitter.position.y(), -237);
    positionYProp->setDisplayLabel(QStringLiteral("Position Y"));
    positionYProp->setUnit(QStringLiteral("px"));
    positionYProp->setSoftRange(-20000.0, 20000.0);
    emitterGroup.addProperty(positionYProp);

    auto rotationXProp = makeProp(QStringLiteral("particle.emitter.rotationX"), ArtifactCore::PropertyType::Float, emitter.rotation.x(), -236);
    rotationXProp->setDisplayLabel(QStringLiteral("Rotation X"));
    rotationXProp->setUnit(QStringLiteral("deg"));
    rotationXProp->setSoftRange(-360.0, 360.0);
    emitterGroup.addProperty(rotationXProp);

    auto rotationYProp = makeProp(QStringLiteral("particle.emitter.rotationY"), ArtifactCore::PropertyType::Float, emitter.rotation.y(), -235);
    rotationYProp->setDisplayLabel(QStringLiteral("Rotation Y"));
    rotationYProp->setUnit(QStringLiteral("deg"));
    rotationYProp->setSoftRange(-360.0, 360.0);
    emitterGroup.addProperty(rotationYProp);

    auto rotationZProp = makeProp(QStringLiteral("particle.emitter.rotationZ"), ArtifactCore::PropertyType::Float, emitter.rotation.z(), -234);
    rotationZProp->setDisplayLabel(QStringLiteral("Rotation Z"));
    rotationZProp->setUnit(QStringLiteral("deg"));
    rotationZProp->setSoftRange(-360.0, 360.0);
    emitterGroup.addProperty(rotationZProp);

    auto rotationSpeedMinProp = makeProp(QStringLiteral("particle.emitter.rotationSpeedMin"), ArtifactCore::PropertyType::Float, emitter.rotationSpeedMin, -233);
    rotationSpeedMinProp->setDisplayLabel(QStringLiteral("Spin Min"));
    rotationSpeedMinProp->setUnit(QStringLiteral("deg/s"));
    rotationSpeedMinProp->setSoftRange(-720.0, 720.0);
    rotationSpeedMinProp->setStep(1.0);
    emitterGroup.addProperty(rotationSpeedMinProp);

    auto rotationSpeedMaxProp = makeProp(QStringLiteral("particle.emitter.rotationSpeedMax"), ArtifactCore::PropertyType::Float, emitter.rotationSpeedMax, -232);
    rotationSpeedMaxProp->setDisplayLabel(QStringLiteral("Spin Max"));
    rotationSpeedMaxProp->setUnit(QStringLiteral("deg/s"));
    rotationSpeedMaxProp->setSoftRange(-720.0, 720.0);
    rotationSpeedMaxProp->setStep(1.0);
    emitterGroup.addProperty(rotationSpeedMaxProp);

    auto directionXProp = makeProp(QStringLiteral("particle.emitter.directionX"), ArtifactCore::PropertyType::Float, emitter.direction.x(), -233);
    directionXProp->setDisplayLabel(QStringLiteral("Direction X"));
    directionXProp->setSoftRange(-1.0, 1.0);
    directionXProp->setStep(0.01);
    emitterGroup.addProperty(directionXProp);

    auto directionYProp = makeProp(QStringLiteral("particle.emitter.directionY"), ArtifactCore::PropertyType::Float, emitter.direction.y(), -232);
    directionYProp->setDisplayLabel(QStringLiteral("Direction Y"));
    directionYProp->setSoftRange(-1.0, 1.0);
    directionYProp->setStep(0.01);
    emitterGroup.addProperty(directionYProp);

    auto directionZProp = makeProp(QStringLiteral("particle.emitter.directionZ"), ArtifactCore::PropertyType::Float, emitter.direction.z(), -231);
    directionZProp->setDisplayLabel(QStringLiteral("Direction Z"));
    directionZProp->setSoftRange(-1.0, 1.0);
    directionZProp->setStep(0.01);
    emitterGroup.addProperty(directionZProp);

    auto radiusProp = makeProp(QStringLiteral("particle.emitter.radius"), ArtifactCore::PropertyType::Float, emitter.radius, -230);
    radiusProp->setDisplayLabel(QStringLiteral("Radius"));
    radiusProp->setUnit(QStringLiteral("px"));
    radiusProp->setSoftRange(0.0, 5000.0);
    emitterGroup.addProperty(radiusProp);

    auto widthProp = makeProp(QStringLiteral("particle.emitter.width"), ArtifactCore::PropertyType::Float, emitter.width, -229);
    widthProp->setDisplayLabel(QStringLiteral("Width"));
    widthProp->setUnit(QStringLiteral("px"));
    widthProp->setSoftRange(0.0, 10000.0);
    emitterGroup.addProperty(widthProp);

    auto heightProp = makeProp(QStringLiteral("particle.emitter.height"), ArtifactCore::PropertyType::Float, emitter.height, -228);
    heightProp->setDisplayLabel(QStringLiteral("Height"));
    heightProp->setUnit(QStringLiteral("px"));
    heightProp->setSoftRange(0.0, 10000.0);
    emitterGroup.addProperty(heightProp);

    auto depthProp = makeProp(QStringLiteral("particle.emitter.depth"), ArtifactCore::PropertyType::Float, emitter.depth, -227);
    depthProp->setDisplayLabel(QStringLiteral("Depth"));
    depthProp->setUnit(QStringLiteral("px"));
    depthProp->setSoftRange(0.0, 10000.0);
    emitterGroup.addProperty(depthProp);

    auto lineLengthProp = makeProp(QStringLiteral("particle.emitter.lineLength"), ArtifactCore::PropertyType::Float, emitter.lineLength, -226);
    lineLengthProp->setDisplayLabel(QStringLiteral("Line Length"));
    lineLengthProp->setUnit(QStringLiteral("px"));
    lineLengthProp->setSoftRange(0.0, 10000.0);
    emitterGroup.addProperty(lineLengthProp);

    auto directionSpreadProp = makeProp(QStringLiteral("particle.emitter.directionSpread"), ArtifactCore::PropertyType::Float, emitter.directionSpread, -225);
    directionSpreadProp->setDisplayLabel(QStringLiteral("Direction Spread"));
    directionSpreadProp->setUnit(QStringLiteral("deg"));
    directionSpreadProp->setSoftRange(0.0, 360.0);
    emitterGroup.addProperty(directionSpreadProp);

    auto rateProp = makeProp(QStringLiteral("particle.emitter.rate"), ArtifactCore::PropertyType::Float, emitter.rate, -224);
    rateProp->setDisplayLabel(QStringLiteral("Rate"));
    rateProp->setUnit(QStringLiteral("/s"));
    rateProp->setSoftRange(0.0, 2000.0);
    emitterGroup.addProperty(rateProp);

    auto burstCountProp = makeProp(QStringLiteral("particle.emitter.burstCount"), ArtifactCore::PropertyType::Integer, emitter.burstCount, -223);
    burstCountProp->setDisplayLabel(QStringLiteral("Burst Count"));
    burstCountProp->setHardRange(1, 100000);
    burstCountProp->setSoftRange(1, 5000);
    emitterGroup.addProperty(burstCountProp);

    auto burstIntervalProp = makeProp(QStringLiteral("particle.emitter.burstInterval"), ArtifactCore::PropertyType::Float, emitter.burstInterval, -222);
    burstIntervalProp->setDisplayLabel(QStringLiteral("Burst Interval"));
    burstIntervalProp->setUnit(QStringLiteral("s"));
    burstIntervalProp->setSoftRange(0.0, 10.0);
    burstIntervalProp->setStep(0.01);
    emitterGroup.addProperty(burstIntervalProp);

    auto maxParticlesProp = makeProp(QStringLiteral("particle.emitter.maxParticles"), ArtifactCore::PropertyType::Integer, emitter.maxParticles, -221);
    maxParticlesProp->setDisplayLabel(QStringLiteral("Max Particles"));
    maxParticlesProp->setHardRange(1, 100000);
    maxParticlesProp->setSoftRange(1, 20000);
    emitterGroup.addProperty(maxParticlesProp);

    auto texturePathProp = makeProp(QStringLiteral("particle.emitter.texturePath"), ArtifactCore::PropertyType::String, emitter.texturePath, -220);
    texturePathProp->setDisplayLabel(QStringLiteral("Texture Path"));
    emitterGroup.addProperty(texturePathProp);

    auto textureRowsProp = makeProp(QStringLiteral("particle.emitter.textureRows"), ArtifactCore::PropertyType::Integer, emitter.textureRows, -219);
    textureRowsProp->setDisplayLabel(QStringLiteral("Texture Rows"));
    textureRowsProp->setHardRange(1, 128);
    textureRowsProp->setSoftRange(1, 16);
    emitterGroup.addProperty(textureRowsProp);

    auto textureColsProp = makeProp(QStringLiteral("particle.emitter.textureCols"), ArtifactCore::PropertyType::Integer, emitter.textureCols, -218);
    textureColsProp->setDisplayLabel(QStringLiteral("Texture Cols"));
    textureColsProp->setHardRange(1, 128);
    textureColsProp->setSoftRange(1, 16);
    emitterGroup.addProperty(textureColsProp);

    auto randomFrameProp = makeProp(QStringLiteral("particle.emitter.randomFrame"), ArtifactCore::PropertyType::Boolean, emitter.randomFrame, -217);
    randomFrameProp->setDisplayLabel(QStringLiteral("Random Frame"));
    emitterGroup.addProperty(randomFrameProp);

    auto startFrameProp = makeProp(QStringLiteral("particle.emitter.startFrame"), ArtifactCore::PropertyType::Integer, emitter.startFrame, -216);
    startFrameProp->setDisplayLabel(QStringLiteral("Start Frame"));
    startFrameProp->setHardRange(0, 4096);
    startFrameProp->setSoftRange(0, 256);
    emitterGroup.addProperty(startFrameProp);

    auto frameCountProp = makeProp(QStringLiteral("particle.emitter.frameCount"), ArtifactCore::PropertyType::Integer, emitter.frameCount, -215);
    frameCountProp->setDisplayLabel(QStringLiteral("Frame Count"));
    frameCountProp->setHardRange(1, 4096);
    frameCountProp->setSoftRange(1, 256);
    emitterGroup.addProperty(frameCountProp);

    auto frameRateProp = makeProp(QStringLiteral("particle.emitter.frameRate"), ArtifactCore::PropertyType::Float, emitter.frameRate, -214);
    frameRateProp->setDisplayLabel(QStringLiteral("Frame Rate"));
    frameRateProp->setUnit(QStringLiteral("fps"));
    frameRateProp->setSoftRange(0.0, 240.0);
    frameRateProp->setStep(0.1);
    emitterGroup.addProperty(frameRateProp);

    auto massProp = makeProp(QStringLiteral("particle.emitter.mass"), ArtifactCore::PropertyType::Float, emitter.mass, -213);
    massProp->setDisplayLabel(QStringLiteral("Mass"));
    massProp->setSoftRange(0.01, 100.0);
    massProp->setStep(0.01);
    emitterGroup.addProperty(massProp);

    auto inheritVelocityProp = makeProp(QStringLiteral("particle.emitter.inheritVelocity"), ArtifactCore::PropertyType::Boolean, emitter.inheritVelocity, -212);
    inheritVelocityProp->setDisplayLabel(QStringLiteral("Inherit Velocity"));
    emitterGroup.addProperty(inheritVelocityProp);

    auto worldSpaceProp = makeProp(QStringLiteral("particle.emitter.worldSpace"), ArtifactCore::PropertyType::Boolean, emitter.worldSpace, -211);
    worldSpaceProp->setDisplayLabel(QStringLiteral("World Space"));
    emitterGroup.addProperty(worldSpaceProp);

    auto preWarmProp = makeProp(QStringLiteral("particle.emitter.preWarm"), ArtifactCore::PropertyType::Boolean, emitter.preWarm, -210);
    preWarmProp->setDisplayLabel(QStringLiteral("Pre Warm"));
    emitterGroup.addProperty(preWarmProp);

    groups.push_back(emitterGroup);

    ArtifactCore::PropertyGroup particleLookGroup(QStringLiteral("Particle"));

    auto lifeMinProp = makeProp(QStringLiteral("particle.particle.lifeMin"), ArtifactCore::PropertyType::Float, emitter.lifeMin, -232);
    lifeMinProp->setDisplayLabel(QStringLiteral("Life Min"));
    lifeMinProp->setUnit(QStringLiteral("s"));
    lifeMinProp->setSoftRange(0.01, 60.0);
    particleLookGroup.addProperty(lifeMinProp);

    auto lifeMaxProp = makeProp(QStringLiteral("particle.particle.lifeMax"), ArtifactCore::PropertyType::Float, emitter.lifeMax, -231);
    lifeMaxProp->setDisplayLabel(QStringLiteral("Life Max"));
    lifeMaxProp->setUnit(QStringLiteral("s"));
    lifeMaxProp->setSoftRange(0.01, 60.0);
    particleLookGroup.addProperty(lifeMaxProp);

    auto speedMinProp = makeProp(QStringLiteral("particle.particle.speedMin"), ArtifactCore::PropertyType::Float, emitter.speedMin, -230);
    speedMinProp->setDisplayLabel(QStringLiteral("Speed Min"));
    speedMinProp->setUnit(QStringLiteral("px/s"));
    speedMinProp->setSoftRange(0.0, 5000.0);
    particleLookGroup.addProperty(speedMinProp);

    auto speedMaxProp = makeProp(QStringLiteral("particle.particle.speedMax"), ArtifactCore::PropertyType::Float, emitter.speedMax, -229);
    speedMaxProp->setDisplayLabel(QStringLiteral("Speed Max"));
    speedMaxProp->setUnit(QStringLiteral("px/s"));
    speedMaxProp->setSoftRange(0.0, 5000.0);
    particleLookGroup.addProperty(speedMaxProp);

    auto velocityRandomXProp = makeProp(QStringLiteral("particle.particle.velocityRandomX"), ArtifactCore::PropertyType::Float, emitter.velocityRandom.x(), -228);
    velocityRandomXProp->setDisplayLabel(QStringLiteral("Velocity Random X"));
    velocityRandomXProp->setUnit(QStringLiteral("px/s"));
    velocityRandomXProp->setSoftRange(0.0, 5000.0);
    particleLookGroup.addProperty(velocityRandomXProp);

    auto velocityRandomYProp = makeProp(QStringLiteral("particle.particle.velocityRandomY"), ArtifactCore::PropertyType::Float, emitter.velocityRandom.y(), -227);
    velocityRandomYProp->setDisplayLabel(QStringLiteral("Velocity Random Y"));
    velocityRandomYProp->setUnit(QStringLiteral("px/s"));
    velocityRandomYProp->setSoftRange(0.0, 5000.0);
    particleLookGroup.addProperty(velocityRandomYProp);

    auto velocityRandomZProp = makeProp(QStringLiteral("particle.particle.velocityRandomZ"), ArtifactCore::PropertyType::Float, emitter.velocityRandom.z(), -226);
    velocityRandomZProp->setDisplayLabel(QStringLiteral("Velocity Random Z"));
    velocityRandomZProp->setUnit(QStringLiteral("px/s"));
    velocityRandomZProp->setSoftRange(0.0, 5000.0);
    particleLookGroup.addProperty(velocityRandomZProp);

    auto scaleMinProp = makeProp(QStringLiteral("particle.particle.scaleMin"), ArtifactCore::PropertyType::Float, emitter.scaleMin, -225);
    scaleMinProp->setDisplayLabel(QStringLiteral("Size Min"));
    scaleMinProp->setUnit(QStringLiteral("px"));
    scaleMinProp->setSoftRange(0.1, 512.0);
    particleLookGroup.addProperty(scaleMinProp);

    auto scaleMaxProp = makeProp(QStringLiteral("particle.particle.scaleMax"), ArtifactCore::PropertyType::Float, emitter.scaleMax, -227);
    scaleMaxProp->setDisplayLabel(QStringLiteral("Size Max"));
    scaleMaxProp->setUnit(QStringLiteral("px"));
    scaleMaxProp->setSoftRange(0.1, 512.0);
    particleLookGroup.addProperty(scaleMaxProp);

    auto scaleMidMinProp = makeProp(QStringLiteral("particle.particle.scaleMidMin"), ArtifactCore::PropertyType::Float, emitter.scaleMidMin, -226);
    scaleMidMinProp->setDisplayLabel(QStringLiteral("Size Mid Min"));
    scaleMidMinProp->setUnit(QStringLiteral("px"));
    scaleMidMinProp->setSoftRange(0.0, 512.0);
    particleLookGroup.addProperty(scaleMidMinProp);

    auto scaleMidMaxProp = makeProp(QStringLiteral("particle.particle.scaleMidMax"), ArtifactCore::PropertyType::Float, emitter.scaleMidMax, -225);
    scaleMidMaxProp->setDisplayLabel(QStringLiteral("Size Mid Max"));
    scaleMidMaxProp->setUnit(QStringLiteral("px"));
    scaleMidMaxProp->setSoftRange(0.0, 512.0);
    particleLookGroup.addProperty(scaleMidMaxProp);

    auto scaleMidPosProp = makeProp(QStringLiteral("particle.particle.scaleMidPosition"), ArtifactCore::PropertyType::Float, emitter.scaleMidPosition, -224);
    scaleMidPosProp->setDisplayLabel(QStringLiteral("Size Mid Pos"));
    scaleMidPosProp->setSoftRange(0.0, 1.0);
    scaleMidPosProp->setStep(0.01);
    particleLookGroup.addProperty(scaleMidPosProp);

    auto scaleEndMinProp = makeProp(QStringLiteral("particle.particle.scaleEndMin"), ArtifactCore::PropertyType::Float, emitter.scaleEndMin, -223);
    scaleEndMinProp->setDisplayLabel(QStringLiteral("End Size Min"));
    scaleEndMinProp->setUnit(QStringLiteral("px"));
    scaleEndMinProp->setSoftRange(0.0, 512.0);
    particleLookGroup.addProperty(scaleEndMinProp);

    auto scaleEndMaxProp = makeProp(QStringLiteral("particle.particle.scaleEndMax"), ArtifactCore::PropertyType::Float, emitter.scaleEndMax, -222);
    scaleEndMaxProp->setDisplayLabel(QStringLiteral("End Size Max"));
    scaleEndMaxProp->setUnit(QStringLiteral("px"));
    scaleEndMaxProp->setSoftRange(0.0, 512.0);
    particleLookGroup.addProperty(scaleEndMaxProp);

    auto opacityMinProp = makeProp(QStringLiteral("particle.particle.opacityMin"), ArtifactCore::PropertyType::Float, emitter.opacityMin, -221);
    opacityMinProp->setDisplayLabel(QStringLiteral("Opacity Min"));
    opacityMinProp->setSoftRange(0.0, 1.0);
    opacityMinProp->setStep(0.01);
    particleLookGroup.addProperty(opacityMinProp);

    auto opacityMaxProp = makeProp(QStringLiteral("particle.particle.opacityMax"), ArtifactCore::PropertyType::Float, emitter.opacityMax, -220);
    opacityMaxProp->setDisplayLabel(QStringLiteral("Opacity Max"));
    opacityMaxProp->setSoftRange(0.0, 1.0);
    opacityMaxProp->setStep(0.01);
    particleLookGroup.addProperty(opacityMaxProp);

    auto opacityMidMinProp = makeProp(QStringLiteral("particle.particle.opacityMidMin"), ArtifactCore::PropertyType::Float, emitter.opacityMidMin, -219);
    opacityMidMinProp->setDisplayLabel(QStringLiteral("Opacity Mid Min"));
    opacityMidMinProp->setSoftRange(0.0, 1.0);
    opacityMidMinProp->setStep(0.01);
    particleLookGroup.addProperty(opacityMidMinProp);

    auto opacityMidMaxProp = makeProp(QStringLiteral("particle.particle.opacityMidMax"), ArtifactCore::PropertyType::Float, emitter.opacityMidMax, -218);
    opacityMidMaxProp->setDisplayLabel(QStringLiteral("Opacity Mid Max"));
    opacityMidMaxProp->setSoftRange(0.0, 1.0);
    opacityMidMaxProp->setStep(0.01);
    particleLookGroup.addProperty(opacityMidMaxProp);

    auto opacityMidPosProp = makeProp(QStringLiteral("particle.particle.opacityMidPosition"), ArtifactCore::PropertyType::Float, emitter.opacityMidPosition, -217);
    opacityMidPosProp->setDisplayLabel(QStringLiteral("Opacity Mid Pos"));
    opacityMidPosProp->setSoftRange(0.0, 1.0);
    opacityMidPosProp->setStep(0.01);
    particleLookGroup.addProperty(opacityMidPosProp);

    auto opacityEndMinProp = makeProp(QStringLiteral("particle.particle.opacityEndMin"), ArtifactCore::PropertyType::Float, emitter.opacityEndMin, -216);
    opacityEndMinProp->setDisplayLabel(QStringLiteral("Opacity End Min"));
    opacityEndMinProp->setSoftRange(0.0, 1.0);
    opacityEndMinProp->setStep(0.01);
    particleLookGroup.addProperty(opacityEndMinProp);

    auto opacityEndMaxProp = makeProp(QStringLiteral("particle.particle.opacityEndMax"), ArtifactCore::PropertyType::Float, emitter.opacityEndMax, -215);
    opacityEndMaxProp->setDisplayLabel(QStringLiteral("Opacity End Max"));
    opacityEndMaxProp->setSoftRange(0.0, 1.0);
    opacityEndMaxProp->setStep(0.01);
    particleLookGroup.addProperty(opacityEndMaxProp);

    auto colorStartProp = makeProp(QStringLiteral("particle.particle.colorStart"), ArtifactCore::PropertyType::Color, emitter.colorStart, -214);
    colorStartProp->setDisplayLabel(QStringLiteral("Color Start"));
    particleLookGroup.addProperty(colorStartProp);

    auto colorMidProp = makeProp(QStringLiteral("particle.particle.colorMid"), ArtifactCore::PropertyType::Color, emitter.colorMid, -213);
    colorMidProp->setDisplayLabel(QStringLiteral("Color Mid"));
    particleLookGroup.addProperty(colorMidProp);

    auto colorMidPosProp = makeProp(QStringLiteral("particle.particle.colorMidPosition"), ArtifactCore::PropertyType::Float, emitter.colorMidPosition, -212);
    colorMidPosProp->setDisplayLabel(QStringLiteral("Color Mid Pos"));
    colorMidPosProp->setSoftRange(0.0, 1.0);
    colorMidPosProp->setStep(0.01);
    particleLookGroup.addProperty(colorMidPosProp);

    auto colorEndProp = makeProp(QStringLiteral("particle.particle.colorEnd"), ArtifactCore::PropertyType::Color, emitter.colorEnd, -211);
    colorEndProp->setDisplayLabel(QStringLiteral("Color End"));
    particleLookGroup.addProperty(colorEndProp);

    groups.push_back(particleLookGroup);

    ArtifactCore::PropertyGroup physicsGroup(QStringLiteral("Physics"));
    auto dragProp = makeProp(QStringLiteral("particle.physics.drag"), ArtifactCore::PropertyType::Float, emitter.drag, -212);
    dragProp->setDisplayLabel(QStringLiteral("Air"));
    dragProp->setSoftRange(0.0, 10.0);
    dragProp->setStep(0.01);
    physicsGroup.addProperty(dragProp);

    auto gravityXProp = makeProp(QStringLiteral("particle.physics.gravityX"), ArtifactCore::PropertyType::Float, emitter.gravity.x(), -211);
    gravityXProp->setDisplayLabel(QStringLiteral("Gravity X"));
    gravityXProp->setUnit(QStringLiteral("px/s2"));
    gravityXProp->setSoftRange(-5000.0, 5000.0);
    physicsGroup.addProperty(gravityXProp);

    auto gravityYProp = makeProp(QStringLiteral("particle.physics.gravityY"), ArtifactCore::PropertyType::Float, emitter.gravity.y(), -210);
    gravityYProp->setDisplayLabel(QStringLiteral("Gravity Y"));
    gravityYProp->setUnit(QStringLiteral("px/s2"));
    gravityYProp->setSoftRange(-5000.0, 5000.0);
    physicsGroup.addProperty(gravityYProp);

    auto gravityZProp = makeProp(QStringLiteral("particle.physics.gravityZ"), ArtifactCore::PropertyType::Float, emitter.gravity.z(), -209);
    gravityZProp->setDisplayLabel(QStringLiteral("Gravity Z"));
    gravityZProp->setUnit(QStringLiteral("px/s2"));
    gravityZProp->setSoftRange(-5000.0, 5000.0);
    physicsGroup.addProperty(gravityZProp);

    auto windDirectionXProp = makeProp(QStringLiteral("particle.physics.windDirectionX"), ArtifactCore::PropertyType::Float, emitter.windDirection.x(), -208);
    windDirectionXProp->setDisplayLabel(QStringLiteral("Wind Dir X"));
    windDirectionXProp->setSoftRange(-1.0, 1.0);
    windDirectionXProp->setStep(0.01);
    physicsGroup.addProperty(windDirectionXProp);

    auto windDirectionYProp = makeProp(QStringLiteral("particle.physics.windDirectionY"), ArtifactCore::PropertyType::Float, emitter.windDirection.y(), -207);
    windDirectionYProp->setDisplayLabel(QStringLiteral("Wind Dir Y"));
    windDirectionYProp->setSoftRange(-1.0, 1.0);
    windDirectionYProp->setStep(0.01);
    physicsGroup.addProperty(windDirectionYProp);

    auto windDirectionZProp = makeProp(QStringLiteral("particle.physics.windDirectionZ"), ArtifactCore::PropertyType::Float, emitter.windDirection.z(), -206);
    windDirectionZProp->setDisplayLabel(QStringLiteral("Wind Dir Z"));
    windDirectionZProp->setSoftRange(-1.0, 1.0);
    windDirectionZProp->setStep(0.01);
    physicsGroup.addProperty(windDirectionZProp);

    auto windStrengthProp = makeProp(QStringLiteral("particle.physics.windStrength"), ArtifactCore::PropertyType::Float, emitter.windStrength, -205);
    windStrengthProp->setDisplayLabel(QStringLiteral("Wind Strength"));
    windStrengthProp->setUnit(QStringLiteral("px/s2"));
    windStrengthProp->setSoftRange(0.0, 5000.0);
    physicsGroup.addProperty(windStrengthProp);

    auto turbulenceFrequencyProp = makeProp(QStringLiteral("particle.physics.turbulenceFrequency"), ArtifactCore::PropertyType::Float, emitter.turbulenceFrequency, -204);
    turbulenceFrequencyProp->setDisplayLabel(QStringLiteral("Turbulence Freq"));
    turbulenceFrequencyProp->setSoftRange(0.0, 1.0);
    turbulenceFrequencyProp->setStep(0.001);
    physicsGroup.addProperty(turbulenceFrequencyProp);

    auto turbulenceAmplitudeProp = makeProp(QStringLiteral("particle.physics.turbulenceAmplitude"), ArtifactCore::PropertyType::Float, emitter.turbulenceAmplitude, -203);
    turbulenceAmplitudeProp->setDisplayLabel(QStringLiteral("Turbulence Amp"));
    turbulenceAmplitudeProp->setUnit(QStringLiteral("px/s2"));
    turbulenceAmplitudeProp->setSoftRange(0.0, 5000.0);
    turbulenceAmplitudeProp->setStep(0.1);
    physicsGroup.addProperty(turbulenceAmplitudeProp);

    auto turbulenceEvolutionProp = makeProp(QStringLiteral("particle.physics.turbulenceEvolution"), ArtifactCore::PropertyType::Float, emitter.turbulenceEvolution, -202);
    turbulenceEvolutionProp->setDisplayLabel(QStringLiteral("Turbulence Evol"));
    turbulenceEvolutionProp->setSoftRange(-1000.0, 1000.0);
    turbulenceEvolutionProp->setStep(0.1);
    physicsGroup.addProperty(turbulenceEvolutionProp);
    groups.push_back(physicsGroup);

    ArtifactCore::PropertyGroup auxGroup(QStringLiteral("Aux"));
    auto auxEnabledProp = makeProp(QStringLiteral("particle.aux.enabled"), ArtifactCore::PropertyType::Boolean, emitter.auxEnabled, -211);
    auxEnabledProp->setDisplayLabel(QStringLiteral("Enable Aux"));
    auxGroup.addProperty(auxEnabledProp);

    auto auxTriggerProp = makeProp(QStringLiteral("particle.aux.trigger"), ArtifactCore::PropertyType::Integer, static_cast<int>(emitter.auxTrigger), -210);
    auxTriggerProp->setDisplayLabel(QStringLiteral("Trigger"));
    auxTriggerProp->setTooltip(QStringLiteral("0=Trails, 1=Birth, 2=Death"));
    auxGroup.addProperty(auxTriggerProp);

    auto auxCountProp = makeProp(QStringLiteral("particle.aux.count"), ArtifactCore::PropertyType::Integer, emitter.auxCount, -209);
    auxCountProp->setDisplayLabel(QStringLiteral("Count"));
    auxCountProp->setHardRange(0, 256);
    auxCountProp->setSoftRange(0, 32);
    auxGroup.addProperty(auxCountProp);

    auto auxIntervalProp = makeProp(QStringLiteral("particle.aux.interval"), ArtifactCore::PropertyType::Float, emitter.auxInterval, -208);
    auxIntervalProp->setDisplayLabel(QStringLiteral("Interval"));
    auxIntervalProp->setUnit(QStringLiteral("s"));
    auxIntervalProp->setSoftRange(0.01, 2.0);
    auxIntervalProp->setStep(0.01);
    auxGroup.addProperty(auxIntervalProp);

    auto auxLifeScaleProp = makeProp(QStringLiteral("particle.aux.lifeScale"), ArtifactCore::PropertyType::Float, emitter.auxLifeScale, -207);
    auxLifeScaleProp->setDisplayLabel(QStringLiteral("Life Scale"));
    auxLifeScaleProp->setSoftRange(0.05, 4.0);
    auxLifeScaleProp->setStep(0.01);
    auxGroup.addProperty(auxLifeScaleProp);

    auto auxSizeScaleProp = makeProp(QStringLiteral("particle.aux.sizeScale"), ArtifactCore::PropertyType::Float, emitter.auxSizeScale, -206);
    auxSizeScaleProp->setDisplayLabel(QStringLiteral("Size Scale"));
    auxSizeScaleProp->setSoftRange(0.05, 4.0);
    auxSizeScaleProp->setStep(0.01);
    auxGroup.addProperty(auxSizeScaleProp);

    auto auxOpacityScaleProp = makeProp(QStringLiteral("particle.aux.opacityScale"), ArtifactCore::PropertyType::Float, emitter.auxOpacityScale, -205);
    auxOpacityScaleProp->setDisplayLabel(QStringLiteral("Opacity Scale"));
    auxOpacityScaleProp->setSoftRange(0.0, 2.0);
    auxOpacityScaleProp->setStep(0.01);
    auxGroup.addProperty(auxOpacityScaleProp);

    auto auxVelocityScaleProp = makeProp(QStringLiteral("particle.aux.velocityScale"), ArtifactCore::PropertyType::Float, emitter.auxVelocityScale, -204);
    auxVelocityScaleProp->setDisplayLabel(QStringLiteral("Velocity Scale"));
    auxVelocityScaleProp->setSoftRange(0.0, 4.0);
    auxVelocityScaleProp->setStep(0.01);
    auxGroup.addProperty(auxVelocityScaleProp);
    groups.push_back(auxGroup);
    return groups;
}

bool ArtifactParticleLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    auto applyPrimaryEmitterValue = [this](const std::function<void(EmitterParams&)>& mutator) {
        if (!impl_->applyPrimaryEmitterParams(mutator)) {
            return false;
        }
        clearFrameCache();
        Q_EMIT particleSystemChanged();
        Q_EMIT changed();
        return true;
    };

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
    if (propertyPath == QStringLiteral("particle.emitterCount")) {
        const int targetCount = std::max(0, value.toInt());
        const int currentCount = emitterCount();
        if (targetCount == currentCount) {
            return true;
        }

        while (emitterCount() < targetCount) {
            EmitterParams params;
            if (!impl_->savedEmitterParams.empty()) {
                params = impl_->savedEmitterParams.back();
            } else if (auto* firstEmitter = firstEmitterOrCreate(impl_->particleSystem.get())) {
                params = firstEmitter->params();
            }
            addEmitter(params);
        }

        while (emitterCount() > targetCount) {
            removeEmitter(emitterCount() - 1);
        }

        impl_->rebuildSavedEmitterParamsFromSystem();
        clearFrameCache();
        Q_EMIT particleSystemChanged();
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.previewWidth")) {
        const int oldWidth = std::max(1, impl_->width);
        impl_->width = std::max(1, value.toInt());
        impl_->scaleEmitterPositions(
            static_cast<float>(impl_->width) / static_cast<float>(oldWidth), 1.0f);
        clearFrameCache();
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.previewHeight")) {
        const int oldHeight = std::max(1, impl_->height);
        impl_->height = std::max(1, value.toInt());
        impl_->scaleEmitterPositions(
            1.0f, static_cast<float>(impl_->height) / static_cast<float>(oldHeight));
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
    if (propertyPath == QStringLiteral("particle.render.softParticles")) {
        auto rs = renderSettings();
        rs.softParticles = value.toBool();
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.render.softParticleDistance")) {
        auto rs = renderSettings();
        rs.softParticleDistance = std::max(0.0f, static_cast<float>(value.toDouble()));
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.render.stretchEnabled")) {
        auto rs = renderSettings();
        rs.stretchEnabled = value.toBool();
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.render.stretchFactor")) {
        auto rs = renderSettings();
        rs.stretchFactor = std::max(0.0f, static_cast<float>(value.toDouble()));
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.emitter.shape")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.shape = static_cast<EmitterShape>(value.toInt());
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.mode")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.mode = static_cast<EmissionMode>(value.toInt());
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.positionX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.position.setX(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.positionY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.position.setY(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rotationX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rotation.setX(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rotationY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rotation.setY(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rotationZ")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rotation.setZ(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rotationSpeedMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rotationSpeedMin = static_cast<float>(value.toDouble());
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rotationSpeedMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rotationSpeedMax = static_cast<float>(value.toDouble());
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.directionX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.direction.setX(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.directionY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.direction.setY(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.directionZ")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.direction.setZ(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.radius")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.radius = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.width")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.width = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.height")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.height = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.depth")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.depth = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.lineLength")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.lineLength = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rate")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rate = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.burstCount")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.burstCount = std::max(1, value.toInt());
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.burstInterval")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.burstInterval = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.texturePath")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.texturePath = value.toString();
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.textureRows")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.textureRows = std::max(1, value.toInt());
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.textureCols")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.textureCols = std::max(1, value.toInt());
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.randomFrame")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.randomFrame = value.toBool();
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.startFrame")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.startFrame = std::max(0, value.toInt());
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.frameCount")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.frameCount = std::max(1, value.toInt());
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.frameRate")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.frameRate = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.mass")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.mass = std::max(0.01f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.inheritVelocity")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.inheritVelocity = value.toBool();
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.worldSpace")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.worldSpace = value.toBool();
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.preWarm")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.preWarm = value.toBool();
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.lifeMin") ||
        propertyPath == QStringLiteral("particle.particle.lifeMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.lifeMin = std::max(0.01f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.lifeMax") ||
        propertyPath == QStringLiteral("particle.particle.lifeMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.lifeMax = std::max(0.01f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.speedMin") ||
        propertyPath == QStringLiteral("particle.particle.speedMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.speedMin = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.speedMax") ||
        propertyPath == QStringLiteral("particle.particle.speedMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.speedMax = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.velocityRandomX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.velocityRandom.setX(std::max(0.0f, static_cast<float>(value.toDouble())));
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.velocityRandomY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.velocityRandom.setY(std::max(0.0f, static_cast<float>(value.toDouble())));
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.velocityRandomZ")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.velocityRandom.setZ(std::max(0.0f, static_cast<float>(value.toDouble())));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.directionSpread")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.directionSpread = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 360.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.scaleMin") ||
        propertyPath == QStringLiteral("particle.particle.scaleMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleMin = std::max(0.01f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.scaleMax") ||
        propertyPath == QStringLiteral("particle.particle.scaleMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleMax = std::max(0.01f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.scaleMidMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleMidMin = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.scaleMidMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleMidMax = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.scaleMidPosition")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleMidPosition = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.scaleEndMin") ||
        propertyPath == QStringLiteral("particle.particle.scaleEndMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleEndMin = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.scaleEndMax") ||
        propertyPath == QStringLiteral("particle.particle.scaleEndMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleEndMax = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.opacityMin") ||
        propertyPath == QStringLiteral("particle.particle.opacityMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityMin = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.opacityMax") ||
        propertyPath == QStringLiteral("particle.particle.opacityMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityMax = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.opacityMidMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityMidMin = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.opacityMidMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityMidMax = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.opacityMidPosition")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityMidPosition = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.opacityEndMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityEndMin = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.opacityEndMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityEndMax = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.drag") ||
        propertyPath == QStringLiteral("particle.physics.drag")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.drag = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.gravityX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.gravity.setX(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.gravityY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.gravity.setY(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.gravityZ")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.gravity.setZ(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.windDirectionX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.windDirection.setX(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.windDirectionY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.windDirection.setY(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.windDirectionZ")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.windDirection.setZ(static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.windStrength")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.windStrength = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.turbulenceFrequency")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.turbulenceFrequency = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.turbulenceAmplitude")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.turbulenceAmplitude = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.turbulenceEvolution")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.turbulenceEvolution = static_cast<float>(value.toDouble());
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.maxParticles")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.maxParticles = std::max(1, value.toInt());
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.enabled")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxEnabled = value.toBool();
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.trigger")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxTrigger = static_cast<AuxTriggerMode>(value.toInt());
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.count")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxCount = std::max(0, value.toInt());
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.interval")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxInterval = std::max(0.001f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.lifeScale")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxLifeScale = std::max(0.01f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.sizeScale")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxSizeScale = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.opacityScale")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxOpacityScale = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.velocityScale")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxVelocityScale = std::max(0.0f, static_cast<float>(value.toDouble()));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.colorStart") ||
        propertyPath == QStringLiteral("particle.particle.colorStart")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.colorStart = value.value<QColor>();
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.colorMid")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.colorMid = value.value<QColor>();
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.colorMidPosition")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.colorMidPosition = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.colorEnd") ||
        propertyPath == QStringLiteral("particle.particle.colorEnd")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.colorEnd = value.value<QColor>();
        });
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

ArtifactParticleDebugLayer::ArtifactParticleDebugLayer() = default;
ArtifactParticleDebugLayer::~ArtifactParticleDebugLayer() = default;

void ArtifactParticleDebugLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer || !particleSystem()) {
        qWarning() << "[ParticleDebugLayer] draw() early exit: renderer="
                   << (renderer ? "ok" : "null")
                   << "particleSystem=" << (particleSystem() ? "ok" : "null");
        return;
    }

    const int64_t frameNumber = currentFrame();
    const bool rendererReady = renderer->isInitialized();
    const int emitterCount = this->emitterCount();

    qInfo() << "[ParticleDebugLayer] draw() frame=" << frameNumber
            << "rendererInitialized=" << rendererReady
            << "emitters=" << emitterCount;

    float fps = 30.0f;
    if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
        fps = comp->frameRate().framerate();
    }
    particleSystem()->goToFrame(std::max<int64_t>(1, frameNumber), fps);

    if (rendererReady) {
        const auto sourceData = particleSystem()->captureRenderData();
        qInfo() << "[ParticleDebugLayer] GPU path: particleCount=" << sourceData.particles.size();
        if (!sourceData.particles.empty()) {
            const QTransform globalTransform = getGlobalTransform();
            ArtifactCore::ParticleRenderData renderData =
                transformParticleRenderData(sourceData, globalTransform, opacity());
            boostDebugParticleRenderData(renderData);
            renderer->drawParticles(renderData);
        } else {
            qWarning() << "[ParticleDebugLayer] GPU path: NO PARTICLES - emitter may not generate";
        }
        return;
    }

    const QRectF bounds = localBounds();
    const int fallbackWidth = std::max(1, static_cast<int>(std::ceil(bounds.width())));
    const int fallbackHeight = std::max(1, static_cast<int>(std::ceil(bounds.height())));
    QImage fallbackFrame =
        renderFrame(fallbackWidth, fallbackHeight, static_cast<float>(frameNumber) / fps);
    if (fallbackFrame.isNull()) {
        qWarning() << "[ParticleDebugLayer] Fallback draw skipped: frame is null";
        return;
    }
    renderer->drawSprite(0.0f,
                         0.0f,
                         static_cast<float>(fallbackFrame.width()),
                         static_cast<float>(fallbackFrame.height()),
                         fallbackFrame,
                         opacity());
}

std::shared_ptr<ArtifactParticleDebugLayer> createParticleDebugLayer()
{
    return std::make_shared<ArtifactParticleDebugLayer>();
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactParticleLayer)
