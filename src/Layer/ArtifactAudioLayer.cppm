module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <mutex>

#include <QDebug>
#include <QJsonObject>
#include <QVariant>
#include <QUuid>

module Artifact.Layer.Audio;

import std;
import ArtifactCore.Utils.PerformanceProfiler;
import Audio.Panner;
import Memory.SharedPtr;

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
import EnvironmentVariable.Expansion;

namespace Artifact
{

  class ArtifactAudioLayer::Impl
  {
  public:
   float volume_ = 1.0f;
   float clipGainDb_ = 0.0f;
   float fadeInSeconds_ = 0.0f;
   float fadeOutSeconds_ = 0.0f;
   std::vector<std::pair<qint64, qint64>> deClickRanges_;
   std::uint64_t deClickRevision_ = 0;
   float deClickThresholdDb_ = -20.0f;
   qint64 deClickMaxClickSamples_ = 64;
   float pan_ = 0.0f;
   bool muted_ = false;
   QString sourcePath_;
   QUuid sourceAssetId_;
   std::uint64_t cachedSourceVersion_ = 0;
   ArtifactCore::SimpleWav wav_;
   SharedPtr<QVector<float>> sharedPcm_;
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
        float clipGainDb = 0.0f;
        float fadeInSeconds = 0.0f;
   float fadeOutSeconds = 0.0f;
        std::uint64_t deClickRevision = 0;
        float deClickThresholdDb = -20.0f;
        qint64 deClickMaxClickSamples = 64;
        float pan = 0.0f;
        ArtifactCore::AudioSegment segment;
    };
   ResampledCache resampledCache_;
   // Guards resampledCache_: getAudio is called from the playback audio
   // path, scrub/UI and render-queue workers concurrently. Critical
   // sections are short (cache probe + memcpy), so a plain mutex is safe
   // for the pull-based audio path.
   mutable std::mutex resampledCacheMutex_;

   void resetResampledCache()
   {
     std::lock_guard<std::mutex> lock(resampledCacheMutex_);
     resampledCache_ = ResampledCache{};
   }

   const QVector<float>& pcm() const
   {
     static const QVector<float> empty;
     return sharedPcm_ ? *sharedPcm_ : empty;
   }

   bool refreshSourceVersionIfNeeded()
   {
     if (sourceAssetId_.isNull()) {
       return false;
     }
     const auto currentVersion = ArtifactCore::AssetManager::instance().sourceVersion(
         sourceAssetId_);
     if (currentVersion == 0 || cachedSourceVersion_ == 0) {
       cachedSourceVersion_ = currentVersion;
       return false;
     }
     if (currentVersion == cachedSourceVersion_) {
       return false;
     }

     cachedSourceVersion_ = currentVersion;
     cache_.clear();
     resetResampledCache();
     if (sourcePath_.isEmpty() || !wav_.loadFromFile(sourcePath_)) {
       sharedPcm_.reset();
       isLoaded_ = false;
       return true;
     }

     sharedPcm_ = ArtifactCore::staticPointerCast<QVector<float>>(
         ArtifactCore::AssetManager::instance().decodedPayload(
             sourceAssetId_, currentVersion, QStringLiteral("audio.pcm.f32")));
     if (!sharedPcm_) {
       sharedPcm_ = ArtifactCore::makeShared<QVector<float>>(wav_.getAudioData());
       sharedPcm_ = ArtifactCore::staticPointerCast<QVector<float>>(
           ArtifactCore::AssetManager::instance().publishDecodedPayload(
               sourceAssetId_, currentVersion, QStringLiteral("audio.pcm.f32"),
               sharedPcm_));
     }
     sourceSampleRate_ = wav_.sampleRate();
     sourceChannelCount_ = std::max(1, wav_.channelCount());
     totalFrames_ = wav_.frameCount();
     duration_ = sourceSampleRate_ > 0
                     ? static_cast<double>(totalFrames_) / sourceSampleRate_
                     : 0.0;
     isLoaded_ = totalFrames_ > 0 && sharedPcm_ && !sharedPcm_->isEmpty();
     return true;
   }

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
  impl_->volume_ = std::isfinite(volume) ? std::clamp(volume, 0.0f, 2.0f) : 1.0f;
  Q_EMIT changed();
}

