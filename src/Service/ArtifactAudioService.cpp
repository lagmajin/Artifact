module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QtMultimedia/QMediaDevices>
#include <QtMultimedia/QAudioDevice>
module Artifact.Service.Audio;

import Configuration.LayeredConfigStore;

import Artifact.Service.Playback;
import Artifact.Layer.Abstract;
import Artifact.Layer.Audio;
import Audio.Mixer;
import Audio.Bus;
import Memory.SharedPtr;

namespace Artifact {

ArtifactCore::SharedPtr<ArtifactCore::AudioMixer> resolveCurrentProjectAudioMixer();
ArtifactAbstractLayerPtr resolveCurrentProjectAudioLayer(
    const ArtifactCore::LayerID& layerId);
bool setVideoLayerAudioVolume(const ArtifactAbstractLayerPtr& layer,
                              float volume);
bool setVideoLayerAudioPan(const ArtifactAbstractLayerPtr& layer, float pan);
bool setVideoLayerAudioMute(const ArtifactAbstractLayerPtr& layer, bool muted);
bool videoLayerAudioMuted(const ArtifactAbstractLayerPtr& layer,
                          bool* isVideoLayer);
bool compositionLayerAudioMuted(const ArtifactAbstractLayerPtr& layer,
                                bool* hasAudioMute);
bool syncCurrentProjectCompositionAudio(float masterVolume, bool masterMuted);

namespace {
 QString layerBusName(const ArtifactCore::LayerID& layerId)
 {
  const auto name = ArtifactCore::AudioMixer::layerBusName(layerId);
  return QString::fromUtf8(name.data(), static_cast<int>(name.length()));
 }
}

class ArtifactAudioService::Impl {
public:
 float masterVolume = 1.0f;
 bool masterMuted = false;
 QString outputDeviceName;

 Impl()
 {
  outputDeviceName = ArtifactCore::LayeredConfigStore::instance()
                         .value(QStringLiteral("audio/outputDeviceName"))
                         .toString().trimmed();
 }

 ArtifactCore::SharedPtr<ArtifactCore::AudioMixer> currentMixer() const
 {
   return resolveCurrentProjectAudioMixer();
 }

