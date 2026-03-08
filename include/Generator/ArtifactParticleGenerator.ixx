module;
#include <QObject>
#include <QVector2D>
#include <QVector3D>
#include <QColor>
#include <QImage>
#include <QRandomGenerator>
#include <memory>
#include <vector>
#include <wobjectdefs.h>

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
export module Artifact.Generator.Particle;





W_REGISTER_ARGTYPE(QVector2D)
W_REGISTER_ARGTYPE(QVector3D)

export namespace Artifact {

/**
 * @brief Single particle data
 */
struct Particle {
    // Position
    QVector3D position;
    QVector3D prevPosition;
    
    // Velocity
    QVector3D velocity;
    QVector3D acceleration;
    
    // Rotation
    float rotation = 0.0f;
    float rotationSpeed = 0.0f;
    
    // Scale
    float scale = 1.0f;
    float scaleStart = 1.0f;
    float scaleEnd = 1.0f;
    
    // Color
    QColor color;
    QColor colorStart;
    QColor colorEnd;
    
    // Opacity
    float opacity = 1.0f;
    float opacityStart = 1.0f;
    float opacityEnd = 0.0f;
    
    // Lifetime
    float life = 1.0f;          // Current life (1.0 = just born, 0.0 = dead)
    float maxLife = 1.0f;       // Total lifetime in seconds
    float age = 0.0f;           // Time alive
    
    // Index
    int id = 0;
    int emitterIndex = 0;
    
    // Custom data
    float customData[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    // State
    bool alive = true;
    bool active = true;
};

/**
 * @brief Emitter shape types
 */
enum class EmitterShape {
    Point,          // 点エミッター
    Sphere,         // 球体
    Box,            // ボックス
    Circle,         // 円（2D）
    Rectangle,      // 矩形（2D）
    Line,           // 線
    Mesh,           // メッシュ表面
    Surface         // サーフェス
};

/**
 * @brief Emission mode
 */
enum class EmissionMode {
    Continuous,     // 連続放出
    Burst,          // 一斉放出
    Triggered       // トリガー放出
};

/**
 * @brief Particle blend mode
 */
enum class ParticleBlendMode {
    Additive,       // 加算
    Subtractive,    // 減算
    Normal,         // 通常
    Screen,         // スクリーン
    Multiply        // 乗算
};

/**
 * @brief Particle emitter parameters
 */
class EmitterParams {
public:
    // Shape
    EmitterShape shape = EmitterShape::Point;
    QVector3D position;
    QVector3D rotation;
    QVector3D scale = QVector3D(1, 1, 1);
    
    // Dimensions
    float radius = 100.0f;          // Sphere/Circle radius
    float width = 200.0f;           // Box/Rectangle width
    float height = 200.0f;          // Box/Rectangle height
    float depth = 200.0f;           // Box depth
    float lineLength = 200.0f;      // Line length
    
    // Emission
    EmissionMode mode = EmissionMode::Continuous;
    float rate = 10.0f;             // Particles per second
    int burstCount = 100;           // Burst particle count
    float burstInterval = 1.0f;     // Interval between bursts
    
    // Initial properties
    float lifeMin = 1.0f;
    float lifeMax = 2.0f;
    float speedMin = 50.0f;
    float speedMax = 100.0f;
    float directionSpread = 360.0f; // Direction random spread (degrees)
    QVector3D direction = QVector3D(0, -1, 0);  // Base direction
    
    // Rotation
    float rotationMin = 0.0f;
    float rotationMax = 360.0f;
    float rotationSpeedMin = 0.0f;
    float rotationSpeedMax = 90.0f;
    
    // Scale
    float scaleMin = 1.0f;
    float scaleMax = 1.0f;
    float scaleEndMin = 0.0f;
    float scaleEndMax = 0.0f;
    
    // Color
    QColor colorStart = QColor(255, 255, 255, 255);
    QColor colorEnd = QColor(255, 255, 255, 0);
    float colorVariation = 0.0f;
    
    // Opacity
    float opacityMin = 1.0f;
    float opacityMax = 1.0f;
    float opacityEndMin = 0.0f;
    float opacityEndMax = 0.0f;
    
    // Physics
    float mass = 1.0f;
    float drag = 0.0f;
    
    // Advanced
    bool inheritVelocity = false;
    bool worldSpace = true;
    bool preWarm = false;
    int maxParticles = 1000;
    
    // Texture
    QString texturePath;
    int textureRows = 1;
    int textureCols = 1;
    bool randomFrame = false;
    int startFrame = 0;
    int frameCount = 1;
    float frameRate = 30.0f;
};

/**
 * @brief Particle effector types
 */
enum class EffectorType {
    Force,          // 力（重力など）
    Vortex,         // 渦
    Turbulence,     // 乱流
    Attractor,      // アトラクター
    Repeller,       // リペラー
    Drag,           // 抵抗
    Wind,           // 風
    Noise,          // ノイズ
    Collision,      // コリジョン
    Kill            // キルゾーン
};

/**
 * @brief Base particle effector
 */
class ParticleEffector {
public:
    EffectorType type;
    bool enabled = true;
    float strength = 1.0f;
    QVector3D position;
    QVector3D direction;
    
