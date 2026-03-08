module;

#include <algorithm>
#include <QVariant>

module Artifact.Layer.Audio;

import Property.Abstract;
import Property.Group;

namespace Artifact
{

 class ArtifactAudioLayer::Impl
 {
 public:
  float volume_ = 1.0f;
  bool muted_ = false;

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

 std::vector<ArtifactCore::PropertyGroup> ArtifactAudioLayer::getLayerPropertyGroups() const
 {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup audioGroup(QStringLiteral("Audio"));

  auto makeProp = [](const QString& name, ArtifactCore::PropertyType type, const QVariant& value, int priority = 0) {
   auto p = std::make_shared<ArtifactCore::AbstractProperty>();
   p->setName(name);
   p->setType(type);
   p->setValue(value);
   p->setDisplayPriority(priority);
   return p;
  };

  auto volumeProp = makeProp(QStringLiteral("audio.volume"), ArtifactCore::PropertyType::Float, impl_->volume_, -120);
  volumeProp->setHardRange(0.0, 1.0);
  volumeProp->setSoftRange(0.0, 1.0);
  volumeProp->setStep(0.01);
  volumeProp->setUnit(QStringLiteral("linear"));
  volumeProp->setTooltip(QStringLiteral("Audio gain (0.0 - 1.0)"));
  audioGroup.addProperty(volumeProp);
  audioGroup.addProperty(makeProp(QStringLiteral("audio.muted"), ArtifactCore::PropertyType::Boolean, impl_->muted_, -110));

  groups.push_back(audioGroup);
  return groups;
 }

 bool ArtifactAudioLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
 {
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
  return true;
 }

}