 ArtifactAbstractLayerPtr currentLayer(const ArtifactCore::LayerID& layerId) const
 {
   return resolveCurrentProjectAudioLayer(layerId);
 }
};

ArtifactAudioService::ArtifactAudioService()
 : impl_(std::make_unique<Impl>())
{
 if (auto* playback = ArtifactPlaybackService::instance()) {
  playback->setAudioOutputDeviceName(impl_->outputDeviceName);
 }
}

ArtifactAudioService::~ArtifactAudioService() = default;

ArtifactAudioService* ArtifactAudioService::instance()
{
 static ArtifactAudioService service;
 return &service;
}

bool ArtifactAudioService::syncCurrentComposition()
{
 return syncCurrentProjectCompositionAudio(
     impl_->masterVolume, impl_->masterMuted);
}

bool ArtifactAudioService::hasCurrentMixer() const
{
 return static_cast<bool>(impl_->currentMixer());
}

QStringList ArtifactAudioService::busNames() const
{
 QStringList result;
 if (const auto mixer = impl_->currentMixer()) {
  for (const auto& name : mixer->busNames()) {
   result.append(QString::fromUtf8(name.data(), static_cast<int>(name.length())));
  }
 }
 return result;
}

QStringList ArtifactAudioService::availableOutputDeviceNames() const
{
 QStringList result;
 for (const auto& device : QMediaDevices::audioOutputs()) {
  const QString name = device.description().trimmed();
  if (!name.isEmpty() && !result.contains(name)) {
   result.append(name);
  }
 }
 return result;
}

void ArtifactAudioService::setOutputDeviceName(const QString& deviceName)
{
 const QString normalizedName = deviceName.trimmed();
 if (impl_->outputDeviceName == normalizedName) {
  return;
 }
 impl_->outputDeviceName = normalizedName;
 ArtifactCore::LayeredConfigStore::instance().setValue(
     QStringLiteral("audio/outputDeviceName"), normalizedName);
 if (auto* playback = ArtifactPlaybackService::instance()) {
  playback->setAudioOutputDeviceName(normalizedName);
 }
}

QString ArtifactAudioService::outputDeviceName() const
{
 return impl_->outputDeviceName;
}

ArtifactPlaybackAudioDiagnostics ArtifactAudioService::playbackDiagnostics() const
{
 if (auto* playback = ArtifactPlaybackService::instance()) {
  return playback->audioDiagnostics();
 }
 return {};
}

void ArtifactAudioService::setMasterVolume(float volume)
{
 impl_->masterVolume = std::isfinite(volume)
     ? std::clamp(volume, 0.0f, 2.0f) : 1.0f;
 if (auto* playback = ArtifactPlaybackService::instance()) {
  playback->setAudioMasterVolume(impl_->masterVolume);
 }
}

float ArtifactAudioService::masterVolume() const
{
 return impl_->masterVolume;
}

void ArtifactAudioService::setMasterMuted(bool muted)
{
 impl_->masterMuted = muted;
 if (auto* playback = ArtifactPlaybackService::instance()) {
  playback->setAudioMasterMuted(muted);
 }
}

bool ArtifactAudioService::masterMuted() const
{
 return impl_->masterMuted;
}

bool ArtifactAudioService::setLayerBusVolume(
    const ArtifactCore::LayerID& layerId, float volume)
{
 const auto mixer = impl_->currentMixer();
 const auto bus = mixer ? mixer->findBusByName(layerBusName(layerId)) : nullptr;
 if (!bus) return false;
 const float normalized = std::isfinite(volume)
     ? std::clamp(volume, 0.0f, 2.0f) : 1.0f;
 if (const auto layer = impl_->currentLayer(layerId)) {
  if (const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(layer)) {
   audioLayer->setVolume(normalized);
  } else {
   setVideoLayerAudioVolume(layer, normalized);
  }
 }
 // The layer bus is a routing node, not a second fader stage. The layer
 // applies its volume while producing PCM, so mirroring it here would square
 // the requested gain.
 return true;
}

bool ArtifactAudioService::setLayerBusPan(
    const ArtifactCore::LayerID& layerId, float pan)
{
 const auto mixer = impl_->currentMixer();
 const auto bus = mixer ? mixer->findBusByName(layerBusName(layerId)) : nullptr;
 if (!bus) return false;
 const float normalized = std::isfinite(pan)
     ? std::clamp(pan, -1.0f, 1.0f) : 0.0f;
 if (const auto layer = impl_->currentLayer(layerId)) {
  if (const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(layer)) {
   audioLayer->setPan(normalized);
  } else {
   setVideoLayerAudioPan(layer, normalized);
  }
 }
 // The layer bus is a routing node, not a second fader stage. The layer
 // applies its pan while producing PCM, so mirroring it here would pan twice.
 return true;
}

bool ArtifactAudioService::setLayerBusMuted(
    const ArtifactCore::LayerID& layerId, bool muted)
{
 const auto mixer = impl_->currentMixer();
 const auto bus = mixer ? mixer->findBusByName(layerBusName(layerId)) : nullptr;
 if (!bus) return false;
  if (const auto layer = impl_->currentLayer(layerId)) {
   if (const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(layer)) {
    audioLayer->setMuted(muted);
   } else {
   setVideoLayerAudioMute(layer, muted);
  }
 }
 bus->setMute(muted);
 return true;
}

bool ArtifactAudioService::setLayerBusSolo(
    const ArtifactCore::LayerID& layerId, bool solo)
{
 const auto mixer = impl_->currentMixer();
 const auto bus = mixer ? mixer->findBusByName(layerBusName(layerId)) : nullptr;
 if (!bus) return false;
 if (const auto layer = impl_->currentLayer(layerId)) {
  layer->setSolo(solo);
  layer->changed();
 }
 bus->setSolo(solo);
 return true;
}

bool ArtifactAudioService::addLayerDeClickRange(
    const ArtifactCore::LayerID& layerId, qint64 startSample, qint64 endSample)
{
 const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(
     impl_->currentLayer(layerId));
 if (!audioLayer) return false;
 audioLayer->addDeClickRange(startSample, endSample);
 return true;
}

bool ArtifactAudioService::clearLayerDeClickRanges(
    const ArtifactCore::LayerID& layerId)
{
 const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(
     impl_->currentLayer(layerId));
 if (!audioLayer) return false;
 audioLayer->clearDeClickRanges();
 return true;
}

QVariantList ArtifactAudioService::layerDeClickRanges(
    const ArtifactCore::LayerID& layerId) const
{
 QVariantList result;
 const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(
     impl_->currentLayer(layerId));
 if (!audioLayer) return result;
 for (const auto& range : audioLayer->deClickRanges()) {
  result.append(QVariantMap{{QStringLiteral("startSample"),
                             QVariant::fromValue(range.first)},
                            {QStringLiteral("endSample"),
                             QVariant::fromValue(range.second)}});
 }
 return result;
}

bool ArtifactAudioService::setLayerDeClickSettings(
    const ArtifactCore::LayerID& layerId, float thresholdDb, qint64 maxClickSamples)
{
 const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(
     impl_->currentLayer(layerId));
 if (!audioLayer) return false;
 audioLayer->setDeClickThresholdDb(thresholdDb);
 audioLayer->setDeClickMaxClickSamples(maxClickSamples);
 return true;
}

QVariantMap ArtifactAudioService::layerDeClickSettings(
    const ArtifactCore::LayerID& layerId) const
{
 const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(
     impl_->currentLayer(layerId));
 if (!audioLayer) return {};
 return {{QStringLiteral("thresholdDb"), audioLayer->deClickThresholdDb()},
         {QStringLiteral("maxClickSamples"),
          QVariant::fromValue(audioLayer->deClickMaxClickSamples())},
         {QStringLiteral("rangeCount"), audioLayer->deClickRangeCount()}};
}

bool ArtifactAudioService::setLayerTrim(
    const ArtifactCore::LayerID& layerId,
    double trimInSeconds, double trimOutSeconds)
{
 const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(
     impl_->currentLayer(layerId));
 if (!audioLayer) return false;
 audioLayer->setTrimInSeconds(static_cast<float>(trimInSeconds));
 audioLayer->setTrimOutSeconds(static_cast<float>(trimOutSeconds));
 return true;
}

QVariantMap ArtifactAudioService::layerTrim(
    const ArtifactCore::LayerID& layerId) const
{
 const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(
     impl_->currentLayer(layerId));
 if (!audioLayer) return {};
 return {{QStringLiteral("trimInSeconds"),
          static_cast<double>(audioLayer->trimInSeconds())},
         {QStringLiteral("trimOutSeconds"),
          static_cast<double>(audioLayer->trimOutSeconds())}};
}

bool ArtifactAudioService::setLayerPlaybackRate(
    const ArtifactCore::LayerID& layerId, double rate)
{
 const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(
     impl_->currentLayer(layerId));
 if (!audioLayer) return false;
 audioLayer->setPlaybackRate(static_cast<float>(rate));
 return true;
}

double ArtifactAudioService::layerPlaybackRate(
    const ArtifactCore::LayerID& layerId) const
{
 const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(
     impl_->currentLayer(layerId));
 return audioLayer ? static_cast<double>(audioLayer->playbackRate()) : 1.0;
}

};
