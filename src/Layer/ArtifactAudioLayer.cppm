module;
#include <utility>

#include <QDebug>
#include <QJsonObject>
#include <QVariant>
#include <QUuid>

module Artifact.Layer.Audio;

import std;
import ArtifactCore.Utils.PerformanceProfiler;
import Audio.Panner;

import Property.Abstract;
import Property.Group;
import Audio.LipSyncTrack;
import Audio.SimpleWav;
import Audio.Cache;
import Artifact.Audio.Waveform;
import Artifact.Layer.Switch;
import Artifact.Composition.Abstract;
import Asset.Manager;
import AssetType;

namespace Artifact
{

  class ArtifactAudioLayer::Impl
  {
  public:
   float volume_ = 1.0f;
   float pan_ = 0.0f;
   bool muted_ = false;
   QString sourcePath_;
   QUuid sourceAssetId_;
   ArtifactCore::SimpleWav wav_;
   QVector<float> interleavedPcm_;
   int sourceSampleRate_ = 0;
   int sourceChannelCount_ = 0;
   bool isLoaded_ = false;

   // AudioCache 統合
   ArtifactCore::AudioCache cache_;
   double duration_ = 0.0;
   qint64 totalFrames_ = 0;

   // 最終デコード結果のキャッシュ（シークバック・ループ時にリサンプリングを回避）
    struct ResampledCache {
        qint64 startSample = -1;
        int sampleRate = 0;
        float volume = 1.0f;
        float pan = 0.0f;
        ArtifactCore::AudioSegment segment;
    };
   ResampledCache resampledCache_;

   Impl() = default;
   ~Impl() = default;
  };

ArtifactAudioLayer::ArtifactAudioLayer() : impl_(new Impl())
{
}

ArtifactAudioLayer::~ArtifactAudioLayer()
{
  ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
  delete impl_;
}

void ArtifactAudioLayer::setVolume(float volume)
{
  impl_->volume_ = std::clamp(volume, 0.0f, 2.0f);
  Q_EMIT changed();
}

float ArtifactAudioLayer::volume() const
{
  return impl_->volume_;
}

void ArtifactAudioLayer::setPan(float pan)
{
  impl_->pan_ = std::clamp(pan, -1.0f, 1.0f);
  Q_EMIT changed();
}

float ArtifactAudioLayer::pan() const
{
  return impl_->pan_;
}

bool ArtifactAudioLayer::isMuted() const
{
  return impl_->muted_;
}

void ArtifactAudioLayer::mute()
{
  impl_->muted_ = !impl_->muted_;
  Q_EMIT changed();
}

bool ArtifactAudioLayer::loadFromPath(const QString& path)
{
  const QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) {
    ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
    impl_->sourceAssetId_ = {};
    impl_->isLoaded_ = false;
    impl_->sourcePath_.clear();
    impl_->interleavedPcm_.clear();
    impl_->sourceSampleRate_ = 0;
    impl_->sourceChannelCount_ = 0;
    Q_EMIT changed();
    return false;
  }

  if (!impl_->wav_.loadFromFile(trimmed)) {
    impl_->isLoaded_ = false;
    qWarning() << "[AudioLayer] load failed path=" << trimmed;
    return false;
  }

   const QUuid nextAssetId = ArtifactCore::AssetManager::instance().acquireSource(
       trimmed, ArtifactCore::AssetType::Audio);
   if (nextAssetId.isNull()) {
     impl_->isLoaded_ = false;
     return false;
   }
   ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
   impl_->sourceAssetId_ = nextAssetId;

   impl_->sourcePath_ = trimmed;
   impl_->interleavedPcm_ = impl_->wav_.getAudioData();
   impl_->sourceSampleRate_ = impl_->wav_.sampleRate();
   impl_->sourceChannelCount_ = std::max(1, impl_->wav_.channelCount());
   impl_->totalFrames_ = impl_->wav_.frameCount();
   impl_->duration_ = static_cast<double>(impl_->totalFrames_) / impl_->sourceSampleRate_;
   impl_->isLoaded_ = impl_->totalFrames_ > 0;

   // 新規ロード時はキャッシュクリア
   impl_->cache_.clear();

   qDebug() << "[AudioLayer] loaded path=" << trimmed
            << "sampleRate=" << impl_->sourceSampleRate_
            << "channels=" << impl_->sourceChannelCount_
            << "frames=" << impl_->totalFrames_
            << "duration=" << impl_->duration_ << "sec";
   Q_EMIT changed();
  return impl_->isLoaded_;
}

