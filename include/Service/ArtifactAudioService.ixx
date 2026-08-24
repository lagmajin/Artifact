module;
#include <utility>
#include <memory>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtGlobal>
export module Artifact.Service.Audio;

import Utils.Id;
import Artifact.Service.Playback;
import Artifact.Playback.Engine;

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
  QStringList availableOutputDeviceNames() const;
  void setOutputDeviceName(const QString& deviceName);
  QString outputDeviceName() const;
  ArtifactPlaybackAudioDiagnostics playbackDiagnostics() const;

  void setMasterVolume(float volume);
  float masterVolume() const;
  void setMasterMuted(bool muted);
  bool masterMuted() const;

  bool setLayerBusVolume(const ArtifactCore::LayerID& layerId, float volume);
  bool setLayerBusPan(const ArtifactCore::LayerID& layerId, float pan);
  bool setLayerBusMuted(const ArtifactCore::LayerID& layerId, bool muted);
  bool setLayerBusSolo(const ArtifactCore::LayerID& layerId, bool solo);
  bool addLayerDeClickRange(const ArtifactCore::LayerID& layerId,
                            qint64 startSample, qint64 endSample);
  bool clearLayerDeClickRanges(const ArtifactCore::LayerID& layerId);
  QVariantList layerDeClickRanges(const ArtifactCore::LayerID& layerId) const;
  bool setLayerDeClickSettings(const ArtifactCore::LayerID& layerId,
                               float thresholdDb, qint64 maxClickSamples);
  QVariantMap layerDeClickSettings(const ArtifactCore::LayerID& layerId) const;
  bool setLayerTrim(const ArtifactCore::LayerID& layerId,
                    double trimInSeconds, double trimOutSeconds);
  QVariantMap layerTrim(const ArtifactCore::LayerID& layerId) const;
  bool setLayerPlaybackRate(const ArtifactCore::LayerID& layerId, double rate);
  double layerPlaybackRate(const ArtifactCore::LayerID& layerId) const;
 };



};