    virtual ~ParticleEffector() = default;
    virtual void apply(Particle& particle, float deltaTime) = 0;
    virtual void apply(std::vector<Particle>& particles, float deltaTime);
};

/**
 * @brief Force effector (gravity, directional force)
 */
class ForceEffector : public ParticleEffector {
public:
    QVector3D force = QVector3D(0, -100.0f, 0);  // Gravity by default
    
    ForceEffector() { type = EffectorType::Force; }
    void apply(Particle& particle, float deltaTime) override;
};

/**
 * @brief Vortex effector
 */
class VortexEffector : public ParticleEffector {
public:
    float radius = 100.0f;
    float angularVelocity = 180.0f;    // degrees per second
    float tightness = 1.0f;
    
    VortexEffector() { type = EffectorType::Vortex; }
    void apply(Particle& particle, float deltaTime) override;
};

/**
 * @brief Turbulence effector
 */
class TurbulenceEffector : public ParticleEffector {
public:
    float frequency = 1.0f;
    float amplitude = 50.0f;
    float octaves = 3;
    float evolution = 0.0f;
    int seed = 0;
    
    TurbulenceEffector() { type = EffectorType::Turbulence; }
    void apply(Particle& particle, float deltaTime) override;
};

/**
 * @brief Attractor effector
 */
class AttractorEffector : public ParticleEffector {
public:
    float radius = 200.0f;
    float falloff = 1.0f;     // 1 = linear, 2 = quadratic
    bool killOnReach = false;
    float killRadius = 10.0f;
    
    AttractorEffector() { type = EffectorType::Attractor; }
    void apply(Particle& particle, float deltaTime) override;
};

/**
 * @brief Repeller effector
 */
class RepellerEffector : public ParticleEffector {
public:
    float radius = 100.0f;
    float falloff = 1.0f;
    
    RepellerEffector() { type = EffectorType::Repeller; }
    void apply(Particle& particle, float deltaTime) override;
};

/**
 * @brief Wind effector
 */
class WindEffector : public ParticleEffector {
public:
    QVector3D windDirection = QVector3D(1, 0, 0);
    float windStrength = 50.0f;
    float turbulence = 10.0f;
    float turbulenceFrequency = 0.5f;
    float evolution = 0.0f;
    
    WindEffector() { type = EffectorType::Wind; }
    void apply(Particle& particle, float deltaTime) override;
};

/**
 * @brief Kill zone effector
 */
class KillZoneEffector : public ParticleEffector {
public:
    enum class ZoneType {
        Box,
        Sphere,
        Plane
    };
    
    ZoneType zoneType = ZoneType::Sphere;
    QVector3D size = QVector3D(100, 100, 100);
    bool invert = false;    // Kill inside vs outside
    
    KillZoneEffector() { type = EffectorType::Kill; }
    void apply(Particle& particle, float deltaTime) override;
};

/**
 * @brief Particle emitter class
 */
class ParticleEmitter : public QObject {
    W_OBJECT(ParticleEmitter)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    EmitterParams params_;
    std::vector<Particle> particles_;
    std::vector<std::unique_ptr<ParticleEffector>> effectors_;
    
    int nextParticleId_ = 0;
    float emitAccumulator_ = 0.0f;
    float burstTimer_ = 0.0f;
    bool active_ = true;
    float time_ = 0.0f;
    
public:
    explicit ParticleEmitter(QObject* parent = nullptr);
    ~ParticleEmitter() override;
    
    // Parameters
    EmitterParams& params() { return params_; }
    const EmitterParams& params() const { return params_; }
    void setParams(const EmitterParams& p) { params_ = p; emit changed(); }
    
    // Active state
    bool active() const { return active_; }
    void setActive(bool a) { active_ = a; }
    
    // Particles
    const std::vector<Particle>& particles() const { return particles_; }
    int particleCount() const { return static_cast<int>(particles_.size()); }
    int maxParticles() const { return params_.maxParticles; }
    
    // Effectors
    void addEffector(std::unique_ptr<ParticleEffector> effector);
    void removeEffector(int index);
    void clearEffectors();
    const std::vector<std::unique_ptr<ParticleEffector>>& effectors() const { return effectors_; }
    
    // Simulation
    void update(float deltaTime);
    void emitParticles(int count);
    void emitBurst();
    
    // Clear
    void clear();
    
    // Pre-warm
    void preWarm(float duration, float stepSize = 0.016f);
    
signals:
    void changed() W_SIGNAL(changed);
    void particleEmitted(int count) W_SIGNAL(particleEmitted, count);
    void particleDied(int id) W_SIGNAL(particleDied, id);
    
private:
    void initializeParticle(Particle& p);
    QVector3D getEmissionPosition() const;
    QVector3D getEmissionDirection() const;
    void updateParticle(Particle& p, float deltaTime);
    void applyEffectors(Particle& p, float deltaTime);
    void removeDeadParticles();
};

/**
 * @brief Particle system renderer settings
 */
class ParticleRenderSettings {
public:
    ParticleBlendMode blendMode = ParticleBlendMode::Additive;
    
