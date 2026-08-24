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

import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Audio;
import Artifact.Layer.Video;
import Audio.Mixer;
import Audio.Bus;
import Memory.SharedPtr;

namespace Artifact {

namespace {
QString layerBusName(const ArtifactCore::LayerID& layerId)
{
 const auto name = ArtifactCore::AudioMixer::layerBusName(layerId);
 return QString::fromUtf8(name.data(), static_cast<int>(name.length()));
}

float linearToDecibels(const float volume)
{
 const float safeVolume = std::isfinite(volume)
     ? std::max(0.001f, volume) : 1.0f;
 return 20.0f * std::log10(safeVolume);
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
  auto* projectService = ArtifactProjectService::instance();
  const auto composition = projectService
      ? projectService->currentComposition().lock()
      : ArtifactCompositionPtr{};
  return composition ? composition->getAudioMixer() : nullptr;
 }

 ArtifactAbstractLayerPtr currentLayer(const ArtifactCore::LayerID& layerId) const
 {
  auto* projectService = ArtifactProjectService::instance();
  const auto composition = projectService
      ? projectService->currentComposition().lock()
      : ArtifactCompositionPtr{};
  return composition ? composition->layerById(layerId) : nullptr;
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
 auto* projectService = ArtifactProjectService::instance();
 const auto composition = projectService
     ? projectService->currentComposition().lock()
     : ArtifactCompositionPtr{};
 if (!composition) {
  return false;
 }

 composition->ensureAudioMixer();
 const auto mixer = composition->getAudioMixer();
 if (!mixer) {
  return false;
 }
 QSet<QString> activeLayerBuses;
 for (const auto& layer : composition->allLayer()) {
  if (!layer || !layer->hasAudio()) {
   continue;
  }
  const QString name = layerBusName(layer->id());
  activeLayerBuses.insert(name);
  auto bus = mixer->ensureLayerBus(layer->id());
  if (!bus) {
   continue;
  }
  if (const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(layer)) {
   bus->setVolume(linearToDecibels(audioLayer->volume()));
   bus->setPan(audioLayer->pan());
   bus->setMute(audioLayer->isMuted());
  } else if (const auto videoLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer)) {
   bus->setVolume(linearToDecibels(static_cast<float>(videoLayer->audioVolume())));
   bus->setPan(static_cast<float>(videoLayer->audioPan()));
   bus->setMute(videoLayer->isAudioMuted());
  }
  bus->setSolo(layer->isSolo());
 }
 for (const auto& rawName : mixer->busNames()) {
  const QString name = QString::fromUtf8(rawName.data(),
                                         static_cast<int>(rawName.length()));
  if (mixer->busKind(mixer->findBusByName(name)) !=
          ArtifactCore::AudioBusKind::Layer ||
      activeLayerBuses.contains(name)) {
   continue;
  }
  if (const auto staleBus = mixer->findBusByName(name)) {
   mixer->removeBus(staleBus);
  }
 }
 if (auto* playback = ArtifactPlaybackService::instance()) {
  playback->setAudioMasterVolume(impl_->masterVolume);
  playback->setAudioMasterMuted(impl_->masterMuted);
 }
 return true;
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
  } else if (const auto videoLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer)) {
   videoLayer->setAudioVolume(normalized);
   layer->changed();
  }
 }
 bus->setVolume(linearToDecibels(normalized));
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
  } else if (const auto videoLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer)) {
   videoLayer->setAudioPan(normalized);
   layer->changed();
  }
 }
 bus->setPan(normalized);
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
   } else if (const auto videoLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer)) {
   videoLayer->setAudioMuted(muted);
   layer->changed();
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
