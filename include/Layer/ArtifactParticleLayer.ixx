module;
#include <memory>
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QObject>
#include <QImage>
#include <QString>
#include <QJsonObject>
#include <QVariant>
export module Artifact.Layer.Particle;




import Artifact.Layer.Abstract;
import Memory.SharedPtr;
import Artifact.Composition.PlaybackController;
import Playback.State;
import Artifact.Generator.Particle;
import Animation.Transform2D;
import Animation.Transform3D;
import Size;
import Utils.Id;
import Utils.String.UniString;

export namespace Artifact {

/**
 * @brief 2D particle layer rendered in composition space
 */
class ArtifactParticleLayer : public ArtifactAbstractLayer {
    W_OBJECT(ArtifactParticleLayer)
private:
    class Impl;
    Impl* impl_;
    
public:
    ArtifactParticleLayer();
    virtual ~ArtifactParticleLayer();
    
    // Layer interface
    void draw(ArtifactIRenderer* renderer) override;
    QRectF localBounds() const override;
    bool isParticleLayer() const override { return true; }
    QString debugState() const;
    QJsonObject toJson() const override;
    void fromJsonProperties(const QJsonObject& obj) override;
    static ArtifactAbstractLayerPtr fromJson(const QJsonObject& obj);
    
    // Layer identification
    bool isNullLayer() const override { return false; }
    bool hasVideo() const override { return true; }
    bool hasAudio() const override { return false; }
    
    // Particle system access
    ParticleSystem* particleSystem();
    const ParticleSystem* particleSystem() const;
    
    // Particle system creation/reset
    void createParticleSystem();
    void resetParticleSystem();
    
    // Particle emitter management
    ParticleEmitter* addEmitter();
    ParticleEmitter* addEmitter(const EmitterParams& params);
    void removeEmitter(int index);
    void clearEmitters();
    int emitterCount() const;
    QVector3D emitterPosition() const;
    bool setEmitterPosition(const QVector3D& position);
    QVector3D emitterDirection() const;
    bool setEmitterDirection(const QVector3D& direction);
    
    // Effector management
    void addForceEffector(const QVector3D& force);
    void addVortexEffector(const QVector3D& position, float radius, float angularVelocity);
    void addTurbulenceEffector(float frequency, float amplitude);
    void addAttractorEffector(const QVector3D& position, float radius, float strength);
    void addWindEffector(const QVector3D& direction, float strength);
    int effectorCount() const;
    int selectedEffectorIndex() const;
    bool setEffectorEnabled(int index, bool enabled);
    float effectorStrength(int index) const;
    bool setEffectorStrength(int index, float strength);
    QVector3D effectorPosition(int index) const;
    bool setEffectorPosition(int index, const QVector3D& position);
    float effectorInfluenceRadius(int index) const;
    bool setEffectorInfluenceRadius(int index, float radius);
    bool moveEffector(int fromIndex, int toIndex);
    void removeEffector(int index);
    void clearEffectors();
    
    // Rendering settings
    ParticleRenderSettings& renderSettings();
    const ParticleRenderSettings& renderSettings() const;
    void setRenderSettings(const ParticleRenderSettings& settings);
    
    // Blend mode for particles
    void setParticleBlendMode(ParticleBlendMode mode);
    ParticleBlendMode particleBlendMode() const;
    
    // Playback control
    void play();
    void pause();
    void stop();
    void reset();
    bool isPlaying() const;
    
    // Time control
    void setTimeScale(float scale);
    float timeScale() const;
    
    // Pre-warm the particle system
    void preWarm(float duration);
    
    // Layer time handling
    void goToFrame(int64_t frameNumber);
    
    // Render output
    QImage renderFrame(int width, int height, float time);
    void renderToImage(QImage& target, float time);
    void renderToImage(QImage& target, int64_t frameNumber);
    
    // Cached rendering
    bool getCachedFrame(int64_t frame, QImage& out);
    void clearFrameCache();
    
    // Presets
    void loadPreset(const QString& presetName);
    QStringList availablePresets() const;
    
    // Serialization
    void applyPropertiesFromJson(const QJsonObject& obj);
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString &propertyPath, const QVariant &value) override;
    
signals:
    void particleSystemChanged() W_SIGNAL(particleSystemChanged);
    void emitterAdded(int index) W_SIGNAL(emitterAdded, index);
    void emitterRemoved(int index) W_SIGNAL(emitterRemoved, index);
    void playbackStateChanged(PlaybackState state) W_SIGNAL(playbackStateChanged, state);
    void frameRendered(int64_t frame) W_SIGNAL(frameRendered, frame);
};

/**
 * @brief 3D particle layer rendered through the composition camera/depth path
 *
 * Simulation and GPU particle rendering are shared with ArtifactParticleLayer,
 * while layer identity and persistence remain explicitly 3D.
 */
class ArtifactParticle3DLayer : public ArtifactParticleLayer {
public:
    ArtifactParticle3DLayer();
    ~ArtifactParticle3DLayer() override;

    QJsonObject toJson() const override;
    void fromJsonProperties(const QJsonObject& obj) override;
};

/**
 * @brief Debug particle layer - renders the same particle system with stronger
 * visibility so visibility issues can be inspected independently from the
 * production particle layer.
 */
class ArtifactParticleDebugLayer : public ArtifactParticleLayer {
public:
    ArtifactParticleDebugLayer();
    ~ArtifactParticleDebugLayer() override;

    void draw(ArtifactIRenderer* renderer) override;
};

// Factory function for creating particle layers
SharedPtr<ArtifactParticleLayer> createParticleLayer();
SharedPtr<ArtifactParticleLayer> createParticleLayer(const QString& preset);
SharedPtr<ArtifactParticle3DLayer> createParticle3DLayer();
SharedPtr<ArtifactParticle3DLayer> createParticle3DLayer(const QString& preset);
SharedPtr<ArtifactParticleDebugLayer> createParticleDebugLayer();

} // namespace Artifact
