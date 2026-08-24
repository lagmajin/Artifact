module;

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
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QUuid>
export module Artifact.Layer.Audio;

namespace Artifact { class ArtifactSwitchLayer; }

import Audio.Volume;
import Audio.Segment;
import Audio.LipSyncTrack;
import Artifact.Audio.Waveform;
import Artifact.Layer.Abstract;

export namespace Artifact
{
 using namespace ArtifactCore;

  class ArtifactAudioLayer :public ArtifactAbstractLayer
  {
   private:
    class Impl;
    Impl* impl_;
  public:
  ArtifactAudioLayer();
  ~ArtifactAudioLayer();

  void setVolume(float volume);
  float volume() const;
  // Non-destructive clip-level gain/fades, independent from mixer volume.
  void setClipGainDb(float gainDb);
  float clipGainDb() const;
  void setFadeInSeconds(float seconds);
  float fadeInSeconds() const;
  void setFadeOutSeconds(float seconds);
  float fadeOutSeconds() const;
  // Fade shape: 0=Linear, 1=Exponential, 2=Logarithmic, 3=S-curve.
  void setFadeInCurve(int curve);
  int fadeInCurve() const;
  void setFadeOutCurve(int curve);
  int fadeOutCurve() const;
  // Non-destructive source trim. Only [trimIn, duration - trimOut] of the
  // source is played; fades are measured inside this trimmed window.
  void setTrimInSeconds(float seconds);
  float trimInSeconds() const;
  void setTrimOutSeconds(float seconds);
  float trimOutSeconds() const;
  // Constant playback rate for the non-time-remap path (clamped 0.1 - 8.0).
  void setPlaybackRate(float rate);
  float playbackRate() const;
  // Non-destructive reverse playback of the trimmed source window
  // (non-time-remap path). Fades stay relative to the clip edges.
  void setReversed(bool reversed);
  bool isReversed() const;
  // Non-destructive source-sample repair ranges. Ranges are normalized and
  // merged on insertion, then persisted with the layer JSON.
  void addDeClickRange(qint64 startSample, qint64 endSample);
  void clearDeClickRanges();
  int deClickRangeCount() const;
  std::vector<std::pair<qint64, qint64>> deClickRanges() const;
  void setDeClickThresholdDb(float thresholdDb);
  float deClickThresholdDb() const;
  void setDeClickMaxClickSamples(qint64 samples);
  qint64 deClickMaxClickSamples() const;
  void setPan(float pan);
  float pan() const;
  bool isMuted() const;
  void mute();
  // Explicit mute state setter; mute() toggles for convenience.
  void setMuted(bool muted);
  bool loadFromPath(const QString& path);
  QString sourcePath() const;
  QUuid sourceAssetId() const;
  bool localizeSourceIdentity();
  bool relinkSourceIdentityToShared();
  bool isSourceIdentityLocalized() const;
  bool isLoaded() const;

  // Audio metadata
  double duration() const;
  int sampleRate() const;
  int channelCount() const;
  qint64 totalFrames() const;
  WaveformData buildWaveformData(int displayWidth) const;
  QString waveformPreviewSummary(int displayWidth = 128) const;
  bool buildLipSyncTrack(ArtifactCore::LipSyncTrack& track, double frameRate) const;
  bool applyLipSyncToSwitchLayer(ArtifactSwitchLayer* switchLayer, double frameRate) const;

  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;

  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

  void draw(ArtifactIRenderer* renderer) override;
  bool hasVideo() const override;
  bool hasAudio() const override;
  bool getAudio(ArtifactCore::AudioSegment &outSegment, const FramePosition &start,
                int frameCount, int sampleRate) override;
 };

};
