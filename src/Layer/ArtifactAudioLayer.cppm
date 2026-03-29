module;

#include <algorithm>
#include <cmath>
#include <QDebug>
#include <QVariant>

module Artifact.Layer.Audio;

import Property.Abstract;
import Property.Group;
import Audio.SimpleWav;
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

  Impl() = default;
  ~Impl() = default;
 };

 ArtifactAudioLayer::ArtifactAudioLayer():impl_(new Impl())
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
  impl_->isLoaded_ = impl_->wav_.frameCount() > 0;
  qDebug() << "[AudioLayer] loaded path=" << trimmed
           << "sampleRate=" << impl_->sourceSampleRate_
           << "channels=" << impl_->sourceChannelCount_
           << "frames=" << impl_->wav_.frameCount();
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
  return impl_->isLoaded_ && !impl_->interleavedPcm_.isEmpty();
 }

 bool ArtifactAudioLayer::getAudio(ArtifactCore::AudioSegment &outSegment,
                                   const FramePosition &start,
                                   int frameCount,
                                   int sampleRate)
  {
  if (!hasAudio() || impl_->muted_ || frameCount <= 0 || sampleRate <= 0 || impl_->sourceSampleRate_ <= 0 || impl_->sourceChannelCount_ <= 0) {
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
  const qint64 sourceFrameCount =
      impl_->interleavedPcm_.size() / std::max(1, impl_->sourceChannelCount_);

  outSegment.sampleRate = sampleRate;
  outSegment.layout = ArtifactCore::AudioChannelLayout::Stereo;
  outSegment.channelData.resize(2);
  outSegment.setFrameCount(frameCount);
  outSegment.zero();

  if (startSample >= sourceFrameCount) {
   qDebug() << "[AudioLayer] getAudio start beyond source"
            << "startFrame=" << start.framePosition()
            << "localFrame=" << localFrame
            << "startSample=" << startSample
            << "sourceFrames=" << sourceFrameCount;
   return false;
  }

  int producedFrames = 0;
  for (int i = 0; i < frameCount; ++i) {
   // Compute the exact fractional source position for this output sample.
   const double srcPos = static_cast<double>(startSample) +
       (static_cast<double>(i) * impl_->sourceSampleRate_) / sampleRate;
   const qint64 srcFrame0 = static_cast<qint64>(std::floor(srcPos));

   if (srcFrame0 < 0) continue;
   if (srcFrame0 >= sourceFrameCount) break; // monotonically increasing — no more frames

   const qint64 srcFrame1 = srcFrame0 + 1;
   const float t = static_cast<float>(srcPos - static_cast<double>(srcFrame0));

   const int base0 = static_cast<int>(srcFrame0) * impl_->sourceChannelCount_;
   float left, right;
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
    const float l1 = (srcFrame1 < sourceFrameCount) ? impl_->interleavedPcm_[base1]     : 0.0f;
    const float r1 = (srcFrame1 < sourceFrameCount) ? impl_->interleavedPcm_[base1 + 1] : 0.0f;
    left  = l0 + t * (l1 - l0);
    right = r0 + t * (r1 - r0);
   }

   outSegment.channelData[0][i] = left  * impl_->volume_;
   outSegment.channelData[1][i] = right * impl_->volume_;
   producedFrames = i + 1;
  }

  if (producedFrames > 0) {
   static int audioProduceLogCount = 0;
   if (audioProduceLogCount < 8) {
    ++audioProduceLogCount;
    qDebug() << "[AudioLayer] produced audio"
             << "startFrame=" << start.framePosition()
             << "requestedFrames=" << frameCount
             << "producedFrames=" << producedFrames
             << "outputSampleRate=" << sampleRate;
   }
   return true;
  }

  return false;
 }

}