QString ArtifactAudioLayer::sourcePath() const
{
  return impl_->sourcePath_;
}

QUuid ArtifactAudioLayer::sourceAssetId() const { return impl_->sourceAssetId_; }

bool ArtifactAudioLayer::localizeSourceIdentity()
{
  if (impl_->sourceAssetId_.isNull() || isSourceIdentityLocalized()) return false;
  const QUuid localizedId = ArtifactCore::AssetManager::instance().localizeSource(impl_->sourceAssetId_);
  if (localizedId.isNull()) return false;
  impl_->sourceAssetId_ = localizedId;
  setDirty(LayerDirtyFlag::Property);
  Q_EMIT changed();
  return true;
}

bool ArtifactAudioLayer::relinkSourceIdentityToShared()
{
  if (!isSourceIdentityLocalized() || impl_->sourcePath_.isEmpty()) return false;
  const QUuid sharedId = ArtifactCore::AssetManager::instance().acquireSource(
      impl_->sourcePath_, ArtifactCore::AssetType::Audio);
  if (sharedId.isNull()) return false;
  ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
  impl_->sourceAssetId_ = sharedId;
  setDirty(LayerDirtyFlag::Property);
  Q_EMIT changed();
  return true;
}

bool ArtifactAudioLayer::isSourceIdentityLocalized() const
{
  return ArtifactCore::AssetManager::instance().isLocalizedSource(impl_->sourceAssetId_);
}

bool ArtifactAudioLayer::isLoaded() const
{
  return impl_->isLoaded_;
}

QJsonObject ArtifactAudioLayer::toJson() const
{
  QJsonObject obj = ArtifactAbstractLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Audio);
  obj["audio.sourcePath"] = impl_->sourcePath_;
  obj["audio.sourceAssetId"] = impl_->sourceAssetId_.toString(QUuid::WithoutBraces);
  obj["audio.sourceLocalized"] = isSourceIdentityLocalized();
  obj["audio.volume"] = static_cast<double>(impl_->volume_);
  obj["audio.pan"] = static_cast<double>(impl_->pan_);
  obj["audio.muted"] = impl_->muted_;
  return obj;
}

void ArtifactAudioLayer::fromJsonProperties(const QJsonObject& obj)
{
  ArtifactAbstractLayer::fromJsonProperties(obj);
  if (obj.contains("audio.sourcePath")) {
    loadFromPath(obj.value("audio.sourcePath").toString());
  } else if (obj.contains("sourcePath")) {
    loadFromPath(obj.value("sourcePath").toString());
  }
  if (obj.value(QStringLiteral("audio.sourceLocalized")).toBool(false)) {
    const QUuid savedId(obj.value(QStringLiteral("audio.sourceAssetId")).toString());
    bool restored = false;
    if (!savedId.isNull() && ArtifactCore::AssetManager::instance().acquireExistingSource(savedId)) {
      ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
      impl_->sourceAssetId_ = savedId;
      restored = true;
    }
    if (!restored) localizeSourceIdentity();
  }
  if (obj.contains("audio.volume")) {
    setVolume(static_cast<float>(obj.value("audio.volume").toDouble(1.0)));
  }
  if (obj.contains("audio.pan")) {
    setPan(static_cast<float>(obj.value("audio.pan").toDouble(0.0)));
  }
  if (obj.contains("audio.muted")) {
    impl_->muted_ = obj.value("audio.muted").toBool(false);
  }
}

