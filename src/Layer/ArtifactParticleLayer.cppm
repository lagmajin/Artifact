module;
#include <QObject>
#include <QImage>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QTransform>
#include <QSizeF>
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
#include <cstdint>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Layer.Particle;
import Memory.SharedPtr;




import Artifact.Layer.Abstract;
import Memory.SharedPtr;
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
import Core.Parallel;

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

float safeParticleFps(float value)
{
    return std::isfinite(value)
        ? std::clamp(value, 0.001f, 1000.0f)
        : 30.0f;
}

float safeParticleFrameTime(const int64_t frame, float fps)
{
    const double rawTime = static_cast<double>(frame) /
                           static_cast<double>(safeParticleFps(fps));
    return std::isfinite(rawTime)
        ? std::clamp(static_cast<float>(rawTime), -1000000.0f, 1000000.0f)
        : 0.0f;
}

int safeParticleDimension(double value)
{
    return std::isfinite(value)
        ? static_cast<int>(std::clamp(value, 1.0, 16384.0))
        : 1;
}

float safeEffectorValue(float value, float fallback,
                        float minimum, float maximum)
{
    return std::isfinite(value)
        ? std::clamp(value, minimum, maximum)
        : fallback;
}

QVector3D safeEffectorVector(const QVector3D& value)
{
    return QVector3D(
        safeEffectorValue(value.x(), 0.0f, -1000000.0f, 1000000.0f),
        safeEffectorValue(value.y(), 0.0f, -1000000.0f, 1000000.0f),
        safeEffectorValue(value.z(), 0.0f, -1000000.0f, 1000000.0f));
}

ArtifactCore::ParticleRenderData transformParticleRenderData(
    const ArtifactCore::ParticleRenderData& source,
    const QTransform& transform,
    float opacity)
{
    ArtifactCore::ParticleRenderData transformed;
    transformed.frameNumber = source.frameNumber;
    transformed.options = source.options;
    transformed.particles.resize(source.particles.size());

    const auto finite = [](double value) { return std::isfinite(value); };
    const bool transformFinite =
        finite(transform.m11()) && finite(transform.m12()) &&
        finite(transform.m13()) && finite(transform.m21()) &&
        finite(transform.m22()) && finite(transform.m23()) &&
        finite(transform.m31()) && finite(transform.m32()) &&
        finite(transform.m33()) && finite(transform.dx()) && finite(transform.dy());
    const QTransform safeTransform = transformFinite ? transform : QTransform();
    const float scaleX = std::isfinite(std::hypot(safeTransform.m11(), safeTransform.m21()))
        ? static_cast<float>(std::hypot(safeTransform.m11(), safeTransform.m21()))
        : 1.0f;
    const float scaleY = std::isfinite(std::hypot(safeTransform.m12(), safeTransform.m22()))
        ? static_cast<float>(std::hypot(safeTransform.m12(), safeTransform.m22()))
        : 1.0f;
    const float scale = std::clamp(std::max(scaleX, scaleY), 0.001f, 1000000.0f);
    const QPointF mappedOrigin = safeTransform.map(QPointF(0.0, 0.0));
    const float rotationOffsetDegrees = static_cast<float>(
        std::atan2(safeTransform.m12(), safeTransform.m11()) *
        180.0 / 3.14159265358979323846);
    const float safeOpacity = std::isfinite(opacity)
        ? std::clamp(opacity, 0.0f, 1.0f)
        : 0.0f;
    const auto safeColor = [](float value) {
        return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
    };
    const auto safeCoordinate = [](double value, float fallback) {
        return std::isfinite(value)
            ? static_cast<float>(std::clamp(value, -10000000.0, 10000000.0))
            : fallback;
    };

    ArtifactCore::Parallel::For(0, static_cast<int>(source.particles.size()),
                                static_cast<int>(source.particles.size()),
                                [&](int index) {
        const auto& src = source.particles[static_cast<size_t>(index)];
        auto& v = transformed.particles[static_cast<size_t>(index)];
        v.px = src.px;
        v.py = src.py;
        v.pz = src.pz;
        v.vx = src.vx;
        v.vy = src.vy;
        v.vz = src.vz;
        v.r = safeColor(src.r);
        v.g = safeColor(src.g);
        v.b = safeColor(src.b);
        v.a = src.a;
        v.size = src.size;
        v.stretch = src.stretch;
        v.rotation = src.rotation;
        v.age = src.age;
        v.lifetime = src.lifetime;
        v.spriteFrame = src.spriteFrame;
        v.spriteRows = src.spriteRows;
        v.spriteCols = src.spriteCols;
        const QPointF mapped = safeTransform.map(QPointF(src.px, src.py));
        const QPointF mappedVelocityPoint =
            safeTransform.map(QPointF(src.vx, src.vy));
        const QPointF mappedVelocity = mappedVelocityPoint - mappedOrigin;
        const float safeSourceX = std::isfinite(src.px) ? src.px : 0.0f;
        const float safeSourceY = std::isfinite(src.py) ? src.py : 0.0f;
        v.px = safeCoordinate(mapped.x(), safeSourceX);
        v.py = safeCoordinate(mapped.y(), safeSourceY);
        v.vx = safeCoordinate(mappedVelocity.x(), 0.0f);
        v.vy = safeCoordinate(mappedVelocity.y(), 0.0f);
        v.rotation = std::isfinite(src.rotation)
            ? std::clamp(src.rotation + rotationOffsetDegrees,
                         -1000000.0f, 1000000.0f)
            : rotationOffsetDegrees;
        v.a = std::isfinite(v.a)
            ? std::clamp(v.a * safeOpacity, 0.0f, 1.0f)
            : 0.0f;
        const float sourceSize = std::isfinite(src.size)
            ? std::clamp(src.size, 0.0f, 1000000.0f)
            : 0.0f;
        // No minimum-size clamp: presets that shrink to zero (sparks, fire)
        // rely on size reaching 0 to make particles disappear at end of life.
        v.size = std::clamp(sourceSize * scale, 0.0f, 1000000.0f);
        if (!std::isfinite(v.stretch) || v.stretch <= 0.0f) {
            const float speed = std::isfinite(std::hypot(v.vx, v.vy))
                ? static_cast<float>(std::hypot(v.vx, v.vy))
                : 0.0f;
            v.stretch = std::clamp(1.0f + speed * 0.004f, 1.0f, 6.0f);
        } else {
            v.stretch = std::clamp(v.stretch, 1.0f, 1000000.0f);
        }
    });

    return transformed;
}

ArtifactCore::ParticleRenderData toCoreParticleRenderData(
    const ParticleRenderData& source)
{
    ArtifactCore::ParticleRenderData converted;
    converted.frameNumber = source.frameNumber;
    converted.particles.reserve(source.particles.size());
    const auto finiteClamped = [](float value, float fallback,
                                  float minimum, float maximum) {
        return std::isfinite(value)
            ? std::clamp(value, minimum, maximum)
            : fallback;
    };
    for (const auto& particle : source.particles) {
        ArtifactCore::ParticleVertex vertex;
        vertex.px = finiteClamped(particle.px, 0.0f, -10000000.0f, 10000000.0f);
        vertex.py = finiteClamped(particle.py, 0.0f, -10000000.0f, 10000000.0f);
        vertex.pz = finiteClamped(particle.pz, 0.0f, -10000000.0f, 10000000.0f);
        vertex.vx = finiteClamped(particle.vx, 0.0f, -1000000.0f, 1000000.0f);
        vertex.vy = finiteClamped(particle.vy, 0.0f, -1000000.0f, 1000000.0f);
        vertex.vz = finiteClamped(particle.vz, 0.0f, -1000000.0f, 1000000.0f);
        vertex.r = finiteClamped(particle.r, 0.0f, 0.0f, 1.0f);
        vertex.g = finiteClamped(particle.g, 0.0f, 0.0f, 1.0f);
        vertex.b = finiteClamped(particle.b, 0.0f, 0.0f, 1.0f);
        vertex.a = finiteClamped(particle.a, 0.0f, 0.0f, 1.0f);
        vertex.size = finiteClamped(particle.size, 0.0f, 0.0f, 1000000.0f);
        vertex.stretch = finiteClamped(particle.stretch, 1.0f, 1.0f, 1000000.0f);
        vertex.rotation = finiteClamped(particle.rotation, 0.0f, -1000000.0f, 1000000.0f);
        vertex.age = finiteClamped(particle.age, 0.0f, 0.0f, 1000000.0f);
        vertex.lifetime = finiteClamped(particle.lifetime, 1.0f, 0.001f, 1000000.0f);
        vertex.spriteRows = std::clamp(particle.spriteRows, 1, 1024);
        vertex.spriteCols = std::clamp(particle.spriteCols, 1, 1024);
        vertex.spriteFrame = std::clamp(
            particle.spriteFrame, 0, vertex.spriteRows * vertex.spriteCols - 1);
        converted.particles.push_back(vertex);
    }
    return converted;
}

// Maps the app-level render settings onto the Core GPU pipeline contract.
// Without this the GPU path always renders with the Core defaults
// (Additive / ScreenAligned / depthTest off) regardless of what the user
// picks in the properties panel.
ArtifactCore::ParticleRenderOptions coreRenderOptionsFromSettings(
    const ParticleRenderSettings& settings)
{
    ArtifactCore::ParticleRenderOptions options;
    switch (settings.blendMode) {
    case ParticleBlendMode::Additive:
        options.blend = ArtifactCore::ParticleBlendPolicy::Additive; break;
    case ParticleBlendMode::Subtractive:
        options.blend = ArtifactCore::ParticleBlendPolicy::Subtractive; break;
    case ParticleBlendMode::Normal:
        options.blend = ArtifactCore::ParticleBlendPolicy::Alpha; break;
    case ParticleBlendMode::Screen:
        options.blend = ArtifactCore::ParticleBlendPolicy::Screen; break;
    case ParticleBlendMode::Multiply:
        options.blend = ArtifactCore::ParticleBlendPolicy::Multiply; break;
    }
    switch (settings.billboardMode) {
    case ParticleRenderSettings::BillboardMode::None:
        options.billboard = ArtifactCore::ParticleBillboardPolicy::None; break;
    case ParticleRenderSettings::BillboardMode::ScreenAligned:
        options.billboard = ArtifactCore::ParticleBillboardPolicy::ScreenAligned; break;
    case ParticleRenderSettings::BillboardMode::ViewPlane:
        options.billboard = ArtifactCore::ParticleBillboardPolicy::ViewPlane; break;
    case ParticleRenderSettings::BillboardMode::VelocityAligned:
        options.billboard = ArtifactCore::ParticleBillboardPolicy::VelocityAligned; break;
    }
    options.depthTest = settings.depthTest;
    options.depthWrite = settings.depthWrite;
    return options;
}