    // Billboard
    enum class BillboardMode {
        None,           // No billboarding
        ScreenAligned,  // Screen-aligned
        ViewPlane,      // View plane aligned
        VelocityAligned // Velocity direction
    };
    BillboardMode billboardMode = BillboardMode::ScreenAligned;
    
    // Sorting
    enum class SortMode {
        None,
        Distance,       // Back to front
        OldestFirst,
        YoungestFirst
    };
    SortMode sortMode = SortMode::Distance;
    
    // Rendering
    bool depthTest = true;
    bool depthWrite = false;
    bool softParticles = false;
    float softParticleDistance = 10.0f;
    
    // Stretched particles
    bool stretchEnabled = false;
    float stretchFactor = 1.0f;
    float minLength = 0.0f;
    
    // Trail
    bool trailEnabled = false;
    int trailLength = 10;
    float trailWidth = 1.0f;
    float trailFade = 0.5f;
};

/**
 * @brief Particle system - manages multiple emitters
 */
class ParticleSystem : public QObject {
    W_OBJECT(ParticleSystem)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    std::vector<std::unique_ptr<ParticleEmitter>> emitters_;
    ParticleRenderSettings renderSettings_;
    
    bool paused_ = false;
    float timeScale_ = 1.0f;
    float time_ = 0.0f;
    
public:
    explicit ParticleSystem(QObject* parent = nullptr);
    ~ParticleSystem() override;
    
    // Emitters
    ParticleEmitter* createEmitter();
    void removeEmitter(ParticleEmitter* emitter);
    void removeEmitter(int index);
    void clearEmitters();
    
    const std::vector<std::unique_ptr<ParticleEmitter>>& emitters() const { return emitters_; }
    int emitterCount() const { return static_cast<int>(emitters_.size()); }
    
    // Total particle count
    int totalParticleCount() const;
    
    // Render settings
    ParticleRenderSettings& renderSettings() { return renderSettings_; }
    const ParticleRenderSettings& renderSettings() const { return renderSettings_; }
    void setRenderSettings(const ParticleRenderSettings& s) { renderSettings_ = s; }
    
    // Simulation
    void update(float deltaTime);
    void setPaused(bool paused) { paused_ = paused; }
    bool isPaused() const { return paused_; }
    
    void setTimeScale(float scale) { timeScale_ = scale; }
    float timeScale() const { return timeScale_; }
    
    float time() const { return time_; }
    
    // Clear all
    void clear();
    
    // Pre-warm all emitters
    void preWarm(float duration);
    
    // Rendering
    void render(QPainter& painter, const QTransform& transform = QTransform());
    void render(QImage& target, const QTransform& transform = QTransform());
    
    // GPU rendering (for shader-based)
    void renderGPU(float* vertexBuffer, int maxVertices, int& vertexCount);
    
signals:
    void emitterAdded(ParticleEmitter* emitter) W_SIGNAL(emitterAdded, emitter);
    void emitterRemoved(ParticleEmitter* emitter) W_SIGNAL(emitterRemoved, emitter);
    void updated(float deltaTime) W_SIGNAL(updated, deltaTime);
};

/**
 * @brief Particle presets
 */
class ParticlePresets {
public:
    // Fire
    static EmitterParams fire();
    static EmitterParams campfire();
    static EmitterParams torch();
    
    // Smoke
    static EmitterParams smoke();
    static EmitterParams steam();
    static EmitterParams dust();
    
    // Water
    static EmitterParams rain();
    static EmitterParams splash();
    static EmitterParams fountain();
    
    // Explosion
    static EmitterParams explosion();
    static EmitterParams debris();
    static EmitterParams sparks();
    
    // Nature
    static EmitterParams leaves();
    static EmitterParams snow();
    static EmitterParams pollen();
    
    // Magic
    static EmitterParams magic();
    static EmitterParams sparkles();
    static EmitterParams energyField();
    
    // Confetti
    static EmitterParams confetti();
    
    // Bubbles
    static EmitterParams bubbles();
};

/**
 * @brief Particle manager - manages all particle systems in composition
 */
class ParticleManager : public QObject {
    W_OBJECT(ParticleManager)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
public:
    explicit ParticleManager(QObject* parent = nullptr);
    ~ParticleManager();
    
    // Create particle system
    ParticleSystem* createSystem(const QString& name);
    ParticleSystem* system(const QString& name) const;
    void removeSystem(const QString& name);
    void clearSystems();
    
    // Update all systems
    void update(float deltaTime);
    
    // Pre-warm all systems
    void preWarm(float duration);
    
    // List systems
    QStringList systemNames() const;
    
    // Pause/resume all
    void setAllPaused(bool paused);
    
signals:
    void systemCreated(const QString& name) W_SIGNAL(systemCreated, name);
    void systemRemoved(const QString& name) W_SIGNAL(systemRemoved, name);
};

} // namespace Artifact