std::vector<ArtifactCore::PropertyGroup> ArtifactAudioLayer::getLayerPropertyGroups() const
{
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  std::vector<ArtifactCore::PropertyGroup> filteredGroups;
  filteredGroups.reserve(groups.size() + 1);
  for (const auto& group : groups) {
    const QString groupName = group.name();
    if (groupName == QStringLiteral("Transform") ||
        groupName == QStringLiteral("Physics")) {
      continue;
    }
    filteredGroups.push_back(group);
  }

  ArtifactCore::PropertyGroup audioGroup(QStringLiteral("Audio"));

  auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type, const QVariant& value, int priority = 0) {
    return persistentLayerProperty(name, type, value, priority);
  };

  audioGroup.addProperty(makeProp(QStringLiteral("audio.sourcePath"), ArtifactCore::PropertyType::String, impl_->sourcePath_, -130));
  auto localizedProp = makeProp(QStringLiteral("source.localized"), ArtifactCore::PropertyType::Boolean,
                                isSourceIdentityLocalized(), -129);
  localizedProp->setDisplayLabel(QStringLiteral("Localized Source"));
  audioGroup.addProperty(localizedProp);
  auto useCountProp = makeProp(QStringLiteral("source.sharedUseCount"), ArtifactCore::PropertyType::Integer,
                               ArtifactCore::AssetManager::instance().useCount(impl_->sourceAssetId_), -128);
  useCountProp->setDisplayLabel(QStringLiteral("Source Uses"));
  audioGroup.addProperty(useCountProp);
  auto volumeProp = makeProp(QStringLiteral("audio.volume"), ArtifactCore::PropertyType::Float, impl_->volume_, -120);
  volumeProp->setHardRange(0.0, 2.0);
  volumeProp->setSoftRange(0.0, 2.0);
  volumeProp->setStep(0.01);
  volumeProp->setUnit(QStringLiteral("linear"));
  volumeProp->setTooltip(QStringLiteral("Audio gain (0.0 - 2.0)"));
  audioGroup.addProperty(volumeProp);
  auto panProp = makeProp(QStringLiteral("audio.pan"), ArtifactCore::PropertyType::Float, impl_->pan_, -115);
  panProp->setHardRange(-1.0, 1.0);
  panProp->setSoftRange(-1.0, 1.0);
  panProp->setStep(0.01);
  panProp->setUnit(QStringLiteral("pan"));
  panProp->setTooltip(QStringLiteral("Audio pan (-1.0 left, 0.0 center, 1.0 right)"));
  audioGroup.addProperty(panProp);
  audioGroup.addProperty(makeProp(QStringLiteral("audio.muted"), ArtifactCore::PropertyType::Boolean, impl_->muted_, -110));
  audioGroup.addProperty(makeProp(QStringLiteral("audio.sampleRate"), ArtifactCore::PropertyType::Integer, impl_->sourceSampleRate_, -100));
  audioGroup.addProperty(makeProp(QStringLiteral("audio.channels"), ArtifactCore::PropertyType::Integer, impl_->sourceChannelCount_, -90));

  filteredGroups.push_back(audioGroup);
  return filteredGroups;
}

bool ArtifactAudioLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
  if (propertyPath == QStringLiteral("source.localized") ||
      propertyPath == QStringLiteral("source.sharedUseCount")) return false;
  if (propertyPath == QStringLiteral("audio.sourcePath")) {
    return loadFromPath(value.toString());
  }
  if (propertyPath == QStringLiteral("audio.volume")) {
    setVolume(static_cast<float>(value.toDouble()));
    return true;
  }
  if (propertyPath == QStringLiteral("audio.pan")) {
    setPan(static_cast<float>(value.toDouble()));
    return true;
  }
  if (propertyPath == QStringLiteral("audio.muted")) {
    const bool target = value.toBool();
    if (impl_->muted_ != target) {
      impl_->muted_ = target;
      Q_EMIT changed();
    }
    return true;
  }
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

void ArtifactAudioLayer::draw(ArtifactIRenderer* renderer)
{
  Q_UNUSED(renderer);
}

bool ArtifactAudioLayer::hasVideo() const
{
  return false;
}

bool ArtifactAudioLayer::hasAudio() const
{
  return impl_->isLoaded_;
}

double ArtifactAudioLayer::duration() const
{
  return impl_->duration_;
}

int ArtifactAudioLayer::sampleRate() const
{
  return impl_->sourceSampleRate_;
}

int ArtifactAudioLayer::channelCount() const
{
  return impl_->sourceChannelCount_;
}

qint64 ArtifactAudioLayer::totalFrames() const
{
  return impl_->totalFrames_;
}