void boostDebugParticleRenderData(ArtifactCore::ParticleRenderData& data)
{
    const auto safeColor = [](float value) {
        return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
    };
    ArtifactCore::Parallel::For(0, static_cast<int>(data.particles.size()),
                                static_cast<int>(data.particles.size()),
                                [&](int index) {
        auto& particle = data.particles[static_cast<size_t>(index)];
        const float safeSize = std::isfinite(particle.size)
            ? std::clamp(particle.size, 0.0f, 1000000.0f)
            : 0.0f;
        particle.size = std::clamp(std::max(18.0f, safeSize * 4.0f), 18.0f, 1000000.0f);
        particle.a = 1.0f;
        particle.r = std::clamp(safeColor(particle.r) * 1.15f + 0.20f, 0.0f, 1.0f);
        particle.g = std::clamp(safeColor(particle.g) * 1.15f + 0.20f, 0.0f, 1.0f);
        particle.b = std::clamp(safeColor(particle.b) * 1.15f + 0.20f, 0.0f, 1.0f);
    });
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

ArtifactCore::ParticleRenderData applyParticleRenderLOD(
    const ArtifactCore::ParticleRenderData& source,
    float screenScale);

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
        for (const auto& emitter : particleSystem->emitters()) {
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
    // This class is the canonical 2D identity. The JSON factory migrates old
    // Particle + is3D=true documents to ArtifactParticle3DLayer.
    setIs3D(false);
    createParticleSystem();
}

ArtifactParticleLayer::~ArtifactParticleLayer()
{
    delete impl_;
}

ArtifactParticle3DLayer::ArtifactParticle3DLayer()
{
    setIs3D(true);
}

ArtifactParticle3DLayer::~ArtifactParticle3DLayer() = default;

QJsonObject ArtifactParticle3DLayer::toJson() const
{
    QJsonObject json = ArtifactParticleLayer::toJson();
    json[QStringLiteral("type")] = static_cast<int>(LayerType::Particle3D);
    json[QStringLiteral("layerType")] = QStringLiteral("Particle3DLayer");
    json[QStringLiteral("is3D")] = true;
    return json;
}

void ArtifactParticle3DLayer::fromJsonProperties(const QJsonObject& obj)
{
    ArtifactParticleLayer::fromJsonProperties(obj);
    setIs3D(true);
}

void ArtifactParticleLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer || !impl_->particleSystem) {
        return;
    }

    const int64_t frameNumber = currentFrame();
    const bool rendererReady = renderer->isInitialized();
    // 1. 決定論的なシミュレーション状態の更新
    // ※ goToFrame は内部で reset() と forward simulation を行う
    float fps = 30.0f;
    if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
        fps = safeParticleFps(comp->frameRate().framerate());
    }
    // フレーム0でも最低1フレーム分のシミュレーションを走らせて初期パーティクルを生成する
    impl_->particleSystem->goToFrame(std::max(int64_t{1}, frameNumber), fps);

    // 2. GPU レンダリングパス
    // Diligent 経路が使える場合は billboard 描画を優先し、ここではソフト描画へ落とさない
    if (rendererReady) {
        const auto sourceData = impl_->particleSystem->captureRenderData();
        auto coreData = toCoreParticleRenderData(sourceData);
        coreData.options = coreRenderOptionsFromSettings(
            impl_->particleSystem->renderSettings());
        const QTransform globalTransform = getGlobalTransform();
        const float screenScale = std::max(std::hypot(globalTransform.m11(), globalTransform.m21()),
                                           std::hypot(globalTransform.m12(), globalTransform.m22()));
        const auto lodData = applyParticleRenderLOD(
            std::move(coreData), screenScale);
        if (!lodData.particles.empty()) {
            const ArtifactCore::ParticleRenderData renderData =
                transformParticleRenderData(lodData, globalTransform, opacity());
            renderer->drawParticles(renderData);
        }
        const auto size = sourceSize();
        drawFractureOverlay(renderer, getGlobalTransform4x4(), QSizeF(size.width, size.height), opacity());
        return;
    }

    // 3. ソフトウェアフォールバックパス
    // renderer が未初期化のときだけ従来の QPainter 描画を使う
    if (frameNumber != impl_->cachedFrameNumber || impl_->cachedFrame.isNull()) {
        float fallbackFps = 30.0f;
        if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
            fallbackFps = safeParticleFps(comp->frameRate().framerate());
        }
        const float time = safeParticleFrameTime(frameNumber, fallbackFps);
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

    const auto size = sourceSize();
    drawFractureOverlay(renderer, getGlobalTransform4x4(), QSizeF(size.width, size.height), opacity());
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
    const auto safeRenderValue = [](double value, double fallback,
                                    double minimum, double maximum) {
        return std::isfinite(value) ? std::clamp(value, minimum, maximum)
                                    : fallback;
    };
    const int safeBlendMode = std::clamp(static_cast<int>(rs.blendMode), 0, 4);
    const int safeBillboardMode =
        std::clamp(static_cast<int>(rs.billboardMode), 0, 3);
    const int safeSortMode = std::clamp(static_cast<int>(rs.sortMode), 0, 3);
    QJsonObject renderJson;
    renderJson["blendMode"] = safeBlendMode;
    renderJson["billboardMode"] = safeBillboardMode;
    renderJson["sortMode"] = safeSortMode;
    renderJson["depthTest"] = rs.depthTest;
    renderJson["depthWrite"] = rs.depthWrite;
    renderJson["softParticles"] = rs.softParticles;
    renderJson["softParticleDistance"] = safeRenderValue(
        rs.softParticleDistance, 0.0, 0.0, 1000000.0);
    renderJson["stretchEnabled"] = rs.stretchEnabled;
    renderJson["stretchFactor"] = safeRenderValue(
        rs.stretchFactor, 0.0, 0.0, 1000000.0);
    json["renderSettings"] = renderJson;
    
    // Save emitters
    const auto safeEmitterColor = [](const QColor& color) {
        return color.isValid() ? color : QColor(255, 255, 255);
    };
    const auto safeEmitterValue = [](double value, double fallback,
                                     double minimum, double maximum) {
        return std::isfinite(value) ? std::clamp(value, minimum, maximum)
                                    : fallback;
    };
    const auto safeEmitterInt = [](int value, int minimum, int maximum) {
        return std::clamp(value, minimum, maximum);
    };
    struct SerializableEmitter {
        const ParticleEmitter* emitter = nullptr;
        EmitterParams params;
    };
    std::vector<SerializableEmitter> serializableEmitters;
    if (impl_->particleSystem) {
        const auto& liveEmitters = impl_->particleSystem->emitters();
        serializableEmitters.reserve(liveEmitters.size());
        for (const auto& emitter : liveEmitters) {
            if (emitter) {
                serializableEmitters.push_back({emitter.get(), emitter->params()});
            }
        }
    } else {
        serializableEmitters.reserve(impl_->savedEmitterParams.size());
        for (const auto& params : impl_->savedEmitterParams) {
            serializableEmitters.push_back({nullptr, params});
        }
    }
    QJsonArray emittersArray;
    for (size_t emitterIndex = 0;
         emitterIndex < serializableEmitters.size();
         ++emitterIndex) {
        const auto& serializableEmitter = serializableEmitters[emitterIndex];
        const auto& params = serializableEmitter.params;
        QJsonObject emitterJson;
        emitterJson["shape"] = safeEmitterInt(static_cast<int>(params.shape), 0, 7);
        emitterJson["mode"] = safeEmitterInt(static_cast<int>(params.mode), 0, 2);
        emitterJson["rate"] = safeEmitterValue(params.rate, 10.0, 0.0, 1000000.0);
        emitterJson["burstCount"] = safeEmitterInt(params.burstCount, 0, 10000000);
        emitterJson["burstInterval"] = safeEmitterValue(
            params.burstInterval, 1.0, 0.0, 1000000.0);
        
        emitterJson["lifeMin"] = safeEmitterValue(params.lifeMin, 1.0, 0.001, 1000000.0);
        emitterJson["lifeMax"] = safeEmitterValue(params.lifeMax, 1.0, 0.001, 1000000.0);
        emitterJson["speedMin"] = safeEmitterValue(params.speedMin, 0.0, 0.0, 1000000.0);
        emitterJson["speedMax"] = safeEmitterValue(params.speedMax, 0.0, 0.0, 1000000.0);
        emitterJson["directionSpread"] = safeEmitterValue(
            params.directionSpread, 0.0, 0.0, 360.0);
        emitterJson["velocityRandomX"] = safeEmitterValue(
            params.velocityRandom.x(), 0.0, 0.0, 1000000.0);
        emitterJson["velocityRandomY"] = safeEmitterValue(
            params.velocityRandom.y(), 0.0, 0.0, 1000000.0);
        emitterJson["velocityRandomZ"] = safeEmitterValue(
            params.velocityRandom.z(), 0.0, 0.0, 1000000.0);
        
        emitterJson["positionX"] = safeEmitterValue(
            params.position.x(), 0.0, -1000000.0, 1000000.0);
        emitterJson["positionY"] = safeEmitterValue(
            params.position.y(), 0.0, -1000000.0, 1000000.0);
        emitterJson["positionZ"] = safeEmitterValue(
            params.position.z(), 0.0, -1000000.0, 1000000.0);
        emitterJson["rotationX"] = safeEmitterValue(
            params.rotation.x(), 0.0, -1000000.0, 1000000.0);
        emitterJson["rotationY"] = safeEmitterValue(
            params.rotation.y(), 0.0, -1000000.0, 1000000.0);
        emitterJson["rotationZ"] = safeEmitterValue(
            params.rotation.z(), 0.0, -1000000.0, 1000000.0);
        emitterJson["rotationSpeedMin"] = safeEmitterValue(
            params.rotationSpeedMin, 0.0, -1000000.0, 1000000.0);
        emitterJson["rotationSpeedMax"] = safeEmitterValue(
            params.rotationSpeedMax, 0.0, -1000000.0, 1000000.0);
        
        emitterJson["directionX"] = safeEmitterValue(
            params.direction.x(), 0.0, -1000000.0, 1000000.0);
        emitterJson["directionY"] = safeEmitterValue(
            params.direction.y(), 0.0, -1000000.0, 1000000.0);
        emitterJson["directionZ"] = safeEmitterValue(
            params.direction.z(), 0.0, -1000000.0, 1000000.0);
        
        emitterJson["radius"] = safeEmitterValue(params.radius, 0.0, 0.0, 1000000.0);
        emitterJson["width"] = safeEmitterValue(params.width, 0.0, 0.0, 1000000.0);
        emitterJson["height"] = safeEmitterValue(params.height, 0.0, 0.0, 1000000.0);
        emitterJson["depth"] = safeEmitterValue(params.depth, 0.0, 0.0, 1000000.0);
        emitterJson["lineLength"] = safeEmitterValue(
            params.lineLength, 0.0, 0.0, 1000000.0);
        
        emitterJson["scaleMin"] = safeEmitterValue(params.scaleMin, 1.0, 0.0, 1000.0);
        emitterJson["scaleMax"] = safeEmitterValue(params.scaleMax, 1.0, 0.0, 1000.0);
        emitterJson["scaleMidMin"] = safeEmitterValue(params.scaleMidMin, 1.0, 0.0, 1000.0);
        emitterJson["scaleMidMax"] = safeEmitterValue(params.scaleMidMax, 1.0, 0.0, 1000.0);
        emitterJson["scaleMidPosition"] = safeEmitterValue(
            params.scaleMidPosition, 0.5, 0.0, 1.0);
        emitterJson["scaleEndMin"] = safeEmitterValue(params.scaleEndMin, 1.0, 0.0, 1000.0);
        emitterJson["scaleEndMax"] = safeEmitterValue(params.scaleEndMax, 1.0, 0.0, 1000.0);
        
        emitterJson["colorStart"] =
            safeEmitterColor(params.colorStart).name(QColor::HexArgb);
        emitterJson["colorMid"] =
            safeEmitterColor(params.colorMid).name(QColor::HexArgb);
        emitterJson["colorEnd"] =
            safeEmitterColor(params.colorEnd).name(QColor::HexArgb);
        emitterJson["colorMidPosition"] = safeEmitterValue(
            params.colorMidPosition, 0.5, 0.0, 1.0);
        emitterJson["colorVariation"] = safeEmitterValue(
            params.colorVariation, 0.0, 0.0, 1.0);
        
        emitterJson["opacityMin"] = safeEmitterValue(params.opacityMin, 1.0, 0.0, 1.0);
        emitterJson["opacityMax"] = safeEmitterValue(params.opacityMax, 1.0, 0.0, 1.0);
        emitterJson["opacityMidMin"] = safeEmitterValue(params.opacityMidMin, 1.0, 0.0, 1.0);
        emitterJson["opacityMidMax"] = safeEmitterValue(params.opacityMidMax, 1.0, 0.0, 1.0);
        emitterJson["opacityMidPosition"] = safeEmitterValue(
            params.opacityMidPosition, 0.5, 0.0, 1.0);
        emitterJson["opacityEndMin"] = safeEmitterValue(params.opacityEndMin, 0.0, 0.0, 1.0);
        emitterJson["opacityEndMax"] = safeEmitterValue(params.opacityEndMax, 0.0, 0.0, 1.0);
        
        emitterJson["drag"] = safeEmitterValue(params.drag, 0.0, 0.0, 1000000.0);
        emitterJson["gravityX"] = safeEmitterValue(
            params.gravity.x(), 0.0, -1000000.0, 1000000.0);
        emitterJson["gravityY"] = safeEmitterValue(
            params.gravity.y(), 0.0, -1000000.0, 1000000.0);
        emitterJson["gravityZ"] = safeEmitterValue(
            params.gravity.z(), 0.0, -1000000.0, 1000000.0);
        emitterJson["windDirectionX"] = safeEmitterValue(
            params.windDirection.x(), 0.0, -1000000.0, 1000000.0);
        emitterJson["windDirectionY"] = safeEmitterValue(
            params.windDirection.y(), 0.0, -1000000.0, 1000000.0);
        emitterJson["windDirectionZ"] = safeEmitterValue(
            params.windDirection.z(), 0.0, -1000000.0, 1000000.0);
        emitterJson["windStrength"] = safeEmitterValue(
            params.windStrength, 0.0, 0.0, 1000000.0);
        emitterJson["turbulenceFrequency"] = safeEmitterValue(
            params.turbulenceFrequency, 0.0, 0.0, 1000000.0);
        emitterJson["turbulenceAmplitude"] = safeEmitterValue(
            params.turbulenceAmplitude, 0.0, 0.0, 1000000.0);
        emitterJson["turbulenceEvolution"] = safeEmitterValue(
            params.turbulenceEvolution, 0.0, -1000000.0, 1000000.0);
        emitterJson["texturePath"] = params.texturePath;
        emitterJson["textureRows"] = safeEmitterInt(params.textureRows, 1, 1024);
        emitterJson["textureCols"] = safeEmitterInt(params.textureCols, 1, 1024);
        emitterJson["randomFrame"] = params.randomFrame;
        emitterJson["startFrame"] = safeEmitterInt(params.startFrame, 0, 1000000000);
        emitterJson["frameCount"] = safeEmitterInt(params.frameCount, 1, 1000000);
        emitterJson["frameRate"] = safeEmitterValue(params.frameRate, 30.0, 0.001, 1000.0);
        emitterJson["mass"] = safeEmitterValue(params.mass, 1.0, 0.0, 1000000.0);
        emitterJson["inheritVelocity"] = params.inheritVelocity;
        emitterJson["worldSpace"] = params.worldSpace;
        emitterJson["preWarm"] = params.preWarm;
        emitterJson["maxParticles"] = safeEmitterInt(params.maxParticles, 1, 10000000);
        emitterJson["deterministic"] = params.deterministic;
        emitterJson["randomSeed"] = static_cast<double>(params.randomSeed);
        emitterJson["fixedTimeStep"] = safeEmitterValue(
            params.fixedTimeStep, 1.0 / 120.0, 0.000001, 1.0);
        emitterJson["maxSubSteps"] = safeEmitterInt(params.maxSubSteps, 1, 256);
        emitterJson["enableSelfCollision"] = params.enableSelfCollision;
        emitterJson["selfCollisionRadius"] = safeEmitterValue(
            params.selfCollisionRadius, 4.0, 0.001, 1000000.0);
        emitterJson["selfCollisionResponse"] = safeEmitterValue(
            params.selfCollisionResponse, 0.35, 0.0, 1.0);
        emitterJson["auxEnabled"] = params.auxEnabled;
        emitterJson["auxTrigger"] = safeEmitterInt(
            static_cast<int>(params.auxTrigger), 0, 2);
        emitterJson["auxCount"] = safeEmitterInt(params.auxCount, 0, 1000000);
        emitterJson["auxInterval"] = safeEmitterValue(params.auxInterval, 0.1, 0.0, 1000000.0);
        emitterJson["auxLifeScale"] = safeEmitterValue(params.auxLifeScale, 0.3, 0.0, 1000000.0);
        emitterJson["auxSizeScale"] = safeEmitterValue(params.auxSizeScale, 0.65, 0.0, 1000000.0);
        emitterJson["auxOpacityScale"] = safeEmitterValue(params.auxOpacityScale, 0.85, 0.0, 1.0);
        emitterJson["auxVelocityScale"] = safeEmitterValue(params.auxVelocityScale, 0.35, 0.0, 1000000.0);
        QJsonArray effectorsArray;
        if (const auto* emitter = serializableEmitter.emitter) {
                for (const auto& effector : emitter->effectors()) {
                    if (!effector) {
                        continue;
                    }
                    QJsonObject effectorJson;
                    effectorJson["type"] = safeEmitterInt(
                        static_cast<int>(effector->type), 0, 10);
                    effectorJson["enabled"] = effector->enabled;
                    effectorJson["strength"] = safeEmitterValue(
                        effector->strength, 1.0, -100000.0, 100000.0);
                    effectorJson["positionX"] = safeEmitterValue(
                        effector->position.x(), 0.0, -1000000.0, 1000000.0);
                    effectorJson["positionY"] = safeEmitterValue(
                        effector->position.y(), 0.0, -1000000.0, 1000000.0);
                    effectorJson["positionZ"] = safeEmitterValue(
                        effector->position.z(), 0.0, -1000000.0, 1000000.0);
                    effectorJson["directionX"] = safeEmitterValue(
                        effector->direction.x(), 0.0, -1000000.0, 1000000.0);
                    effectorJson["directionY"] = safeEmitterValue(
                        effector->direction.y(), 0.0, -1000000.0, 1000000.0);
                    effectorJson["directionZ"] = safeEmitterValue(
                        effector->direction.z(), 0.0, -1000000.0, 1000000.0);
                    if (const auto* typed = dynamic_cast<const ForceEffector*>(effector.get())) {
                        effectorJson["forceX"] = safeEmitterValue(
                            typed->force.x(), 0.0, -1000000.0, 1000000.0);
                        effectorJson["forceY"] = safeEmitterValue(
                            typed->force.y(), 0.0, -1000000.0, 1000000.0);
                        effectorJson["forceZ"] = safeEmitterValue(
                            typed->force.z(), 0.0, -1000000.0, 1000000.0);
                    } else if (const auto* typed = dynamic_cast<const VortexEffector*>(effector.get())) {
                        effectorJson["radius"] = safeEmitterValue(
                            typed->radius, 100.0, 0.0, 100000.0);
                        effectorJson["angularVelocity"] = safeEmitterValue(
                            typed->angularVelocity, 0.0, -100000.0, 100000.0);
                        effectorJson["tightness"] = safeEmitterValue(
                            typed->tightness, 1.0, 0.0, 100000.0);
                    } else if (const auto* typed = dynamic_cast<const TurbulenceEffector*>(effector.get())) {
                        effectorJson["frequency"] = safeEmitterValue(
                            typed->frequency, 1.0, 0.0, 10000.0);
                        effectorJson["amplitude"] = safeEmitterValue(
                            typed->amplitude, 1.0, 0.0, 100000.0);
                        const int safeOctaves = std::isfinite(typed->octaves)
                            ? static_cast<int>(std::clamp(
                                static_cast<double>(typed->octaves), 1.0, 12.0))
                            : 3;
                        effectorJson["octaves"] = safeOctaves;
                        effectorJson["evolution"] = safeEmitterValue(
                            typed->evolution, 0.0, -100000.0, 100000.0);
                        effectorJson["seed"] = typed->seed;
                    } else if (const auto* typed = dynamic_cast<const AttractorEffector*>(effector.get())) {
                        effectorJson["radius"] = safeEmitterValue(
                            typed->radius, 100.0, 0.0, 100000.0);
                        effectorJson["falloff"] = safeEmitterValue(
                            typed->falloff, 1.0, 0.0, 100000.0);
                        effectorJson["killOnReach"] = typed->killOnReach;
                        effectorJson["killRadius"] = safeEmitterValue(
                            typed->killRadius, 10.0, 0.0, 100000.0);
                    } else if (const auto* typed = dynamic_cast<const RepellerEffector*>(effector.get())) {
                        effectorJson["radius"] = safeEmitterValue(
                            typed->radius, 100.0, 0.0, 100000.0);
                        effectorJson["falloff"] = safeEmitterValue(
                            typed->falloff, 1.0, 0.0, 100000.0);
                    } else if (const auto* typed = dynamic_cast<const WindEffector*>(effector.get())) {
                        effectorJson["windDirectionX"] = safeEmitterValue(
                            typed->windDirection.x(), 0.0, -1000000.0, 1000000.0);
                        effectorJson["windDirectionY"] = safeEmitterValue(
                            typed->windDirection.y(), 0.0, -1000000.0, 1000000.0);
                        effectorJson["windDirectionZ"] = safeEmitterValue(
                            typed->windDirection.z(), 0.0, -1000000.0, 1000000.0);
                        effectorJson["windStrength"] = safeEmitterValue(
                            typed->windStrength, 0.0, 0.0, 100000.0);
                        effectorJson["turbulence"] = safeEmitterValue(
                            typed->turbulence, 0.0, 0.0, 100000.0);
                        effectorJson["turbulenceFrequency"] = safeEmitterValue(
                            typed->turbulenceFrequency, 1.0, 0.0, 10000.0);
                        effectorJson["evolution"] = safeEmitterValue(
                            typed->evolution, 0.0, -100000.0, 100000.0);
                    } else if (const auto* typed = dynamic_cast<const FlockingEffector*>(effector.get())) {
                        effectorJson["neighborhoodRadius"] = safeEmitterValue(
                            typed->neighborhoodRadius, 100.0, 0.0, 100000.0);
                        effectorJson["separationWeight"] = safeEmitterValue(
                            typed->separationWeight, 1.0, 0.0, 100000.0);
                        effectorJson["alignmentWeight"] = safeEmitterValue(
                            typed->alignmentWeight, 1.0, 0.0, 100000.0);
                        effectorJson["cohesionWeight"] = safeEmitterValue(
                            typed->cohesionWeight, 1.0, 0.0, 100000.0);
                        effectorJson["maxAcceleration"] = safeEmitterValue(
                            typed->maxAcceleration, 100.0, 0.0, 100000.0);
                    } else if (const auto* typed = dynamic_cast<const KillZoneEffector*>(effector.get())) {
                        effectorJson["zoneType"] = safeEmitterInt(
                            static_cast<int>(typed->zoneType), 0, 2);
                        effectorJson["sizeX"] = safeEmitterValue(
                            typed->size.x(), 0.0, 0.0, 100000.0);
                        effectorJson["sizeY"] = safeEmitterValue(
                            typed->size.y(), 0.0, 0.0, 100000.0);
                        effectorJson["sizeZ"] = safeEmitterValue(
                            typed->size.z(), 0.0, 0.0, 100000.0);
                        effectorJson["invert"] = typed->invert;
                    }
                    effectorsArray.append(effectorJson);
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
    auto layer = ArtifactCore::makeShared<ArtifactParticleLayer>();
    layer->ArtifactAbstractLayer::fromJsonProperties(obj);
    layer->setIs3D(false);
    layer->applyPropertiesFromJson(obj);
    return layer;
}

void ArtifactParticleLayer::fromJsonProperties(const QJsonObject& obj)
{
    ArtifactAbstractLayer::fromJsonProperties(obj);
    setIs3D(false);
    applyPropertiesFromJson(obj);
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
        auto rs = renderSettings();
        if (renderJson.contains("blendMode")) {
            rs.blendMode = static_cast<ParticleBlendMode>(
                std::clamp(renderJson["blendMode"].toInt(), 0, 4));
        }
        if (renderJson.contains("billboardMode")) {
            rs.billboardMode = static_cast<ParticleRenderSettings::BillboardMode>(
                std::clamp(renderJson["billboardMode"].toInt(), 0, 3));
        }
        if (renderJson.contains("sortMode")) {
            rs.sortMode = static_cast<ParticleRenderSettings::SortMode>(
                std::clamp(renderJson["sortMode"].toInt(), 0, 3));
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
            const double value = renderJson["softParticleDistance"].toDouble();
            rs.softParticleDistance = std::isfinite(value)
                ? std::clamp(value, 0.0, 1000000.0) : 0.0;
        }
        if (renderJson.contains("stretchEnabled")) {
            rs.stretchEnabled = renderJson["stretchEnabled"].toBool();
        }
        if (renderJson.contains("stretchFactor")) {
            const double value = renderJson["stretchFactor"].toDouble();
            rs.stretchFactor = std::isfinite(value)
                ? std::clamp(value, 0.0, 1000000.0) : 0.0;
        }
        setRenderSettings(rs);
    }
    
    // Load emitters
    if (obj.contains("emitters")) {
        // Loading is a state replacement, not an interactive edit. Avoid the
        // public clear path here so restoring a document does not mark it dirty.
        impl_->particleSystem->clearEmitters();
        impl_->savedEmitterParams.clear();
        clearFrameCache();
        QJsonArray emittersArray = obj["emitters"].toArray();
        constexpr qsizetype kMaxRestoredEmitters = 1024;
        qsizetype restoredEmitterCount = 0;
        for (const auto& emitterVal : emittersArray) {
            if (!emitterVal.isObject()) {
                continue;
            }
            if (restoredEmitterCount >= kMaxRestoredEmitters) {
                break;
            }
            ++restoredEmitterCount;
            QJsonObject emitterJson = emitterVal.toObject();
            EmitterParams params;
            
            if (emitterJson.contains("shape")) {
                params.shape = static_cast<EmitterShape>(
                    std::clamp(emitterJson["shape"].toInt(0), 0, 7));
            }
            if (emitterJson.contains("mode")) {
                params.mode = static_cast<EmissionMode>(
                    std::clamp(emitterJson["mode"].toInt(0), 0, 2));
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
                const QColor color(emitterJson["colorStart"].toString());
                if (color.isValid()) params.colorStart = color;
            }
            if (emitterJson.contains("colorMid")) {
                const QColor color(emitterJson["colorMid"].toString());
                if (color.isValid()) params.colorMid = color;
            }
            if (emitterJson.contains("colorEnd")) {
                const QColor color(emitterJson["colorEnd"].toString());
                if (color.isValid()) params.colorEnd = color;
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
            if (emitterJson.contains("deterministic")) {
                params.deterministic = emitterJson["deterministic"].toBool(true);
            }
            if (emitterJson.contains("randomSeed")) {
                params.randomSeed = static_cast<std::uint32_t>(
                    emitterJson["randomSeed"].toVariant().toULongLong());
            }
            if (emitterJson.contains("fixedTimeStep")) {
                params.fixedTimeStep = emitterJson["fixedTimeStep"].toDouble();
            }
            if (emitterJson.contains("maxSubSteps")) {
                params.maxSubSteps = emitterJson["maxSubSteps"].toInt();
            }
            if (emitterJson.contains("enableSelfCollision")) {
                params.enableSelfCollision =
                    emitterJson["enableSelfCollision"].toBool(false);
            }
            if (emitterJson.contains("selfCollisionRadius")) {
                params.selfCollisionRadius =
                    emitterJson["selfCollisionRadius"].toDouble();
            }
            if (emitterJson.contains("selfCollisionResponse")) {
                params.selfCollisionResponse =
                    emitterJson["selfCollisionResponse"].toDouble();
            }
            if (emitterJson.contains("auxEnabled")) {
                params.auxEnabled = emitterJson["auxEnabled"].toBool();
            }
            if (emitterJson.contains("auxTrigger")) {
                params.auxTrigger = static_cast<AuxTriggerMode>(std::clamp(
                    emitterJson["auxTrigger"].toInt(), 0, 2));
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

            const auto safeEmitterValue = [](const double value,
                                             const double fallback,
                                             const double minimum,
                                             const double maximum) {
                return std::isfinite(value)
                    ? std::clamp(value, minimum, maximum)
                    : fallback;
            };
            params.rate = safeEmitterValue(params.rate, 10.0, 0.0, 1000000.0);
            params.burstInterval = safeEmitterValue(params.burstInterval, 1.0, 0.0, 1000000.0);
            params.lifeMin = safeEmitterValue(params.lifeMin, 1.0, 0.001, 1000000.0);
            params.lifeMax = safeEmitterValue(params.lifeMax, params.lifeMin, 0.001, 1000000.0);
            if (params.lifeMax < params.lifeMin) params.lifeMax = params.lifeMin;
            params.speedMin = safeEmitterValue(params.speedMin, 0.0, 0.0, 1000000.0);
            params.speedMax = safeEmitterValue(params.speedMax, params.speedMin, 0.0, 1000000.0);
            if (params.speedMax < params.speedMin) params.speedMax = params.speedMin;
            params.directionSpread = safeEmitterValue(params.directionSpread, 0.0, 0.0, 360.0);
            params.frameRate = safeEmitterValue(params.frameRate, 30.0, 0.001, 1000.0);
            params.mass = safeEmitterValue(params.mass, 1.0, 0.0, 1000000.0);
            params.maxParticles = std::clamp(params.maxParticles, 1, 10000000);
            params.fixedTimeStep = safeEmitterValue(
                params.fixedTimeStep, 1.0 / 120.0, 0.000001, 1.0);
            params.maxSubSteps = std::clamp(params.maxSubSteps, 1, 256);
            params.selfCollisionRadius = safeEmitterValue(
                params.selfCollisionRadius, 4.0, 0.001, 1000000.0);
            params.selfCollisionResponse = safeEmitterValue(
                params.selfCollisionResponse, 0.35, 0.0, 1.0);
            params.burstCount = std::clamp(params.burstCount, 0, 10000000);
            params.auxCount = std::clamp(params.auxCount, 0, 1000000);
            params.auxInterval = safeEmitterValue(params.auxInterval, 0.0, 0.0, 1000000.0);
            params.auxLifeScale = safeEmitterValue(params.auxLifeScale, 1.0, 0.0, 1000000.0);
            params.auxSizeScale = safeEmitterValue(params.auxSizeScale, 1.0, 0.0, 1000000.0);
            params.auxOpacityScale = safeEmitterValue(params.auxOpacityScale, 1.0, 0.0, 1.0);
            params.auxVelocityScale = safeEmitterValue(params.auxVelocityScale, 1.0, 0.0, 1000000.0);
            const auto safeEmitterComponent = [&](const double value,
                                                   const double fallback = 0.0) {
                return safeEmitterValue(value, fallback, -1000000.0, 1000000.0);
            };
            params.velocityRandom.setX(safeEmitterComponent(params.velocityRandom.x()));
            params.velocityRandom.setY(safeEmitterComponent(params.velocityRandom.y()));
            params.velocityRandom.setZ(safeEmitterComponent(params.velocityRandom.z()));
            params.position.setX(safeEmitterComponent(params.position.x()));
            params.position.setY(safeEmitterComponent(params.position.y()));
            params.position.setZ(safeEmitterComponent(params.position.z()));
            params.rotation.setX(safeEmitterComponent(params.rotation.x()));
            params.rotation.setY(safeEmitterComponent(params.rotation.y()));
            params.rotation.setZ(safeEmitterComponent(params.rotation.z()));
            params.direction.setX(safeEmitterComponent(params.direction.x()));
            params.direction.setY(safeEmitterComponent(params.direction.y()));
            params.direction.setZ(safeEmitterComponent(params.direction.z()));
            params.scaleMin = safeEmitterValue(params.scaleMin, 1.0, 0.0, 1000.0);
            params.scaleMax = safeEmitterValue(params.scaleMax, params.scaleMin, 0.0, 1000.0);
            params.scaleEndMin = safeEmitterValue(params.scaleEndMin, 1.0, 0.0, 1000.0);
            params.scaleEndMax = safeEmitterValue(params.scaleEndMax, params.scaleEndMin, 0.0, 1000.0);
            params.scaleMidMin = safeEmitterValue(params.scaleMidMin, 1.0, 0.0, 1000.0);
            params.scaleMidMax = safeEmitterValue(params.scaleMidMax, params.scaleMidMin, 0.0, 1000.0);
            params.scaleMidPosition = safeEmitterValue(params.scaleMidPosition, 0.5, 0.0, 1.0);
            if (params.scaleMax < params.scaleMin) params.scaleMax = params.scaleMin;
            if (params.scaleEndMax < params.scaleEndMin) params.scaleEndMax = params.scaleEndMin;
            if (params.scaleMidMax < params.scaleMidMin) params.scaleMidMax = params.scaleMidMin;
            params.textureRows = std::clamp(params.textureRows, 1, 1024);
            params.textureCols = std::clamp(params.textureCols, 1, 1024);
            params.startFrame = std::clamp(params.startFrame, 0, 1000000000);
            params.frameCount = std::clamp(params.frameCount, 1, 1000000);
            params.auxTrigger = static_cast<AuxTriggerMode>(
                std::clamp(static_cast<int>(params.auxTrigger), 0, 2));
            params.texturePath = params.texturePath.trimmed().left(32768);
            
            ParticleEmitter* emitter = addEmitter(params);
            if (emitter && emitterJson.contains("effectors")) {
                const QJsonArray effectorsArray = emitterJson["effectors"].toArray();
                constexpr qsizetype kMaxRestoredEffectors = 1024;
                qsizetype restoredEffectorCount = 0;
                for (const auto& effectorVal : effectorsArray) {
                    if (!effectorVal.isObject()) {
                        continue;
                    }
                    if (restoredEffectorCount >= kMaxRestoredEffectors) {
                        break;
                    }
                    const QJsonObject effectorJson = effectorVal.toObject();
                    const int typeValue = effectorJson["type"].toInt(-1);
                    if (typeValue < 0 || typeValue > 10) {
                        continue;
                    }
                    const auto type = static_cast<EffectorType>(typeValue);
                    std::unique_ptr<ParticleEffector> effector;
                    switch (type) {
                        case EffectorType::Force: effector = std::make_unique<ForceEffector>(); break;
                        case EffectorType::Vortex: effector = std::make_unique<VortexEffector>(); break;
                        case EffectorType::Turbulence: effector = std::make_unique<TurbulenceEffector>(); break;
                        case EffectorType::Attractor: effector = std::make_unique<AttractorEffector>(); break;
                        case EffectorType::Repeller: effector = std::make_unique<RepellerEffector>(); break;
                        case EffectorType::Wind: effector = std::make_unique<WindEffector>(); break;
                        case EffectorType::Flocking: effector = std::make_unique<FlockingEffector>(); break;
                        case EffectorType::Kill: effector = std::make_unique<KillZoneEffector>(); break;
                    }
                    if (!effector) {
                        continue;
                    }
                    ++restoredEffectorCount;

                    effector->enabled = effectorJson["enabled"].toBool(true);
                    const float strength = static_cast<float>(effectorJson["strength"].toDouble(1.0));
                    effector->strength = std::isfinite(strength)
                        ? std::clamp(strength, -100000.0f, 100000.0f) : 1.0f;
                    const auto safeComponent = [](const double value) {
                        return std::isfinite(value)
                            ? std::clamp(static_cast<float>(value), -1000000.0f, 1000000.0f)
                            : 0.0f;
                    };
                    effector->position.setX(safeComponent(effectorJson["positionX"].toDouble()));
                    effector->position.setY(safeComponent(effectorJson["positionY"].toDouble()));
                    effector->position.setZ(safeComponent(effectorJson["positionZ"].toDouble()));
                    effector->direction.setX(safeComponent(effectorJson["directionX"].toDouble()));
                    effector->direction.setY(safeComponent(effectorJson["directionY"].toDouble()));
                    effector->direction.setZ(safeComponent(effectorJson["directionZ"].toDouble()));

                    switch (type) {
                        case EffectorType::Force: {
                            auto* typed = static_cast<ForceEffector*>(effector.get());
                            typed->force.setX(safeComponent(effectorJson["forceX"].toDouble()));
                            typed->force.setY(safeComponent(effectorJson["forceY"].toDouble()));
                            typed->force.setZ(safeComponent(effectorJson["forceZ"].toDouble()));
                            break;
                        }
                        case EffectorType::Vortex: {
                            auto* typed = static_cast<VortexEffector*>(effector.get());
                            const float radius = static_cast<float>(effectorJson["radius"].toDouble(100.0));
                            const float angularVelocity = static_cast<float>(effectorJson["angularVelocity"].toDouble());
                            const float tightness = static_cast<float>(effectorJson["tightness"].toDouble(1.0));
                            typed->radius = std::isfinite(radius) ? std::clamp(radius, 0.0f, 100000.0f) : 100.0f;
                            typed->angularVelocity = std::isfinite(angularVelocity) ? std::clamp(angularVelocity, -100000.0f, 100000.0f) : 0.0f;
                            typed->tightness = std::isfinite(tightness) ? std::clamp(tightness, 0.0f, 100000.0f) : 1.0f;
                            break;
                        }
                        case EffectorType::Turbulence: {
                            auto* typed = static_cast<TurbulenceEffector*>(effector.get());
                            const float frequency = static_cast<float>(effectorJson["frequency"].toDouble(1.0));
                            const float amplitude = static_cast<float>(effectorJson["amplitude"].toDouble(1.0));
                            const float evolution = static_cast<float>(effectorJson["evolution"].toDouble());
                            typed->frequency = std::isfinite(frequency)
                                ? std::clamp(frequency, 0.0f, 10000.0f) : 1.0f;
                            typed->amplitude = std::isfinite(amplitude)
                                ? std::clamp(amplitude, 0.0f, 100000.0f) : 1.0f;
                            typed->octaves = std::clamp(effectorJson["octaves"].toInt(3), 1, 12);
                            typed->evolution = std::isfinite(evolution)
                                ? std::clamp(evolution, -100000.0f, 100000.0f) : 0.0f;
                            typed->seed = effectorJson["seed"].toInt(0);
                            break;
                        }
                        case EffectorType::Attractor: {
                            auto* typed = static_cast<AttractorEffector*>(effector.get());
                            const float radius = static_cast<float>(effectorJson["radius"].toDouble(100.0));
                            const float falloff = static_cast<float>(effectorJson["falloff"].toDouble(1.0));
                            const float killRadius = static_cast<float>(effectorJson["killRadius"].toDouble(10.0));
                            typed->radius = std::isfinite(radius) ? std::clamp(radius, 0.0f, 100000.0f) : 100.0f;
                            typed->falloff = std::isfinite(falloff) ? std::clamp(falloff, 0.0f, 100000.0f) : 1.0f;
                            typed->killOnReach = effectorJson["killOnReach"].toBool(false);
                            typed->killRadius = std::isfinite(killRadius) ? std::clamp(killRadius, 0.0f, 100000.0f) : 10.0f;
                            break;
                        }
                        case EffectorType::Repeller: {
                            auto* typed = static_cast<RepellerEffector*>(effector.get());
                            const float radius = static_cast<float>(effectorJson["radius"].toDouble(100.0));
                            const float falloff = static_cast<float>(effectorJson["falloff"].toDouble(1.0));
                            typed->radius = std::isfinite(radius) ? std::clamp(radius, 0.0f, 100000.0f) : 100.0f;
                            typed->falloff = std::isfinite(falloff) ? std::clamp(falloff, 0.0f, 100000.0f) : 1.0f;
                            break;
                        }
                        case EffectorType::Wind: {
                            auto* typed = static_cast<WindEffector*>(effector.get());
                            typed->windDirection.setX(safeComponent(effectorJson["windDirectionX"].toDouble()));
                            typed->windDirection.setY(safeComponent(effectorJson["windDirectionY"].toDouble()));
                            typed->windDirection.setZ(safeComponent(effectorJson["windDirectionZ"].toDouble()));
                            const float windStrength = static_cast<float>(effectorJson["windStrength"].toDouble());
                            const float turbulence = static_cast<float>(effectorJson["turbulence"].toDouble());
                            const float turbulenceFrequency = static_cast<float>(effectorJson["turbulenceFrequency"].toDouble(1.0));
                            const float evolution = static_cast<float>(effectorJson["evolution"].toDouble());
                            typed->windStrength = std::isfinite(windStrength) ? std::clamp(windStrength, 0.0f, 100000.0f) : 0.0f;
                            typed->turbulence = std::isfinite(turbulence) ? std::clamp(turbulence, 0.0f, 100000.0f) : 0.0f;
                            typed->turbulenceFrequency = std::isfinite(turbulenceFrequency) ? std::clamp(turbulenceFrequency, 0.0f, 10000.0f) : 1.0f;
                            typed->evolution = std::isfinite(evolution) ? std::clamp(evolution, -100000.0f, 100000.0f) : 0.0f;
                            break;
                        }
                        case EffectorType::Flocking: {
                            auto* typed = static_cast<FlockingEffector*>(effector.get());
                            const float neighborhoodRadius = static_cast<float>(effectorJson["neighborhoodRadius"].toDouble(100.0));
                            const float separationWeight = static_cast<float>(effectorJson["separationWeight"].toDouble(1.0));
                            const float alignmentWeight = static_cast<float>(effectorJson["alignmentWeight"].toDouble(1.0));
                            const float cohesionWeight = static_cast<float>(effectorJson["cohesionWeight"].toDouble(1.0));
                            const float maxAcceleration = static_cast<float>(effectorJson["maxAcceleration"].toDouble(100.0));
                            typed->neighborhoodRadius = std::isfinite(neighborhoodRadius) ? std::clamp(neighborhoodRadius, 0.0f, 100000.0f) : 100.0f;
                            typed->separationWeight = std::isfinite(separationWeight) ? std::clamp(separationWeight, 0.0f, 100000.0f) : 1.0f;
                            typed->alignmentWeight = std::isfinite(alignmentWeight) ? std::clamp(alignmentWeight, 0.0f, 100000.0f) : 1.0f;
                            typed->cohesionWeight = std::isfinite(cohesionWeight) ? std::clamp(cohesionWeight, 0.0f, 100000.0f) : 1.0f;
                            typed->maxAcceleration = std::isfinite(maxAcceleration) ? std::clamp(maxAcceleration, 0.0f, 100000.0f) : 100.0f;
                            break;
                        }
                        case EffectorType::Kill: {
                            auto* typed = static_cast<KillZoneEffector*>(effector.get());
                            typed->zoneType = static_cast<KillZoneEffector::ZoneType>(
                                std::clamp(effectorJson["zoneType"].toInt(0), 0, 2));
                            const auto safeSize = [](const double value) {
                                return std::isfinite(value)
                                    ? std::clamp(static_cast<float>(value), 0.0f, 100000.0f)
                                    : 0.0f;
                            };
                            typed->size.setX(safeSize(effectorJson["sizeX"].toDouble()));
                            typed->size.setY(safeSize(effectorJson["sizeY"].toDouble()));
                            typed->size.setZ(safeSize(effectorJson["sizeZ"].toDouble()));
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
    Q_EMIT particleSystemChanged();
    Q_EMIT changed();
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
    const int previousCount = impl_->particleSystem->emitterCount();
    if (index < 0 || index >= previousCount) {
        return;
    }
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
    Q_EMIT particleSystemChanged();
    Q_EMIT changed();
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
        effector->force = safeEffectorVector(force);
        emitter->addEffector(std::move(effector));
        impl_->rebuildSavedEmitterParamsFromSystem();
        clearFrameCache();
    }
}

void ArtifactParticleLayer::addVortexEffector(const QVector3D& position, float radius, float angularVelocity)
{
    auto* emitter = firstEmitterOrCreate(impl_->particleSystem.get());
    if (emitter) {
        auto effector = std::make_unique<VortexEffector>();
        effector->position = safeEffectorVector(position);
        effector->radius = safeEffectorValue(radius, 100.0f, 0.0f, 100000.0f);
        effector->angularVelocity = safeEffectorValue(
            angularVelocity, 0.0f, -100000.0f, 100000.0f);
        emitter->addEffector(std::move(effector));
        impl_->rebuildSavedEmitterParamsFromSystem();
        clearFrameCache();
    }
}

void ArtifactParticleLayer::addTurbulenceEffector(float frequency, float amplitude)
{
    auto* emitter = firstEmitterOrCreate(impl_->particleSystem.get());
    if (emitter) {
        auto effector = std::make_unique<TurbulenceEffector>();
        effector->frequency = std::isfinite(frequency)
            ? std::clamp(frequency, 0.0f, 10000.0f) : 1.0f;
        effector->amplitude = std::isfinite(amplitude)
            ? std::clamp(amplitude, 0.0f, 100000.0f) : 1.0f;
        emitter->addEffector(std::move(effector));
        impl_->rebuildSavedEmitterParamsFromSystem();
        clearFrameCache();
    }
}

void ArtifactParticleLayer::addAttractorEffector(const QVector3D& position, float radius, float strength)
{
    auto* emitter = firstEmitterOrCreate(impl_->particleSystem.get());
    if (emitter) {
        auto effector = std::make_unique<AttractorEffector>();
        effector->position = safeEffectorVector(position);
        effector->radius = safeEffectorValue(radius, 100.0f, 0.0f, 100000.0f);
        effector->strength = safeEffectorValue(
            strength, 1.0f, -100000.0f, 100000.0f);
        emitter->addEffector(std::move(effector));
        impl_->rebuildSavedEmitterParamsFromSystem();
        clearFrameCache();
    }
}

void ArtifactParticleLayer::addWindEffector(const QVector3D& direction, float strength)
{
    auto* emitter = firstEmitterOrCreate(impl_->particleSystem.get());
    if (emitter) {
        auto effector = std::make_unique<WindEffector>();
        effector->windDirection = safeEffectorVector(direction);
        effector->windStrength = safeEffectorValue(
            strength, 0.0f, 0.0f, 100000.0f);
        emitter->addEffector(std::move(effector));
        impl_->rebuildSavedEmitterParamsFromSystem();
        clearFrameCache();
    }
}

void ArtifactParticleLayer::clearEffectors()
{
    for (const auto& emitter : impl_->particleSystem->emitters()) {
        if (emitter) emitter->clearEffectors();
    }
    impl_->rebuildSavedEmitterParamsFromSystem();
    clearFrameCache();
    Q_EMIT particleSystemChanged();
    Q_EMIT changed();
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
    impl_->particleSystem->renderSettings().blendMode =
        static_cast<ParticleBlendMode>(std::clamp(static_cast<int>(mode), 0, 4));
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
        fps = safeParticleFps(comp->frameRate().framerate());
    }
    float time = safeParticleFrameTime(frameNumber, fps);
    
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
    const int safeWidth = std::clamp(width, 1, 16384);
    const int safeHeight = std::clamp(height, 1, 16384);
    const float safeTime = std::isfinite(time)
        ? std::clamp(time, -1000000.0f, 1000000.0f)
        : 0.0f;
    QImage image(safeWidth, safeHeight, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    renderToImage(image, safeTime);
    impl_->cachedFrame = image;

    float fps = 30.0f;
    if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
        fps = safeParticleFps(comp->frameRate().framerate());
    }
    impl_->cachedFrameNumber = static_cast<int64_t>(safeTime * fps);
    return image;
}

void ArtifactParticleLayer::renderToImage(QImage& target, float time)
{
    if (!std::isfinite(time)) {
        time = 0.0f;
    } else {
        time = std::clamp(time, -1000000.0f, 1000000.0f);
    }
    // Update particle system to this time
    const float currentTime = std::isfinite(impl_->lastTime) ? impl_->lastTime : 0.0f;
    if (impl_->playing && time < currentTime) {
        reset();
    }
    const float baseTime = std::isfinite(impl_->lastTime) ? impl_->lastTime : 0.0f;
    const float deltaTime = time - baseTime;
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
        fps = safeParticleFps(comp->frameRate().framerate());
    }
    emit frameRendered(static_cast<int64_t>(time * fps));
}

void ArtifactParticleLayer::renderToImage(QImage& target, int64_t frameNumber)
{
    float fps = 30.0f;
    if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
        fps = safeParticleFps(comp->frameRate().framerate());
    }
    float time = safeParticleFrameTime(frameNumber, fps);
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
    for (auto& group : groups) {
        group.removeProperty(QStringLiteral("layer.is3D"));
    }
    ArtifactCore::PropertyGroup particleGroup(QStringLiteral("Particle System"));

    auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type, const QVariant& value, int priority = 0) {
        return persistentLayerProperty(name, type, value, priority);
    };

    particleGroup.addProperty(makeProp(QStringLiteral("particle.playing"), ArtifactCore::PropertyType::Boolean, isPlaying(), -140));
    auto timeScaleProp = makeProp(QStringLiteral("particle.timeScale"), ArtifactCore::PropertyType::Float, timeScale(), -130);
    timeScaleProp->setHardRange(0.0, 1000000.0);
    particleGroup.addProperty(timeScaleProp);
    // Keep both the editor and the property cache non-negative.
    auto emitterCountProp = makeProp(QStringLiteral("particle.emitterCount"), ArtifactCore::PropertyType::Integer, emitterCount(), -120);
    emitterCountProp->setMinValue(0);
    emitterCountProp->setHardRange(0, 1024);
    particleGroup.addProperty(emitterCountProp);
    auto previewWidthProp = makeProp(QStringLiteral("particle.previewWidth"), ArtifactCore::PropertyType::Integer, impl_->width, -110);
    previewWidthProp->setHardRange(1, 16384);
    particleGroup.addProperty(previewWidthProp);
    auto previewHeightProp = makeProp(QStringLiteral("particle.previewHeight"), ArtifactCore::PropertyType::Integer, impl_->height, -100);
    previewHeightProp->setHardRange(1, 16384);
    particleGroup.addProperty(previewHeightProp);

    const auto& rs = renderSettings();
    auto blendModeProp = makeProp(QStringLiteral("particle.render.blendMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(rs.blendMode), -90);
    blendModeProp->setHardRange(0, 4);
    blendModeProp->setTooltip(QStringLiteral("0=Additive, 1=Subtractive, 2=Normal, 3=Screen, 4=Multiply"));
    particleGroup.addProperty(blendModeProp);
    auto billboardModeProp = makeProp(QStringLiteral("particle.render.billboardMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(rs.billboardMode), -80);
    billboardModeProp->setHardRange(0, 3);
    billboardModeProp->setTooltip(QStringLiteral("0=None, 1=ScreenAligned, 2=ViewPlane, 3=VelocityAligned"));
    particleGroup.addProperty(billboardModeProp);
    auto sortModeProp = makeProp(QStringLiteral("particle.render.sortMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(rs.sortMode), -70);
    sortModeProp->setHardRange(0, 3);
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
    softParticleDistanceProp->setHardRange(0.0, 1000000.0);
    softParticleDistanceProp->setSoftRange(0.0, 200.0);
    softParticleDistanceProp->setStep(0.1);
    particleGroup.addProperty(softParticleDistanceProp);

    auto stretchEnabledProp = makeProp(QStringLiteral("particle.render.stretchEnabled"), ArtifactCore::PropertyType::Boolean, rs.stretchEnabled, -38);
    stretchEnabledProp->setDisplayLabel(QStringLiteral("Velocity Stretch"));
    particleGroup.addProperty(stretchEnabledProp);

    auto stretchFactorProp = makeProp(QStringLiteral("particle.render.stretchFactor"), ArtifactCore::PropertyType::Float, rs.stretchFactor, -37);
    stretchFactorProp->setDisplayLabel(QStringLiteral("Stretch Factor"));
    stretchFactorProp->setHardRange(0.0, 1000000.0);
    stretchFactorProp->setSoftRange(0.0, 20.0);
    stretchFactorProp->setStep(0.1);
    particleGroup.addProperty(stretchFactorProp);

    groups.push_back(particleGroup);

    ArtifactCore::PropertyGroup emitterGroup(QStringLiteral("Emitter"));
    const EmitterParams emitter = impl_->primaryEmitterParams().value_or(EmitterParams{});

    auto emitterShapeProp = makeProp(QStringLiteral("particle.emitter.shape"), ArtifactCore::PropertyType::Integer, static_cast<int>(emitter.shape), -240);
    emitterShapeProp->setHardRange(0, 7);
    emitterShapeProp->setDisplayLabel(QStringLiteral("Shape"));
    emitterShapeProp->setTooltip(QStringLiteral("0=Point, 1=Sphere, 2=Box, 3=Circle, 4=Rectangle, 5=Line, 6=Mesh, 7=Surface"));
    emitterGroup.addProperty(emitterShapeProp);

    auto emitterModeProp = makeProp(QStringLiteral("particle.emitter.mode"), ArtifactCore::PropertyType::Integer, static_cast<int>(emitter.mode), -239);
    emitterModeProp->setHardRange(0, 2);
    emitterModeProp->setDisplayLabel(QStringLiteral("Emission Mode"));
    emitterModeProp->setTooltip(QStringLiteral("0=Continuous, 1=Burst, 2=Triggered"));
    emitterGroup.addProperty(emitterModeProp);

    auto positionXProp = makeProp(QStringLiteral("particle.emitter.positionX"), ArtifactCore::PropertyType::Float, emitter.position.x(), -238);
    positionXProp->setDisplayLabel(QStringLiteral("Position X"));
    positionXProp->setUnit(QStringLiteral("px"));
    positionXProp->setHardRange(-1000000.0, 1000000.0);
    positionXProp->setSoftRange(-20000.0, 20000.0);
    emitterGroup.addProperty(positionXProp);

    auto positionYProp = makeProp(QStringLiteral("particle.emitter.positionY"), ArtifactCore::PropertyType::Float, emitter.position.y(), -237);
    positionYProp->setDisplayLabel(QStringLiteral("Position Y"));
    positionYProp->setUnit(QStringLiteral("px"));
    positionYProp->setHardRange(-1000000.0, 1000000.0);
    positionYProp->setSoftRange(-20000.0, 20000.0);
    emitterGroup.addProperty(positionYProp);

    auto rotationXProp = makeProp(QStringLiteral("particle.emitter.rotationX"), ArtifactCore::PropertyType::Float, emitter.rotation.x(), -236);
    rotationXProp->setDisplayLabel(QStringLiteral("Rotation X"));
    rotationXProp->setUnit(QStringLiteral("deg"));
    rotationXProp->setHardRange(-1000000.0, 1000000.0);
    rotationXProp->setSoftRange(-360.0, 360.0);
    emitterGroup.addProperty(rotationXProp);

    auto rotationYProp = makeProp(QStringLiteral("particle.emitter.rotationY"), ArtifactCore::PropertyType::Float, emitter.rotation.y(), -235);
    rotationYProp->setDisplayLabel(QStringLiteral("Rotation Y"));
    rotationYProp->setUnit(QStringLiteral("deg"));
    rotationYProp->setHardRange(-1000000.0, 1000000.0);
    rotationYProp->setSoftRange(-360.0, 360.0);
    emitterGroup.addProperty(rotationYProp);

    auto rotationZProp = makeProp(QStringLiteral("particle.emitter.rotationZ"), ArtifactCore::PropertyType::Float, emitter.rotation.z(), -234);
    rotationZProp->setDisplayLabel(QStringLiteral("Rotation Z"));
    rotationZProp->setUnit(QStringLiteral("deg"));
    rotationZProp->setHardRange(-1000000.0, 1000000.0);
    rotationZProp->setSoftRange(-360.0, 360.0);
    emitterGroup.addProperty(rotationZProp);

    auto rotationSpeedMinProp = makeProp(QStringLiteral("particle.emitter.rotationSpeedMin"), ArtifactCore::PropertyType::Float, emitter.rotationSpeedMin, -233);
    rotationSpeedMinProp->setDisplayLabel(QStringLiteral("Spin Min"));
    rotationSpeedMinProp->setUnit(QStringLiteral("deg/s"));
    rotationSpeedMinProp->setHardRange(-1000000.0, 1000000.0);
    rotationSpeedMinProp->setSoftRange(-720.0, 720.0);
    rotationSpeedMinProp->setStep(1.0);
    emitterGroup.addProperty(rotationSpeedMinProp);

    auto rotationSpeedMaxProp = makeProp(QStringLiteral("particle.emitter.rotationSpeedMax"), ArtifactCore::PropertyType::Float, emitter.rotationSpeedMax, -232);
    rotationSpeedMaxProp->setDisplayLabel(QStringLiteral("Spin Max"));
    rotationSpeedMaxProp->setUnit(QStringLiteral("deg/s"));
    rotationSpeedMaxProp->setHardRange(-1000000.0, 1000000.0);
    rotationSpeedMaxProp->setSoftRange(-720.0, 720.0);
    rotationSpeedMaxProp->setStep(1.0);
    emitterGroup.addProperty(rotationSpeedMaxProp);

    auto directionXProp = makeProp(QStringLiteral("particle.emitter.directionX"), ArtifactCore::PropertyType::Float, emitter.direction.x(), -233);
    directionXProp->setDisplayLabel(QStringLiteral("Direction X"));
    directionXProp->setHardRange(-1000000.0, 1000000.0);
    directionXProp->setSoftRange(-1.0, 1.0);
    directionXProp->setStep(0.01);
    emitterGroup.addProperty(directionXProp);

    auto directionYProp = makeProp(QStringLiteral("particle.emitter.directionY"), ArtifactCore::PropertyType::Float, emitter.direction.y(), -232);
    directionYProp->setDisplayLabel(QStringLiteral("Direction Y"));
    directionYProp->setHardRange(-1000000.0, 1000000.0);
    directionYProp->setSoftRange(-1.0, 1.0);
    directionYProp->setStep(0.01);
    emitterGroup.addProperty(directionYProp);

    auto directionZProp = makeProp(QStringLiteral("particle.emitter.directionZ"), ArtifactCore::PropertyType::Float, emitter.direction.z(), -231);
    directionZProp->setDisplayLabel(QStringLiteral("Direction Z"));
    directionZProp->setHardRange(-1000000.0, 1000000.0);
    directionZProp->setSoftRange(-1.0, 1.0);
    directionZProp->setStep(0.01);
    emitterGroup.addProperty(directionZProp);

    auto radiusProp = makeProp(QStringLiteral("particle.emitter.radius"), ArtifactCore::PropertyType::Float, emitter.radius, -230);
    radiusProp->setDisplayLabel(QStringLiteral("Radius"));
    radiusProp->setUnit(QStringLiteral("px"));
    radiusProp->setHardRange(0.0, 1000000.0);
    radiusProp->setSoftRange(0.0, 5000.0);
    emitterGroup.addProperty(radiusProp);

    auto widthProp = makeProp(QStringLiteral("particle.emitter.width"), ArtifactCore::PropertyType::Float, emitter.width, -229);
    widthProp->setDisplayLabel(QStringLiteral("Width"));
    widthProp->setUnit(QStringLiteral("px"));
    widthProp->setHardRange(0.0, 1000000.0);
    widthProp->setSoftRange(0.0, 10000.0);
    emitterGroup.addProperty(widthProp);

    auto heightProp = makeProp(QStringLiteral("particle.emitter.height"), ArtifactCore::PropertyType::Float, emitter.height, -228);
    heightProp->setDisplayLabel(QStringLiteral("Height"));
    heightProp->setUnit(QStringLiteral("px"));
    heightProp->setHardRange(0.0, 1000000.0);
    heightProp->setSoftRange(0.0, 10000.0);
    emitterGroup.addProperty(heightProp);

    auto depthProp = makeProp(QStringLiteral("particle.emitter.depth"), ArtifactCore::PropertyType::Float, emitter.depth, -227);
    depthProp->setDisplayLabel(QStringLiteral("Depth"));
    depthProp->setUnit(QStringLiteral("px"));
    depthProp->setHardRange(0.0, 1000000.0);
    depthProp->setSoftRange(0.0, 10000.0);
    emitterGroup.addProperty(depthProp);

    auto lineLengthProp = makeProp(QStringLiteral("particle.emitter.lineLength"), ArtifactCore::PropertyType::Float, emitter.lineLength, -226);
    lineLengthProp->setDisplayLabel(QStringLiteral("Line Length"));
    lineLengthProp->setUnit(QStringLiteral("px"));
    lineLengthProp->setHardRange(0.0, 1000000.0);
    lineLengthProp->setSoftRange(0.0, 10000.0);
    emitterGroup.addProperty(lineLengthProp);

    auto directionSpreadProp = makeProp(QStringLiteral("particle.emitter.directionSpread"), ArtifactCore::PropertyType::Float, emitter.directionSpread, -225);
    directionSpreadProp->setDisplayLabel(QStringLiteral("Direction Spread"));
    directionSpreadProp->setUnit(QStringLiteral("deg"));
    directionSpreadProp->setHardRange(0.0, 360.0);
    directionSpreadProp->setSoftRange(0.0, 360.0);
    emitterGroup.addProperty(directionSpreadProp);

    auto rateProp = makeProp(QStringLiteral("particle.emitter.rate"), ArtifactCore::PropertyType::Float, emitter.rate, -224);
    rateProp->setDisplayLabel(QStringLiteral("Rate"));
    rateProp->setUnit(QStringLiteral("/s"));
    rateProp->setHardRange(0.0, 1000000.0);
    rateProp->setSoftRange(0.0, 2000.0);
    emitterGroup.addProperty(rateProp);

    auto burstCountProp = makeProp(QStringLiteral("particle.emitter.burstCount"), ArtifactCore::PropertyType::Integer, emitter.burstCount, -223);
    burstCountProp->setDisplayLabel(QStringLiteral("Burst Count"));
    burstCountProp->setHardRange(0, 10000000);
    burstCountProp->setSoftRange(1, 5000);
    emitterGroup.addProperty(burstCountProp);

    auto burstIntervalProp = makeProp(QStringLiteral("particle.emitter.burstInterval"), ArtifactCore::PropertyType::Float, emitter.burstInterval, -222);
    burstIntervalProp->setDisplayLabel(QStringLiteral("Burst Interval"));
    burstIntervalProp->setUnit(QStringLiteral("s"));
    burstIntervalProp->setHardRange(0.0, 1000000.0);
    burstIntervalProp->setSoftRange(0.0, 10.0);
    burstIntervalProp->setStep(0.01);
    emitterGroup.addProperty(burstIntervalProp);

    auto maxParticlesProp = makeProp(QStringLiteral("particle.emitter.maxParticles"), ArtifactCore::PropertyType::Integer, emitter.maxParticles, -221);
    maxParticlesProp->setDisplayLabel(QStringLiteral("Max Particles"));
    maxParticlesProp->setHardRange(1, 10000000);
    maxParticlesProp->setSoftRange(1, 20000);
    emitterGroup.addProperty(maxParticlesProp);

    auto texturePathProp = makeProp(QStringLiteral("particle.emitter.texturePath"), ArtifactCore::PropertyType::String, emitter.texturePath, -220);
    texturePathProp->setDisplayLabel(QStringLiteral("Texture Path"));
    emitterGroup.addProperty(texturePathProp);

    auto textureRowsProp = makeProp(QStringLiteral("particle.emitter.textureRows"), ArtifactCore::PropertyType::Integer, emitter.textureRows, -219);
    textureRowsProp->setDisplayLabel(QStringLiteral("Texture Rows"));
    textureRowsProp->setHardRange(1, 1024);
    textureRowsProp->setSoftRange(1, 16);
    emitterGroup.addProperty(textureRowsProp);

    auto textureColsProp = makeProp(QStringLiteral("particle.emitter.textureCols"), ArtifactCore::PropertyType::Integer, emitter.textureCols, -218);
    textureColsProp->setDisplayLabel(QStringLiteral("Texture Cols"));
    textureColsProp->setHardRange(1, 1024);
    textureColsProp->setSoftRange(1, 16);
    emitterGroup.addProperty(textureColsProp);

    auto randomFrameProp = makeProp(QStringLiteral("particle.emitter.randomFrame"), ArtifactCore::PropertyType::Boolean, emitter.randomFrame, -217);
    randomFrameProp->setDisplayLabel(QStringLiteral("Random Frame"));
    emitterGroup.addProperty(randomFrameProp);

    auto startFrameProp = makeProp(QStringLiteral("particle.emitter.startFrame"), ArtifactCore::PropertyType::Integer, emitter.startFrame, -216);
    startFrameProp->setDisplayLabel(QStringLiteral("Start Frame"));
    startFrameProp->setHardRange(0, 1000000000);
    startFrameProp->setSoftRange(0, 256);
    emitterGroup.addProperty(startFrameProp);

    auto frameCountProp = makeProp(QStringLiteral("particle.emitter.frameCount"), ArtifactCore::PropertyType::Integer, emitter.frameCount, -215);
    frameCountProp->setDisplayLabel(QStringLiteral("Frame Count"));
    frameCountProp->setHardRange(1, 1000000);
    frameCountProp->setSoftRange(1, 256);
    emitterGroup.addProperty(frameCountProp);

    auto frameRateProp = makeProp(QStringLiteral("particle.emitter.frameRate"), ArtifactCore::PropertyType::Float, emitter.frameRate, -214);
    frameRateProp->setHardRange(0.001, 1000.0);
    frameRateProp->setDisplayLabel(QStringLiteral("Frame Rate"));
    frameRateProp->setUnit(QStringLiteral("fps"));
    frameRateProp->setSoftRange(0.0, 240.0);
    frameRateProp->setStep(0.1);
    emitterGroup.addProperty(frameRateProp);

    auto massProp = makeProp(QStringLiteral("particle.emitter.mass"), ArtifactCore::PropertyType::Float, emitter.mass, -213);
    massProp->setDisplayLabel(QStringLiteral("Mass"));
    massProp->setHardRange(0.0, 1000000.0);
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

    ArtifactCore::PropertyGroup simulationGroup(QStringLiteral("Simulation"));
    auto deterministicProp = makeProp(
        QStringLiteral("particle.simulation.deterministic"),
        ArtifactCore::PropertyType::Boolean, emitter.deterministic, -240);
    deterministicProp->setDisplayLabel(QStringLiteral("Deterministic"));
    deterministicProp->setTooltip(QStringLiteral(
        "Use fixed-step simulation and a stable random seed for repeatable seeking."));
    simulationGroup.addProperty(deterministicProp);

    auto seedProp = makeProp(
        QStringLiteral("particle.simulation.randomSeed"),
        ArtifactCore::PropertyType::Integer,
        static_cast<qlonglong>(emitter.randomSeed), -239);
    seedProp->setDisplayLabel(QStringLiteral("Random Seed"));
    seedProp->setHardRange(0, 2147483647);
    simulationGroup.addProperty(seedProp);

    auto fixedStepProp = makeProp(
        QStringLiteral("particle.simulation.fixedTimeStep"),
        ArtifactCore::PropertyType::Float, emitter.fixedTimeStep, -238);
    fixedStepProp->setDisplayLabel(QStringLiteral("Fixed Time Step"));
    fixedStepProp->setUnit(QStringLiteral("s"));
    fixedStepProp->setHardRange(0.000001, 1.0);
    fixedStepProp->setSoftRange(1.0 / 240.0, 1.0 / 30.0);
    fixedStepProp->setStep(1.0 / 120.0);
    simulationGroup.addProperty(fixedStepProp);

    auto maxSubStepsProp = makeProp(
        QStringLiteral("particle.simulation.maxSubSteps"),
        ArtifactCore::PropertyType::Integer, emitter.maxSubSteps, -237);
    maxSubStepsProp->setDisplayLabel(QStringLiteral("Max Substeps"));
    maxSubStepsProp->setHardRange(1, 256);
    maxSubStepsProp->setSoftRange(1, 32);
    simulationGroup.addProperty(maxSubStepsProp);

    auto selfCollisionProp = makeProp(
        QStringLiteral("particle.simulation.selfCollision"),
        ArtifactCore::PropertyType::Boolean, emitter.enableSelfCollision, -236);
    selfCollisionProp->setDisplayLabel(QStringLiteral("Self Collision"));
    simulationGroup.addProperty(selfCollisionProp);

    auto selfCollisionRadiusProp = makeProp(
        QStringLiteral("particle.simulation.selfCollisionRadius"),
        ArtifactCore::PropertyType::Float, emitter.selfCollisionRadius, -235);
    selfCollisionRadiusProp->setDisplayLabel(
        QStringLiteral("Collision Radius"));
    selfCollisionRadiusProp->setUnit(QStringLiteral("px"));
    selfCollisionRadiusProp->setHardRange(0.001, 1000000.0);
    selfCollisionRadiusProp->setSoftRange(0.1, 100.0);
    simulationGroup.addProperty(selfCollisionRadiusProp);

    auto selfCollisionResponseProp = makeProp(
        QStringLiteral("particle.simulation.selfCollisionResponse"),
        ArtifactCore::PropertyType::Float, emitter.selfCollisionResponse, -234);
    selfCollisionResponseProp->setDisplayLabel(
        QStringLiteral("Collision Response"));
    selfCollisionResponseProp->setHardRange(0.0, 1.0);
    selfCollisionResponseProp->setSoftRange(0.0, 1.0);
    simulationGroup.addProperty(selfCollisionResponseProp);
    groups.push_back(simulationGroup);

    ArtifactCore::PropertyGroup particleLookGroup(QStringLiteral("Particle"));

    auto lifeMinProp = makeProp(QStringLiteral("particle.particle.lifeMin"), ArtifactCore::PropertyType::Float, emitter.lifeMin, -232);
    lifeMinProp->setDisplayLabel(QStringLiteral("Life Min"));
    lifeMinProp->setUnit(QStringLiteral("s"));
    lifeMinProp->setHardRange(0.001, 1000000.0);
    lifeMinProp->setSoftRange(0.01, 60.0);
    particleLookGroup.addProperty(lifeMinProp);

    auto lifeMaxProp = makeProp(QStringLiteral("particle.particle.lifeMax"), ArtifactCore::PropertyType::Float, emitter.lifeMax, -231);
    lifeMaxProp->setDisplayLabel(QStringLiteral("Life Max"));
    lifeMaxProp->setUnit(QStringLiteral("s"));
    lifeMaxProp->setHardRange(0.001, 1000000.0);
    lifeMaxProp->setSoftRange(0.01, 60.0);
    particleLookGroup.addProperty(lifeMaxProp);

    auto speedMinProp = makeProp(QStringLiteral("particle.particle.speedMin"), ArtifactCore::PropertyType::Float, emitter.speedMin, -230);
    speedMinProp->setDisplayLabel(QStringLiteral("Speed Min"));
    speedMinProp->setUnit(QStringLiteral("px/s"));
    speedMinProp->setHardRange(0.0, 1000000.0);
    speedMinProp->setSoftRange(0.0, 5000.0);
    particleLookGroup.addProperty(speedMinProp);

    auto speedMaxProp = makeProp(QStringLiteral("particle.particle.speedMax"), ArtifactCore::PropertyType::Float, emitter.speedMax, -229);
    speedMaxProp->setDisplayLabel(QStringLiteral("Speed Max"));
    speedMaxProp->setUnit(QStringLiteral("px/s"));
    speedMaxProp->setHardRange(0.0, 1000000.0);
    speedMaxProp->setSoftRange(0.0, 5000.0);
    particleLookGroup.addProperty(speedMaxProp);

    auto velocityRandomXProp = makeProp(QStringLiteral("particle.particle.velocityRandomX"), ArtifactCore::PropertyType::Float, emitter.velocityRandom.x(), -228);
    velocityRandomXProp->setDisplayLabel(QStringLiteral("Velocity Random X"));
    velocityRandomXProp->setUnit(QStringLiteral("px/s"));
    velocityRandomXProp->setHardRange(0.0, 1000000.0);
    velocityRandomXProp->setSoftRange(0.0, 5000.0);
    particleLookGroup.addProperty(velocityRandomXProp);

    auto velocityRandomYProp = makeProp(QStringLiteral("particle.particle.velocityRandomY"), ArtifactCore::PropertyType::Float, emitter.velocityRandom.y(), -227);
    velocityRandomYProp->setDisplayLabel(QStringLiteral("Velocity Random Y"));
    velocityRandomYProp->setUnit(QStringLiteral("px/s"));
    velocityRandomYProp->setHardRange(0.0, 1000000.0);
    velocityRandomYProp->setSoftRange(0.0, 5000.0);
    particleLookGroup.addProperty(velocityRandomYProp);

    auto velocityRandomZProp = makeProp(QStringLiteral("particle.particle.velocityRandomZ"), ArtifactCore::PropertyType::Float, emitter.velocityRandom.z(), -226);
    velocityRandomZProp->setDisplayLabel(QStringLiteral("Velocity Random Z"));
    velocityRandomZProp->setUnit(QStringLiteral("px/s"));
    velocityRandomZProp->setHardRange(0.0, 1000000.0);
    velocityRandomZProp->setSoftRange(0.0, 5000.0);
    particleLookGroup.addProperty(velocityRandomZProp);

    auto scaleMinProp = makeProp(QStringLiteral("particle.particle.scaleMin"), ArtifactCore::PropertyType::Float, emitter.scaleMin, -225);
    scaleMinProp->setDisplayLabel(QStringLiteral("Size Min"));
    scaleMinProp->setUnit(QStringLiteral("px"));
    scaleMinProp->setHardRange(0.0, 1000.0);
    scaleMinProp->setSoftRange(0.1, 512.0);
    particleLookGroup.addProperty(scaleMinProp);

    auto scaleMaxProp = makeProp(QStringLiteral("particle.particle.scaleMax"), ArtifactCore::PropertyType::Float, emitter.scaleMax, -227);
    scaleMaxProp->setDisplayLabel(QStringLiteral("Size Max"));
    scaleMaxProp->setUnit(QStringLiteral("px"));
    scaleMaxProp->setHardRange(0.0, 1000.0);
    scaleMaxProp->setSoftRange(0.1, 512.0);
    particleLookGroup.addProperty(scaleMaxProp);

    auto scaleMidMinProp = makeProp(QStringLiteral("particle.particle.scaleMidMin"), ArtifactCore::PropertyType::Float, emitter.scaleMidMin, -226);
    scaleMidMinProp->setDisplayLabel(QStringLiteral("Size Mid Min"));
    scaleMidMinProp->setUnit(QStringLiteral("px"));
    scaleMidMinProp->setHardRange(0.0, 1000.0);
    scaleMidMinProp->setSoftRange(0.0, 512.0);
    particleLookGroup.addProperty(scaleMidMinProp);

    auto scaleMidMaxProp = makeProp(QStringLiteral("particle.particle.scaleMidMax"), ArtifactCore::PropertyType::Float, emitter.scaleMidMax, -225);
    scaleMidMaxProp->setDisplayLabel(QStringLiteral("Size Mid Max"));
    scaleMidMaxProp->setUnit(QStringLiteral("px"));
    scaleMidMaxProp->setHardRange(0.0, 1000.0);
    scaleMidMaxProp->setSoftRange(0.0, 512.0);
    particleLookGroup.addProperty(scaleMidMaxProp);

    auto scaleMidPosProp = makeProp(QStringLiteral("particle.particle.scaleMidPosition"), ArtifactCore::PropertyType::Float, emitter.scaleMidPosition, -224);
    scaleMidPosProp->setDisplayLabel(QStringLiteral("Size Mid Pos"));
    scaleMidPosProp->setHardRange(0.0, 1.0);
    scaleMidPosProp->setSoftRange(0.0, 1.0);
    scaleMidPosProp->setStep(0.01);
    particleLookGroup.addProperty(scaleMidPosProp);

    auto scaleEndMinProp = makeProp(QStringLiteral("particle.particle.scaleEndMin"), ArtifactCore::PropertyType::Float, emitter.scaleEndMin, -223);
    scaleEndMinProp->setDisplayLabel(QStringLiteral("End Size Min"));
    scaleEndMinProp->setUnit(QStringLiteral("px"));
    scaleEndMinProp->setHardRange(0.0, 1000.0);
    scaleEndMinProp->setSoftRange(0.0, 512.0);
    particleLookGroup.addProperty(scaleEndMinProp);

    auto scaleEndMaxProp = makeProp(QStringLiteral("particle.particle.scaleEndMax"), ArtifactCore::PropertyType::Float, emitter.scaleEndMax, -222);
    scaleEndMaxProp->setDisplayLabel(QStringLiteral("End Size Max"));
    scaleEndMaxProp->setUnit(QStringLiteral("px"));
    scaleEndMaxProp->setHardRange(0.0, 1000.0);
    scaleEndMaxProp->setSoftRange(0.0, 512.0);
    particleLookGroup.addProperty(scaleEndMaxProp);

    auto opacityMinProp = makeProp(QStringLiteral("particle.particle.opacityMin"), ArtifactCore::PropertyType::Float, emitter.opacityMin, -221);
    opacityMinProp->setDisplayLabel(QStringLiteral("Opacity Min"));
    opacityMinProp->setHardRange(0.0, 1.0);
    opacityMinProp->setSoftRange(0.0, 1.0);
    opacityMinProp->setStep(0.01);
    particleLookGroup.addProperty(opacityMinProp);

    auto opacityMaxProp = makeProp(QStringLiteral("particle.particle.opacityMax"), ArtifactCore::PropertyType::Float, emitter.opacityMax, -220);
    opacityMaxProp->setDisplayLabel(QStringLiteral("Opacity Max"));
    opacityMaxProp->setHardRange(0.0, 1.0);
    opacityMaxProp->setSoftRange(0.0, 1.0);
    opacityMaxProp->setStep(0.01);
    particleLookGroup.addProperty(opacityMaxProp);

    auto opacityMidMinProp = makeProp(QStringLiteral("particle.particle.opacityMidMin"), ArtifactCore::PropertyType::Float, emitter.opacityMidMin, -219);
    opacityMidMinProp->setDisplayLabel(QStringLiteral("Opacity Mid Min"));
    opacityMidMinProp->setHardRange(0.0, 1.0);
    opacityMidMinProp->setSoftRange(0.0, 1.0);
    opacityMidMinProp->setStep(0.01);
    particleLookGroup.addProperty(opacityMidMinProp);

    auto opacityMidMaxProp = makeProp(QStringLiteral("particle.particle.opacityMidMax"), ArtifactCore::PropertyType::Float, emitter.opacityMidMax, -218);
    opacityMidMaxProp->setDisplayLabel(QStringLiteral("Opacity Mid Max"));
    opacityMidMaxProp->setHardRange(0.0, 1.0);
    opacityMidMaxProp->setSoftRange(0.0, 1.0);
    opacityMidMaxProp->setStep(0.01);
    particleLookGroup.addProperty(opacityMidMaxProp);

    auto opacityMidPosProp = makeProp(QStringLiteral("particle.particle.opacityMidPosition"), ArtifactCore::PropertyType::Float, emitter.opacityMidPosition, -217);
    opacityMidPosProp->setDisplayLabel(QStringLiteral("Opacity Mid Pos"));
    opacityMidPosProp->setHardRange(0.0, 1.0);
    opacityMidPosProp->setSoftRange(0.0, 1.0);
    opacityMidPosProp->setStep(0.01);
    particleLookGroup.addProperty(opacityMidPosProp);

    auto opacityEndMinProp = makeProp(QStringLiteral("particle.particle.opacityEndMin"), ArtifactCore::PropertyType::Float, emitter.opacityEndMin, -216);
    opacityEndMinProp->setDisplayLabel(QStringLiteral("Opacity End Min"));
    opacityEndMinProp->setHardRange(0.0, 1.0);
    opacityEndMinProp->setSoftRange(0.0, 1.0);
    opacityEndMinProp->setStep(0.01);
    particleLookGroup.addProperty(opacityEndMinProp);

    auto opacityEndMaxProp = makeProp(QStringLiteral("particle.particle.opacityEndMax"), ArtifactCore::PropertyType::Float, emitter.opacityEndMax, -215);
    opacityEndMaxProp->setDisplayLabel(QStringLiteral("Opacity End Max"));
    opacityEndMaxProp->setHardRange(0.0, 1.0);
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
    colorMidPosProp->setHardRange(0.0, 1.0);
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
    dragProp->setHardRange(0.0, 1000000.0);
    dragProp->setSoftRange(0.0, 10.0);
    dragProp->setStep(0.01);
    physicsGroup.addProperty(dragProp);

    auto gravityXProp = makeProp(QStringLiteral("particle.physics.gravityX"), ArtifactCore::PropertyType::Float, emitter.gravity.x(), -211);
    gravityXProp->setDisplayLabel(QStringLiteral("Gravity X"));
    gravityXProp->setUnit(QStringLiteral("px/s2"));
    gravityXProp->setHardRange(-1000000.0, 1000000.0);
    gravityXProp->setSoftRange(-5000.0, 5000.0);
    physicsGroup.addProperty(gravityXProp);

    auto gravityYProp = makeProp(QStringLiteral("particle.physics.gravityY"), ArtifactCore::PropertyType::Float, emitter.gravity.y(), -210);
    gravityYProp->setDisplayLabel(QStringLiteral("Gravity Y"));
    gravityYProp->setUnit(QStringLiteral("px/s2"));
    gravityYProp->setHardRange(-1000000.0, 1000000.0);
    gravityYProp->setSoftRange(-5000.0, 5000.0);
    physicsGroup.addProperty(gravityYProp);

    auto gravityZProp = makeProp(QStringLiteral("particle.physics.gravityZ"), ArtifactCore::PropertyType::Float, emitter.gravity.z(), -209);
    gravityZProp->setDisplayLabel(QStringLiteral("Gravity Z"));
    gravityZProp->setUnit(QStringLiteral("px/s2"));
    gravityZProp->setHardRange(-1000000.0, 1000000.0);
    gravityZProp->setSoftRange(-5000.0, 5000.0);
    physicsGroup.addProperty(gravityZProp);

    auto windDirectionXProp = makeProp(QStringLiteral("particle.physics.windDirectionX"), ArtifactCore::PropertyType::Float, emitter.windDirection.x(), -208);
    windDirectionXProp->setDisplayLabel(QStringLiteral("Wind Dir X"));
    windDirectionXProp->setHardRange(-1000000.0, 1000000.0);
    windDirectionXProp->setSoftRange(-1.0, 1.0);
    windDirectionXProp->setStep(0.01);
    physicsGroup.addProperty(windDirectionXProp);

    auto windDirectionYProp = makeProp(QStringLiteral("particle.physics.windDirectionY"), ArtifactCore::PropertyType::Float, emitter.windDirection.y(), -207);
    windDirectionYProp->setDisplayLabel(QStringLiteral("Wind Dir Y"));
    windDirectionYProp->setHardRange(-1000000.0, 1000000.0);
    windDirectionYProp->setSoftRange(-1.0, 1.0);
    windDirectionYProp->setStep(0.01);
    physicsGroup.addProperty(windDirectionYProp);

    auto windDirectionZProp = makeProp(QStringLiteral("particle.physics.windDirectionZ"), ArtifactCore::PropertyType::Float, emitter.windDirection.z(), -206);
    windDirectionZProp->setDisplayLabel(QStringLiteral("Wind Dir Z"));
    windDirectionZProp->setHardRange(-1000000.0, 1000000.0);
    windDirectionZProp->setSoftRange(-1.0, 1.0);
    windDirectionZProp->setStep(0.01);
    physicsGroup.addProperty(windDirectionZProp);

    auto windStrengthProp = makeProp(QStringLiteral("particle.physics.windStrength"), ArtifactCore::PropertyType::Float, emitter.windStrength, -205);
    windStrengthProp->setDisplayLabel(QStringLiteral("Wind Strength"));
    windStrengthProp->setUnit(QStringLiteral("px/s2"));
    windStrengthProp->setHardRange(0.0, 1000000.0);
    windStrengthProp->setSoftRange(0.0, 5000.0);
    physicsGroup.addProperty(windStrengthProp);

    auto turbulenceFrequencyProp = makeProp(QStringLiteral("particle.physics.turbulenceFrequency"), ArtifactCore::PropertyType::Float, emitter.turbulenceFrequency, -204);
    turbulenceFrequencyProp->setDisplayLabel(QStringLiteral("Turbulence Freq"));
    turbulenceFrequencyProp->setHardRange(0.0, 1000.0);
    turbulenceFrequencyProp->setSoftRange(0.0, 1.0);
    turbulenceFrequencyProp->setStep(0.001);
    physicsGroup.addProperty(turbulenceFrequencyProp);

    auto turbulenceAmplitudeProp = makeProp(QStringLiteral("particle.physics.turbulenceAmplitude"), ArtifactCore::PropertyType::Float, emitter.turbulenceAmplitude, -203);
    turbulenceAmplitudeProp->setDisplayLabel(QStringLiteral("Turbulence Amp"));
    turbulenceAmplitudeProp->setUnit(QStringLiteral("px/s2"));
    turbulenceAmplitudeProp->setHardRange(0.0, 1000000.0);
    turbulenceAmplitudeProp->setSoftRange(0.0, 5000.0);
    turbulenceAmplitudeProp->setStep(0.1);
    physicsGroup.addProperty(turbulenceAmplitudeProp);

    auto turbulenceEvolutionProp = makeProp(QStringLiteral("particle.physics.turbulenceEvolution"), ArtifactCore::PropertyType::Float, emitter.turbulenceEvolution, -202);
    turbulenceEvolutionProp->setDisplayLabel(QStringLiteral("Turbulence Evol"));
    turbulenceEvolutionProp->setHardRange(-1000000.0, 1000000.0);
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
    auxTriggerProp->setHardRange(0, 2);
    auxTriggerProp->setTooltip(QStringLiteral("0=Trails, 1=Birth, 2=Death"));
    auxGroup.addProperty(auxTriggerProp);

    auto auxCountProp = makeProp(QStringLiteral("particle.aux.count"), ArtifactCore::PropertyType::Integer, emitter.auxCount, -209);
    auxCountProp->setDisplayLabel(QStringLiteral("Count"));
    auxCountProp->setHardRange(0, 1000000);
    auxCountProp->setSoftRange(0, 32);
    auxGroup.addProperty(auxCountProp);

    auto auxIntervalProp = makeProp(QStringLiteral("particle.aux.interval"), ArtifactCore::PropertyType::Float, emitter.auxInterval, -208);
    auxIntervalProp->setDisplayLabel(QStringLiteral("Interval"));
    auxIntervalProp->setUnit(QStringLiteral("s"));
    auxIntervalProp->setHardRange(0.0, 1000000.0);
    auxIntervalProp->setSoftRange(0.01, 2.0);
    auxIntervalProp->setStep(0.01);
    auxGroup.addProperty(auxIntervalProp);

    auto auxLifeScaleProp = makeProp(QStringLiteral("particle.aux.lifeScale"), ArtifactCore::PropertyType::Float, emitter.auxLifeScale, -207);
    auxLifeScaleProp->setDisplayLabel(QStringLiteral("Life Scale"));
    auxLifeScaleProp->setHardRange(0.0, 1000000.0);
    auxLifeScaleProp->setSoftRange(0.05, 4.0);
    auxLifeScaleProp->setStep(0.01);
    auxGroup.addProperty(auxLifeScaleProp);

    auto auxSizeScaleProp = makeProp(QStringLiteral("particle.aux.sizeScale"), ArtifactCore::PropertyType::Float, emitter.auxSizeScale, -206);
    auxSizeScaleProp->setDisplayLabel(QStringLiteral("Size Scale"));
    auxSizeScaleProp->setHardRange(0.0, 1000000.0);
    auxSizeScaleProp->setSoftRange(0.05, 4.0);
    auxSizeScaleProp->setStep(0.01);
    auxGroup.addProperty(auxSizeScaleProp);

    auto auxOpacityScaleProp = makeProp(QStringLiteral("particle.aux.opacityScale"), ArtifactCore::PropertyType::Float, emitter.auxOpacityScale, -205);
    auxOpacityScaleProp->setDisplayLabel(QStringLiteral("Opacity Scale"));
    auxOpacityScaleProp->setHardRange(0.0, 1.0);
    auxOpacityScaleProp->setSoftRange(0.0, 1.0);
    auxOpacityScaleProp->setStep(0.01);
    auxGroup.addProperty(auxOpacityScaleProp);

    auto auxVelocityScaleProp = makeProp(QStringLiteral("particle.aux.velocityScale"), ArtifactCore::PropertyType::Float, emitter.auxVelocityScale, -204);
    auxVelocityScaleProp->setDisplayLabel(QStringLiteral("Velocity Scale"));
    auxVelocityScaleProp->setHardRange(0.0, 1000000.0);
    auxVelocityScaleProp->setSoftRange(0.0, 4.0);
    auxVelocityScaleProp->setStep(0.01);
    auxGroup.addProperty(auxVelocityScaleProp);
    groups.push_back(auxGroup);
    return groups;
}

bool ArtifactParticleLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == QStringLiteral("layer.is3D")) {
        return false;
    }
    auto applyPrimaryEmitterValue = [this](const std::function<void(EmitterParams&)>& mutator) {
        if (!impl_->applyPrimaryEmitterParams(mutator)) {
            return false;
        }
        clearFrameCache();
        Q_EMIT particleSystemChanged();
        Q_EMIT changed();
        return true;
    };
    const auto safeParticleFloat = [](const QVariant& input,
                                     const float fallback = 0.0f,
                                     const float minimum = -1000000.0f,
                                     const float maximum = 1000000.0f) {
        const float raw = static_cast<float>(input.toDouble());
        return std::isfinite(raw) ? std::clamp(raw, minimum, maximum) : fallback;
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
        constexpr int kMaxEmitterCount = 1024;
        const int targetCount = std::clamp(value.toInt(), 0, kMaxEmitterCount);
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
        impl_->width = std::clamp(value.toInt(), 1, 16384);
        impl_->scaleEmitterPositions(
            static_cast<float>(impl_->width) / static_cast<float>(oldWidth), 1.0f);
        clearFrameCache();
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.previewHeight")) {
        const int oldHeight = std::max(1, impl_->height);
        impl_->height = std::clamp(value.toInt(), 1, 16384);
        impl_->scaleEmitterPositions(
            1.0f, static_cast<float>(impl_->height) / static_cast<float>(oldHeight));
        clearFrameCache();
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.render.blendMode")) {
        auto rs = renderSettings();
        rs.blendMode = static_cast<ParticleBlendMode>(std::clamp(value.toInt(), 0, 4));
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.render.billboardMode")) {
        auto rs = renderSettings();
        rs.billboardMode = static_cast<ParticleRenderSettings::BillboardMode>(
            std::clamp(value.toInt(), 0, 3));
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.render.sortMode")) {
        auto rs = renderSettings();
        rs.sortMode = static_cast<ParticleRenderSettings::SortMode>(
            std::clamp(value.toInt(), 0, 3));
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
        rs.softParticleDistance = safeParticleFloat(value, 0.0f, 0.0f, 1000000.0f);
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
        rs.stretchFactor = safeParticleFloat(value, 0.0f, 0.0f, 1000000.0f);
        setRenderSettings(rs);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("particle.emitter.shape")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.shape = static_cast<EmitterShape>(std::clamp(value.toInt(), 0, 7));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.mode")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.mode = static_cast<EmissionMode>(std::clamp(value.toInt(), 0, 2));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.positionX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.position.setX(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.positionY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.position.setY(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rotationX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rotation.setX(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rotationY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rotation.setY(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rotationZ")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rotation.setZ(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rotationSpeedMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rotationSpeedMin = safeParticleFloat(value, params.rotationSpeedMin);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rotationSpeedMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rotationSpeedMax = safeParticleFloat(value, params.rotationSpeedMax);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.directionX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.direction.setX(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.directionY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.direction.setY(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.directionZ")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.direction.setZ(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.radius")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.radius = safeParticleFloat(value, params.radius, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.width")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.width = safeParticleFloat(value, params.width, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.height")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.height = safeParticleFloat(value, params.height, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.depth")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.depth = safeParticleFloat(value, params.depth, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.lineLength")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.lineLength = safeParticleFloat(value, params.lineLength, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.rate")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.rate = safeParticleFloat(value, 10.0f, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.burstCount")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.burstCount = std::clamp(value.toInt(), 0, 10000000);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.burstInterval")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.burstInterval = safeParticleFloat(value, 1.0f, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.texturePath")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.texturePath = value.toString().trimmed().left(32768);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.textureRows")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.textureRows = std::clamp(value.toInt(), 1, 1024);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.textureCols")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.textureCols = std::clamp(value.toInt(), 1, 1024);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.randomFrame")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.randomFrame = value.toBool();
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.startFrame")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.startFrame = std::clamp(value.toInt(), 0, 1000000000);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.frameCount")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.frameCount = std::clamp(value.toInt(), 1, 1000000);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.frameRate")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.frameRate = safeParticleFloat(value, 30.0f, 0.001f, 1000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.mass")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.mass = safeParticleFloat(value, 1.0f, 0.0f, 1000000.0f);
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
    if (propertyPath ==
        QStringLiteral("particle.simulation.deterministic")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.deterministic = value.toBool();
        });
    }
    if (propertyPath == QStringLiteral("particle.simulation.randomSeed")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            const auto raw = value.toLongLong();
            params.randomSeed = static_cast<std::uint32_t>(
                std::clamp<qlonglong>(raw, 0, 2147483647));
        });
    }
    if (propertyPath ==
        QStringLiteral("particle.simulation.fixedTimeStep")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.fixedTimeStep = safeParticleFloat(
                value, 1.0f / 120.0f, 0.000001f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.simulation.maxSubSteps")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.maxSubSteps = std::clamp(value.toInt(), 1, 256);
        });
    }
    if (propertyPath == QStringLiteral("particle.simulation.selfCollision")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.enableSelfCollision = value.toBool();
        });
    }
    if (propertyPath ==
        QStringLiteral("particle.simulation.selfCollisionRadius")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.selfCollisionRadius = safeParticleFloat(
                value, 4.0f, 0.001f, 1000000.0f);
        });
    }
    if (propertyPath ==
        QStringLiteral("particle.simulation.selfCollisionResponse")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.selfCollisionResponse = safeParticleFloat(
                value, 0.35f, 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.lifeMin") ||
        propertyPath == QStringLiteral("particle.particle.lifeMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.lifeMin = safeParticleFloat(value, 1.0f, 0.001f, 1000000.0f);
            if (params.lifeMax < params.lifeMin) params.lifeMax = params.lifeMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.lifeMax") ||
        propertyPath == QStringLiteral("particle.particle.lifeMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.lifeMax = safeParticleFloat(value, params.lifeMin, 0.001f, 1000000.0f);
            if (params.lifeMax < params.lifeMin) params.lifeMax = params.lifeMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.speedMin") ||
        propertyPath == QStringLiteral("particle.particle.speedMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.speedMin = safeParticleFloat(value, 0.0f, 0.0f, 1000000.0f);
            if (params.speedMax < params.speedMin) params.speedMax = params.speedMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.speedMax") ||
        propertyPath == QStringLiteral("particle.particle.speedMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.speedMax = safeParticleFloat(value, params.speedMin, 0.0f, 1000000.0f);
            if (params.speedMax < params.speedMin) params.speedMax = params.speedMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.velocityRandomX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.velocityRandom.setX(safeParticleFloat(value, 0.0f, 0.0f, 1000000.0f));
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.velocityRandomY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.velocityRandom.setY(safeParticleFloat(value, 0.0f, 0.0f, 1000000.0f));
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.velocityRandomZ")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.velocityRandom.setZ(safeParticleFloat(value, 0.0f, 0.0f, 1000000.0f));
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.directionSpread")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.directionSpread = safeParticleFloat(value, 0.0f, 0.0f, 360.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.scaleMin") ||
        propertyPath == QStringLiteral("particle.particle.scaleMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleMin = safeParticleFloat(value, 1.0f, 0.0f, 1000.0f);
            if (params.scaleMax < params.scaleMin) params.scaleMax = params.scaleMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.scaleMax") ||
        propertyPath == QStringLiteral("particle.particle.scaleMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleMax = safeParticleFloat(value, params.scaleMin, 0.0f, 1000.0f);
            if (params.scaleMax < params.scaleMin) params.scaleMax = params.scaleMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.scaleMidMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleMidMin = safeParticleFloat(value, 1.0f, 0.0f, 1000.0f);
            if (params.scaleMidMax < params.scaleMidMin) params.scaleMidMax = params.scaleMidMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.scaleMidMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleMidMax = safeParticleFloat(value, params.scaleMidMin, 0.0f, 1000.0f);
            if (params.scaleMidMax < params.scaleMidMin) params.scaleMidMax = params.scaleMidMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.scaleMidPosition")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleMidPosition = safeParticleFloat(value, 0.5f, 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.scaleEndMin") ||
        propertyPath == QStringLiteral("particle.particle.scaleEndMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleEndMin = safeParticleFloat(value, 1.0f, 0.0f, 1000.0f);
            if (params.scaleEndMax < params.scaleEndMin) params.scaleEndMax = params.scaleEndMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.scaleEndMax") ||
        propertyPath == QStringLiteral("particle.particle.scaleEndMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.scaleEndMax = safeParticleFloat(value, params.scaleEndMin, 0.0f, 1000.0f);
            if (params.scaleEndMax < params.scaleEndMin) params.scaleEndMax = params.scaleEndMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.opacityMin") ||
        propertyPath == QStringLiteral("particle.particle.opacityMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityMin = safeParticleFloat(value, 1.0f, 0.0f, 1.0f);
            if (params.opacityMax < params.opacityMin) params.opacityMax = params.opacityMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.opacityMax") ||
        propertyPath == QStringLiteral("particle.particle.opacityMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityMax = safeParticleFloat(value, params.opacityMin, 0.0f, 1.0f);
            if (params.opacityMax < params.opacityMin) params.opacityMax = params.opacityMin;
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.opacityMidMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityMidMin = safeParticleFloat(value, params.opacityMidMin, 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.opacityMidMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityMidMax = safeParticleFloat(value, params.opacityMidMax, 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.opacityMidPosition")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityMidPosition = safeParticleFloat(value, params.opacityMidPosition, 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.opacityEndMin")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityEndMin = safeParticleFloat(value, params.opacityEndMin, 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.opacityEndMax")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.opacityEndMax = safeParticleFloat(value, params.opacityEndMax, 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.drag") ||
        propertyPath == QStringLiteral("particle.physics.drag")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.drag = safeParticleFloat(value, 0.0f, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.gravityX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.gravity.setX(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.gravityY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.gravity.setY(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.gravityZ")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.gravity.setZ(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.windDirectionX")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.windDirection.setX(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.windDirectionY")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.windDirection.setY(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.windDirectionZ")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.windDirection.setZ(safeParticleFloat(value));
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.windStrength")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.windStrength = safeParticleFloat(value, 0.0f, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.turbulenceFrequency")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.turbulenceFrequency = safeParticleFloat(value, 0.0f, 0.0f, 1000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.turbulenceAmplitude")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.turbulenceAmplitude = safeParticleFloat(value, 0.0f, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.physics.turbulenceEvolution")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.turbulenceEvolution = safeParticleFloat(value, 0.0f, -1000000.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.maxParticles")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.maxParticles = std::clamp(value.toInt(), 1, 10000000);
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.enabled")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxEnabled = value.toBool();
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.trigger")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxTrigger = static_cast<AuxTriggerMode>(std::clamp(value.toInt(), 0, 2));
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.count")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxCount = std::clamp(value.toInt(), 0, 1000000);
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.interval")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxInterval = safeParticleFloat(value, 0.0f, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.lifeScale")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxLifeScale = safeParticleFloat(value, 1.0f, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.sizeScale")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxSizeScale = safeParticleFloat(value, 1.0f, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.opacityScale")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxOpacityScale = safeParticleFloat(value, 1.0f, 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.aux.velocityScale")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.auxVelocityScale = safeParticleFloat(value, 1.0f, 0.0f, 1000000.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.colorStart") ||
        propertyPath == QStringLiteral("particle.particle.colorStart")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            const QColor color = value.value<QColor>();
            if (color.isValid()) params.colorStart = color;
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.colorMid")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            const QColor color = value.value<QColor>();
            if (color.isValid()) params.colorMid = color;
        });
    }
    if (propertyPath == QStringLiteral("particle.particle.colorMidPosition")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            params.colorMidPosition = safeParticleFloat(value, 0.5f, 0.0f, 1.0f);
        });
    }
    if (propertyPath == QStringLiteral("particle.emitter.colorEnd") ||
        propertyPath == QStringLiteral("particle.particle.colorEnd")) {
        return applyPrimaryEmitterValue([&](EmitterParams& params) {
            const QColor color = value.value<QColor>();
            if (color.isValid()) params.colorEnd = color;
        });
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

// ==================== Factory Functions ====================

SharedPtr<ArtifactParticleLayer> createParticleLayer()
{
    return ArtifactCore::makeShared<ArtifactParticleLayer>();
}

ArtifactCore::ParticleRenderData applyParticleRenderLOD(
    const ArtifactCore::ParticleRenderData& source,
    float screenScale)
{
    if (source.particles.size() < 256 || !std::isfinite(screenScale)) return source;
    const float safeScreenScale = std::clamp(screenScale, 0.0f, 1000000.0f);
    if (safeScreenScale >= 0.75f) return source;
    const float keepRatio = std::clamp(safeScreenScale / 0.75f, 0.125f, 1.0f);
    const std::size_t targetCount = std::max<std::size_t>(
        64, static_cast<std::size_t>(std::ceil(source.particles.size() * keepRatio)));
    if (targetCount >= source.particles.size()) return source;
    ArtifactCore::ParticleRenderData reduced;
    reduced.frameNumber = source.frameNumber;
    reduced.options = source.options;
    reduced.particles.reserve(targetCount);
    const std::size_t stride = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::ceil(static_cast<float>(source.particles.size()) /
                                           static_cast<float>(targetCount))));
    for (std::size_t i = 0; i < source.particles.size() && reduced.particles.size() < targetCount; i += stride) {
        reduced.particles.push_back(source.particles[i]);
    }
    return reduced;
}

SharedPtr<ArtifactParticleLayer> createParticleLayer(const QString& preset)
{
    auto layer = ArtifactCore::makeShared<ArtifactParticleLayer>();
    layer->loadPreset(preset);
    return layer;
}

SharedPtr<ArtifactParticle3DLayer> createParticle3DLayer()
{
    return ArtifactCore::makeShared<ArtifactParticle3DLayer>();
}

SharedPtr<ArtifactParticle3DLayer> createParticle3DLayer(const QString& preset)
{
    auto layer = ArtifactCore::makeShared<ArtifactParticle3DLayer>();
    layer->loadPreset(preset);
    return layer;
}

ArtifactParticleDebugLayer::ArtifactParticleDebugLayer() = default;
ArtifactParticleDebugLayer::~ArtifactParticleDebugLayer() = default;

void ArtifactParticleDebugLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer || !particleSystem()) {
        return;
    }

    const int64_t frameNumber = currentFrame();
    const bool rendererReady = renderer->isInitialized();
    float fps = 30.0f;
    if (auto comp = static_cast<ArtifactAbstractComposition*>(composition())) {
        fps = safeParticleFps(comp->frameRate().framerate());
    }
    particleSystem()->goToFrame(std::max<int64_t>(1, frameNumber), fps);

    if (rendererReady) {
        const auto sourceData = particleSystem()->captureRenderData();
        auto coreData = toCoreParticleRenderData(sourceData);
        coreData.options = coreRenderOptionsFromSettings(
            particleSystem()->renderSettings());
        const QTransform globalTransform = getGlobalTransform();
        const float screenScale = std::max(std::hypot(globalTransform.m11(), globalTransform.m21()),
                                           std::hypot(globalTransform.m12(), globalTransform.m22()));
        const auto lodData = applyParticleRenderLOD(
            std::move(coreData), screenScale);
        if (!lodData.particles.empty()) {
            ArtifactCore::ParticleRenderData renderData =
                transformParticleRenderData(lodData, globalTransform, opacity());
            boostDebugParticleRenderData(renderData);
            renderer->drawParticles(renderData);
        }
        return;
    }

    const QRectF bounds = localBounds();
    const int fallbackWidth = safeParticleDimension(std::ceil(bounds.width()));
    const int fallbackHeight = safeParticleDimension(std::ceil(bounds.height()));
    QImage fallbackFrame =
        renderFrame(fallbackWidth, fallbackHeight, safeParticleFrameTime(frameNumber, fps));
    if (fallbackFrame.isNull()) {
        return;
    }
    renderer->drawSprite(0.0f,
                         0.0f,
                         static_cast<float>(fallbackFrame.width()),
                         static_cast<float>(fallbackFrame.height()),
                         fallbackFrame,
                         opacity());
}

SharedPtr<ArtifactParticleDebugLayer> createParticleDebugLayer()
{
    return ArtifactCore::makeShared<ArtifactParticleDebugLayer>();
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactParticleLayer)
