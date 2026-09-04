module;
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QVector3D>
#include <QMatrix4x4>
#include <QVariant>
#include <QUuid>
#include <cmath>
#include <algorithm>

module Artifact.Layer.SpatialAudio;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Audio.Segment;
import Audio.Spatial.Params;
import Audio.Spatial.Math;
import Audio.Spatial.Renderer;
import Property.Group;
import Property.Types;
import Memory.SharedPtr;
import Time.Position;

namespace Artifact {

class ArtifactSpatialAudioLayer::Impl {
public:
    ArtifactCore::Audio::Spatial::SpatialParams spatial_;
    ArtifactCore::Audio::Spatial::SpatialRenderer renderer_;
    QString sourcePath_;
    ArtifactCore::AudioSegment cachedSegment_;
    bool cacheValid_ = false;
    float lastSampleRate_ = 48000.0f;
    QString objectId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    float gain_ = 1.0f;
    bool muted_ = false;
    bool enabled_ = true;
};

ArtifactSpatialAudioLayer::ArtifactSpatialAudioLayer() : impl_(new Impl()) {
    setIs3D(true);
    setLayerName("Spatial Audio 1");
}

ArtifactSpatialAudioLayer::~ArtifactSpatialAudioLayer() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactSpatialAudioLayer::draw(ArtifactIRenderer* renderer) {
    (void)renderer;
}

UniString ArtifactSpatialAudioLayer::className() const { return "ArtifactSpatialAudioLayer"; }
bool ArtifactSpatialAudioLayer::is3D() const { return true; }
bool ArtifactSpatialAudioLayer::hasAudio() const { return true; }
bool ArtifactSpatialAudioLayer::hasVideo() const { return false; }
QRectF ArtifactSpatialAudioLayer::localBounds() const { return QRectF(-25, -25, 50, 50); }

bool ArtifactSpatialAudioLayer::getAudio(ArtifactCore::AudioSegment& outSegment, const FramePosition& start,
                                        int frameCount, int sampleRate) {
    if (frameCount <= 0) return false;
    if (!impl_->enabled_ || impl_->muted_) {
        outSegment.channelData = QVector<QVector<float>>(2, QVector<float>(frameCount, 0.0f));
        outSegment.sampleRate = sampleRate;
        outSegment.layout = ArtifactCore::AudioChannelLayout::Stereo;
        return true;
    }
    if (!impl_->cacheValid_ || impl_->cachedSegment_.channelData.isEmpty()) {
        if (impl_->sourcePath_.isEmpty()) {
            outSegment.channelData = QVector<QVector<float>>(2, QVector<float>(frameCount, 0.0f));
            outSegment.sampleRate = sampleRate;
            outSegment.layout = ArtifactCore::AudioChannelLayout::Stereo;
            return true;
        }
        return false;
    }

    ArtifactCore::AudioSegment in = impl_->cachedSegment_;
    if (in.channelData.isEmpty()) return false;

    int avail = in.frameCount();
    if (avail < frameCount) frameCount = avail;

    ArtifactCore::AudioSegment clipped;
    clipped.sampleRate = sampleRate;
    clipped.layout = in.layout;
    clipped.startFrame = start.value();
    clipped.channelData.resize(in.channelData.size());
    for (int c = 0; c < (int)in.channelData.size(); ++c) {
        clipped.channelData[c] = in.channelData[c].mid(0, frameCount);
    }

    QMatrix4x4 g = getGlobalTransform4x4();
    QVector3D pos = g.map(QVector3D(0,0,0));
    ArtifactCore::Audio::Spatial::Vec3 src{pos.x(), pos.y(), pos.z()};

    QVector3D listenerPos3{0,0,0};
    QQuaternion listenerRotQ;
    if (auto comp = composition()) {
        ArtifactCore::SharedPtr<ArtifactAbstractLayer> activeCam;
        float bestPriority = -1e30f;
        for (auto& l : comp->allLayerRef()) {
            if (!l || !l->isVisible()) continue;
            auto cam = ArtifactCore::dynamicPointerCast<ArtifactCameraLayer>(l);
            if (!cam || !cam->isActiveCamera()) continue;
            float pr = cam->cameraPriority();
            if (!activeCam || pr > bestPriority) { activeCam = l; bestPriority = pr; }
        }
        if (activeCam) {
            QMatrix4x4 cg = activeCam->getGlobalTransform4x4();
            QVector3D cp = cg.map(QVector3D(0,0,0));
            listenerPos3 = cp;
            QQuaternion q = QQuaternion::fromRotationMatrix(cg.normalMatrix());
            listenerRotQ = q;
        }
    }

    ArtifactCore::Audio::Spatial::Vec3 listenerPos{listenerPos3.x(), listenerPos3.y(), listenerPos3.z()};
    ArtifactCore::Audio::Spatial::Quat listenerRot{listenerRotQ.x(), listenerRotQ.y(), listenerRotQ.z(), listenerRotQ.scalar()};

    impl_->renderer_.setSampleRate((float)sampleRate);
    impl_->renderer_.publishParams(impl_->spatial_);
    impl_->renderer_.processBlock(clipped, outSegment, frameCount, src, listenerPos, listenerRot);
    for (auto& channel : outSegment.channelData) {
        for (float& sample : channel) sample *= impl_->gain_;
    }
    return true;
}

ArtifactCore::Audio::Spatial::SpatialParams ArtifactSpatialAudioLayer::spatialParams() const {
    return impl_->spatial_;
}

void ArtifactSpatialAudioLayer::setSpatialParams(const ArtifactCore::Audio::Spatial::SpatialParams& params) {
    impl_->spatial_ = ArtifactCore::Audio::Spatial::sanitizedSpatialParams(params);
    impl_->renderer_.publishParams(impl_->spatial_);
    changed();
}

void ArtifactSpatialAudioLayer::setSourcePath(const QString& path) {
    impl_->sourcePath_ = path.trimmed();
    impl_->cacheValid_ = false;
    changed();
}

QString ArtifactSpatialAudioLayer::sourcePath() const { return impl_->sourcePath_; }
QString ArtifactSpatialAudioLayer::objectId() const { return impl_->objectId_; }
float ArtifactSpatialAudioLayer::gain() const { return impl_->gain_; }
bool ArtifactSpatialAudioLayer::isMuted() const { return impl_->muted_; }
bool ArtifactSpatialAudioLayer::isEnabled() const { return impl_->enabled_; }
void ArtifactSpatialAudioLayer::setGain(float value) { impl_->gain_ = std::clamp(std::isfinite(value) ? value : 1.0f, 0.0f, 4.0f); changed(); }
void ArtifactSpatialAudioLayer::setMuted(bool value) { impl_->muted_ = value; changed(); }
void ArtifactSpatialAudioLayer::setEnabled(bool value) { impl_->enabled_ = value; changed(); }

bool ArtifactSpatialAudioLayer::loadFromPath(const QString& path) {
    setSourcePath(path);
    return !path.isEmpty();
}

QJsonObject ArtifactSpatialAudioLayer::toJson() const {
    QJsonObject obj = ArtifactAbstractLayer::toJson();
    obj[QStringLiteral("type")] = static_cast<int>(LayerType::SpatialAudio);
    obj[QStringLiteral("spatial.sourcePath")] = impl_->sourcePath_;
    obj[QStringLiteral("spatial.objectId")] = impl_->objectId_;
    obj[QStringLiteral("spatial.gain")] = impl_->gain_;
    obj[QStringLiteral("spatial.muted")] = impl_->muted_;
    obj[QStringLiteral("spatial.enabled")] = impl_->enabled_;
    obj[QStringLiteral("spatial.minDistance")] = impl_->spatial_.minDistance;
    obj[QStringLiteral("spatial.maxDistance")] = impl_->spatial_.maxDistance;
    obj[QStringLiteral("spatial.rolloff")] = impl_->spatial_.rolloff;
    obj[QStringLiteral("spatial.spread")] = impl_->spatial_.spread;
    obj[QStringLiteral("spatial.distanceModel")] = static_cast<int>(impl_->spatial_.model);
    obj[QStringLiteral("spatial.coneInner")] = impl_->spatial_.coneInnerAngle;
    obj[QStringLiteral("spatial.coneOuter")] = impl_->spatial_.coneOuterAngle;
    obj[QStringLiteral("spatial.coneOuterGain")] = impl_->spatial_.coneOuterGain;
    obj[QStringLiteral("spatial.doppler")] = impl_->spatial_.doppler;
    return obj;
}

void ArtifactSpatialAudioLayer::fromJsonProperties(const QJsonObject& obj) {
    ArtifactAbstractLayer::fromJsonProperties(obj);
    if (obj.contains(QStringLiteral("spatial.sourcePath"))) impl_->sourcePath_ = obj.value(QStringLiteral("spatial.sourcePath")).toString();
    if (obj.contains(QStringLiteral("spatial.objectId"))) impl_->objectId_ = obj.value(QStringLiteral("spatial.objectId")).toString();
    if (impl_->objectId_.isEmpty()) impl_->objectId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (obj.contains(QStringLiteral("spatial.gain"))) impl_->gain_ = (float)obj.value(QStringLiteral("spatial.gain")).toDouble(1.0);
    if (obj.contains(QStringLiteral("spatial.muted"))) impl_->muted_ = obj.value(QStringLiteral("spatial.muted")).toBool(false);
    if (obj.contains(QStringLiteral("spatial.enabled"))) impl_->enabled_ = obj.value(QStringLiteral("spatial.enabled")).toBool(true);
    if (obj.contains(QStringLiteral("spatial.minDistance"))) impl_->spatial_.minDistance = (float)obj.value(QStringLiteral("spatial.minDistance")).toDouble(1.0);
    if (obj.contains(QStringLiteral("spatial.maxDistance"))) impl_->spatial_.maxDistance = (float)obj.value(QStringLiteral("spatial.maxDistance")).toDouble(100.0);
    if (obj.contains(QStringLiteral("spatial.rolloff"))) impl_->spatial_.rolloff = (float)obj.value(QStringLiteral("spatial.rolloff")).toDouble(1.0);
    if (obj.contains(QStringLiteral("spatial.spread"))) impl_->spatial_.spread = (float)obj.value(QStringLiteral("spatial.spread")).toDouble(0.0);
    if (obj.contains(QStringLiteral("spatial.distanceModel"))) impl_->spatial_.model = static_cast<ArtifactCore::Audio::Spatial::DistanceModel>(obj.value(QStringLiteral("spatial.distanceModel")).toInt(1));
    if (obj.contains(QStringLiteral("spatial.coneInner"))) impl_->spatial_.coneInnerAngle = (float)obj.value(QStringLiteral("spatial.coneInner")).toDouble(360.0);
    if (obj.contains(QStringLiteral("spatial.coneOuter"))) impl_->spatial_.coneOuterAngle = (float)obj.value(QStringLiteral("spatial.coneOuter")).toDouble(360.0);
    if (obj.contains(QStringLiteral("spatial.coneOuterGain"))) impl_->spatial_.coneOuterGain = (float)obj.value(QStringLiteral("spatial.coneOuterGain")).toDouble(0.0);
    if (obj.contains(QStringLiteral("spatial.doppler"))) impl_->spatial_.doppler = obj.value(QStringLiteral("spatial.doppler")).toBool(false);
    if (!std::isfinite(impl_->gain_)) impl_->gain_ = 1.0f;
    impl_->gain_ = std::clamp(impl_->gain_, 0.0f, 4.0f);
    impl_->spatial_ = ArtifactCore::Audio::Spatial::sanitizedSpatialParams(impl_->spatial_);
}

std::vector<ArtifactCore::PropertyGroup> ArtifactSpatialAudioLayer::getLayerPropertyGroups() const {
    std::vector<ArtifactCore::PropertyGroup> groups;
    ArtifactCore::PropertyGroup g;
    g.setName("Spatial Audio");
    auto p = [&](QString path, ArtifactCore::PropertyType t, QVariant v, int prio) {
        auto prop = persistentLayerProperty(path, t, v, prio);
        prop->setAnimatable(true);
        return prop;
    };
    g.addProperty(p(QStringLiteral("spatial.minDistance"), ArtifactCore::PropertyType::Float, impl_->spatial_.minDistance, -120));
    g.addProperty(p(QStringLiteral("spatial.maxDistance"), ArtifactCore::PropertyType::Float, impl_->spatial_.maxDistance, -119));
    g.addProperty(p(QStringLiteral("spatial.rolloff"), ArtifactCore::PropertyType::Float, impl_->spatial_.rolloff, -118));
    g.addProperty(p(QStringLiteral("spatial.spread"), ArtifactCore::PropertyType::Float, impl_->spatial_.spread, -117));
    g.addProperty(p(QStringLiteral("spatial.distanceModel"), ArtifactCore::PropertyType::Integer, (int)impl_->spatial_.model, -116));
    g.addProperty(p(QStringLiteral("spatial.coneInner"), ArtifactCore::PropertyType::Float, impl_->spatial_.coneInnerAngle, -115));
    g.addProperty(p(QStringLiteral("spatial.coneOuter"), ArtifactCore::PropertyType::Float, impl_->spatial_.coneOuterAngle, -114));
    g.addProperty(p(QStringLiteral("spatial.coneOuterGain"), ArtifactCore::PropertyType::Float, impl_->spatial_.coneOuterGain, -113));
    g.addProperty(p(QStringLiteral("spatial.gain"), ArtifactCore::PropertyType::Float, impl_->gain_, -112));
    g.addProperty(p(QStringLiteral("spatial.muted"), ArtifactCore::PropertyType::Bool, impl_->muted_, -111));
    g.addProperty(p(QStringLiteral("spatial.enabled"), ArtifactCore::PropertyType::Bool, impl_->enabled_, -110));
    g.addProperty(p(QStringLiteral("spatial.sourcePath"), ArtifactCore::PropertyType::String, impl_->sourcePath_, -109));
    groups.push_back(std::move(g));
    return groups;
}

bool ArtifactSpatialAudioLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value) {
    auto sp = impl_->spatial_;
    if (propertyPath == QStringLiteral("spatial.minDistance")) sp.minDistance = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.maxDistance")) sp.maxDistance = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.rolloff")) sp.rolloff = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.spread")) sp.spread = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.gain")) { setGain(value.toFloat()); return true; }
    else if (propertyPath == QStringLiteral("spatial.muted")) { setMuted(value.toBool()); return true; }
    else if (propertyPath == QStringLiteral("spatial.enabled")) { setEnabled(value.toBool()); return true; }
    else if (propertyPath == QStringLiteral("spatial.distanceModel")) sp.model = static_cast<ArtifactCore::Audio::Spatial::DistanceModel>(value.toInt());
    else if (propertyPath == QStringLiteral("spatial.coneInner")) sp.coneInnerAngle = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.coneOuter")) sp.coneOuterAngle = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.coneOuterGain")) sp.coneOuterGain = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.sourcePath")) { setSourcePath(value.toString()); return true; }
    else return false;
    setSpatialParams(sp);
    return true;
}

} // namespace Artifact
