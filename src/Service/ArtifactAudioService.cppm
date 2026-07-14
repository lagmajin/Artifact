module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <QString>
#include <QStringList>
module Artifact.Service.Audio;

import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Audio;
import Artifact.Layer.Video;
import Audio.Mixer;
import Audio.Bus;

namespace Artifact {

namespace {
QString layerBusName(const ArtifactCore::LayerID& layerId)
{
 return QStringLiteral("layer_") + layerId.toString();
}

float linearToDecibels(const float volume)
{
 return 20.0f * std::log10(std::max(0.001f, volume));
}
}

class ArtifactAudioService::Impl {
public:
 float masterVolume = 1.0f;
 bool masterMuted = false;

 std::shared_ptr<ArtifactCore::AudioMixer> currentMixer() const
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
 for (const auto& layer : composition->allLayer()) {
  if (!layer || !layer->hasAudio()) {
   continue;
  }
  const QString name = layerBusName(layer->id());
  auto bus = mixer->findBusByName(name);
  if (!bus) {
   bus = mixer->createBus(name);
  }
  if (!bus) {
   continue;
  }
  if (const auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
   bus->setVolume(linearToDecibels(audioLayer->volume()));
   bus->setPan(audioLayer->pan());
   bus->setMute(audioLayer->isMuted());
  } else if (const auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
   bus->setVolume(linearToDecibels(static_cast<float>(videoLayer->audioVolume())));
   bus->setPan(static_cast<float>(videoLayer->audioPan()));
   bus->setMute(videoLayer->isAudioMuted());
  }
  bus->setSolo(layer->isSolo());
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
   result.append(QString::fromStdString(name));
  }
 }
 return result;
}

void ArtifactAudioService::setMasterVolume(float volume)
{
 impl_->masterVolume = std::clamp(volume, 0.0f, 2.0f);
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
 const float normalized = std::clamp(volume, 0.0f, 2.0f);
 if (const auto layer = impl_->currentLayer(layerId)) {
  if (const auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
   audioLayer->setVolume(normalized);
  } else if (const auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
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
 const float normalized = std::clamp(pan, -1.0f, 1.0f);
 if (const auto layer = impl_->currentLayer(layerId)) {
  if (const auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
   audioLayer->setPan(normalized);
  } else if (const auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
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
  if (const auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
   if (audioLayer->isMuted() != muted) {
    audioLayer->mute();
   }
  } else if (const auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
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

};