float ArtifactAudioLayer::volume() const
{
  return impl_->volume_;
}

void ArtifactAudioLayer::setClipGainDb(float gainDb)
{
  impl_->clipGainDb_ = std::isfinite(gainDb) ? std::clamp(gainDb, -60.0f, 12.0f) : 0.0f;
  Q_EMIT changed();
}

float ArtifactAudioLayer::clipGainDb() const
{
  return impl_->clipGainDb_;
}

void ArtifactAudioLayer::setFadeInSeconds(float seconds)
{
  impl_->fadeInSeconds_ = std::isfinite(seconds) ? std::max(0.0f, seconds) : 0.0f;
  Q_EMIT changed();
}

float ArtifactAudioLayer::fadeInSeconds() const
{
  return impl_->fadeInSeconds_;
}

void ArtifactAudioLayer::setFadeOutSeconds(float seconds)
{
  impl_->fadeOutSeconds_ = std::isfinite(seconds) ? std::max(0.0f, seconds) : 0.0f;
  Q_EMIT changed();
}

float ArtifactAudioLayer::fadeOutSeconds() const
{
  return impl_->fadeOutSeconds_;
}

void ArtifactAudioLayer::addDeClickRange(qint64 startSample, qint64 endSample)
{
  const qint64 start = std::max<qint64>(0, std::min(startSample, endSample));
  const qint64 end = std::max<qint64>(start, std::max(startSample, endSample));
  if (end <= start) return;
  impl_->deClickRanges_.emplace_back(start, end);
  std::sort(impl_->deClickRanges_.begin(), impl_->deClickRanges_.end());
  std::vector<std::pair<qint64, qint64>> merged;
  merged.reserve(impl_->deClickRanges_.size());
  for (const auto& range : impl_->deClickRanges_) {
    if (!merged.empty() && range.first <= merged.back().second) {
      merged.back().second = std::max(merged.back().second, range.second);
    } else {
      merged.push_back(range);
    }
  }
  impl_->deClickRanges_ = std::move(merged);
  ++impl_->deClickRevision_;
  impl_->resetResampledCache();
  Q_EMIT changed();
}

void ArtifactAudioLayer::clearDeClickRanges()
{
  if (impl_->deClickRanges_.empty()) return;
  impl_->deClickRanges_.clear();
  ++impl_->deClickRevision_;
  impl_->resetResampledCache();
  Q_EMIT changed();
}

int ArtifactAudioLayer::deClickRangeCount() const
{
  return static_cast<int>(impl_->deClickRanges_.size());
}

std::vector<std::pair<qint64, qint64>> ArtifactAudioLayer::deClickRanges() const
{
  return impl_->deClickRanges_;
}

void ArtifactAudioLayer::setDeClickThresholdDb(float thresholdDb)
{
  impl_->deClickThresholdDb_ = std::isfinite(thresholdDb)
      ? std::clamp(thresholdDb, -80.0f, 0.0f) : -20.0f;
  ++impl_->deClickRevision_;
  impl_->resetResampledCache();
  Q_EMIT changed();
}

float ArtifactAudioLayer::deClickThresholdDb() const
{
  return impl_->deClickThresholdDb_;
}

void ArtifactAudioLayer::setDeClickMaxClickSamples(qint64 samples)
{
  impl_->deClickMaxClickSamples_ = std::clamp<qint64>(samples, 1, 4096);
  ++impl_->deClickRevision_;
  impl_->resetResampledCache();
  Q_EMIT changed();
}

qint64 ArtifactAudioLayer::deClickMaxClickSamples() const
{
  return impl_->deClickMaxClickSamples_;
}