WaveformData ArtifactAudioLayer::buildWaveformData(int displayWidth) const
{
  WaveformData data;
  data.width = displayWidth;
  data.sampleRate = impl_->sourceSampleRate_;

  if (!impl_->isLoaded_ || impl_->sourceSampleRate_ <= 0 ||
      impl_->sourceChannelCount_ <= 0 || displayWidth <= 0 ||
      impl_->interleavedPcm_.isEmpty()) {
    return data;
  }

  auto *composition = static_cast<ArtifactAbstractComposition *>(this->composition());
  const double compositionFps =
      (composition && composition->frameRate().framerate() > 0.0)
          ? composition->frameRate().framerate()
          : 30.0;

  const qint64 sourceStartFrame =
      std::max<qint64>(0, startTime().framePosition());
  const qint64 sourceDurationFrames =
      std::max<qint64>(1, outPoint().framePosition() - inPoint().framePosition());
  const qint64 sourceFrameCount =
      static_cast<qint64>(impl_->interleavedPcm_.size() /
                          std::max(1, impl_->sourceChannelCount_));

  const qint64 startSample = std::clamp<qint64>(
      std::llround((static_cast<double>(sourceStartFrame) / compositionFps) *
                   impl_->sourceSampleRate_),
      0, std::max<qint64>(0, sourceFrameCount - 1));
  const qint64 sampleCount = std::clamp<qint64>(
      std::llround((static_cast<double>(sourceDurationFrames) / compositionFps) *
                   impl_->sourceSampleRate_),
      1, std::max<qint64>(1, sourceFrameCount - startSample));

  const int firstSample = static_cast<int>(std::clamp<qint64>(startSample, 0, sourceFrameCount));
  const int lastSample = static_cast<int>(std::clamp<qint64>(startSample + sampleCount, firstSample, sourceFrameCount));
  if (firstSample >= lastSample) {
    return data;
  }

  const int sampleSpan = lastSample - firstSample;
  const int channelCount = std::max(1, impl_->sourceChannelCount_);

  data.peaks.resize(displayWidth);
  data.rms.resize(displayWidth);

  float minVal = 0.0f;
  float maxVal = 0.0f;
  for (int bin = 0; bin < displayWidth; ++bin) {
    const int binStart = firstSample +
                         static_cast<int>((static_cast<double>(sampleSpan) * bin) /
                                          std::max(1, displayWidth));
    const int binEnd = firstSample +
                       static_cast<int>((static_cast<double>(sampleSpan) * (bin + 1)) /
                                        std::max(1, displayWidth));
    if (binStart >= binEnd) {
      break;
    }

    float peak = 0.0f;
    float sumSquares = 0.0f;
    int count = 0;
    for (int frame = binStart; frame < binEnd; ++frame) {
      const int base = frame * channelCount;
      float sample = 0.0f;
      for (int ch = 0; ch < channelCount; ++ch) {
        const int index = base + ch;
        if (index < impl_->interleavedPcm_.size()) {
          sample += impl_->interleavedPcm_[index];
        }
      }
      sample /= static_cast<float>(channelCount);
      peak = std::max(peak, std::abs(sample));
      sumSquares += sample * sample;
      ++count;
    }

    const float rms = count > 0
                          ? std::sqrt(sumSquares / static_cast<float>(count))
                          : 0.0f;
    data.peaks[bin] = peak;
    data.rms[bin] = rms;
    minVal = std::min(minVal, -peak);
    maxVal = std::max(maxVal, peak);
  }

  data.minSample = minVal;
  data.maxSample = maxVal;
  return data;
}

QString ArtifactAudioLayer::waveformPreviewSummary(int displayWidth) const
{
  const auto waveform = buildWaveformData(displayWidth);
  return Artifact::waveformPreviewSummary(waveform.peaks, waveform.rms);
}

bool ArtifactAudioLayer::buildLipSyncTrack(ArtifactCore::LipSyncTrack& track,
                                           double frameRate) const
{
  if (!impl_ || !impl_->isLoaded_ || impl_->sourceSampleRate_ <= 0 ||
      impl_->sourceChannelCount_ <= 0 || impl_->interleavedPcm_.isEmpty()) {
    return false;
  }

  if (frameRate <= 0.0) {
    return false;
  }

  ArtifactCore::AudioSegment segment;
  segment.sampleRate = impl_->sourceSampleRate_;
  segment.layout = impl_->sourceChannelCount_ == 1
                       ? ArtifactCore::AudioChannelLayout::Mono
                       : ArtifactCore::AudioChannelLayout::Stereo;
  segment.channelData.resize(impl_->sourceChannelCount_);
  segment.setFrameCount(static_cast<int>(impl_->totalFrames_));

  const int sourceFrames =
      static_cast<int>(std::min<qint64>(impl_->totalFrames_,
                                        impl_->interleavedPcm_.size() /
                                            std::max(1, impl_->sourceChannelCount_)));
  for (int frame = 0; frame < sourceFrames; ++frame) {
    for (int ch = 0; ch < impl_->sourceChannelCount_; ++ch) {
      const int index = frame * impl_->sourceChannelCount_ + ch;
      if (index < impl_->interleavedPcm_.size()) {
        segment.channelData[ch][frame] = impl_->interleavedPcm_[index];
      }
    }
  }

  return track.analyze(segment, frameRate);
}

