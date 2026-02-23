module;
#include <QObject>
#include <QImage>
#include <QJsonObject>
#include <memory>
#include <wobjectdefs.h>

export module Artifact.Layer.Particle;

import std;
import Artifact.Layer.Abstract;
import Artifact.Generator.Particle;
import Animation.Transform2D;
import Animation.Transform3D;
import Size;
import Utils.Id;
import Utils.String.UniString;

export namespace Artifact {

/**
 * @brief Particle layer - a layer that renders particle systems
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
    void draw() override;
    QJsonObject toJson() const override;
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
    
    // Effector management
    void addForceEffector(const QVector3D& force);
    void addVortexEffector(const QVector3D& position, float radius, float angularVelocity);
    void addTurbulenceEffector(float frequency, float amplitude);
    void addAttractorEffector(const QVector3D& position, float radius, float strength);
    void addWindEffector(const QVector3D& direction, float strength);
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
    void goToFrame(int64_t frameNumber) override;
    
    // Render output
    QImage renderFrame(int width, int height, float time);
    void renderToImage(QImage& target, float time);
    
    // Cached rendering
    bool getCachedFrame(int64_t frame, QImage& out);
    void clearFrameCache();
    
    // Presets
    void loadPreset(const QString& presetName);
    QStringList availablePresets() const;
    
    // Serialization
    void applyPropertiesFromJson(const QJsonObject& obj);
    
signals:
    void particleSystemChanged() W_SIGNAL(particleSystemChanged);
    void emitterAdded(int index) W_SIGNAL(emitterAdded, index);
    void emitterRemoved(int index) W_SIGNAL(emitterRemoved, index);
    void playbackStateChanged(bool playing) W_SIGNAL(playbackStateChanged, playing);
    void frameRendered(int64_t frame) W_SIGNAL(frameRendered, frame);
};

// Factory function for creating particle layers
std::shared_ptr<ArtifactParticleLayer> createParticleLayer();
std::shared_ptr<ArtifactParticleLayer> createParticleLayer(const QString& preset);

} // namespace Artifact