void ArtifactAudioLayer::setPan(float pan)
{
  impl_->pan_ = std::isfinite(pan) ? std::clamp(pan, -1.0f, 1.0f) : 0.0f;
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
  // テンプレート ($VAR 等) はメンバ保持し、ファイル IO のみ展開結果を使う。
  const QString templatePath = path.trimmed();
  QString trimmed = templatePath;
  if (containsExpansionMarker(trimmed)) {
      ExpansionContext expansionContext;
      trimmed = expandTokens(trimmed, expansionContext);
  }
  if (templatePath.isEmpty()) {
    ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
    impl_->sourceAssetId_ = {};
    impl_->cachedSourceVersion_ = 0;
    impl_->isLoaded_ = false;
    impl_->sourcePath_.clear();
    impl_->sharedPcm_.reset();
    impl_->sourceSampleRate_ = 0;
    impl_->sourceChannelCount_ = 0;
    impl_->deClickRanges_.clear();
    ++impl_->deClickRevision_;
    impl_->resetResampledCache();
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
   impl_->cachedSourceVersion_ = ArtifactCore::AssetManager::instance().sourceVersion(nextAssetId);

   impl_->sourcePath_ = templatePath;
   const auto sourceVersion = ArtifactCore::AssetManager::instance().sourceVersion(nextAssetId);
   impl_->sharedPcm_ = ArtifactCore::staticPointerCast<QVector<float>>(
       ArtifactCore::AssetManager::instance().decodedPayload(
           nextAssetId, sourceVersion, QStringLiteral("audio.pcm.f32")));
   if (!impl_->sharedPcm_) {
     impl_->sharedPcm_ = ArtifactCore::makeShared<QVector<float>>(impl_->wav_.getAudioData());
     impl_->sharedPcm_ = ArtifactCore::staticPointerCast<QVector<float>>(
         ArtifactCore::AssetManager::instance().publishDecodedPayload(
             nextAssetId, sourceVersion, QStringLiteral("audio.pcm.f32"), impl_->sharedPcm_));
   }
   impl_->sourceSampleRate_ = impl_->wav_.sampleRate();
   impl_->sourceChannelCount_ = std::max(1, impl_->wav_.channelCount());
   impl_->totalFrames_ = impl_->wav_.frameCount();
   impl_->duration_ = impl_->sourceSampleRate_ > 0
       ? static_cast<double>(impl_->totalFrames_) / impl_->sourceSampleRate_
       : 0.0;
   impl_->isLoaded_ = impl_->totalFrames_ > 0 && impl_->sourceSampleRate_ > 0;

   // 新規ロード時はキャッシュクリア
   impl_->cache_.clear();
   impl_->deClickRanges_.clear();
   ++impl_->deClickRevision_;
   impl_->resetResampledCache();

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
  impl_->cachedSourceVersion_ = ArtifactCore::AssetManager::instance().sourceVersion(localizedId);
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
  impl_->cachedSourceVersion_ = ArtifactCore::AssetManager::instance().sourceVersion(sharedId);
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
  obj["audio.clipGainDb"] = static_cast<double>(impl_->clipGainDb_);
  obj["audio.fadeInSeconds"] = static_cast<double>(impl_->fadeInSeconds_);
  obj["audio.fadeOutSeconds"] = static_cast<double>(impl_->fadeOutSeconds_);
  QJsonArray deClickRanges;
  for (const auto& range : impl_->deClickRanges_) {
    QJsonObject item;
    item["startSample"] = static_cast<double>(range.first);
    item["endSample"] = static_cast<double>(range.second);
    deClickRanges.append(item);
  }
  obj["audio.deClickRanges"] = deClickRanges;
  obj["audio.deClickThresholdDb"] = static_cast<double>(impl_->deClickThresholdDb_);
  obj["audio.deClickMaxClickSamples"] = static_cast<double>(impl_->deClickMaxClickSamples_);
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
      impl_->cachedSourceVersion_ = ArtifactCore::AssetManager::instance().sourceVersion(savedId);
      restored = true;
    }
    if (!restored) localizeSourceIdentity();
  }
  if (obj.contains("audio.volume")) {
    setVolume(static_cast<float>(obj.value("audio.volume").toDouble(1.0)));
  }
  if (obj.contains("audio.clipGainDb")) {
    setClipGainDb(static_cast<float>(obj.value("audio.clipGainDb").toDouble(0.0)));
  }
  if (obj.contains("audio.fadeInSeconds")) {
    setFadeInSeconds(static_cast<float>(obj.value("audio.fadeInSeconds").toDouble(0.0)));
  }
  if (obj.contains("audio.fadeOutSeconds")) {
    setFadeOutSeconds(static_cast<float>(obj.value("audio.fadeOutSeconds").toDouble(0.0)));
  }
  clearDeClickRanges();
  const auto deClickRanges = obj.value("audio.deClickRanges").toArray();
  for (const auto& value : deClickRanges) {
    const auto item = value.toObject();
    addDeClickRange(static_cast<qint64>(item.value("startSample").toDouble()),
                    static_cast<qint64>(item.value("endSample").toDouble()));
  }
  if (obj.contains("audio.deClickThresholdDb")) {
    setDeClickThresholdDb(static_cast<float>(obj.value("audio.deClickThresholdDb").toDouble(-20.0)));
  }
  if (obj.contains("audio.deClickMaxClickSamples")) {
    setDeClickMaxClickSamples(static_cast<qint64>(obj.value("audio.deClickMaxClickSamples").toDouble(64.0)));
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
  auto clipGainProp = makeProp(QStringLiteral("audio.clipGainDb"), ArtifactCore::PropertyType::Float,
                               impl_->clipGainDb_, -119);
  clipGainProp->setDisplayLabel(QStringLiteral("Clip Gain"));
  clipGainProp->setHardRange(-60.0, 12.0);
  clipGainProp->setSoftRange(-12.0, 6.0);
  clipGainProp->setStep(0.1);
  clipGainProp->setUnit(QStringLiteral("dB"));
  clipGainProp->setTooltip(QStringLiteral("Non-destructive clip gain"));
  audioGroup.addProperty(clipGainProp);
  auto fadeInProp = makeProp(QStringLiteral("audio.fadeInSeconds"), ArtifactCore::PropertyType::Float,
                             impl_->fadeInSeconds_, -118);
  fadeInProp->setDisplayLabel(QStringLiteral("Fade In"));
  fadeInProp->setHardRange(0.0, 3600.0);
  fadeInProp->setSoftRange(0.0, 10.0);
  fadeInProp->setStep(0.01);
  fadeInProp->setUnit(QStringLiteral("s"));
  audioGroup.addProperty(fadeInProp);
  auto fadeOutProp = makeProp(QStringLiteral("audio.fadeOutSeconds"), ArtifactCore::PropertyType::Float,
                              impl_->fadeOutSeconds_, -117);
  fadeOutProp->setDisplayLabel(QStringLiteral("Fade Out"));
  fadeOutProp->setHardRange(0.0, 3600.0);
  fadeOutProp->setSoftRange(0.0, 10.0);
  fadeOutProp->setStep(0.01);
  fadeOutProp->setUnit(QStringLiteral("s"));
  audioGroup.addProperty(fadeOutProp);
  auto deClickThresholdProp = makeProp(
      QStringLiteral("audio.deClickThresholdDb"), ArtifactCore::PropertyType::Float,
      impl_->deClickThresholdDb_, -116);
  deClickThresholdProp->setDisplayLabel(QStringLiteral("De-click Threshold"));
  deClickThresholdProp->setHardRange(-80.0, 0.0);
  deClickThresholdProp->setSoftRange(-40.0, -6.0);
  deClickThresholdProp->setStep(0.5);
  deClickThresholdProp->setUnit(QStringLiteral("dB"));
  deClickThresholdProp->setTooltip(QStringLiteral("Detection threshold for persisted de-click repairs"));
  audioGroup.addProperty(deClickThresholdProp);
  auto deClickWidthProp = makeProp(
      QStringLiteral("audio.deClickMaxClickSamples"), ArtifactCore::PropertyType::Integer,
      static_cast<qint64>(impl_->deClickMaxClickSamples_), -115);
  deClickWidthProp->setDisplayLabel(QStringLiteral("De-click Max Width"));
  deClickWidthProp->setHardRange(1.0, 4096.0);
  deClickWidthProp->setSoftRange(1.0, 256.0);
  deClickWidthProp->setUnit(QStringLiteral("samples"));
  deClickWidthProp->setTooltip(QStringLiteral("Maximum isolated click width to repair"));
  audioGroup.addProperty(deClickWidthProp);
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
  if (propertyPath == QStringLiteral("audio.clipGainDb")) {
    setClipGainDb(static_cast<float>(value.toDouble()));
    return true;
  }
  if (propertyPath == QStringLiteral("audio.fadeInSeconds")) {
    setFadeInSeconds(static_cast<float>(value.toDouble()));
    return true;
  }
  if (propertyPath == QStringLiteral("audio.fadeOutSeconds")) {
    setFadeOutSeconds(static_cast<float>(value.toDouble()));
    return true;
  }
  if (propertyPath == QStringLiteral("audio.deClickThresholdDb")) {
    setDeClickThresholdDb(static_cast<float>(value.toDouble()));
    return true;
  }
  if (propertyPath == QStringLiteral("audio.deClickMaxClickSamples")) {
    setDeClickMaxClickSamples(static_cast<qint64>(value.toLongLong()));
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
  impl_->refreshSourceVersionIfNeeded();
  return impl_->isLoaded_;
}

double ArtifactAudioLayer::duration() const
{
  impl_->refreshSourceVersionIfNeeded();
  return impl_->duration_;
}

int ArtifactAudioLayer::sampleRate() const
{
  impl_->refreshSourceVersionIfNeeded();
  return impl_->sourceSampleRate_;
}

int ArtifactAudioLayer::channelCount() const
{
  impl_->refreshSourceVersionIfNeeded();
  return impl_->sourceChannelCount_;
}

qint64 ArtifactAudioLayer::totalFrames() const
{
  impl_->refreshSourceVersionIfNeeded();
  return impl_->totalFrames_;
}

WaveformData ArtifactAudioLayer::buildWaveformData(int displayWidth) const
{
  impl_->refreshSourceVersionIfNeeded();
  WaveformData data;
  data.width = displayWidth;
  data.sampleRate = impl_->sourceSampleRate_;

  if (!impl_->isLoaded_ || impl_->sourceSampleRate_ <= 0 ||
      impl_->sourceChannelCount_ <= 0 || displayWidth <= 0 ||
      impl_->pcm().isEmpty()) {
    return data;
  }

  auto *composition = static_cast<ArtifactAbstractComposition *>(this->composition());
  const double requestedFps = composition ? composition->frameRate().framerate() : 0.0;
  const double compositionFps =
      (std::isfinite(requestedFps) && requestedFps > 0.0) ? requestedFps : 30.0;

  const qint64 sourceStartFrame =
      std::max<qint64>(0, startTime().framePosition());
  const qint64 sourceFrameCount =
      static_cast<qint64>(impl_->pcm().size() /
                          std::max(1, impl_->sourceChannelCount_));

  const long double sourceStartSamples =
      static_cast<long double>(sourceStartFrame) /
      static_cast<long double>(compositionFps) * impl_->sourceSampleRate_;
  const long double durationFrames = std::max<long double>(
      1.0L,
      static_cast<long double>(outPoint().framePosition()) -
      static_cast<long double>(inPoint().framePosition()));
  const long double sourceDurationSamples =
      durationFrames / static_cast<long double>(compositionFps) *
      impl_->sourceSampleRate_;
  const auto clampRounded = [](long double value, qint64 lower, qint64 upper) {
    if (!std::isfinite(value)) return lower;
    const long double rounded = std::round(value);
    if (rounded <= static_cast<long double>(lower)) return lower;
    if (rounded >= static_cast<long double>(upper)) return upper;
    return static_cast<qint64>(rounded);
  };
  const qint64 startSample = clampRounded(
      sourceStartSamples, 0, std::max<qint64>(0, sourceFrameCount - 1));
  const qint64 sampleCount = clampRounded(
      sourceDurationSamples, 1, std::max<qint64>(1, sourceFrameCount - startSample));

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
        if (index < impl_->pcm().size()) {
          sample += impl_->pcm()[index];
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
  impl_->refreshSourceVersionIfNeeded();
  if (!impl_ || !impl_->isLoaded_ || impl_->sourceSampleRate_ <= 0 ||
      impl_->sourceChannelCount_ <= 0 || impl_->pcm().isEmpty()) {
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
                                        impl_->pcm().size() /
                                            std::max(1, impl_->sourceChannelCount_)));
  for (int frame = 0; frame < sourceFrames; ++frame) {
    for (int ch = 0; ch < impl_->sourceChannelCount_; ++ch) {
      const int index = frame * impl_->sourceChannelCount_ + ch;
      if (index < impl_->pcm().size()) {
        segment.channelData[ch][frame] = impl_->pcm()[index];
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
  impl_->refreshSourceVersionIfNeeded();
  return impl_->cache_.getCacheSize();
}

size_t ArtifactAudioLayer::getCacheMemoryUsage() const
{
  impl_->refreshSourceVersionIfNeeded();
  return impl_->cache_.getMemoryUsage();
}

// フレーム単位のPCMをデコードしてキャッシュに追加
bool ArtifactAudioLayer::decodeFrameToCache(qint64 frameNumber)
{
  impl_->refreshSourceVersionIfNeeded();
  ArtifactCore::ScopedPerformanceTimer timer("Audio/Layer/decodeFrameToCache");
  const int channelCount = std::max(1, impl_->sourceChannelCount_);
  const qint64 pcmFrameCount = impl_->pcm().size() / channelCount;
  const qint64 availableFrameCount = std::min(impl_->totalFrames_, pcmFrameCount);
  if (frameNumber < 0 || frameNumber >= availableFrameCount ||
      impl_->sourceSampleRate_ <= 0) {
    return false;
  }

  // フレームのサンプル範囲を計算
  const qint64 startSample = frameNumber;
  const qint64 requestedFrameCount = std::min<qint64>(
      static_cast<qint64>(impl_->sourceSampleRate_),
      availableFrameCount - startSample);
  const qint64 endSample = startSample + requestedFrameCount;
  const int frameSampleCount = static_cast<int>(endSample - startSample);

  if (frameSampleCount <= 0) return false;

  // AudioSegment を作成
  ArtifactCore::AudioSegment segment;
  segment.sampleRate = impl_->sourceSampleRate_;
  segment.layout = (impl_->sourceChannelCount_ >= 2) ? ArtifactCore::AudioChannelLayout::Stereo : ArtifactCore::AudioChannelLayout::Mono;
  segment.channelData.resize(impl_->sourceChannelCount_);
  segment.setFrameCount(frameSampleCount);

  // PCMデータをコピー
  const qsizetype baseIndex = static_cast<qsizetype>(startSample) * channelCount;
  for (int ch = 0; ch < channelCount; ++ch) {
    for (int s = 0; s < segment.frameCount(); ++s) {
      const qsizetype pcmIndex = baseIndex + static_cast<qsizetype>(s) * channelCount + ch;
      if (pcmIndex < impl_->pcm().size()) {
        segment.channelData[ch][s] = impl_->pcm()[pcmIndex];
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
  const double requestedFps = composition ? composition->frameRate().framerate() : 0.0;
  const double compositionFps =
      (std::isfinite(requestedFps) && requestedFps > 0.0) ? requestedFps : 30.0;

  const qint64 sourceFrameCount = impl_->pcm().size() / std::max(1, impl_->sourceChannelCount_);
  // Time-remapped layers map each composition frame through the remap curve
  // (same convention as the video layer); otherwise the layer-local offset applies.
  const bool useTimeRemap = isTimeRemapEnabled();
  long double samplePosition;
  if (useTimeRemap) {
      samplePosition =
          static_cast<long double>(getSourceFrameAtCompFrame(start.framePosition())) /
          static_cast<long double>(compositionFps) *
          static_cast<long double>(impl_->sourceSampleRate_);
  } else {
      const long double localFrame =
          static_cast<long double>(start.framePosition()) -
          static_cast<long double>(inPoint().framePosition()) +
          static_cast<long double>(startTime().framePosition());
      samplePosition =
          localFrame / static_cast<long double>(compositionFps) *
          static_cast<long double>(impl_->sourceSampleRate_);
  }

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

  if (!std::isfinite(samplePosition) || samplePosition < 0.0L ||
      samplePosition >= static_cast<long double>(sourceFrameCount)) {
    return false;
  }
  const qint64 startSample = static_cast<qint64>(std::floor(samplePosition));

  // リサンプリング結果キャッシュを確認（チャンネル数も一致するか検証）
  // Time-remapped output is not linear in startSample, so bypass the cache.
  auto& rc = impl_->resampledCache_;
  {
    std::lock_guard<std::mutex> cacheLock(impl_->resampledCacheMutex_);
    if (!useTimeRemap &&
        rc.startSample == startSample && rc.sampleRate == sampleRate &&
        rc.volume == impl_->volume_ && rc.pan == impl_->pan_ &&
        rc.clipGainDb == impl_->clipGainDb_ &&
        rc.fadeInSeconds == impl_->fadeInSeconds_ &&
        rc.fadeOutSeconds == impl_->fadeOutSeconds_ &&
        rc.deClickRevision == impl_->deClickRevision_ &&
        rc.deClickThresholdDb == impl_->deClickThresholdDb_ &&
        rc.deClickMaxClickSamples == impl_->deClickMaxClickSamples_ &&
        rc.segment.channelCount() >= outChannels &&
        rc.segment.frameCount() >= frameCount) {
      for (int ch = 0; ch < outChannels; ++ch) {
        std::copy_n(rc.segment.channelData[ch].data(), frameCount,
                    outSegment.channelData[ch].data());
      }
      return true;
    }
  }

  const int srcChannels = impl_->sourceChannelCount_;
  const float volume = impl_->volume_;
  const float clipGain = std::pow(10.0f, impl_->clipGainDb_ / 20.0f);
  const double clipDurationSeconds = static_cast<double>(sourceFrameCount) /
      std::max(1, impl_->sourceSampleRate_);

  int producedFrames = 0;
  for (int i = 0; i < frameCount; ++i) {
    // Non-remap: constant-rate resample from the block start. Time-remap:
    // each output frame maps through the remap curve (variable speed).
    const double srcPos = useTimeRemap
        ? static_cast<double>(getSourceFrameAtCompFrame(
              start.framePosition() + i)) *
              impl_->sourceSampleRate_ / compositionFps
        : static_cast<double>(startSample) +
              (static_cast<double>(i) * impl_->sourceSampleRate_) / sampleRate;
    const qint64 srcFrame0 = static_cast<qint64>(std::floor(srcPos));
    if (srcFrame0 < 0) continue;
    if (srcFrame0 >= sourceFrameCount) break;

    const qint64 srcFrame1 = srcFrame0 + 1;
    const float t = static_cast<float>(srcPos - static_cast<double>(srcFrame0));
    const int base0 = static_cast<int>(srcFrame0) * srcChannels;
    const int base1 = (srcFrame1 < sourceFrameCount)
        ? static_cast<int>(srcFrame1) * srcChannels : base0;

    const double sourceTimeSeconds = srcPos / std::max(1, impl_->sourceSampleRate_);
    float fade = 1.0f;
    if (impl_->fadeInSeconds_ > 0.0f) {
      fade *= static_cast<float>(std::clamp(
          sourceTimeSeconds / static_cast<double>(impl_->fadeInSeconds_), 0.0, 1.0));
    }
    if (impl_->fadeOutSeconds_ > 0.0f) {
      fade *= static_cast<float>(std::clamp(
          (clipDurationSeconds - sourceTimeSeconds) /
              static_cast<double>(impl_->fadeOutSeconds_), 0.0, 1.0));
    }
    const float clipScale = clipGain * fade;

    if (srcChannels == 1) {
      // Mono source: interpolate and distribute to all output channels
      const float s0 = impl_->pcm()[base0];
      const float s1 = (srcFrame1 < sourceFrameCount)
          ? impl_->pcm()[base1]
          : 0.0f;
      const float sample = (s0 + t * (s1 - s0)) * volume * clipScale;
      for (int ch = 0; ch < outChannels; ++ch) {
        outSegment.channelData[ch][i] = sample;
      }
    } else {
      // Multi-channel source: interpolate each channel independently
      const int copyCh = std::min(srcChannels, outChannels);
      for (int ch = 0; ch < copyCh; ++ch) {
        const float s0 = impl_->pcm()[base0 + ch];
        const float s1 = (srcFrame1 < sourceFrameCount)
            ? impl_->pcm()[base1 + ch]
            : 0.0f;
        outSegment.channelData[ch][i] = (s0 + t * (s1 - s0)) * volume * clipScale;
      }
      // Extra output channels remain zero (already zeroed)
    }
    producedFrames = i + 1;
  }

  // Apply panning only for stereo/mono output (first 2 channels)
  if (producedFrames > 0 && !impl_->deClickRanges_.empty()) {
    AudioSyncTools deClickTools;
    for (const auto& range : impl_->deClickRanges_) {
      const qint64 rangeStart = std::max(range.first, startSample);
      const qint64 rangeEnd = std::min(range.second,
          startSample + static_cast<qint64>(std::ceil(
              producedFrames * static_cast<double>(impl_->sourceSampleRate_) /
              std::max(1, sampleRate))));
      if (rangeEnd <= rangeStart) continue;
      const qint64 localStart = static_cast<qint64>(std::floor(
          (rangeStart - startSample) * static_cast<double>(sampleRate) /
          std::max(1, impl_->sourceSampleRate_)));
      const qint64 localEnd = static_cast<qint64>(std::ceil(
          (rangeEnd - startSample) * static_cast<double>(sampleRate) /
          std::max(1, impl_->sourceSampleRate_)));
      const qint64 localCount = std::min<qint64>(producedFrames, localEnd) -
          std::max<qint64>(0, localStart);
      if (localCount > 0) {
        outSegment = deClickTools.deClick(outSegment,
            std::max<qint64>(0, localStart), localCount,
            impl_->deClickThresholdDb_, impl_->deClickMaxClickSamples_);
      }
    }
  }

  // Apply panning only for stereo/mono output (first 2 channels)
  if (outChannels <= 2 && producedFrames > 0) {
    const auto gains = ArtifactCore::AudioPanner::calculateConstantPowerGains(impl_->pan_);
    for (int i = 0; i < producedFrames; ++i) {
      if (outChannels >= 1) outSegment.channelData[0][i] *= gains.channelGains[0];
      if (outChannels >= 2) outSegment.channelData[1][i] *= gains.channelGains[1];
    }
  }

  if (producedFrames > 0 && !useTimeRemap) {
    std::lock_guard<std::mutex> cacheLock(impl_->resampledCacheMutex_);
    rc.startSample = startSample;
    rc.sampleRate = sampleRate;
    rc.volume = impl_->volume_;
    rc.clipGainDb = impl_->clipGainDb_;
    rc.fadeInSeconds = impl_->fadeInSeconds_;
    rc.fadeOutSeconds = impl_->fadeOutSeconds_;
    rc.deClickRevision = impl_->deClickRevision_;
    rc.deClickThresholdDb = impl_->deClickThresholdDb_;
    rc.deClickMaxClickSamples = impl_->deClickMaxClickSamples_;
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