bool ArtifactAudioLayer::applyLipSyncToSwitchLayer(ArtifactSwitchLayer* switchLayer,
                                                   double frameRate) const
{
  if (!switchLayer) {
    return false;
  }

  ArtifactCore::LipSyncTrack track;
  if (!buildLipSyncTrack(track, frameRate)) {
    return false;
  }

  switchLayer->applyLipSyncTrack(track);
  return true;
}

size_t ArtifactAudioLayer::getCacheSize() const
{
  return impl_->cache_.getCacheSize();
}

size_t ArtifactAudioLayer::getCacheMemoryUsage() const
{
  return impl_->cache_.getMemoryUsage();
}

// フレーム単位のPCMをデコードしてキャッシュに追加
bool ArtifactAudioLayer::decodeFrameToCache(qint64 frameNumber)
{
  ArtifactCore::ScopedPerformanceTimer timer("Audio/Layer/decodeFrameToCache");
  if (frameNumber < 0 || frameNumber >= impl_->totalFrames_) {
    return false;
  }

  // フレームのサンプル範囲を計算
  const qint64 startSample = frameNumber;
  const qint64 endSample = std::min(startSample + impl_->sourceSampleRate_, impl_->totalFrames_);
  const int frameSampleCount = static_cast<int>(endSample - startSample);

  if (frameSampleCount <= 0) return false;

  // AudioSegment を作成
  ArtifactCore::AudioSegment segment;
  segment.sampleRate = impl_->sourceSampleRate_;
  segment.layout = (impl_->sourceChannelCount_ >= 2) ? ArtifactCore::AudioChannelLayout::Stereo : ArtifactCore::AudioChannelLayout::Mono;
  segment.channelData.resize(impl_->sourceChannelCount_);
  segment.setFrameCount(frameSampleCount);

  // PCMデータをコピー
  const int baseIndex = static_cast<int>(startSample * impl_->sourceChannelCount_);
  for (int ch = 0; ch < impl_->sourceChannelCount_; ++ch) {
    for (int s = 0; s < segment.frameCount(); ++s) {
      const int pcmIndex = baseIndex + s * impl_->sourceChannelCount_ + ch;
      if (pcmIndex < impl_->interleavedPcm_.size()) {
        segment.channelData[ch][s] = impl_->interleavedPcm_[pcmIndex];
      } else {
        segment.channelData[ch][s] = 0.0f;
      }
    }
  }

  // キャッシュに追加
  impl_->cache_.addCache(frameNumber, std::move(segment));
  return true;
}

