module;

#include <QDebug>
#include <QJsonObject>
#include <QVariant>

module Artifact.Layer.Audio;

import std;

import Property.Abstract;
import Property.Group;
import Audio.SimpleWav;
import Audio.Cache;
import Artifact.Composition.Abstract;

namespace Artifact
{

  class ArtifactAudioLayer::Impl
  {
  public:
   float volume_ = 1.0f;
   bool muted_ = false;
   QString sourcePath_;
   ArtifactCore::SimpleWav wav_;
   QVector<float> interleavedPcm_;
   int sourceSampleRate_ = 0;
   int sourceChannelCount_ = 0;
   bool isLoaded_ = false;

   // AudioCache 統合
   ArtifactCore::AudioCache cache_;
   double duration_ = 0.0;
   qint64 totalFrames_ = 0;

   Impl() = default;
   ~Impl() = default;
  };

ArtifactAudioLayer::ArtifactAudioLayer() : impl_(new Impl())
{
}

ArtifactAudioLayer::~ArtifactAudioLayer()
{
  delete impl_;
}

void ArtifactAudioLayer::setVolume(float volume)
{
  impl_->volume_ = std::clamp(volume, 0.0f, 1.0f);
  Q_EMIT changed();
}

float ArtifactAudioLayer::volume() const
{
  return impl_->volume_;
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

bool ArtifactAudioLayer::isLoaded() const
{
  return impl_->isLoaded_;
}

QJsonObject ArtifactAudioLayer::toJson() const
{
  QJsonObject obj = ArtifactAbstractLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Audio);
  obj["audio.sourcePath"] = impl_->sourcePath_;
  obj["audio.volume"] = static_cast<double>(impl_->volume_);
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
  if (obj.contains("audio.volume")) {
    setVolume(static_cast<float>(obj.value("audio.volume").toDouble(1.0)));
  }
  if (obj.contains("audio.muted")) {
    impl_->muted_ = obj.value("audio.muted").toBool(false);
  }
}

std::vector<ArtifactCore::PropertyGroup> ArtifactAudioLayer::getLayerPropertyGroups() const
{
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup audioGroup(QStringLiteral("Audio"));

  auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type, const QVariant& value, int priority = 0) {
    return persistentLayerProperty(name, type, value, priority);
  };

  audioGroup.addProperty(makeProp(QStringLiteral("audio.sourcePath"), ArtifactCore::PropertyType::String, impl_->sourcePath_, -130));
  auto volumeProp = makeProp(QStringLiteral("audio.volume"), ArtifactCore::PropertyType::Float, impl_->volume_, -120);
  volumeProp->setHardRange(0.0, 1.0);
  volumeProp->setSoftRange(0.0, 1.0);
  volumeProp->setStep(0.01);
  volumeProp->setUnit(QStringLiteral("linear"));
  volumeProp->setTooltip(QStringLiteral("Audio gain (0.0 - 1.0)"));
  audioGroup.addProperty(volumeProp);
  audioGroup.addProperty(makeProp(QStringLiteral("audio.muted"), ArtifactCore::PropertyType::Boolean, impl_->muted_, -110));
  audioGroup.addProperty(makeProp(QStringLiteral("audio.sampleRate"), ArtifactCore::PropertyType::Integer, impl_->sourceSampleRate_, -100));
  audioGroup.addProperty(makeProp(QStringLiteral("audio.channels"), ArtifactCore::PropertyType::Integer, impl_->sourceChannelCount_, -90));

  groups.push_back(audioGroup);
  return groups;
}

bool ArtifactAudioLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
  if (propertyPath == QStringLiteral("audio.sourcePath")) {
    return loadFromPath(value.toString());
  }
  if (propertyPath == QStringLiteral("audio.volume")) {
    setVolume(static_cast<float>(value.toDouble()));
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

  outSegment.sampleRate = sampleRate;
  outSegment.layout = ArtifactCore::AudioChannelLayout::Stereo;
  outSegment.channelData.resize(2);
  outSegment.setFrameCount(frameCount);
  outSegment.zero();

  if (startSample >= sourceFrameCount) {
    return false;
  }

  int producedFrames = 0;
  for (int i = 0; i < frameCount; ++i) {
    const double srcPos = static_cast<double>(startSample) +
        (static_cast<double>(i) * impl_->sourceSampleRate_) / sampleRate;
    const qint64 srcFrame0 = static_cast<qint64>(std::floor(srcPos));

    if (srcFrame0 < 0) continue;
    if (srcFrame0 >= sourceFrameCount) break;

    const qint64 srcFrame1 = srcFrame0 + 1;
    const float t = static_cast<float>(srcPos - static_cast<double>(srcFrame0));

    const int base0 = static_cast<int>(srcFrame0) * impl_->sourceChannelCount_;
    float left = 0.0f;
    float right = 0.0f;
    if (impl_->sourceChannelCount_ == 1) {
      const float s0 = impl_->interleavedPcm_[base0];
      const float s1 = (srcFrame1 < sourceFrameCount)
          ? impl_->interleavedPcm_[static_cast<int>(srcFrame1) * impl_->sourceChannelCount_]
          : 0.0f;
      left = right = s0 + t * (s1 - s0);
    } else {
      const int base1 = (srcFrame1 < sourceFrameCount)
          ? static_cast<int>(srcFrame1) * impl_->sourceChannelCount_ : base0;
      const float l0 = impl_->interleavedPcm_[base0];
      const float r0 = impl_->interleavedPcm_[base0 + 1];
      const float l1 = (srcFrame1 < sourceFrameCount) ? impl_->interleavedPcm_[base1] : 0.0f;
      const float r1 = (srcFrame1 < sourceFrameCount) ? impl_->interleavedPcm_[base1 + 1] : 0.0f;
      left  = l0 + t * (l1 - l0);
      right = r0 + t * (r1 - r0);
    }

    outSegment.channelData[0][i] = left * impl_->volume_;
    outSegment.channelData[1][i] = right * impl_->volume_;
    producedFrames = i + 1;
  }

  if (producedFrames > 0) {
    // デバッグログ（キャッシュ統計）
    static int audioProduceLogCount = 0;
    if (audioProduceLogCount < 8) {
      ++audioProduceLogCount;
      qDebug() << "[AudioLayer] getAudio produced" << producedFrames << "frames"
               << "cache size:" << impl_->cache_.getCacheSize()
               << "memory:" << impl_->cache_.getMemoryUsage() << "bytes";
    }
    return true;
  }

  return false;
}

} // namespace Artifact
