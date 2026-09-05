module;
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QVector3D>
#include <QVector>
#include <QList>
#include <QString>
#include <cstdint>
#include <utility>
#include <vector>
#include <QQuaternion>
#include <QMatrix4x4>
#include <QVariant>
#include <QUuid>
#include <cmath>
#include <algorithm>

module Artifact.Layer.SpatialAudio;

import Artifact.Layer.Abstract;
import Artifact.Layer.Audio;
import Artifact.Layer.Camera;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Audio.Segment;
import Audio.Spatial.Params;
import Audio.Spatial.Math;
import Audio.Spatial.Renderer;
import Property.Group;
import Property.Types;
import Memory.SharedPtr;
import Frame.Position;
import Frame.Rate;
import Time.Rational;
import Property.Abstract;

namespace Artifact {

class ArtifactSpatialAudioLayer::Impl {
public:
    ArtifactCore::Audio::Spatial::SpatialParams spatial_;
    ArtifactCore::Audio::Spatial::SpatialRenderer renderer_;
    QString sourcePath_;

    QString objectId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    float gain_ = 1.0f;
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
bool ArtifactSpatialAudioLayer::hasAudio() const { return ArtifactAudioLayer::hasAudio(); }
bool ArtifactSpatialAudioLayer::hasVideo() const { return false; }
QRectF ArtifactSpatialAudioLayer::localBounds() const { return QRectF(-25, -25, 50, 50); }

bool ArtifactSpatialAudioLayer::getAudio(ArtifactCore::AudioSegment& outSegment, const FramePosition& start,
                                        int frameCount, int sampleRate) {
    if (frameCount <= 0 || sampleRate <= 0) return false;
    if (!impl_->enabled_ || ArtifactAudioLayer::isMuted()) {
        impl_->renderer_.reset();
        return false;
    }
    // Reuse the normal audio source, clip timing and resampling path. Decoding
    // happens on load, never here; start is the requested composition frame.
    ArtifactCore::AudioSegment clipped;
    if (!ArtifactAudioLayer::getAudio(clipped, start, frameCount, sampleRate)) {
        impl_->renderer_.reset();
        return false;
    }
    // The first playback path supports mono/stereo sources only. Wider
    // speaker beds need a separate, explicit routing/downmix contract.
    if (clipped.channelCount() > 2) return false;
    // The renderer chooses the output layout from the object's spatial
    // parameters. Start with a fresh segment so a caller's scratch layout
    // cannot change object routing.
    outSegment = ArtifactCore::AudioSegment{};

    const auto* comp = static_cast<ArtifactAbstractComposition*>(composition());
    const auto rate = comp ? comp->frameRate() : ArtifactCore::FrameRate(30.0f);
    const auto fpsNumerator = rate.hasExactRational()
        ? rate.numerator()
        : static_cast<int64_t>(std::llround(std::max(1.0f, rate.framerate())));
    const auto fpsDenominator = rate.hasExactRational() ? rate.denominator() : 1;
    const ArtifactCore::RationalTime blockTime(
        start.framePosition() * fpsDenominator, fpsNumerator);

    QMatrix4x4 g = getGlobalTransform4x4At(blockTime);
    QVector3D pos = g.map(QVector3D(0,0,0));
    ArtifactCore::Audio::Spatial::Vec3 src{pos.x(), pos.y(), pos.z()};
    QQuaternion sourceRotQ = QQuaternion::fromRotationMatrix(g.normalMatrix());

    QVector3D listenerPos3{0,0,0};
    QQuaternion listenerRotQ;
    if (comp) {
        ArtifactCore::SharedPtr<ArtifactAbstractLayer> activeCam;
        float bestPriority = -1e30f;
        for (auto& l : comp->allLayerRef()) {
            if (!l || !l->isVisible() || !l->isActiveAt(start)) continue;
            auto cam = ArtifactCore::dynamicPointerCast<ArtifactCameraLayer>(l);
            if (!cam || !cam->isActiveCamera()) continue;
            float pr = cam->cameraPriority();
            if (!activeCam || pr > bestPriority) { activeCam = l; bestPriority = pr; }
        }
        if (activeCam) {
            QMatrix4x4 cg = activeCam->getGlobalTransform4x4At(blockTime);
            QVector3D cp = cg.map(QVector3D(0,0,0));
            listenerPos3 = cp;
            QQuaternion q = QQuaternion::fromRotationMatrix(cg.normalMatrix());
            listenerRotQ = q;
        }
    }

    ArtifactCore::Audio::Spatial::Quat sourceRot{sourceRotQ.x(), sourceRotQ.y(), sourceRotQ.z(), sourceRotQ.scalar()};
    ArtifactCore::Audio::Spatial::Vec3 listenerPos{listenerPos3.x(), listenerPos3.y(), listenerPos3.z()};
    ArtifactCore::Audio::Spatial::Quat listenerRot{listenerRotQ.x(), listenerRotQ.y(), listenerRotQ.z(), listenerRotQ.scalar()};

    impl_->renderer_.setSampleRate((float)sampleRate);
    auto params = impl_->spatial_;
    const auto number = [&](const char* path, float fallback) {
        const auto prop = getProperty(QString::fromLatin1(path));
        if (!prop || prop->getKeyFrames().empty()) return fallback;
        const QVariant v = prop->interpolateValue(blockTime);
        const float result = v.isValid() ? v.toFloat() : fallback;
        return std::isfinite(result) ? result : fallback;
    };
    params.minDistance = number("spatial.minDistance", params.minDistance);
    params.maxDistance = number("spatial.maxDistance", params.maxDistance);
    params.rolloff = number("spatial.rolloff", params.rolloff);
    params.spread = number("spatial.spread", params.spread);
    params.stereoWidthDegrees = number("spatial.stereoWidth", params.stereoWidthDegrees);
    params.coneInnerAngle = number("spatial.coneInner", params.coneInnerAngle);
    params.coneOuterAngle = number("spatial.coneOuter", params.coneOuterAngle);
    params.coneOuterGain = number("spatial.coneOuterGain", params.coneOuterGain);
    params.airAbsorption = number("spatial.airAbsorption", params.airAbsorption);
    params.lfeSend = number("spatial.lfeSend", params.lfeSend);
    params.lfeCutoffHz = number("spatial.lfeCutoffHz", params.lfeCutoffHz);
    const float objectGain = std::clamp(number("spatial.gain", impl_->gain_), 0.0f, 4.0f);
    impl_->renderer_.publishParams(params);
    impl_->renderer_.processBlock(clipped, outSegment, frameCount, src, sourceRot,
                                  listenerPos, listenerRot);
    for (auto& channel : outSegment.channelData) {
        for (float& sample : channel) sample *= objectGain;
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
    loadFromPath(impl_->sourcePath_);
}

QString ArtifactSpatialAudioLayer::sourcePath() const {
    return ArtifactAudioLayer::isLoaded() ? ArtifactAudioLayer::sourcePath() : impl_->sourcePath_;
}
QString ArtifactSpatialAudioLayer::objectId() const { return impl_->objectId_; }
float ArtifactSpatialAudioLayer::gain() const { return impl_->gain_; }
bool ArtifactSpatialAudioLayer::isMuted() const { return ArtifactAudioLayer::isMuted(); }
bool ArtifactSpatialAudioLayer::isEnabled() const { return impl_->enabled_; }
void ArtifactSpatialAudioLayer::setGain(float value) { impl_->gain_ = std::clamp(std::isfinite(value) ? value : 1.0f, 0.0f, 4.0f); changed(); }
void ArtifactSpatialAudioLayer::setMuted(bool value) { ArtifactAudioLayer::setMuted(value); }
void ArtifactSpatialAudioLayer::setEnabled(bool value) { impl_->enabled_ = value; changed(); }

bool ArtifactSpatialAudioLayer::loadFromPath(const QString& path) {
    impl_->sourcePath_ = path.trimmed();
    impl_->renderer_.reset();
    const bool loaded = ArtifactAudioLayer::loadFromPath(impl_->sourcePath_);
    if (loaded && ArtifactAudioLayer::channelCount() > 2) {
        ArtifactAudioLayer::loadFromPath(QString());
        changed();
        return false;
    }
    changed();
    return loaded;
}

QJsonObject ArtifactSpatialAudioLayer::toJson() const {
    QJsonObject obj = ArtifactAudioLayer::toJson();
    obj[QStringLiteral("type")] = static_cast<int>(LayerType::SpatialAudio);
    obj[QStringLiteral("spatial.sourcePath")] = sourcePath();
    obj[QStringLiteral("spatial.objectId")] = impl_->objectId_;
    obj[QStringLiteral("spatial.gain")] = impl_->gain_;
    obj[QStringLiteral("spatial.muted")] = ArtifactAudioLayer::isMuted();
    obj[QStringLiteral("spatial.enabled")] = impl_->enabled_;
    obj[QStringLiteral("spatial.minDistance")] = impl_->spatial_.minDistance;
    obj[QStringLiteral("spatial.maxDistance")] = impl_->spatial_.maxDistance;
    obj[QStringLiteral("spatial.rolloff")] = impl_->spatial_.rolloff;
    obj[QStringLiteral("spatial.spread")] = impl_->spatial_.spread;
    obj[QStringLiteral("spatial.stereoWidth")] = impl_->spatial_.stereoWidthDegrees;
    obj[QStringLiteral("spatial.distanceModel")] = static_cast<int>(impl_->spatial_.model);
    obj[QStringLiteral("spatial.coneInner")] = impl_->spatial_.coneInnerAngle;
    obj[QStringLiteral("spatial.coneOuter")] = impl_->spatial_.coneOuterAngle;
    obj[QStringLiteral("spatial.coneOuterGain")] = impl_->spatial_.coneOuterGain;
    obj[QStringLiteral("spatial.airAbsorption")] = impl_->spatial_.airAbsorption;
    obj[QStringLiteral("spatial.lfeSend")] = impl_->spatial_.lfeSend;
    obj[QStringLiteral("spatial.lfeCutoffHz")] = impl_->spatial_.lfeCutoffHz;
    obj[QStringLiteral("spatial.outputLayout")] = static_cast<int>(impl_->spatial_.outputLayout);
    obj[QStringLiteral("spatial.renderMode")] = static_cast<int>(impl_->spatial_.renderMode);
    obj[QStringLiteral("spatial.doppler")] = impl_->spatial_.doppler;
    obj[QStringLiteral("spatial.dopplerFactor")] = impl_->spatial_.dopplerFactor;
    return obj;
}

void ArtifactSpatialAudioLayer::fromJsonProperties(const QJsonObject& obj) {
    QJsonObject sourceJson = obj;
    if (obj.contains(QStringLiteral("spatial.sourcePath"))) {
        sourceJson[QStringLiteral("audio.sourcePath")] = obj.value(QStringLiteral("spatial.sourcePath"));
    }
    ArtifactAudioLayer::fromJsonProperties(sourceJson);
    impl_->sourcePath_ = sourceJson.value(QStringLiteral("audio.sourcePath")).toString();
    impl_->renderer_.reset();
    if (obj.contains(QStringLiteral("spatial.objectId"))) impl_->objectId_ = obj.value(QStringLiteral("spatial.objectId")).toString();
    if (impl_->objectId_.isEmpty()) impl_->objectId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (obj.contains(QStringLiteral("spatial.gain"))) impl_->gain_ = (float)obj.value(QStringLiteral("spatial.gain")).toDouble(1.0);
    if (obj.contains(QStringLiteral("spatial.muted"))) ArtifactAudioLayer::setMuted(obj.value(QStringLiteral("spatial.muted")).toBool(false));
    if (obj.contains(QStringLiteral("spatial.enabled"))) impl_->enabled_ = obj.value(QStringLiteral("spatial.enabled")).toBool(true);
    if (obj.contains(QStringLiteral("spatial.minDistance"))) impl_->spatial_.minDistance = (float)obj.value(QStringLiteral("spatial.minDistance")).toDouble(1.0);
    if (obj.contains(QStringLiteral("spatial.maxDistance"))) impl_->spatial_.maxDistance = (float)obj.value(QStringLiteral("spatial.maxDistance")).toDouble(100.0);
    if (obj.contains(QStringLiteral("spatial.rolloff"))) impl_->spatial_.rolloff = (float)obj.value(QStringLiteral("spatial.rolloff")).toDouble(1.0);
    if (obj.contains(QStringLiteral("spatial.spread"))) impl_->spatial_.spread = (float)obj.value(QStringLiteral("spatial.spread")).toDouble(0.0);
    if (obj.contains(QStringLiteral("spatial.stereoWidth"))) impl_->spatial_.stereoWidthDegrees = (float)obj.value(QStringLiteral("spatial.stereoWidth")).toDouble(30.0);
    if (obj.contains(QStringLiteral("spatial.distanceModel"))) impl_->spatial_.model = static_cast<ArtifactCore::Audio::Spatial::DistanceModel>(obj.value(QStringLiteral("spatial.distanceModel")).toInt(1));
    if (obj.contains(QStringLiteral("spatial.coneInner"))) impl_->spatial_.coneInnerAngle = (float)obj.value(QStringLiteral("spatial.coneInner")).toDouble(360.0);
    if (obj.contains(QStringLiteral("spatial.coneOuter"))) impl_->spatial_.coneOuterAngle = (float)obj.value(QStringLiteral("spatial.coneOuter")).toDouble(360.0);
    if (obj.contains(QStringLiteral("spatial.coneOuterGain"))) impl_->spatial_.coneOuterGain = (float)obj.value(QStringLiteral("spatial.coneOuterGain")).toDouble(0.0);
    if (obj.contains(QStringLiteral("spatial.airAbsorption"))) impl_->spatial_.airAbsorption = (float)obj.value(QStringLiteral("spatial.airAbsorption")).toDouble(0.0);
    if (obj.contains(QStringLiteral("spatial.lfeSend"))) impl_->spatial_.lfeSend = (float)obj.value(QStringLiteral("spatial.lfeSend")).toDouble(0.0);
    if (obj.contains(QStringLiteral("spatial.lfeCutoffHz"))) impl_->spatial_.lfeCutoffHz = (float)obj.value(QStringLiteral("spatial.lfeCutoffHz")).toDouble(120.0);
    if (obj.contains(QStringLiteral("spatial.outputLayout"))) impl_->spatial_.outputLayout =
        static_cast<ArtifactCore::AudioChannelLayout>(obj.value(QStringLiteral("spatial.outputLayout")).toInt());
    if (obj.contains(QStringLiteral("spatial.renderMode"))) impl_->spatial_.renderMode =
        static_cast<ArtifactCore::Audio::Spatial::SpatialRenderMode>(obj.value(QStringLiteral("spatial.renderMode")).toInt());
    if (obj.contains(QStringLiteral("spatial.doppler"))) impl_->spatial_.doppler = obj.value(QStringLiteral("spatial.doppler")).toBool(false);
    if (obj.contains(QStringLiteral("spatial.dopplerFactor"))) impl_->spatial_.dopplerFactor = (float)obj.value(QStringLiteral("spatial.dopplerFactor")).toDouble(1.0);
    if (!std::isfinite(impl_->gain_)) impl_->gain_ = 1.0f;
    impl_->gain_ = std::clamp(impl_->gain_, 0.0f, 4.0f);
    impl_->spatial_ = ArtifactCore::Audio::Spatial::sanitizedSpatialParams(impl_->spatial_);
    setIs3D(true);
}

std::vector<ArtifactCore::PropertyGroup> ArtifactSpatialAudioLayer::getLayerPropertyGroups() const {
    std::vector<ArtifactCore::PropertyGroup> groups;
    ArtifactCore::PropertyGroup g;
    g.setName("Spatial Audio");
    auto p = [&](QString path, ArtifactCore::PropertyType t, QVariant v, int prio) {
        auto prop = persistentLayerProperty(path, t, v, prio);
        prop->setAnimatable(t == ArtifactCore::PropertyType::Float &&
                            path != QStringLiteral("spatial.dopplerFactor"));
        if (path == QStringLiteral("spatial.minDistance")) prop->setDisplayLabel(QStringLiteral("Minimum Distance"));
        else if (path == QStringLiteral("spatial.maxDistance")) prop->setDisplayLabel(QStringLiteral("Maximum Distance"));
        else if (path == QStringLiteral("spatial.rolloff")) prop->setDisplayLabel(QStringLiteral("Rolloff"));
        else if (path == QStringLiteral("spatial.spread")) prop->setDisplayLabel(QStringLiteral("Spread"));
        else if (path == QStringLiteral("spatial.stereoWidth")) prop->setDisplayLabel(QStringLiteral("Stereo Width"));
        else if (path == QStringLiteral("spatial.distanceModel")) prop->setDisplayLabel(QStringLiteral("Distance Model"));
        else if (path == QStringLiteral("spatial.coneInner")) prop->setDisplayLabel(QStringLiteral("Cone Inner Angle"));
        else if (path == QStringLiteral("spatial.coneOuter")) prop->setDisplayLabel(QStringLiteral("Cone Outer Angle"));
        else if (path == QStringLiteral("spatial.coneOuterGain")) prop->setDisplayLabel(QStringLiteral("Cone Outer Gain"));
        else if (path == QStringLiteral("spatial.airAbsorption")) prop->setDisplayLabel(QStringLiteral("Air Absorption"));
        else if (path == QStringLiteral("spatial.lfeSend")) prop->setDisplayLabel(QStringLiteral("LFE Send"));
        else if (path == QStringLiteral("spatial.lfeCutoffHz")) prop->setDisplayLabel(QStringLiteral("LFE Cutoff"));
        else if (path == QStringLiteral("spatial.outputLayout")) prop->setDisplayLabel(QStringLiteral("Speaker Layout"));
        else if (path == QStringLiteral("spatial.renderMode")) prop->setDisplayLabel(QStringLiteral("Monitoring"));
        else if (path == QStringLiteral("spatial.dopplerFactor")) prop->setDisplayLabel(QStringLiteral("Doppler Factor"));
        else if (path == QStringLiteral("spatial.gain")) prop->setDisplayLabel(QStringLiteral("Object Gain"));
        else if (path == QStringLiteral("spatial.muted")) prop->setDisplayLabel(QStringLiteral("Muted"));
        else if (path == QStringLiteral("spatial.enabled")) prop->setDisplayLabel(QStringLiteral("Enabled"));
        else if (path == QStringLiteral("spatial.sourcePath")) prop->setDisplayLabel(QStringLiteral("Source Path"));
        if (path == QStringLiteral("spatial.minDistance") ||
            path == QStringLiteral("spatial.maxDistance")) {
            prop->setHardRange(0.001, 1000000.0);
            prop->setStep(0.01);
        } else if (path == QStringLiteral("spatial.rolloff")) {
            prop->setHardRange(0.0, 10.0);
            prop->setStep(0.01);
        } else if (path == QStringLiteral("spatial.stereoWidth")) {
            prop->setHardRange(0.0, 120.0);
            prop->setStep(1.0);
        } else if (path == QStringLiteral("spatial.spread") ||
                   path == QStringLiteral("spatial.coneOuterGain") ||
                   path == QStringLiteral("spatial.airAbsorption") ||
                   path == QStringLiteral("spatial.lfeSend")) {
            prop->setHardRange(0.0, 1.0);
            prop->setStep(0.01);
        } else if (path == QStringLiteral("spatial.coneInner") ||
                   path == QStringLiteral("spatial.coneOuter")) {
            prop->setHardRange(0.0, 360.0);
            prop->setStep(1.0);
        } else if (path == QStringLiteral("spatial.lfeCutoffHz")) {
            prop->setHardRange(40.0, 250.0);
            prop->setStep(1.0);
        } else if (path == QStringLiteral("spatial.gain")) {
            prop->setHardRange(0.0, 4.0);
            prop->setStep(0.01);
        }
        return prop;
    };
    g.addProperty(p(QStringLiteral("spatial.minDistance"), ArtifactCore::PropertyType::Float, impl_->spatial_.minDistance, -120));
    g.addProperty(p(QStringLiteral("spatial.maxDistance"), ArtifactCore::PropertyType::Float, impl_->spatial_.maxDistance, -119));
    g.addProperty(p(QStringLiteral("spatial.rolloff"), ArtifactCore::PropertyType::Float, impl_->spatial_.rolloff, -118));
    g.addProperty(p(QStringLiteral("spatial.spread"), ArtifactCore::PropertyType::Float, impl_->spatial_.spread, -117));
    g.addProperty(p(QStringLiteral("spatial.stereoWidth"), ArtifactCore::PropertyType::Float, impl_->spatial_.stereoWidthDegrees, -116));
    g.addProperty(p(QStringLiteral("spatial.distanceModel"), ArtifactCore::PropertyType::Integer, (int)impl_->spatial_.model, -116));
    g.addProperty(p(QStringLiteral("spatial.coneInner"), ArtifactCore::PropertyType::Float, impl_->spatial_.coneInnerAngle, -115));
    g.addProperty(p(QStringLiteral("spatial.coneOuter"), ArtifactCore::PropertyType::Float, impl_->spatial_.coneOuterAngle, -114));
    g.addProperty(p(QStringLiteral("spatial.coneOuterGain"), ArtifactCore::PropertyType::Float, impl_->spatial_.coneOuterGain, -113));
    g.addProperty(p(QStringLiteral("spatial.airAbsorption"), ArtifactCore::PropertyType::Float, impl_->spatial_.airAbsorption, -112));
    g.addProperty(p(QStringLiteral("spatial.lfeSend"), ArtifactCore::PropertyType::Float, impl_->spatial_.lfeSend, -111));
    g.addProperty(p(QStringLiteral("spatial.lfeCutoffHz"), ArtifactCore::PropertyType::Float, impl_->spatial_.lfeCutoffHz, -110));
    g.addProperty(p(QStringLiteral("spatial.outputLayout"), ArtifactCore::PropertyType::Integer,
                    static_cast<int>(impl_->spatial_.outputLayout), -111));
    g.addProperty(p(QStringLiteral("spatial.renderMode"), ArtifactCore::PropertyType::Integer,
                    static_cast<int>(impl_->spatial_.renderMode), -110));
    g.addProperty(p(QStringLiteral("spatial.dopplerFactor"), ArtifactCore::PropertyType::Float, impl_->spatial_.dopplerFactor, -111));
    g.addProperty(p(QStringLiteral("spatial.gain"), ArtifactCore::PropertyType::Float, impl_->gain_, -110));
    g.addProperty(p(QStringLiteral("spatial.muted"), ArtifactCore::PropertyType::Bool, ArtifactAudioLayer::isMuted(), -111));
    g.addProperty(p(QStringLiteral("spatial.enabled"), ArtifactCore::PropertyType::Bool, impl_->enabled_, -110));
    g.addProperty(p(QStringLiteral("spatial.sourcePath"), ArtifactCore::PropertyType::String, sourcePath(), -109));
    groups.push_back(std::move(g));
    return groups;
}

bool ArtifactSpatialAudioLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value) {
    auto sp = impl_->spatial_;
    if (propertyPath == QStringLiteral("spatial.minDistance")) sp.minDistance = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.maxDistance")) sp.maxDistance = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.rolloff")) sp.rolloff = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.spread")) sp.spread = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.stereoWidth")) sp.stereoWidthDegrees = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.gain")) { setGain(value.toFloat()); return true; }
    else if (propertyPath == QStringLiteral("spatial.muted")) { setMuted(value.toBool()); return true; }
    else if (propertyPath == QStringLiteral("spatial.enabled")) { setEnabled(value.toBool()); return true; }
    else if (propertyPath == QStringLiteral("spatial.distanceModel")) sp.model = static_cast<ArtifactCore::Audio::Spatial::DistanceModel>(value.toInt());
    else if (propertyPath == QStringLiteral("spatial.coneInner")) sp.coneInnerAngle = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.coneOuter")) sp.coneOuterAngle = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.coneOuterGain")) sp.coneOuterGain = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.airAbsorption")) sp.airAbsorption = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.lfeSend")) sp.lfeSend = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.lfeCutoffHz")) sp.lfeCutoffHz = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.outputLayout")) sp.outputLayout =
        static_cast<ArtifactCore::AudioChannelLayout>(value.toInt());
    else if (propertyPath == QStringLiteral("spatial.renderMode")) sp.renderMode =
        static_cast<ArtifactCore::Audio::Spatial::SpatialRenderMode>(value.toInt());
    else if (propertyPath == QStringLiteral("spatial.dopplerFactor")) sp.dopplerFactor = value.toFloat();
    else if (propertyPath == QStringLiteral("spatial.sourcePath")) { return loadFromPath(value.toString()); }
    else return ArtifactAudioLayer::setLayerPropertyValue(propertyPath, value);
    setSpatialParams(sp);
    return true;
}

} // namespace Artifact