bool ArtifactAudioLayer::getAudio(ArtifactCore::AudioSegment& outSegment,
                                  const FramePosition& start,
                                  int frameCount,
                                  int sampleRate)
{
  ArtifactCore::ScopedPerformanceTimer timer("Audio/Layer/getAudio");
  if (!hasAudio() || impl_->muted_ || frameCount <= 0 || sampleRate <= 0 ||
      impl_->sourceSampleRate_ <= 0 || impl_->sourceChannelCount_ <= 0) {
    return false;
  }

  auto* composition = static_cast<ArtifactAbstractComposition*>(this->composition());
  const double compositionFps =
      (composition && composition->frameRate().framerate() > 0.0)
          ? composition->frameRate().framerate()
          : 30.0;

  const qint64 localFrame =
      start.framePosition() - inPoint().framePosition() + startTime().framePosition();
  const double startSeconds = static_cast<double>(localFrame) / compositionFps;
  const qint64 startSample =
      static_cast<qint64>(std::floor(startSeconds * impl_->sourceSampleRate_));
  const qint64 sourceFrameCount = impl_->interleavedPcm_.size() / std::max(1, impl_->sourceChannelCount_);

  const int outChannels = std::max(1, impl_->sourceChannelCount_);
  AudioChannelLayout outLayout = AudioChannelLayout::Stereo;
  if (outChannels == 1) outLayout = AudioChannelLayout::Mono;
  else if (outChannels == 2) outLayout = AudioChannelLayout::Stereo;
  else if (outChannels == 6) outLayout = AudioChannelLayout::Surround51;
  else if (outChannels == 8) outLayout = AudioChannelLayout::Surround71;
  outSegment.sampleRate = sampleRate;
  outSegment.layout = outLayout;
  outSegment.channelData.resize(outChannels);
  outSegment.setFrameCount(frameCount);
  outSegment.zero();

  if (startSample >= sourceFrameCount) {
    return false;
  }

  // リサンプリング結果キャッシュを確認（チャンネル数も一致するか検証）
  auto& rc = impl_->resampledCache_;
  if (rc.startSample == startSample && rc.sampleRate == sampleRate &&
      rc.volume == impl_->volume_ && rc.pan == impl_->pan_ &&
      rc.segment.channelCount() >= outChannels &&
      rc.segment.frameCount() >= frameCount) {
    for (int ch = 0; ch < outChannels; ++ch) {
      std::copy_n(rc.segment.channelData[ch].data(), frameCount,
                  outSegment.channelData[ch].data());
    }
    return true;
  }

  const int srcChannels = impl_->sourceChannelCount_;
  const float volume = impl_->volume_;

  int producedFrames = 0;
  for (int i = 0; i < frameCount; ++i) {
    const double srcPos = static_cast<double>(startSample) +
        (static_cast<double>(i) * impl_->sourceSampleRate_) / sampleRate;
    const qint64 srcFrame0 = static_cast<qint64>(std::floor(srcPos));
    if (srcFrame0 < 0) continue;
    if (srcFrame0 >= sourceFrameCount) break;

    const qint64 srcFrame1 = srcFrame0 + 1;
    const float t = static_cast<float>(srcPos - static_cast<double>(srcFrame0));
    const int base0 = static_cast<int>(srcFrame0) * srcChannels;
    const int base1 = (srcFrame1 < sourceFrameCount)
        ? static_cast<int>(srcFrame1) * srcChannels : base0;

    if (srcChannels == 1) {
      // Mono source: interpolate and distribute to all output channels
      const float s0 = impl_->interleavedPcm_[base0];
      const float s1 = (srcFrame1 < sourceFrameCount)
          ? impl_->interleavedPcm_[base0]
          : 0.0f;
      const float sample = (s0 + t * (s1 - s0)) * volume;
      for (int ch = 0; ch < outChannels; ++ch) {
        outSegment.channelData[ch][i] = sample;
      }
    } else {
      // Multi-channel source: interpolate each channel independently
      const int copyCh = std::min(srcChannels, outChannels);
      for (int ch = 0; ch < copyCh; ++ch) {
        const float s0 = impl_->interleavedPcm_[base0 + ch];
        const float s1 = (srcFrame1 < sourceFrameCount)
            ? impl_->interleavedPcm_[base1 + ch]
            : 0.0f;
        outSegment.channelData[ch][i] = (s0 + t * (s1 - s0)) * volume;
      }
      // Extra output channels remain zero (already zeroed)
    }
    producedFrames = i + 1;
  }

  // Apply panning only for stereo/mono output (first 2 channels)
  if (outChannels <= 2 && producedFrames > 0) {
    const auto gains = ArtifactCore::AudioPanner::calculateConstantPowerGains(impl_->pan_);
    for (int i = 0; i < producedFrames; ++i) {
      if (outChannels >= 1) outSegment.channelData[0][i] *= gains.channelGains[0];
      if (outChannels >= 2) outSegment.channelData[1][i] *= gains.channelGains[1];
    }
  }

  if (producedFrames > 0) {
    rc.startSample = startSample;
    rc.sampleRate = sampleRate;
    rc.volume = impl_->volume_;
    rc.pan = impl_->pan_;
    rc.segment.sampleRate = sampleRate;
    rc.segment.layout = outLayout;
    rc.segment.channelData.resize(outChannels);
    rc.segment.setFrameCount(producedFrames);
    for (int ch = 0; ch < outChannels; ++ch) {
      std::copy_n(outSegment.channelData[ch].data(), producedFrames,
                  rc.segment.channelData[ch].data());
    }
    return true;
  }

  return false;
}

} // namespace Artifact
