module;
#include <QJsonObject>
#include <QVariant>
#include <QRectF>
#include <QString>
#include <QUuid>

export module Artifact.Layer.SpatialAudio;

import Artifact.Layer.Abstract;
import Memory.SharedPtr;
import Audio.Spatial.Params;

export namespace Artifact {

class ArtifactSpatialAudioLayer : public ArtifactAbstractLayer {
public:
    ArtifactSpatialAudioLayer();
    ~ArtifactSpatialAudioLayer() override;

    void draw(ArtifactIRenderer* renderer) override;
    UniString className() const override;
    bool is3D() const override;
    bool hasAudio() const override;
    bool hasVideo() const override;
    QRectF localBounds() const override;

    bool getAudio(ArtifactCore::AudioSegment& outSegment, const FramePosition& start,
                  int frameCount, int sampleRate) override;

    ArtifactCore::Audio::Spatial::SpatialParams spatialParams() const;
    void setSpatialParams(const ArtifactCore::Audio::Spatial::SpatialParams& params);

    void setSourcePath(const QString& path);
    QString sourcePath() const;
    bool loadFromPath(const QString& path);
    QString objectId() const;
    float gain() const;
    bool isMuted() const;
    bool isEnabled() const;
    void setGain(float value);
    void setMuted(bool value);
    void setEnabled(bool value);

    QJsonObject toJson() const override;
    void fromJsonProperties(const QJsonObject& obj) override;

    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

private:
    class Impl;
    Impl* impl_;
};

using ArtifactSpatialAudioLayerPtr = ArtifactCore::SharedPtr<ArtifactSpatialAudioLayer>;

} // namespace Artifact
