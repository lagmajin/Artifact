module;
#include <utility>
#include <memory>
#include <QStringList>
export module Artifact.Service.Audio;

import Utils.Id;

export namespace Artifact {

 class ArtifactAudioService {
 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
 public:
  ArtifactAudioService();
  ~ArtifactAudioService();

  static ArtifactAudioService* instance();

  ArtifactAudioService(const ArtifactAudioService&) = delete;
  ArtifactAudioService& operator=(const ArtifactAudioService&) = delete;

  bool syncCurrentComposition();
  bool hasCurrentMixer() const;
  QStringList busNames() const;

  void setMasterVolume(float volume);
  float masterVolume() const;
  void setMasterMuted(bool muted);
  bool masterMuted() const;

  bool setLayerBusVolume(const ArtifactCore::LayerID& layerId, float volume);
  bool setLayerBusPan(const ArtifactCore::LayerID& layerId, float pan);
  bool setLayerBusMuted(const ArtifactCore::LayerID& layerId, bool muted);
  bool setLayerBusSolo(const ArtifactCore::LayerID& layerId, bool solo);
 };



};
