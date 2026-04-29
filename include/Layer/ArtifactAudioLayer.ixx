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
#include <QVariant>
export module Artifact.Layer.Audio;


import Audio.Volume;
import Audio.Segment;
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
   bool decodeFrameToCache(qint64 frameNumber);
 public:
  ArtifactAudioLayer();
  ~ArtifactAudioLayer();

  void setVolume(float volume);
  float volume() const;
  bool isMuted() const;
  void mute();
  bool loadFromPath(const QString& path);
  QString sourcePath() const;
  bool isLoaded() const;

  // Audio metadata
  double duration() const;
  int sampleRate() const;
  int channelCount() const;
  qint64 totalFrames() const;
  WaveformData buildWaveformData(int displayWidth) const;

  // Cache information
  size_t getCacheSize() const;
  size_t getCacheMemoryUsage() const;
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
